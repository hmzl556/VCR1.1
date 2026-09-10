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
#include "dfu_execute.h"
#include "app_cache_owner.h"
#include "app_image_staging.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hal_flash.h"
#include "flash_manage.h"
#include "porting.h"
#include "task.h"
#include "app_ble_cfg.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"
#ifdef CFG_PRF_DFU

/* Private define ------------------------------------------------------------*/
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif 

#define DFU_MTU_DEFAULT              251 
#define DFU_CREDIT_MAX               3
#define DFU_CACHE_BUF_SIZE           512 //according to Qflash_Write()
#define DFU_CRC32_INITIAL_VALUE      0xFFFFFFFF

#define BR_MASTERBOOT_START_ADDRESS                    (0x01000000)
#define BR_MASTERBOOT_SIZE                             (0x2000)
#define BR_BOOTSETTING_START_ADDRESS                   (0x01002000)
#define BR_BOOTSETTING_SIZE                            (0x1000)
#define BR_APP_DATA_START_ADDRESS                      (0x01003000)
#define BR_APP_DATA_SIZE                               (0x1000)
#define BR_APP1_START_ADDRESS                          (FLASH_APP_ADDR)
#define BR_APP1_DEFAULT_SIZE                           (FLASH_APP_SIZE)
#define BR_APP2_START_ADDRESS                          (FLASH_APP_CACHE_ADDR)
#define BR_APP2_DEFAULT_SIZE                           (FLASH_APP_CACHE_SIZE)
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
static bool dfu_transaction_active;
static bool dfu_terminal_pending;
static bool dfu_reset_after_terminal;
boot_setting_t boot_setting __attribute__((at(BR_BOOTSETTING_START_ADDRESS))) __attribute__((used));
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
#define dfu_malloc_cache    NEW
#define dfu_free            FREE

static void dfu_transaction_release(bool wait_for_notification)
{
    if(dfu_cache != NULL)
    {
        dfu_free(dfu_cache);
        dfu_cache = NULL;
    }
    dfu_transaction_active = false;
    dfu_terminal_pending = wait_for_notification;
    flash_manage_autolock(true);
    if(!wait_for_notification)
    {
        dfu_reset_after_terminal = false;
        app_cache_release(APP_CACHE_OWNER_DFU);
    }
}

static uint8_t dfu_calc_checksum(uint8_t *data, uint32_t size)
{
    uint32_t n = 0;
    uint8_t sum = 0;

    while (n < size) {
        sum += data[n];
        n++;
    }
    return sum;
}

static int dfu_write_image(uint32_t addr, const uint8_t* data, uint16_t len)
{
    int ret = DFU_RESULT_SUCCESS;
    if(addr % FLASH_SECTOR_SIZE == 0) {
        BR_LOG_DEBUG("%s, flash_erase_sector --> 0x%08X\r\n", __func__, addr);
        hal_flash_erase_sector(addr);
    }
    BR_LOG_DEBUG("%s, flash_write --> 0x%08X\r\n", __func__, addr);
    //BR_LOG_DEBUG_HEX("flash", data, len);
    hal_flash_write(addr, (uint8_t *)data, len);
#ifdef BR1601A01
    ret = memcmp((const uint8_t *)addr, data, len);
#elif BR1601A02
    uint8_t buf[DFU_CACHE_BUF_SIZE] = {0};
    hal_flash_read(addr, buf, len);
    ret = memcmp((const uint8_t *)buf, data, len);
#endif
    BR_LOG_DEBUG("%s, ret %d\r\n",__func__, ret);
    return ret;
}

static uint8_t dfu_transaction_start(uint32_t image_size)
{
    if(dfu_terminal_pending)
    {
        return DFU_RESULT_INSUFFICIENT_RESOURCES;
    }
    if(!app_cache_acquire(APP_CACHE_OWNER_DFU))
    {
        BR_LOG_WARNING("dfu cache busy owner:%d\r\n", app_cache_owner_get());
        return DFU_RESULT_INSUFFICIENT_RESOURCES;
    }
    if(!app_image_staging_invalidate())
    {
        BR_LOG_ERROR("dfu image metadata invalidate failed\r\n");
        app_cache_release(APP_CACHE_OWNER_DFU);
        return DFU_RESULT_INSUFFICIENT_RESOURCES;
    }

    memset(&m_ota_image,0,sizeof(m_ota_image));
    rc_mtu_offset = 0;
    m_ota_image.address = BR_APP2_START_ADDRESS;
    m_ota_image.total_size = image_size;

    if(dfu_cache == NULL)
    {
        dfu_cache = (uint8_t*)dfu_malloc_cache(DFU_CACHE_BUF_SIZE);

        if(dfu_cache)
            memset(dfu_cache,0xFF,DFU_CACHE_BUF_SIZE);
        else
        {
            BR_LOG_ERROR("dfu_cache malloc filed\r\n");
            app_cache_release(APP_CACHE_OWNER_DFU);
            return DFU_RESULT_INSUFFICIENT_RESOURCES;
        }
    }

    dfu_transaction_active = true;
    dfu_terminal_pending = false;
    dfu_reset_after_terminal = false;
    return DFU_RESULT_SUCCESS;
}

