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
 * @file ble_blps.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
 
#include "ble_blps.h"

gatt_svc_t blood_pressure_svc = NULL;
gatt_chr_t blood_pressure_chr0 = NULL;
gatt_chr_t blood_pressure_chr1 = NULL;
gatt_chr_t blood_pressure_chr2 = NULL;
gatt_chr_t blood_pressure_chr3 = NULL;

void blood_pressure_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            //co_printf("BLOOD_PRESSURE Start %X\n", in->result);
            break;
        }
    }
}

void blood_pressure_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("GATTS_EV_CHR_VALUE_SEND_START\n");
            gatts_chr_val_indicate(in->peer,in->chr,0x00,1);
            break;
        }
    }
}

void blood_pressure_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("GATTS_EV_CHR_VALUE_SEND_START\n");
            gatts_chr_val_notif(in->peer,in->chr,0x00,1);
            break;
        }
    }
}

void blood_pressure_chr2_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x64};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            //co_printf("Blood Pressure Measurement value read successfully\n");
            break;
        }
    }
}

void blood_pressure_chr3_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ:{
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            //co_printf("Record Access Control Point Write val_len:%d\n", in->val_len);
            gatts_chr_val_write_rsp(in->peer,blood_pressure_chr0,GATT_OK);

            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("GATTS_EV_CHR_VALUE_SEND_START\n");
            gatts_chr_val_indicate(in->peer,in->chr,0x00,1);
            break;
        }
    }
}

int ble_blood_pressure_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x1810)
    };
    blood_pressure_svc = gatts_svc_create(&svc_uuid, blood_pressure_svc_cbk, NULL);

    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A35)
    };
    blood_pressure_chr0 = gatts_chr_add(blood_pressure_svc, &chr0_uuid
                           , GATT_CHR_PROP_INDICATE
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , blood_pressure_chr0_cbk, NULL);
    gatts_dsc_ccc_add(blood_pressure_chr0
                   , GATT_CHR_PROP_INDICATE 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr1_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A36)
    };
    blood_pressure_chr1 = gatts_chr_add(blood_pressure_svc, &chr1_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , blood_pressure_chr1_cbk, NULL);
    gatts_dsc_ccc_add(blood_pressure_chr1
                   , GATT_CHR_PROP_NOTIFY 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr2_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A49)
    };
    blood_pressure_chr2 = gatts_chr_add(blood_pressure_svc, &chr2_uuid
                           , GATT_CHR_PROP_READ
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , blood_pressure_chr2_cbk, NULL);

    uuid_t chr3_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A52)
    };
    blood_pressure_chr3 = gatts_chr_add(blood_pressure_svc, &chr3_uuid
                           , GATT_CHR_PROP_INDICATE | GATT_CHR_PROP_WRITE
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , blood_pressure_chr3_cbk, NULL);
    gatts_dsc_ccc_add(blood_pressure_chr3
                   , GATT_CHR_PROP_INDICATE 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(blood_pressure_svc);
    
    return 0;
}
