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
 * @file ble_glps.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "ble_glps.h"



gatt_svc_t svc_glps = NULL;
gatt_chr_t chr_glps_meas = NULL;
gatt_chr_t chr_glps_meas_ctx = NULL;
gatt_chr_t chr_glps_feature = NULL;
gatt_chr_t chr_glps_racp = NULL;

void svc_glps_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            //co_printf("Svc Start %X\n", in->result);
            break;
        }
    }
}

void chr_glps_meas_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("chr_glps_meas_ctx_ccc_enbale\n");
            break;
        }
    }
}

void chr_glps_meas_ctx_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("chr_glps_meas_ctx_ccc_enbale\n");
            break;
        }
    }
}

void chr_glps_feature_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x01, 0x02};//test data
            //co_printf("Chr1 value read\n");

            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
    }
}

void chr_glps_racp_cbk(gatt_event_t event, void *context, void *param)
{
    //co_printf("%s %d %02x\n",__FUNCTION__,__LINE__,event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;

            //co_printf("chr_glps_racp_write\n");
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            //co_printf("chr_glps_meas_ctx_ccc_enbale\n");
            break;
        }
    }
}

int ble_glps_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x1808)
    };
    svc_glps = gatts_svc_create(&svc_uuid, svc_glps_cbk, NULL);

    uuid_t chr_glps_meas_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A18)
    };
    chr_glps_meas = gatts_chr_add(svc_glps, &chr_glps_meas_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , ATT_PERM_READ 
                           , chr_glps_meas_cbk, NULL);
    gatts_dsc_ccc_add(chr_glps_meas
                   , GATT_CHR_PROP_NOTIFY 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr_glps_meas_ctx_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A34)
    };
    chr_glps_meas_ctx = gatts_chr_add(svc_glps, &chr_glps_meas_ctx_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , ATT_PERM_READ 
                           , chr_glps_meas_ctx_cbk, NULL);
    gatts_dsc_ccc_add(chr_glps_meas_ctx
                   , GATT_CHR_PROP_NOTIFY 
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr_glps_feature_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A51)
    };
    chr_glps_feature = gatts_chr_add(svc_glps, &chr_glps_feature_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ 
                           , chr_glps_feature_cbk, NULL);

    uuid_t chr_glps_rec_access_ctrl_pt_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A52)
    };
    chr_glps_racp = gatts_chr_add(svc_glps, &chr_glps_rec_access_ctrl_pt_uuid
                           , GATT_CHR_PROP_WRITE|GATT_CHR_PROP_INDICATE
                           , ATT_PERM_READ 
                           , chr_glps_racp_cbk, NULL);
    gatts_dsc_ccc_add(chr_glps_racp
                   , GATT_CHR_PROP_INDICATE 
                   , ATT_PERM_READ | ATT_PERM_WRITE);


    gatts_svc_start(svc_glps);
    
    return 0;
}

