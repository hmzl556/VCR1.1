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
 * @file ble_dfu.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include "ble_dfu.h"
#include "cs_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "qflash.h"

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

#define OTA_CMD_CONN_PARAM_UPDATE                     1
#define OTA_CMD_MTU_UPDATE                            2
#define OTA_CMD_VERSION                               3
#define OTA_CMD_CREATE_OTA_SETTING                    4
#define OTA_CMD_CREATE_OTA_IMAGE                      5
#define OTA_CMD_VALIDATE_OTA_IMAGE                    6
#define OTA_CMD_ACTIVATE_OTA_IMAGE                    7
#define OTA_CMD_JUMP_IMAGE_UPDATE                     8

#define OTA_RC_STATE_NONE                             0
#define OTA_RC_STATE_DFU_SETTING                      1
#define OTA_RC_STATE_DFU_IMAGE                        2

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

#define DFU_OPCODE_START                    (0xF1)
#define DFU_OPCODE_DATA                     (0xF2)
#define DFU_OPCODE_END                      (0xF3)
#define DFU_OPCODE_READCHIPVER              (0xF4)
#define DFU_OPCODE_ACK                      (0xFC)

struct s_start
{
    uint8_t mtu;
    uint8_t credit;
    uint8_t checksum;
};

struct s_data
{
    uint16_t index;
    uint8_t checksum;
};

struct s_end
{
    uint8_t checksum;
};

union u_rsp
{
    uint8_t *data;
    struct s_start  start;
    struct s_data  trans;
    struct s_end  end;
};

typedef struct
{
    uint8_t length;
    uint8_t rsp_code;
    uint8_t opcode;
    uint8_t result;
    union u_rsp rsp;
}dfu_response_t;

typedef struct{
    uint32_t start_address;
    uint32_t size;
    uint32_t crc;
    uint32_t version;
}Dfu_setting_bank_t;

typedef struct{

    uint32_t crc;
    Dfu_setting_bank_t app1;
    Dfu_setting_bank_t app2;
    Dfu_setting_bank_t image_update;
    uint8_t signature[64];
}Dfu_setting_t;

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
/* Private variables ---------------------------------------------------------*/
gatt_svc_t dfu_svc = NULL;
gatt_chr_t dfu_chr0 = NULL;
gatt_chr_t dfu_chr1 = NULL;
peer_t ble_peer_dfu_dev = NULL;
static uint8_t* dfu_cache = NULL;
static uint16_t rc_mtu_offset;
static uint16_t cache2_len = 0;
static boot_setting_t m_bootsetting;
static bool dfu_succeed = false;
static dfu_env_t dfu_env = {0};
boot_setting_t boot_setting __attribute__((at(BR_BOOTSETTING_START_ADDRESS))) __attribute__((used));

static void* ble_dfu_malloc_cache(uint16_t size)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    return (void *)brt_malloc(size);//&env;
}

static void ble_dfu_free(void *p)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    brt_free(p);
    return;
}

static uint8_t ble_dfu_transaction_start(uint32_t image_size)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    memset(&m_ota_image,0,sizeof(m_ota_image));
    rc_mtu_offset = 0;
    dfu_env.prn_cnt = 0;
    dfu_succeed = false;
    
    m_ota_image.address = BR_APP2_START_ADDRESS;
    m_ota_image.total_size = image_size;

    if(dfu_cache == NULL)
    {
        dfu_cache = (uint8_t*)ble_dfu_malloc_cache(DFU_CACHE_BUF_SIZE);
        if(dfu_cache)
            memset(dfu_cache,0xFF,DFU_CACHE_BUF_SIZE);
        else
        {
            //co_printf("dfu_cache malloc filed");
            return DFU_RESULT_INSUFFICIENT_RESOURCES;
        }
    }

    return DFU_RESULT_SUCCESS;
}

