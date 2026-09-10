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
 * @file ble_dtss.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include <stdio.h>

#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "ble_dtss.h"

ble_dtss_env_t dtss_env = {
    .peer = NULL,
    .dtss_svc = NULL,
    .dtss_chr0 = NULL,
    .dtss_chr1 = NULL,
    .ble_send_to_uart = NULL,
};

void ble_send_to_uart(const uint8_t *data, uint16_t len);

void dtss_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            co_printf("dts svc start rst:%X\n", in->result);
            break;
        }
        default:
            break;
    }
}

void dtss_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE:
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            co_printf("Chr0 value write, peer:%08X, chr:%08x, len:%d\n", in->peer, in->chr, in->val_len);
            if(event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            }

            ble_send_to_uart(in->val,in->val_len);

            break;
        }
        default:
            break;
    }
}

void dtss_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x01, 0x02};
            co_printf("Chr1 value read\n");
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE:
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            if(event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            }
            co_printf("Chr1 value write, peer:%08X, chr:%08x, len:%d\n", in->peer, in->chr, in->val_len);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            dtss_env.peer = in->peer;
            dtss_env.dtss_chr1 = in->chr;
            break;
        }
        default:
            break;
    }
}

int ble_dtss_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF00)
    };
    dtss_env.dtss_svc = gatts_svc_create(&svc_uuid, dtss_svc_cbk, NULL);

    uuid_t chr0_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF01)
    };
    dtss_env.dtss_chr0 = gatts_chr_add(dtss_env.dtss_svc, &chr0_uuid
                           , GATT_CHR_PROP_WRITE_WITHOUT_RSP
                           , (ATT_PERM_READ | ATT_PERM_WRITE)
                           , dtss_chr0_cbk, NULL);

    uuid_t chr1_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0xFF02)
    };
    dtss_env.dtss_chr1 = gatts_chr_add(dtss_env.dtss_svc, &chr1_uuid
                           , GATT_CHR_PROP_NOTIFY
                           , ATT_PERM_READ
                           , dtss_chr1_cbk, NULL);

    gatts_dsc_ccc_add(dtss_env.dtss_chr1
                   , GATT_CHR_PROP_NOTIFY
                   , ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(dtss_env.dtss_svc);

    return 0;
}

void ble_send_to_peer(uint8_t *data, uint16_t len)
{
    if((data != NULL) && (len > 0) ){
        gatts_chr_val_notif(dtss_env.peer, dtss_env.dtss_chr1, data, len);
    }else{
        co_printf("send to peer err, data is NULL or len is 0\n");
    }
}

void ble_send_to_uart(const uint8_t *data, uint16_t len)
{
    if(dtss_env.ble_send_to_uart != NULL){
        dtss_env.ble_send_to_uart(data,len);
    }
}

void ble_send_to_uart_cbk_reg(ble_send_to_uart_cbk ble_send_to_uart)
{
    dtss_env.ble_send_to_uart = ble_send_to_uart;
}

