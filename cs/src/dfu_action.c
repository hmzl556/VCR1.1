/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Barrot's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY BARROT'S "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL BARROT'S BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file br_dfu_protocol.c
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "dfu_action.h"
#include "dfu_crc.h"
#include "stdio.h"
#include <stdbool.h>
#include <string.h>
#include "br_log.h"
#include "br1601axx_qflash.h"
#include "ke_mem.h"
#include "ke_msg.h"
#include "ke_timer.h"
#include "br_ble_task.h"

#ifdef BLE_BRT_DFU_SERVER

/* Private define ------------------------------------------------------------*/
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif 

#define DFU_CACHE_BUF_SIZE           512 //according to Qflash_Write()

extern int Image$$ER_IROM1$$Base;
#define CURRENT_APP_START_ADDRESS                      (uint32_t)&Image$$ER_IROM1$$Base

#define BR_MASTERBOOT_START_ADDRESS                    (0x01000000)
#define BR_MASTERBOOT_SIZE                             (0x2000)
#define BR_BOOTSETTING_START_ADDRESS                   (0x01002000)
#define BR_BOOTSETTING_SIZE                            (0x1000)
#define BR_APP_DATA_START_ADDRESS                      (0x01003000)
#define BR_APP_DATA_SIZE                               (0x1000)
#define BR_APP1_START_ADDRESS                          (0x01004000)
#define BR_APP1_DEFAULT_SIZE                           (0x1C000)
#define BR_APP2_START_ADDRESS                          (0x01020000)
#define BR_APP2_DEFAULT_SIZE                           (0x1C000)
#define BR_IMAGE_UPDATE_START_ADDRESS                  (0x0103C000)
#define BR_IMAGE_UPDATE_SIZE                           (0x4000)

#define BR_BOOTSETTING_ACTIVATION_YES                       (0x00000001)
#define BR_BOOTSETTING_ACTIVATION_NO                        (0xFFFFFFFF)
#define BR_BOOTSETTING_MASTER_BOOT_FORCE_UPDATE_YES         (0x00000001)
#define BR_BOOTSETTING_MASTER_BOOT_FORCE_UPDATE_NO          (0xFFFFFFFF)

#define DFU_RESULT_SUCCESS                  (0x00)
#define DFU_RESULT_CS_ERROR                 (0x01)
#define DFU_RESULT_ACK_TIMEOUT              (0x02)
#define DFU_RESULT_TERMINATED_BY_MASTER     (0x03)
#define DFU_RESULT_TERMINATED_BY_SLAVE      (0x04)
#define DFU_RESULT_DEVICE_STATE_ERROR       (0x05)
#define DFU_RESULT_OPCODE_NOT_SUPPORT       (0x06)
#define DFU_RESULT_INSUFFICIENT_RESOURCES   (0x07)
#define DFU_RESULT_FW_OVER_SIZE             (0x08)
#define DFU_RESULT_INVALID_PARAMETER        (0x09)

// DFU_RSP_SIZE
#define DFU_RSP_SIZE_NO_DATA      0
#define DFU_RSP_SIZE_NO_EXT_DATA  3
#define DFU_RSP_SIZE_CHECKSUM     (3+4*2)
#define DFU_RSP_SIZE_SELECT       (3+4*3)
#define DFU_RSP_SIZE_DAT_ACK      (3+3)

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    uint32_t start_address;
    uint32_t size;
    uint32_t crc;
    uint32_t version;
    uint32_t activation;
    uint32_t reserve[5];
}br_bank_t;

typedef struct
{
    uint32_t crc;
    uint32_t master_boot_force_update;
    br_bank_t app1;
    br_bank_t app2;
    br_bank_t ImageUpdate;
    uint8_t public_key[64];
}boot_setting_t;

enum
{
    DFU_STATUS_ENABLED = 0,
    DFU_STATUS_DISABLED,
    DFU_STATUS_LOCKED,
};

static struct{
    uint32_t total_size;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t crc;
}m_ota_image;

typedef struct
{
    uint32_t     obj_size;
    uint32_t     obj_recv_len;
    uint32_t     obj_crc32;
    uint8_t      lock_status;
    uint8_t      obj_actived;
    uint8_t      obj_type;
    uint8_t      prn;
    uint8_t      prn_cnt;//??
}dfu_env_t;
 