static uint8_t ble_dfu_transaction_data(uint8_t* data , uint16_t len)
{
    int ret = DFU_RESULT_SUCCESS;
    //Check if the length is larger than the buffer remaining
    uint16_t cache_free_len = DFU_CACHE_BUF_SIZE - rc_mtu_offset;
    cache2_len = len - MIN(cache_free_len,len);
    
    m_ota_image.size += len;
    //co_printf("m_ota_image.total_size:%d,m_ota_image.size:%d",m_ota_image.total_size,m_ota_image.size);
    
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
    
    //The flash is write in @DFU_CACHE_BUF_SIZE bytes
    if(rc_mtu_offset == DFU_CACHE_BUF_SIZE)
    {
        //The flash is erased in 4K bytes
        if((m_ota_image.address + m_ota_image.offset) % FLASH_SECTOR_SIZE == 0)
        {
            //co_printf("bsp_flash_erase_sector --> 0x%08X\r\n",m_ota_image.address + m_ota_image.offset);
            Qflash_Erase_Sector(m_ota_image.address + m_ota_image.offset);
        }
        //co_printf("bsp_flash_write --> 0x%08X\r\n",m_ota_image.address + m_ota_image.offset);
        //LOG_VBS_ARRAY_EX("flash",dfu_cache, DFU_CACHE_BUF_SIZE);
        Qflash_Write(m_ota_image.address + m_ota_image.offset, dfu_cache, DFU_CACHE_BUF_SIZE);
        ret |= memcmp((const uint8_t *)(m_ota_image.address + m_ota_image.offset), (const uint8_t *)dfu_cache, DFU_CACHE_BUF_SIZE);
        
        m_ota_image.offset += rc_mtu_offset;
        rc_mtu_offset = 0;
        
        if(cache2_len != 0)
        {
            memcpy(dfu_cache + rc_mtu_offset, data+len-cache2_len, cache2_len);
            rc_mtu_offset = cache2_len;
            
            //write last packet
            if(m_ota_image.total_size == m_ota_image.size)
            {
                //The flash is erased in 4K bytes
                if((m_ota_image.address + m_ota_image.offset) % FLASH_SECTOR_SIZE == 0)
                {
                    Qflash_Erase_Sector(m_ota_image.address + m_ota_image.offset);
                }
                
                memset(dfu_cache+rc_mtu_offset,0xFF,DFU_CACHE_BUF_SIZE-rc_mtu_offset);
                //co_printf("flash",dfu_cache, DFU_CACHE_BUF_SIZE);
                Qflash_Write(m_ota_image.address + m_ota_image.offset, dfu_cache, DFU_CACHE_BUF_SIZE);
                ret |= memcmp((const uint8_t *)(m_ota_image.address + m_ota_image.offset), (const uint8_t *)dfu_cache, DFU_CACHE_BUF_SIZE);
            }
            //co_printf("cache2_len:%d",cache2_len);
        }
    }
    return (ret == DFU_RESULT_SUCCESS) ? DFU_RESULT_SUCCESS : DFU_RESULT_DEVICE_STATE_ERROR;
}

static  uint32_t ble_dfu_crc32_sw(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc)
{
    uint8_t i;
    uint32_t crc = *p_crc;
    while(size--){
        crc ^= (uint32_t)(*p_data++) << 24;
        for(i = 0; i < 8; ++i){
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04C11DB7;
            else
                crc <<= 1;
        }
    }
    return crc;
}

#define LITTLE_TO_BIG(u32)    \
(uint32_t) ((u32 & 0x000000FF) << 24) | \
(uint32_t) ((u32 & 0x0000FF00) << 8) | \
(uint32_t) ((u32 & 0x00FF0000) >> 8) | \
(uint32_t) ((u32 & 0xFF000000) >> 24)

uint32_t ble_dfu_crc32(uint8_t * p_data, uint32_t len)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    if(len%4 > 0)return 0;
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_CRC, ENABLE);
    CRC32_ResetCrc();
    uint32_t index = 0;
    uint32_t *u32p_data = (uint32_t *)p_data;
    for (index = 0; index < len/4; index++)
    {
            CRC->CRC32DAT = LITTLE_TO_BIG(u32p_data[index]);
    }
    return (CRC->CRC32DAT);
}

static uint8_t ble_dfu_transaction_end(uint32_t crc,uint32_t rcv_len)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    //check ota file crc
    uint8_t ret = DFU_RESULT_SUCCESS;
    uint32_t flash_crc = 0;

#if 0
    flash_crc = dfu_crc32_sw((uint8_t*)m_ota_image.address,rcv_len,NULL);
