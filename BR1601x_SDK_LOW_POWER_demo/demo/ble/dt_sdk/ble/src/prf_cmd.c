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
 * @file prf_cmd.c
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
#include "app_ble.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
enum {
    CMD_IDX_CHAR_NTF,
    CMD_IDX_CHAR_WRITE,
    CMD_IDX_SVC,
    CMD_IDX_END,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_chr_t cmd_handle[CMD_IDX_END] = {NULL};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Callback function for handling GATT events in the Command Service.
 *
 * This function is invoked in response to various GATT events related to the Command Service.
 * The Command Service is designed to allow the exchange of commands and data over the air,
 * including the execution of AT commands. This callback processes the events and performs
 * the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param context The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_cmd_cbk(gatt_event_t event, void *context, void *param)
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
            param.data_rcv.conhdl = ble_get_conhdl_by_conidx(in->peer);
            param.data_rcv.len = in->val_len;
            param.data_rcv.data = in->val + in->val_off;
            ble_event_callback(BLE_GATTS_CMD_REPORT, &param);
            break;
        } 
        case GATTS_EV_CHR_VALUE_SEND_START: 
            break;
        case GATTS_EV_CHR_VALUE_SEND_CFM:
            break;
        default:
            break;
    }
}

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint32_t prf_cmd_rsp_send(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    uint32_t ret = GATT_OK;   
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
    ret = gatts_chr_val_notif(conidx, cmd_handle[CMD_IDX_CHAR_NTF], (uint8_t *)data, length);
    return ret;
}

int prf_cmd_init(void)
{
    gatt_chr_add_params_t char_add_params[] =
    {

        [CMD_IDX_CHAR_NTF] = {&cmd_handle[CMD_IDX_CHAR_NTF], {UUID_LEN_16,UUID16_VAL_SET(0xFF81)}, 0, 0, 
                                                                      GATT_CHR_PROP_NOTIFY, 0,  
                                                                      prf_cmd_cbk, NULL},

        [CMD_IDX_CHAR_WRITE] = {&cmd_handle[CMD_IDX_CHAR_WRITE], {UUID_LEN_16,UUID16_VAL_SET(0xFF82)}, GATT_CHR_PROP_WRITE_WITHOUT_RSP, 0, 
                                                                      NULL, 0, 
                                                                      prf_cmd_cbk, NULL },
    };
    
    gatt_svc_create_params_t svc_create_params[] = 
    {   
        [0] = {&cmd_handle[CMD_IDX_SVC],{UUID_LEN_16,UUID16_VAL_SET(0xFF80)}, prf_cmd_cbk, NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t),char_add_params},
    };
    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }

    return 0;
}