static uint8_t dfu_transaction_data(uint8_t* data , uint16_t len)
{
    int ret = DFU_RESULT_SUCCESS;
    //Check if the length is larger than the buffer remaining
    uint16_t cache_free_len = DFU_CACHE_BUF_SIZE - rc_mtu_offset;
    cache2_len = len - MIN(cache_free_len,len);
    
    m_ota_image.size += len;
    BR_LOG_DEBUG("%s, total_size:%d, rcv_size:%d\r\n", __func__, m_ota_image.total_size, m_ota_image.size);
    
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
    
    BR_LOG_DEBUG("%s, rc_mtu_offset %d\r\n", __func__, rc_mtu_offset);
    
    //The flash is write in @DFU_CACHE_BUF_SIZE bytes
    if(rc_mtu_offset == DFU_CACHE_BUF_SIZE)
    {
        ret = dfu_write_image(m_ota_image.address + m_ota_image.offset, dfu_cache, DFU_CACHE_BUF_SIZE);
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
                ret |= dfu_write_image(m_ota_image.address + m_ota_image.offset, (const uint8_t *)dfu_cache, DFU_CACHE_BUF_SIZE);
            }
            BR_LOG_DEBUG("%s, cache2_len:%d\r\n", __func__, cache2_len);
        }
    }
    return (ret == DFU_RESULT_SUCCESS) ? DFU_RESULT_SUCCESS : DFU_RESULT_DEVICE_STATE_ERROR;
}

static  uint32_t dfu_crc32_sw(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc)
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

static uint8_t dfu_transaction_end(uint32_t crc,uint32_t rcv_len)
{
	uint8_t ret = DFU_RESULT_SUCCESS;
    //check ota file crc
    uint32_t flash_crc = DFU_CRC32_INITIAL_VALUE;

	/*Read data from flash and calculate the CRC32 checksum*/
    uint8_t buf[FLASH_PAGE_SIZE] = {0};
    uint32_t len = rcv_len;
    uint32_t addr = m_ota_image.address;
    while(len) {
        uint32_t read_size = MIN(len, FLASH_PAGE_SIZE);
        hal_flash_read(addr, buf, read_size);
        flash_crc = dfu_crc32_sw(buf, read_size, &flash_crc);
        len -= read_size;
        addr += read_size;
    }
    BR_LOG_DEBUG("%s, flash_crc %08x,crc %08x\r\n",__func__, flash_crc, crc);

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
		
		m_bootsetting.crc = DFU_CRC32_INITIAL_VALUE;
	    m_bootsetting.crc = dfu_crc32_sw((uint8_t *)&m_bootsetting.crc + 4, sizeof(boot_setting_t) - 4, &m_bootsetting.crc);

        uint32_t times = 10, retry = true;//todo 
        while(retry && times) {
            ret = retry = dfu_write_image(BR_BOOTSETTING_START_ADDRESS, (uint8_t *)&m_bootsetting, sizeof(boot_setting_t));
            times --; 
        }
    }
	else {
		ret = DFU_RESULT_CS_ERROR;
	}

    dfu_transaction_release(true);
    dfu_reset_after_terminal = (ret == DFU_RESULT_SUCCESS);
    return ret;
}


static void dfu_end(dfu_response_t *response)
{
    response->result = dfu_transaction_end(dfu_env.obj_crc32,dfu_env.obj_recv_len);
}

static void dfu_data(dfu_data_t* p_data, uint8_t length, dfu_response_t *response)
{
    response->length = DFU_RSP_SIZE_DAT_ACK;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        BR_LOG_DEBUG("%s, lock_status %d\r\n",__func__, dfu_env.lock_status);
        return;
    }
    
    uint16_t index = DECODE2BYTE_LITTLE(p_data->index);
    length -= 2; //sizeof(p_data->index)
    BR_LOG_DEBUG("%s, index:%d, len:%d\r\n",__func__, index, length);
    //index need judged?
    dfu_transaction_data(p_data->data, length);

    // Cal CRC
    dfu_env.obj_crc32 = dfu_crc32_sw(p_data->data, length, &dfu_env.obj_crc32);
    dfu_env.obj_recv_len += length;
    dfu_env.prn_cnt++; 
    BR_LOG_DEBUG("%s, prn:%d, cnt:%d, crc:%08x\r\n", __func__, dfu_env.prn, dfu_env.prn_cnt, dfu_env.obj_crc32);
    if(dfu_env.prn && (dfu_env.prn <= dfu_env.prn_cnt))
    {
        dfu_env.prn_cnt = 0;
        response->length = DFU_RSP_SIZE_DAT_ACK;
        response->rsp_code = DFU_OPCODE_ACK;
        response->opcode = DFU_OPCODE_DATA;
        response->result = DFU_RESULT_SUCCESS;
        response->rsp.trans.index = index;
        response->rsp.trans.checksum = dfu_calc_checksum(&response->rsp_code, response->length-1);
    }
    BR_LOG_DEBUG("%s, obj_recv_len:%d, obj_size:%d\r\n",__func__, dfu_env.obj_recv_len,dfu_env.obj_size);
    if(dfu_env.obj_recv_len == dfu_env.obj_size)
    {
         dfu_env.prn_cnt = 0;
         response->length = 4;
         response->rsp_code = DFU_OPCODE_END;
         response->opcode = 1;
         response->result = DFU_RESULT_SUCCESS;
         dfu_end(response);
         response->rsp.end.checksum = dfu_calc_checksum(&response->rsp_code, response->length-1);
    }
}