#else
    flash_crc = ble_dfu_crc32((uint8_t*)m_ota_image.address,rcv_len);//br1601a01 hardware crc32
#endif
    //co_printf("flash_crc %08x,crc %08x",flash_crc,crc);

    if(flash_crc == crc){
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

        m_bootsetting.crc = ble_dfu_crc32((uint8_t *)&m_bootsetting.crc + 4, sizeof(boot_setting_t) - 4);

        Qflash_Erase_Sector(BR_BOOTSETTING_START_ADDRESS);
        Qflash_Write(BR_BOOTSETTING_START_ADDRESS, (uint8_t *)&m_bootsetting, sizeof(boot_setting_t));
    }
    else {
        ret = DFU_RESULT_CS_ERROR;
    }

    if(dfu_cache)
    {
        ble_dfu_free(dfu_cache);
    }
    
    if(ret == DFU_RESULT_SUCCESS)
        dfu_succeed = true;
    
    return ret;
}

bool ble_dfu_is_finished(void)
{
    return dfu_succeed;
}

static void ble_dfu_end(dfu_response_t *response)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    response->result = ble_dfu_transaction_end(dfu_env.obj_crc32,dfu_env.obj_recv_len);
}

static void ble_dfu_data(uint8_t* data, uint8_t length, dfu_response_t *response)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    response->length = DFU_RSP_SIZE_DAT_ACK;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        //co_printf("env status %d\n", dfu_env.lock_status);
        return;
    }
    
    uint16_t index = data[1] + (data[2]<<8);
    //co_printf("index:%d, len:%d\n", index, length-4);
    //index need judged?

    ble_dfu_transaction_data(&data[3], length-4);

    // Cal CRC
    dfu_env.obj_crc32 = ble_dfu_crc32_sw(&data[3], length-4,&dfu_env.obj_crc32);
    dfu_env.obj_recv_len += length-4;
    dfu_env.prn_cnt++; 
    //co_printf("prn:%d, cnt:%d,crc:%08x\n", dfu_env.prn, dfu_env.prn_cnt, dfu_env.obj_crc32);
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
    //co_printf("dfu_env.obj_recv_len:%d,dfu_env.obj_size:%d",dfu_env.obj_recv_len,dfu_env.obj_size);
    if(dfu_env.obj_recv_len == dfu_env.obj_size)
    {
         dfu_env.prn_cnt = 0;
         response->length = 4;
         response->rsp_code = DFU_OPCODE_END;
         response->opcode = 1;
         response->result = 0;
         ble_dfu_end(response);
         response->rsp.end.checksum = (response->rsp_code + response->opcode + response->result)&0xff;
    }
}

static void ble_dfu_start(uint8_t* data, uint8_t length, dfu_response_t *response)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    if(length != 7)
    {
        //response need fixed
        //co_printf("Created size NOT match to images.length:%08x\n", length);
        response->result = DFU_RESULT_INVALID_PARAMETER;
        return;
    }

    Qflash_Lock(0);//unlock 0-256
    
    dfu_env.obj_type = data[1];
    dfu_env.obj_size = (data[2]<<0)|(data[3]<<8)|(data[4]<<16)|(data[5]<<24);
    dfu_env.obj_recv_len = 0;
    dfu_env.obj_crc32 = 0xffffffff;;
    dfu_env.prn = 3;

    ble_dfu_transaction_start(dfu_env.obj_size);
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    response->result = DFU_RESULT_SUCCESS;
    response->length += sizeof(struct s_start);
    response->opcode = data[0];
    response->rsp.start.mtu = 251;
    response->rsp.start.credit = dfu_env.prn;
    response->rsp.start.checksum = (response->rsp_code + response->opcode + response->result + response->rsp.start.mtu + response->rsp.start.credit)&0xff;
}

static uint8_t ble_dfu_data_checksum(uint8_t *data, uint16_t len)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    uint8_t check_sum = 0;
    for(uint16_t i=0; i<len; i++)
    {
        check_sum = (check_sum + data[i])&0xff;
    }

    return check_sum;
}

void ble_dfu_action_init(void)
{
    Qflash_Init();
    
    dfu_env.lock_status = DFU_STATUS_ENABLED;
}

