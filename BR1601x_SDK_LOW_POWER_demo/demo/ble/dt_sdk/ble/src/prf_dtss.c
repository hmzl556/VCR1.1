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

/**
 * INCLUDE FILES
 ****************************************************************************************
 */ 
#include <stdio.h>
#include "gap.h"
#include "gatt.h"
#include "prf_dtss.h"
#include "app_ble.h"
#include "cfg.h"
#include "task.h"
#include "app_gpio.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define  DTSS_TX_PROP           GATT_CHR_PROP_NOTIFY //or GATT_CHR_PROP_INDICATE

enum {
    DTSS_IDX_TX,//notify
    DTSS_IDX_RX,//write cmd
    DTSS_IDX_FC,
    DTSS_IDX_SVC,
    DTSS_IDX_END,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_chr_t dtss_handle[DTSS_IDX_END] = {NULL};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Sends a completion indication for a DTSS operation.
 *
 * This function sends a completion indication to the peer device after a DTSS operation is completed.
 * It provides feedback on the status of the operation, indicating whether the data transmission was successful.
 *
 * @param event The GATT event that triggered the completion indication.
 * @param ctx The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the operation such as status codes.
 */ 
static void prf_dtss_send_cmp_ind(gatt_event_t event, void *ctx, void *param)
{
    gatt_chr_val_send_cfm_t *par = (gatt_chr_val_send_cfm_t *)param;

    app_ble_msg_t arg;
    arg.ntf_cmp.conhdl = ble_get_conhdl_by_conidx(par->peer);
    arg.ntf_cmp.handle = (par->chr == dtss_handle[DTSS_IDX_TX]) ? DTSS_IDX_TX : DTSS_IDX_FC;
    app_ble_evt_t evt = (par->chr == dtss_handle[DTSS_IDX_TX]) ?  BLE_GATTS_TRANS_DATA_CMP : BLE_GATTS_TRANS_FC_CMP;
	ble_event_callback(evt, &arg);
}

/**
 * @brief Callback function for handling GATT events in the Data Transparent Service (DTSS).
 *
 * This function is invoked in response to various GATT events related to the DTSS.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param context The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_dtss_cbk(gatt_event_t event, void *context, void *param)
{
    BR_LOG_DEBUG("%s,event:%x\r\n", __func__, event);
    
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            //gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE_REQ: 
        case GATTS_EV_CHR_VALUE_WRITE:{
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            if(event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            }
            app_ble_msg_t param;
            app_ble_evt_t evt = (in->chr == dtss_handle[DTSS_IDX_RX]) ? BLE_GATTS_TRANS_DATA_RPT : BLE_GATTS_TRANS_FC_RPT;
            param.data_rcv.conhdl = ble_get_conhdl_by_conidx(in->peer);
            param.data_rcv.len = in->val_len;
            param.data_rcv.data = in->val + in->val_off;
            ble_event_callback(evt, &param);
            break;
        } 
        case GATTS_EV_CHR_VALUE_SEND_START:{
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;   
            app_ble_msg_t param;
            app_ble_evt_t evt = (in->chr == dtss_handle[DTSS_IDX_TX]) ? BLE_GATTS_TRANS_DATA_CCCD_RPT : BLE_GATTS_TRANS_FC_CCCD_RPT; 
            param.rcv_ntf.conhdl = ble_get_conhdl_by_conidx(in->peer);
            param.rcv_ntf.data = in->type;
    		ble_event_callback(evt, &param);
            break;
        }  
        case GATTS_EV_CHR_VALUE_SEND_CFM:{
            prf_dtss_send_cmp_ind(event, context, param);
            break;
        }
        default:
            break;
    }
}

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint32_t prf_dtss_send_data(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    uint32_t ret = GATT_OK;   
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
#if DTSS_TX_PROP == GATT_CHR_PROP_NOTIFY
    ret = gatts_chr_val_notif(conidx, dtss_handle[DTSS_IDX_TX], (uint8_t *)data, length);
    gatt_chr_val_send_cfm_t cfm;
    cfm.peer = conidx;
    cfm.chr = dtss_handle[DTSS_IDX_TX];  
    prf_dtss_send_cmp_ind(GATTS_EV_CHR_VALUE_SEND_CFM, NULL, (gatt_chr_val_send_cfm_t *)&cfm);
#else
    ret = gatts_chr_val_indicate(conidx, dtss_handle[DTSS_IDX_TX], data, length);
#endif
    return ret;
}

#ifdef CFG_BLE_FC
uint32_t prf_dtss_send_fc(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    uint32_t ret = GATT_OK;
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
#if DTSS_TX_PROP == GATT_CHR_PROP_NOTIFY
	ret = gatts_chr_val_notif(conidx, dtss_handle[DTSS_IDX_FC], (uint8_t *)data, length);
    gatt_chr_val_send_cfm_t cfm;
    cfm.peer = conidx;
    cfm.chr = dtss_handle[DTSS_IDX_FC];    
    prf_dtss_send_cmp_ind(GATTS_EV_CHR_VALUE_SEND_CFM, NULL, (gatt_chr_val_send_cfm_t *)&cfm);
#else
    ret = gatts_chr_val_indicate(conidx, dtss_handle[DTSS_IDX_FC], data, length);
#endif
    return ret;
}
#endif

int prf_dtss_init(void)
{
    gatt_chr_add_params_t char_add_params[] =
    {

        [DTSS_IDX_TX] = {&dtss_handle[DTSS_IDX_TX], {UUID_LEN_16,UUID16_VAL_SET(0xFF01)}, 0, 0, 
                                                                      DTSS_TX_PROP, 0,  
                                                                      prf_dtss_cbk, NULL},

        [DTSS_IDX_RX] = {&dtss_handle[DTSS_IDX_RX], {UUID_LEN_16,UUID16_VAL_SET(0xFF02)}, GATT_CHR_PROP_WRITE_WITHOUT_RSP | GATT_CHR_PROP_WRITE, 0, 
                                                                      NULL, 0, 
                                                                      prf_dtss_cbk, NULL },
#ifdef CFG_BLE_FC                                                                     
        [DTSS_IDX_FC] = {&dtss_handle[DTSS_IDX_FC], {UUID_LEN_16,UUID16_VAL_SET(0xFF03)}, GATT_CHR_PROP_WRITE_WITHOUT_RSP, 0, 
                                                                      GATT_CHR_PROP_NOTIFY, 0,
                                                                      prf_dtss_cbk, NULL},
#endif
    };
    
    gatt_svc_create_params_t svc_create_params[] = 
    {   
        [0] = {&dtss_handle[DTSS_IDX_SVC],{UUID_LEN_16,UUID16_VAL_SET(0xFF00)}, prf_dtss_cbk, NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t),char_add_params},
    };
    uint8_t sec_level = 0;
    cfg_reserver_get(AUTH_OFF, &sec_level, AUTH_LEN);
    if(sec_level == 1) {
        char_add_params[DTSS_IDX_TX].ccc_secu |= GATT_SECU_WRITE_AUTHEN | GATT_SECU_WRITE_ENCRYPT;//todo
    }
    cfg_get(CFG_TYPE_GATTS_SVCUUID, &svc_create_params[0].uuid, sizeof(uuid_t));
    cfg_get(CFG_TYPE_GATTS_NTFCHAR_UUID, &char_add_params[DTSS_IDX_TX].uuid, sizeof(uuid_t));
    cfg_get(CFG_TYPE_GATTS_WRTCHAR_UUID, &char_add_params[DTSS_IDX_RX].uuid, sizeof(uuid_t));
#ifdef CFG_BLE_FC
    cfg_get(CFG_TYPE_GATTS_FCCHAR_UUID, &char_add_params[DTSS_IDX_FC].uuid, sizeof(uuid_t));
#endif
    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }
    
    return 0;
}

