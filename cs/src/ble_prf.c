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
 * @file ble_prf.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_prf.h"
#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "cs_core.h"

gatt_svc_t generic_access_svc = NULL;
gatt_chr_t generic_access_chr0 = NULL;
gatt_chr_t generic_access_chr1 = NULL;

gatt_svc_t generic_attribute_svc = NULL;
gatt_chr_t generic_attribute_chr0 = NULL;
gatt_chr_t generic_attribute_chr1 = NULL;
gatt_chr_t generic_attribute_chr2 = NULL;

#define DEVICE_NAME   "BR1601Axx"

void generic_access_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            //co_printf("GENERIC_ACCESS Start %X\n", in->result);
            break;
        }
    }
}

void generic_access_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    static uint8_t *name = NULL;
    static uint8_t name_len = 0;
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ:{
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            //co_printf("Device_Name write val_len:%d\n", in->val_len);
            gatts_chr_val_write_rsp(in->peer,generic_access_chr0,GATT_OK);
            if(name != NULL){
                brt_free(name);
                name = NULL;
                name_len = in->val_len;
                name = (uint8_t*)brt_malloc(in->val_len);
                memcpy(name,in->val,in->val_len);
            } else{
                name_len = in->val_len;
                name = (uint8_t*)brt_malloc(in->val_len);
                memcpy(name,in->val,in->val_len);
            }
            break;
        }
        case GATTS_EV_CHR_VALUE_READ_REQ:{
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            if(name == NULL){
                gatts_chr_val_read_rsp(in->peer,generic_access_chr0,GATT_OK,DEVICE_NAME,sizeof(DEVICE_NAME)-1);
            }else{
                gatts_chr_val_read_rsp(in->peer,generic_access_chr0,GATT_OK,name,name_len);
            }
            //co_printf("Device_Name value read successfully\r\n");
            break;
        }
    }
}

void generic_access_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x00,0x00};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            //co_printf("Appearance value read successfully\n");
            break;
        }
    }
}

int ble_generic_access_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x1800)
    };
    generic_access_svc = gatts_svc_create(&svc_uuid, generic_access_svc_cbk, NULL);
    
    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A00)
    };
    generic_access_chr0 = gatts_chr_add(generic_access_svc, &chr0_uuid
                           , GATT_CHR_PROP_WRITE|GATT_CHR_PROP_READ
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , generic_access_chr0_cbk, NULL);

    uuid_t chr1_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A01)
    };
    generic_access_chr1 = gatts_chr_add(generic_access_svc, &chr1_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ 
                           , generic_access_chr1_cbk, NULL);

    gatts_svc_start(generic_access_svc);
    
    return 0;
}

void generic_attribute_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            //co_printf("GENERIC_ATTRIBUTE Start %X\n", in->result);
            break;
        }
    }
}

void generic_attribute_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ:{
            break;
        }
    }
}

void generic_attribute_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x01};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            //co_printf("Client Support Features value read successfully\n");
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE_REQ:{
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            //co_printf("Client Support Features write val_len:%d\n", in->val_len);
            gatts_chr_val_write_rsp(in->peer,generic_attribute_chr0,GATT_OK);
            //There may be other logic present
        }
    }
}

void generic_attribute_chr2_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0xDE,0x65,0xFC,0xAC,0x52,0xD5,0x4B,0x61,0x4E,0x52,0x28,0x62,0x0C,0x78,0x28,0xE8};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            //co_printf("Database Hash value read successfully\n");
            break;
        }
    }
}

int ble_generic_attribute_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x1801)
    };
    generic_attribute_svc = gatts_svc_create(&svc_uuid, generic_attribute_svc_cbk, NULL);
    
    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A05)
    };
    generic_attribute_chr0 = gatts_chr_add(generic_attribute_svc, &chr0_uuid
                           , GATT_CHR_PROP_INDICATE
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           ,generic_attribute_chr0_cbk, NULL);
    gatts_dsc_ccc_add(generic_attribute_chr0
                   , GATT_CHR_PROP_INDICATE 
                   , ATT_PERM_READ | ATT_PERM_WRITE);
    
    
    uuid_t chr1_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2B29)
    };
    generic_attribute_chr1 = gatts_chr_add(generic_attribute_svc, &chr1_uuid
                           , GATT_CHR_PROP_WRITE|GATT_CHR_PROP_READ
                           , ATT_PERM_READ 
                           , generic_attribute_chr1_cbk, NULL);

    uuid_t chr2_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2B2A)
    };
    generic_attribute_chr2 = gatts_chr_add(generic_attribute_svc, &chr2_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ 
                           , generic_attribute_chr2_cbk, NULL);

    gatts_svc_start(generic_attribute_svc);
    return 0;
}


int ble_prf_init(void)
{
    ble_generic_access_init();
    ble_generic_attribute_init();

    return 0;
}
