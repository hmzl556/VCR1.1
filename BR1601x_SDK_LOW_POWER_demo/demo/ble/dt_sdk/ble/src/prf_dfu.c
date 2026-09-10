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
 * @file prf_dfu.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "prf_dfu.h"
#include "gatt.h"
#include "app_ble.h"
#include "dfu_execute.h"
#include "task.h"
#include <string.h>
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define GATT_UUID_SERVICE_DFU          0xFF10
#define GATT_UUID_DFU_CTRL             0xFF11
#define GATT_UUID_DFU_ENHANCE_CTRL     0xFF12
#define GATT_UUID_ATCMD                0xFF13

enum {
    DFU_IDX_CHAR_CTRL,
    DFU_IDX_CHAR_ENHANCE_CTRL,
    DFU_IDX_CHAR_ATCMD,
    DFU_IDX_SVC,
    DFU_IDX_NB,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_svc_t dfu_chr[DFU_IDX_NB] = {INST_INVALID};
static peer_t dfu_terminal_peer;
static gatt_svc_t dfu_terminal_chr;
static uint8_t dfu_terminal_response[16];
static uint16_t dfu_terminal_response_length;
static tid_t dfu_terminal_retry_id;
static uint32_t dfu_terminal_retry_delay_ms;
static bool dfu_terminal_pending;
static bool dfu_terminal_inflight;

#define DFU_RETRY_INITIAL_MS    10U
#define DFU_RETRY_MAX_MS        1000U

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Calculates the checksum of the DFU data.
 *
 * This function computes the checksum of the provided data array.
 * The checksum is used to verify the integrity of the data during the DFU process.
 *
 * @param data The data array for which the checksum is to be calculated.
 * @param len The length of the data array.
 * @return Returns the calculated checksum value.
 */
static uint8_t prf_dfu_data_checksum(uint8_t *data, uint16_t len)
{
    uint8_t check_sum = 0;
    for(uint16_t i=0; i<len; i++)
    {
        check_sum = (check_sum + data[i])&0xff;
    }

    return check_sum;
}

/**
 * @brief Sends a DFU response to a peer device.
 *
 * This function constructs and sends a response to a DFU operation request from a peer device.
 * The response includes the status and any relevant data.
 *
 * @param peer The peer device to which the response is sent.
 * @param chr The characteristic through which the response is sent.
 * @param dfu_rsp_data The DFU response data to be sent.
 */
static void prf_dfu_terminal_retry_task(uint8_t event, void *argument);

static bool prf_dfu_error_retryable(uint32_t result)
{
    return result == GATT_RES_INSUFFICIENT_RESOURCES ||
           result == (uint32_t)GATT_RES_PENDING;
}

static void prf_dfu_stop_retry(void)
{
    if(dfu_terminal_retry_id != 0U)
    {
        timer_delete(&dfu_terminal_retry_id);
        dfu_terminal_retry_id = 0U;
    }
}

static void prf_dfu_schedule_retry(void)
{
    if(dfu_terminal_retry_id != 0U)
    {
        return;
    }
    if(!TIMER_CREATE_ONESHOT(&dfu_terminal_retry_id,
                             dfu_terminal_retry_delay_ms, NULL,
                             prf_dfu_terminal_retry_task))
    {
        dfu_terminal_pending = false;
        dfu_action_notification_failed();
        return;
    }
    if(dfu_terminal_retry_delay_ms < DFU_RETRY_MAX_MS)
    {
        dfu_terminal_retry_delay_ms <<= 1U;
        if(dfu_terminal_retry_delay_ms > DFU_RETRY_MAX_MS)
        {
            dfu_terminal_retry_delay_ms = DFU_RETRY_MAX_MS;
        }
    }
}

static void prf_dfu_try_terminal_response(void)
{
    uint32_t result;

    if(!dfu_terminal_pending || dfu_terminal_inflight)
    {
        return;
    }
    result = gatts_chr_val_notif(dfu_terminal_peer, dfu_terminal_chr,
                                 dfu_terminal_response,
                                 dfu_terminal_response_length);
    BR_LOG_INFO("[DFU] terminal notify len:%u result:%08lx\r\n",
                (unsigned int)dfu_terminal_response_length,
                (unsigned long)result);
    if(result == GATT_OK)
    {
        dfu_terminal_pending = false;
        dfu_terminal_inflight = true;
        dfu_terminal_retry_delay_ms = DFU_RETRY_INITIAL_MS;
        prf_dfu_stop_retry();
    }
    else if(prf_dfu_error_retryable(result))
    {
        prf_dfu_schedule_retry();
    }
    else
    {
        dfu_terminal_pending = false;
        dfu_action_notification_failed();
    }
}

static void prf_dfu_terminal_retry_task(uint8_t event, void *argument)
{
    (void)event;
    (void)argument;
    dfu_terminal_retry_id = 0U;
    prf_dfu_try_terminal_response();
}

static void prf_dfu_response(peer_t peer, gatt_svc_t chr, dfu_response_t *dfu_rsp_data)
{
    bool terminal_response;

    if(dfu_rsp_data->length == 0)
        return;

    if((dfu_rsp_data->opcode == DFU_OPCODE_DATA)&&(dfu_rsp_data->rsp.trans.index == 0) )
    {
        return;
    }
    terminal_response = dfu_rsp_data->rsp_code == DFU_OPCODE_END ||
                        dfu_rsp_data->opcode == DFU_OPCODE_END;
    if(!terminal_response)
    {
        gatts_chr_val_notif(peer, chr, (uint8_t*)&dfu_rsp_data->rsp_code,
                            dfu_rsp_data->length);
        return;
    }
    if(dfu_rsp_data->length > sizeof(dfu_terminal_response))
    {
        dfu_action_notification_failed();
        return;
    }
    memcpy(dfu_terminal_response, &dfu_rsp_data->rsp_code,
           dfu_rsp_data->length);
    dfu_terminal_peer = peer;
    dfu_terminal_chr = chr;
    dfu_terminal_response_length = dfu_rsp_data->length;
    dfu_terminal_retry_delay_ms = DFU_RETRY_INITIAL_MS;
    dfu_terminal_pending = true;
    prf_dfu_try_terminal_response();
}

/**
 * @brief Callback function for handling GATT events in the DFU process.
 *
 * This function is invoked in response to various GATT events related to the DFU process.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param context The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_dfu_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event)
    {
        case GATTS_EV_CHR_VALUE_SEND_STATE: {
            gatt_chr_val_send_state_t *ind_value = (gatt_chr_val_send_state_t *)param;
            if(ind_value->chr == dfu_chr[DFU_IDX_CHAR_CTRL] /*|| ind_value->chr == dfu_chr[DFU_IDX_CHAR_ENHANCE_CTRL]*/){
                if(ind_value->type & GATT_SEND_VAL_NOTIFICATION)
                {
                    //enabled notify 
                    BR_LOG_DEBUG("br_dfu channal enable ntf\r\n");
                    if(ind_value->chr == dfu_chr[DFU_IDX_CHAR_CTRL]){ 
                        app_ble_msg_t param;
                        param.rcv_ntf.conhdl = ble_get_conhdl_by_conidx(ind_value->peer);
                        param.rcv_ntf.data = ind_value->type & GATT_SEND_VAL_NOTIFICATION;
                        param.rcv_ntf.handle = DFU_IDX_CHAR_CTRL;
                		ble_event_callback(BLE_GATTS_OTA_CCCD_REPORT, &param);
                    }
                }
            }
        }
        break;
        case GATTS_EV_CHR_VALUE_WRITE_REQ:
        case GATTS_EV_CHR_VALUE_WRITE: {
            gatt_chr_val_write_req_t *write = (gatt_chr_val_write_req_t *)param;
            const uint8_t *data = write->val + write->val_off;
            uint16_t data_len = write->val_len;
            BR_LOG_DEBUG_HEX("Write data",data, data_len);
            
            if(event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(write->peer, write->chr, GATT_OK);
            }
            if(prf_dfu_data_checksum((uint8_t*)data, data_len-1) == data[data_len-1]){
                dfu_response_t dfu_rsp_data = {0};
                ///Remove checksum
                dfu_action_write((uint8_t*)data, data_len-1, &dfu_rsp_data);
                prf_dfu_response(write->peer, write->chr, &dfu_rsp_data);
            }
            else{
                BR_LOG_ERROR("Checksun error");
            }
        }
        break;
        case GATTS_EV_CHR_VALUE_SEND_CFM:
        {
            gatt_chr_val_send_cfm_t *input = (gatt_chr_val_send_cfm_t *)param;
            app_ble_msg_t message;

            if(input->chr == dfu_chr[DFU_IDX_CHAR_CTRL])
            {
                if(dfu_terminal_inflight)
                {
                    dfu_terminal_inflight = false;
                    message.ntf_cmp.conhdl =
                        ble_get_conhdl_by_conidx(input->peer);
                    message.ntf_cmp.handle = DFU_IDX_CHAR_CTRL;
                    ble_event_callback(BLE_GATTS_OTA_CMP, &message);
                }
                else if(dfu_terminal_pending)
                {
                    prf_dfu_try_terminal_response();
                }
            }
            break;
        }
    default:
        /* For all other events, do nothing */
        break;
    }
}

