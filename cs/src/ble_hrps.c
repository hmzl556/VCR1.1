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
 * @file ble_hrps.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
 
#include "ble_hrps.h"

gatt_svc_t hrps_svc = NULL;
gatt_chr_t hrps_chr0 = NULL;

void hrps_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            //co_printf("HEART_RATE_SERVICE Start %X\n", in->result);
            break;
        }
    }
}

void hrps_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    uint8_t val[] = {"Heart Rate Measurement:162bpm,Sensor Contact Not Supported"};
    switch (event) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("GATTS_EV_CHR_VALUE_SEND_START\n");
            gatts_chr_val_notif(in->peer,in->chr,val,sizeof(val)-1);
            break;
        }
    }
}

int ble_hrps_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x180D)
    };
    hrps_svc = gatts_svc_create(&svc_uuid, hrps_svc_cbk, NULL);

    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A37)
    };
    hrps_chr0 = gatts_chr_add(hrps_svc, &chr0_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , hrps_chr0_cbk, NULL);    
    gatts_dsc_ccc_add(hrps_chr0
                   , GATT_CHR_PROP_NOTIFY 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(hrps_svc);
    
    return 0;
}