void ble_dfu_action_write(uint8_t *data, uint32_t len, dfu_response_t *response)
{
    //co_printf("%s %d %02X \r\n",__FUNCTION__,__LINE__,data[0]);
    uint8_t opcode = data[0];
    response->rsp_code = DFU_OPCODE_ACK;
    response->opcode = opcode;
    response->result = DFU_RESULT_SUCCESS;
    response->length = DFU_RSP_SIZE_NO_EXT_DATA;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        //co_printf("DFU disabled: %02x ", dfu_env.lock_status);
        response->result = DFU_RESULT_DEVICE_STATE_ERROR;
        return;
    }

    //co_printf("%s,len:%d, opcode:%x\n", __func__, len, opcode);
    switch(opcode)
    {
        case DFU_OPCODE_START:
            ble_dfu_start(data,len,response);
            return;
        case DFU_OPCODE_DATA:
            ble_dfu_data(data,len,response);
            break;
        case DFU_OPCODE_END:
            response->result = DFU_RESULT_TERMINATED_BY_MASTER;
            break;
        default:
            response->result = DFU_RESULT_OPCODE_NOT_SUPPORT;
            break;
    }
    //co_printf("response->result(%d).\n", response->result);
}

void ble_dfu_response(dfu_response_t *dfu_rsp_data)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    if(dfu_rsp_data->length == 0)
        return;

    if((dfu_rsp_data->opcode == DFU_OPCODE_DATA)&&(dfu_rsp_data->rsp.trans.index == 0) )
    {
        return;
    }

    for(uint8_t i=0;i<dfu_rsp_data->length;i++){
        //co_printf("%02X ",*(((uint8_t*)&dfu_rsp_data->rsp_code)+i));
    }
    gatts_chr_val_notif(ble_peer_dfu_dev,dfu_chr0,(uint8_t*)&dfu_rsp_data->rsp_code,dfu_rsp_data->length);
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
}

void ble_dfu_svc_cbk(gatt_event_t event, void *context, void *param)
{
    
}

void ble_dfu_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
    dfu_response_t dfu_rsp_data = {0};
     switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            //co_printf("Chr0 value write %D %X\n", in->val_len, *in->val);
            if(ble_dfu_data_checksum((uint8_t*)(in->val), in->val_len-1) == in->val[in->val_len-1])
            {
                ble_dfu_action_write((uint8_t*)in->val, in->val_len, &dfu_rsp_data);
                ble_dfu_response(&dfu_rsp_data);
            }
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
                ble_peer_dfu_dev = in->peer;
                dfu_chr0 = in->chr;
            //co_printf("ble_peer_dfu_dev %p dfu_chr0 %p\r\n",ble_peer_dfu_dev,dfu_chr0);
            break;
        }
    }
    //co_printf("%s %d\r\n",__FUNCTION__,__LINE__);
}

void ble_dfu_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    
}

int ble_dfu_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF10)
    };
    dfu_svc = gatts_svc_create(&svc_uuid, ble_dfu_svc_cbk, NULL);

    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF11)
    };
    dfu_chr0 = gatts_chr_add(dfu_svc, &chr0_uuid
                           , GATT_CHR_PROP_WRITE_WITHOUT_RSP | GATT_CHR_PROP_NOTIFY
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , ble_dfu_chr0_cbk, NULL);

    gatts_dsc_ccc_add(dfu_chr0
                   , GATT_CHR_PROP_NOTIFY 
                   , ATT_PERM_READ | ATT_PERM_WRITE);
    
    uuid_t chr1_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF12)
    };
    dfu_chr1 = gatts_chr_add(dfu_svc, &chr1_uuid
                           , GATT_CHR_PROP_WRITE_WITHOUT_RSP | GATT_CHR_PROP_NOTIFY
                           , ATT_PERM_READ 
                           , ble_dfu_chr1_cbk, NULL);
    
//    gatts_dsc_ccc_add(dfu_chr1
//                   , GATT_CHR_PROP_NOTIFY 
//                   , ATT_PERM_READ | ATT_PERM_WRITE);
    
    
    gatts_svc_start(dfu_svc);
    
    return 0;
}