/**
 * @brief The callback function for the ATCMD characteristic under the DFU service.
 * 
 * This function is used to handle events related to the ATCMD characteristic in the DFU service. 
 * It will be called when the status or data of the ATCMD characteristic changes.
 * Depending on the type of event, it can perform corresponding operations, such as reading or writing the characteristic value.
 * 
 * @param event The GATT event that triggered the callback.
 * @param context The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_dfu_atcmd_cbk(gatt_event_t event, void *context, void *param)
{
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
            ble_event_callback(BLE_GATTS_OTA_ATCMD_REPORT, &param);
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
void prf_dfu_disconnect(void)
{
    prf_dfu_stop_retry();
    dfu_terminal_response_length = 0U;
    dfu_terminal_pending = false;
    dfu_terminal_inflight = false;
}

int prf_dfu_init(void)
{
    dfu_terminal_retry_id = 0U;
    dfu_terminal_retry_delay_ms = DFU_RETRY_INITIAL_MS;
    dfu_terminal_pending = false;
    dfu_terminal_inflight = false;

    gatt_chr_add_params_t char_add_params[] ={
        [DFU_IDX_CHAR_CTRL]         = {&dfu_chr[DFU_IDX_CHAR_CTRL], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_DFU_CTRL)},
                                        GATT_CHR_PROP_WRITE_WITHOUT_RSP, 0, GATT_CHR_PROP_NOTIFY, 0, prf_dfu_cbk, NULL},
        [DFU_IDX_CHAR_ENHANCE_CTRL] = {&dfu_chr[DFU_IDX_CHAR_ENHANCE_CTRL], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_DFU_ENHANCE_CTRL)},
                                        GATT_CHR_PROP_WRITE_WITHOUT_RSP, 0, GATT_CHR_PROP_NOTIFY, 0, prf_dfu_cbk, NULL},
        [DFU_IDX_CHAR_ATCMD]        = {&dfu_chr[DFU_IDX_CHAR_ATCMD], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_ATCMD)},
                                        GATT_CHR_PROP_WRITE_WITHOUT_RSP, 0, GATT_CHR_PROP_NOTIFY, 0, prf_dfu_atcmd_cbk, NULL},                              
    };
    
    gatt_svc_create_params_t svc_create_params[] = 
    {
        [0] = {&dfu_chr[DFU_IDX_SVC], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_SERVICE_DFU)},
                            NULL,NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t), char_add_params},
    };

    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }

	return 0;
}

uint32_t prf_dfu_atcmd_rsp_send(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    uint32_t ret = GATT_OK;   
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
    ret = gatts_chr_val_notif(conidx, dfu_chr[DFU_IDX_CHAR_ATCMD], (uint8_t *)data, length);
    return ret;
}