/* Private constants ---------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static uint8_t* dfu_cache = NULL;
static uint16_t rc_mtu_offset;
static uint16_t cache2_len = 0;
static boot_setting_t m_bootsetting;
static dfu_env_t dfu_env = {0};
boot_setting_t boot_setting __attribute__((at(BR_BOOTSETTING_START_ADDRESS))) __attribute__((used));
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static void* dfu_malloc_cache(uint16_t size)
{
    return ke_malloc(size, KE_MEM_ENV);//&env;
}

static void dfu_free(void *p)
{
    ke_free(p);
    return;
}

static uint8_t dfu_transaction_start(uint32_t image_size)
{
    memset(&m_ota_image,0,sizeof(m_ota_image));
    rc_mtu_offset = 0;
    dfu_env.prn_cnt = 0;
    
    
    
    m_ota_image.address = BR_APP2_START_ADDRESS;
    m_ota_image.total_size = image_size;

    if(dfu_cache == NULL)
    {
        dfu_cache = (uint8_t*)dfu_malloc_cache(DFU_CACHE_BUF_SIZE);

        if(dfu_cache)
            memset(dfu_cache,0xFF,DFU_CACHE_BUF_SIZE);
        else
        {
            BR_LOG_ERROR("dfu_cache malloc filed");
            return DFU_RESULT_INSUFFICIENT_RESOURCES;
        }
    }

    return DFU_RESULT_SUCCESS;
}

static uint8_t dfu_transaction_data(uint8_t* data , uint16_t len)
{
    //Check if the length is larger than the buffer remaining
    uint16_t cache_free_len = DFU_CACHE_BUF_SIZE - rc_mtu_offset;
    cache2_len = len - MIN(cache_free_len,len);
    
    m_ota_image.size += len;
    BR_LOG_DEBUG("m_ota_image.total_size:%d,m_ota_image.size:%d",m_ota_image.total_size,m_ota_image.size);
    
    //check last packet(Not accepted data + Level 2 cache data < written data)
    if(m_ota_image.total_size - m_ota_image.size + cache2_len <= DFU_CACHE_BUF_SIZE)
    {
        memcpy(dfu_cache + rc_mtu_offset, data ,MIN(cache_free_len,len));
        rc_mtu_offset += MIN(cache_free_len,len);
        
        //Data other than the last packet of valid data is set to 0xFF
        if(m_ota_image.total_size == m_ota_image.size)
        {
            memset(dfu_cache+rc_mtu_offset,0xFF,DFU_CACHE_BUF_SIZE-rc_mtu_offset);
            rc_mtu_offset = DFU_CACHE_BUF_SIZE;
        }
    }
    else
    {
        memcpy(dfu_cache + rc_mtu_offset, data , MIN(cache_free_len,len));
        rc_mtu_offset += MIN(cache_free_len,len);
    }
    
    BR_LOG_DEBUG("rc_mtu_offset %d",rc_mtu_offset);
    
    //The flash is write in @DFU_CACHE_BUF_SIZE bytes
    if(rc_mtu_offset == DFU_CACHE_BUF_SIZE)
    {
        //The flash is erased in 4K bytes
        if((m_ota_image.address + m_ota_image.offset) % FLASH_SECTOR_SIZE == 0)
        {
            BR_LOG_DEBUG("bsp_flash_erase_sector --> 0x%08X\r\n",m_ota_image.address + m_ota_image.offset);
            Qflash_Erase_Sector(m_ota_image.address + m_ota_image.offset);
        }
        BR_LOG_DEBUG("bsp_flash_write --> 0x%08X\r\n",m_ota_image.address + m_ota_image.offset);
        //LOG_VBS_ARRAY_EX("flash",dfu_cache, DFU_CACHE_BUF_SIZE);
        Qflash_Write(m_ota_image.address + m_ota_image.offset, dfu_cache, DFU_CACHE_BUF_SIZE);
        
        m_ota_image.offset += rc_mtu_offset;
        rc_mtu_offset = 0;
        
        if(cache2_len != 0)
        {
            memcpy(dfu_cache + rc_mtu_offset, data+len-cache2_len, cache2_len);
            rc_mtu_offset = cache2_len;
            
            //write last packet
            if(m_ota_image.total_size == m_ota_image.size)
            {
                memset(dfu_cache+rc_mtu_offset,0xFF,DFU_CACHE_BUF_SIZE-rc_mtu_offset);
                BR_LOG_DEBUG("flash",dfu_cache, DFU_CACHE_BUF_SIZE);
                Qflash_Write(m_ota_image.address + m_ota_image.offset, dfu_cache, DFU_CACHE_BUF_SIZE);
            }
            BR_LOG_DEBUG("cache2_len:%d",cache2_len);
        }
    }
    return DFU_RESULT_SUCCESS;
}

static  uint32_t dfu_crc32_sw(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc)
{
    uint32_t crc;
    crc = (p_crc == NULL) ? 0xFFFFFFFF : ~(*p_crc);
    for (uint32_t i = 0; i < size; i++){
        crc = crc ^ p_data[i];
        for (uint32_t j = 8; j > 0; j--) crc = (crc >> 1) ^ (0xEDB88320U & ((crc & 1) ? 0xFFFFFFFF : 0));
    }
    return ~crc;
}

static uint8_t dfu_transaction_end(uint32_t crc,uint32_t rcv_len)
{
    //check ota file crc
    uint32_t flash_crc = 0;

#if 0
    flash_crc = dfu_crc32_sw((uint8_t*)m_ota_image.address,rcv_len,NULL);
#else
    flash_crc = dfu_crc32((uint8_t*)m_ota_image.address,rcv_len);//br1601a01 hardware crc32
#endif
    BR_LOG_DEBUG("flash_crc %08x,crc %08x",flash_crc,crc);

    //get flash bootsetting,addr:0x01002000
    memcpy(&m_bootsetting,&boot_setting,sizeof(boot_setting_t));

    m_bootsetting.app1.activation = BR_BOOTSETTING_ACTIVATION_YES;//in case of ota file crashed,APP1 can still keep,see the logic in masterboot_brt project

    m_bootsetting.app2.start_address = m_ota_image.address;
    m_bootsetting.app2.size = rcv_len;
    m_bootsetting.app2.version = 0;//FIX ME
    m_bootsetting.app2.crc = flash_crc;
    m_bootsetting.app2.activation = BR_BOOTSETTING_ACTIVATION_YES;
    
    m_bootsetting.ImageUpdate.activation = BR_BOOTSETTING_ACTIVATION_NO;
    m_bootsetting.master_boot_force_update = BR_BOOTSETTING_MASTER_BOOT_FORCE_UPDATE_NO;

    m_bootsetting.crc = dfu_crc32((uint8_t *)&m_bootsetting.crc + 4, sizeof(boot_setting_t) - 4);

    Qflash_Erase_Sector(BR_BOOTSETTING_START_ADDRESS);
    Qflash_Write(BR_BOOTSETTING_START_ADDRESS, (uint8_t *)&m_bootsetting, sizeof(boot_setting_t));
    //Qflash_Lock(FLASH_LOCK_F_256K);
    if(dfu_cache)
    {
        dfu_free(dfu_cache);
    }
    
    ke_timer_set(APP_DFU_BLE_RESET_TIMER, TASK_APP, 2000);

    return DFU_RESULT_SUCCESS;
}


static void dfu_end(dfu_response_t *response)
{
    response->result = dfu_transaction_end(dfu_env.obj_crc32,dfu_env.obj_recv_len);
}

static void dfu_data(uint8_t* data, uint8_t length, dfu_response_t *response)
{
    response->length = DFU_RSP_SIZE_DAT_ACK;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        BR_LOG_DEBUG("env status %d\n", dfu_env.lock_status);
        return;
    }
    
    uint16_t index = data[1] + (data[2]<<8);
    BR_LOG_DEBUG("index:%d, len:%d\n", index, length-4);
    //index need judged?

    dfu_transaction_data(&data[3], length-4);

    // Cal CRC
    dfu_env.obj_crc32 = dfu_crc32_sw(&data[3], length-4,&dfu_env.obj_crc32);
    dfu_env.obj_recv_len += length-4;
    dfu_env.prn_cnt++; 
    BR_LOG_DEBUG("prn:%d, cnt:%d,crc:%08x\n", dfu_env.prn, dfu_env.prn_cnt, dfu_env.obj_crc32);
    if(dfu_env.prn && (dfu_env.prn <= dfu_env.prn_cnt))
    {
        dfu_env.prn_cnt = 0;
        response->length = DFU_RSP_SIZE_DAT_ACK;
        response->rsp_code = DFU_OPCODE_ACK;
        response->opcode = DFU_OPCODE_DATA;
        response->result = DFU_RESULT_SUCCESS;
        response->rsp.trans.index = index;
        response->rsp.trans.checksum = (response->rsp_code + response->opcode + response->result + ((response->rsp.trans.index)&0xff) + ((response->rsp.trans.index)>>8))&0xff;
    }
    BR_LOG_DEBUG("dfu_env.obj_recv_len:%d,dfu_env.obj_size:%d",dfu_env.obj_recv_len,dfu_env.obj_size);
    if(dfu_env.obj_recv_len == dfu_env.obj_size)
    {
         dfu_env.prn_cnt = 0;
         response->length = 4;
         response->rsp_code = DFU_OPCODE_END;
         response->opcode = 1;
         response->result = 0;
         dfu_end(response);
         response->rsp.end.checksum = (response->rsp_code + response->opcode + response->result)&0xff;
    }
}

static void dfu_start(uint8_t* data, uint8_t length, dfu_response_t *response)
{
    if(length != 7)
    {
        //response need fixed
        BR_LOG_DEBUG("Created size NOT match to images.length:%08x\n", length);
        response->result = DFU_RESULT_INVALID_PARAMETER;
        return;
    }
    
    dfu_env.obj_type = data[1];
    dfu_env.obj_size = (data[2]<<0)|(data[3]<<8)|(data[4]<<16)|(data[5]<<24);
    dfu_env.obj_recv_len = 0;
    dfu_env.obj_crc32 = 0;
    dfu_env.prn = 3;

    dfu_transaction_start(dfu_env.obj_size);

    response->result = DFU_RESULT_SUCCESS;
    response->length += sizeof(struct s_start);
    response->opcode = data[0];
    response->rsp.start.mtu = 251;
    response->rsp.start.credit = dfu_env.prn;
    response->rsp.start.checksum = (response->rsp_code + response->opcode + response->result + response->rsp.start.mtu + response->rsp.start.credit)&0xff;
}

int app_dfu_ble_reset_handler(ke_msg_id_t const msgid, void const *p_param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    BR_LOG_DEBUG("Boot reset...");
    NVIC_SystemReset();
    return KE_MSG_CONSUMED;
}

void dfu_action_init(void)
{
    Qflash_Init();
    
    dfu_env.lock_status = DFU_STATUS_ENABLED;
}

void dfu_action_write(uint8_t *data, uint32_t len, dfu_response_t *response)
{
    uint8_t opcode = data[0];
    response->rsp_code = DFU_OPCODE_ACK;
    response->opcode = opcode;
    response->result = DFU_RESULT_SUCCESS;
    response->length = DFU_RSP_SIZE_NO_EXT_DATA;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        BR_LOG_DEBUG("DFU disabled: %02x ", dfu_env.lock_status);
        response->result = DFU_RESULT_DEVICE_STATE_ERROR;
        return;
    }

    BR_LOG_DEBUG("%s,len:%d, opcode:%x\n", __func__, len, opcode);
    switch(opcode)
    {
        case DFU_OPCODE_START:
            dfu_start(data,len,response);
            return;
        case DFU_OPCODE_DATA:
            dfu_data(data,len,response);
            break;
        case DFU_OPCODE_END:
            response->result = DFU_RESULT_TERMINATED_BY_MASTER;
            break;
        default:
            response->result = DFU_RESULT_OPCODE_NOT_SUPPORT;
            break;
    }
    BR_LOG_DEBUG("response->result(%d).\n", response->result);
}
#endif
