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
 * @file ble_cgms.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */


#include "ble_cgms.h"
typedef struct att_desc
{
    gatt_svc_t svc_cgms;
    gatt_chr_t chr_cgm_meas;
    gatt_chr_t chr_cgm_fea;
    gatt_chr_t chr_cgm_st;
    gatt_chr_t chr_cgm_sess_start_time;
    gatt_chr_t chr_cgm_sess_run_time;
    gatt_chr_t chr_cgm_racp;
    gatt_chr_t chr_cgm_ops_ctrl_pt;
}att_desc_t;

static att_desc_t cgms_att_db;

void svc_cgms_creat_cbk(gatt_event_t ev,void *ctx,void*par)
{
    switch (ev) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)par;
            co_printf("cgm svc start rst:%x\n", in->result);
            break;
        }
    }
}

void chr_cgm_meas_cbk(gatt_event_t ev,void *ctx,void *par)
{
    switch (ev) {
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)par;

            co_printf("type %04x\n", in->type);
            break;
        }
    }
}

void chr_cgm_fea_cbk(gatt_event_t ev,void *ctx,void *par)
{
    switch (ev) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)par;
            uint8_t val[] = {0x01, 0x02};
            co_printf("feature read\n");

            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
    }
}

void chr_cgm_st_cbk(gatt_event_t ev,void *ctx,void *par)
{
    switch (ev) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)par;
            uint8_t val[] = {0x01, 0x02};
            co_printf("status read\n");

            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
    }
}

void chr_cgm_sess_start_time_stcbk(gatt_event_t ev,void *ctx,void *par)
{
    switch (ev) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)par;
            uint8_t val[] = {0x01, 0x02};
            co_printf("status read\n");

            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)par;
            co_printf("cgm_sess_start_time %d %x\n", in->val_len, *in->val);
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
    }
}

void chr_cgm_sess_run_time_cbk(gatt_event_t ev,void *ctx,void *par)
{
    switch (ev) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)par;
            uint8_t val[] = {0x01, 0x02};
            co_printf("cgm_sess_run_time read\n");

            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
    }
}

void chr_cgm_racp_cbk(gatt_event_t event, void *context, void *par)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)par;
            co_printf("cgm_racp %d %x\n", in->val_len, *in->val);
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)par;
            co_printf("ccc type %04x\n", in->type);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_CFM:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)par;
            co_printf("ccc type %04x\n", in->type);
            break;
        }
    }
}

void chr_cgm_ops_ctrl_pt_cbk(gatt_event_t event, void *context, void *par)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)par;
            co_printf("cgm_ops_ctrl_pt %d %x\n", in->val_len, *in->val);
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)par;
            co_printf("ccc type %04x\n", in->type);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_CFM:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)par;
            co_printf("ccc type %04x\n", in->type);
            break;
        }
    }
}

int ble_cgms_init(void)
{
    uuid_t svc_cgms_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x181F)
    };
    cgms_att_db.svc_cgms = gatts_svc_create(&svc_cgms_uuid,(gatt_cbk_t *)svc_cgms_creat_cbk, NULL);

    uuid_t chr_cgm_meas_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AA7)
    };
    cgms_att_db.chr_cgm_meas = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_meas_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , (ATT_PERM_READ )
                           , (gatt_cbk_t *)chr_cgm_meas_cbk, NULL);

    gatts_dsc_ccc_add(cgms_att_db.chr_cgm_meas
                   , GATT_CHR_PROP_NOTIFY
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr_cgm_fea_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AA8)
    };
    cgms_att_db.chr_cgm_fea = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_fea_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ
                           , (gatt_cbk_t *)chr_cgm_fea_cbk, NULL);

    uuid_t chr_cgm_st_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AA9)
    };
    cgms_att_db.chr_cgm_st = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_st_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ
                           , (gatt_cbk_t *)chr_cgm_st_cbk, NULL);

    uuid_t chr_cgm_sess_start_time_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AAA)
    };
    cgms_att_db.chr_cgm_sess_start_time = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_sess_start_time_uuid
                           , (GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE)
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , (gatt_cbk_t *)chr_cgm_sess_start_time_stcbk, NULL);

    uuid_t chr_cgm_sess_run_time_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AAB)
    };
    cgms_att_db.chr_cgm_sess_run_time = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_sess_run_time_uuid
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ
                           , (gatt_cbk_t *)chr_cgm_sess_run_time_cbk, NULL);

    uuid_t chr_cgm_racp_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A52)
    };
    cgms_att_db.chr_cgm_racp = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_racp_uuid
                           , GATT_CHR_PROP_INDICATE | GATT_CHR_PROP_WRITE
                           , ATT_PERM_READ | ATT_PERM_WRITE
                           , (gatt_cbk_t *)chr_cgm_racp_cbk, NULL);

    gatts_dsc_ccc_add(cgms_att_db.chr_cgm_racp
                   , GATT_CHR_PROP_INDICATE
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr_cgm_ops_ctrl_pt_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2AAC)
    };
    cgms_att_db.chr_cgm_ops_ctrl_pt = gatts_chr_add(cgms_att_db.svc_cgms, &chr_cgm_ops_ctrl_pt_uuid
                           , GATT_CHR_PROP_INDICATE | GATT_CHR_PROP_WRITE
                           , ATT_PERM_READ | ATT_PERM_WRITE
                           , (gatt_cbk_t *)chr_cgm_ops_ctrl_pt_cbk, NULL);

    gatts_dsc_ccc_add(cgms_att_db.chr_cgm_ops_ctrl_pt
                   , GATT_CHR_PROP_INDICATE
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(cgms_att_db.svc_cgms);

    return 0;
}