static void dfu_start(dfu_start_t* p_start, uint8_t length, dfu_response_t *response)
{
    if(length != sizeof(dfu_start_t))
    {
        //response need fixed
        BR_LOG_DEBUG("Created size NOT match to images.length:%08x\r\n", length);
        response->result = DFU_RESULT_INVALID_PARAMETER;
        return;
    }
    
    dfu_env.obj_type = p_start->type;
    dfu_env.obj_size = DECODE4BYTE_LITTLE(p_start->total_len);
    dfu_env.obj_recv_len = 0;
    dfu_env.obj_crc32 = 0xffffffff;
    dfu_env.prn = DFU_CREDIT_MAX;
    dfu_env.prn_cnt = 0;

    response->result = dfu_transaction_start(dfu_env.obj_size);
    if(response->result != DFU_RESULT_SUCCESS)
    {
        return;
    }
    response->length += sizeof(struct s_start);
    response->rsp.start.mtu = DFU_MTU_DEFAULT;
    response->rsp.start.credit = DFU_CREDIT_MAX;
    response->rsp.start.checksum = dfu_calc_checksum(&response->rsp_code, response->length-1);
}


void dfu_action_init(void)
{
    dfu_env.lock_status = DFU_STATUS_ENABLED;
    dfu_transaction_active = false;
    dfu_terminal_pending = false;
    dfu_reset_after_terminal = false;
}

void dfu_action_notification_complete(void)
{
    if(dfu_terminal_pending &&
       app_cache_owner_get() == APP_CACHE_OWNER_DFU)
    {
        bool reset_system = dfu_reset_after_terminal;

        dfu_terminal_pending = false;
        dfu_reset_after_terminal = false;
        app_cache_release(APP_CACHE_OWNER_DFU);
        if(reset_system)
        {
            void user_system_reset_task(void);
            TIMER_CREATE(2000, NULL, user_system_reset_task);
        }
    }
}

void dfu_action_notification_failed(void)
{
    if(dfu_terminal_pending &&
       app_cache_owner_get() == APP_CACHE_OWNER_DFU)
    {
        dfu_transaction_release(false);
    }
}

void dfu_action_disconnect(void)
{
    if(app_cache_owner_get() != APP_CACHE_OWNER_DFU)
    {
        return;
    }
    memset(&m_ota_image, 0, sizeof(m_ota_image));
    rc_mtu_offset = 0U;
    dfu_transaction_release(false);
}

void dfu_action_write(uint8_t *data, uint32_t len, dfu_response_t *response)
{
    dfu_packet_t *p_packet = (dfu_packet_t *)data;
    response->rsp_code = DFU_OPCODE_ACK;
    response->opcode = p_packet->opcode;
    response->result = DFU_RESULT_SUCCESS;
    response->length = DFU_RSP_SIZE_NO_EXT_DATA;
    if(dfu_env.lock_status != DFU_STATUS_ENABLED)
    {
        BR_LOG_DEBUG("%s, DFU disabled: %02x \r\n",__func__, dfu_env.lock_status);
        response->result = DFU_RESULT_DEVICE_STATE_ERROR;
        return;
    }

    BR_LOG_DEBUG("%s,len:%d, opcode:%x\r\n", __func__, len, p_packet->opcode);
    if((p_packet->opcode == DFU_OPCODE_DATA ||
        p_packet->opcode == DFU_OPCODE_END) &&
       (!dfu_transaction_active ||
        app_cache_owner_get() != APP_CACHE_OWNER_DFU))
    {
        response->result = DFU_RESULT_DEVICE_STATE_ERROR;
        return;
    }
    ///Remove opcode
    switch(p_packet->opcode)
    {
        case DFU_OPCODE_START:
            dfu_start((dfu_start_t *)p_packet->data, len-1,response);
            if(response->result == DFU_RESULT_SUCCESS)
            {
                flash_manage_autolock(false);
            }
            return;
        case DFU_OPCODE_DATA:
            dfu_data((dfu_data_t*)p_packet->data, len-1,response);
            break;
        case DFU_OPCODE_END:
            response->result = DFU_RESULT_TERMINATED_BY_MASTER;
            dfu_transaction_release(true);
            break;
        default:
            response->result = DFU_RESULT_OPCODE_NOT_SUPPORT;
            break;
    }
    BR_LOG_DEBUG("%s, response->result(%d).\r\n", __func__, response->result);
}
#endif
