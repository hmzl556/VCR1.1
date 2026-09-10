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
 * @file prf_dtsc.c
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
#include "prf_search.h"
#include "prf_dtsc.h"
#include "app_ble.h"
#include "cfg.h"
//#define BR_DEBUG_LEVEL  0xFF
#include "log.h"

/*
 * DEFINITIONS
 ****************************************************************************************
 */
#define DTS_SERVER_NTF_UUID     {UUID_LEN_128, {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00}}
#define DTS_SERVER_WRITE_UUID   {UUID_LEN_128, {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00}}
#define DTS_SERVER_FC_UUID      {UUID_LEN_128, {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00}}
#define CLIENT_CHAR_CFG_UUID    {UUID_LEN_16,  {0x02, 0x29}}

/**
 * @brief Enumerates the characteristic types of the DTSC (Data Transfer Service Client) service.
 *
 * This enumeration lists the different characteristics of the DTSC service, which may include
 * notification, write, and feature characteristics.
 */
enum
{
    DTSC_SRV_NTF_CHAR = 0,   /**< Notification characteristic for receiving updates from the server. */
    DTSC_SRV_WRITE_CHAR,     /**< Write characteristic for sending data to the server. */
#ifdef CFG_BLE_FC   
    DTSC_SRV_FC_CHAR,        /**< Feature characteristic for feature-related operations. */
#endif
    DTSC_SRV_CHAR_MAX,       /**< Maximum number of characteristics in the DTSC service. */
};

/**
 * @brief Enumerates the descriptor types of the DTSC service.
 *
 * This enumeration lists the different descriptors of the DTSC service, which may include
 * client configuration and feature configuration descriptors.
 */
enum
{
    DTSC_SRV_NTF_CLI_CFG = 0, /**< Client configuration descriptor for notification characteristic. */
#ifdef CFG_BLE_FC   
    DTSC_SRV_FC_CLI_CFG,       /**< Client configuration descriptor for feature characteristic. */
#endif
    DTSC_SRV_DESC_MAX,         /**< Maximum number of descriptors in the DTSC service. */
};

/*
 * TYPEDEFS
 ****************************************************************************************
 */
/**
 * @brief Data structure representing the content of the DTSC service.
 *
 * This structure contains the service, characteristics, and descriptors of the DTSC service.
 */
typedef struct
{
    gatt_svc_t  svc;
    gatt_chr_t  chars[DTSC_SRV_CHAR_MAX];
    gatt_dsc_t  descs[DTSC_SRV_DESC_MAX];
    gatt_ccc_val_t desc_value[DTSC_SRV_DESC_MAX];
}dtsc_dts_content_t;

/**
 * @brief Environment structure for the DTSC profile.
 *
 * This structure contains the configuration and content for each connection index.
 */
typedef struct {
    prf_db_cfg_t  dtsc_cfg;
    dtsc_dts_content_t dtsc_content[APP_CON_IDX_MAX];
}dtsc_env_t;

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static dtsc_env_t dtsc_env;
static uuid_t dtsc_uuid128 = {UUID_LEN_128, {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00}};
/// State machine used to retrieve DTS characteristics information
static prf_char_def_t dtsc_char[DTSC_SRV_CHAR_MAX] =
{
    [DTSC_SRV_NTF_CHAR]     = {DTS_SERVER_NTF_UUID, TRUE,  GATT_CHR_PROP_NOTIFY},
    [DTSC_SRV_WRITE_CHAR]   = {DTS_SERVER_WRITE_UUID, TRUE,  GATT_CHR_PROP_WRITE_WITHOUT_RSP},
#ifdef CFG_BLE_FC   
    [DTSC_SRV_FC_CHAR]      = {DTS_SERVER_FC_UUID, FALSE, GATT_CHR_PROP_NOTIFY | GATT_CHR_PROP_WRITE_WITHOUT_RSP},
#endif
};
/// State machine used to retrieve DTS service characteristic description information
static prf_char_desc_def_t dtsc_desc[DTSC_SRV_DESC_MAX] =
{
    ///DTS Client Config
    [DTSC_SRV_NTF_CLI_CFG]  = {CLIENT_CHAR_CFG_UUID, TRUE, DTSC_SRV_NTF_CHAR},
#ifdef CFG_BLE_FC 
    [DTSC_SRV_FC_CLI_CFG]   = {CLIENT_CHAR_CFG_UUID, FALSE, DTSC_SRV_FC_CHAR},
#endif
};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Handles write indications for descriptors and receives data for characteristics in the DTSC service.
 *
 * This function is a callback handler for write indications received for descriptors in the DTSC service.
 * It processes the write request, updates the corresponding characteristic configuration, and also handles
 * the reception of data for the corresponding characteristics.
 *
 * @param ev The GATT event type, indicating the nature of the write indication or data reception.
 * @param ctx The context of the event, typically a pointer to the GATT profile structure.
 * @param arg Additional argument, which may contain the write request data or characteristic values.
 *
 * @note This function is responsible for managing descriptor writes that affect characteristic behavior,
 *       such as enabling or disabling notifications or indications via client characteristic configuration
 *       descriptors (CCCDs). It also processes incoming characteristic values, ensuring that the data is
 *       valid and applying any necessary updates to the local state or behavior of the DTSC service.
 */
static void prf_dtsc_cbk(gatt_event_t ev, void *ctx, void *arg)
{
    BR_LOG_DEBUG("%s, ev:%x\r\n",__func__,ev);
    switch(ev)
    {
        //case GATTC_EV_DSC_CCC_DISABLE:
        case GATTC_EV_DSC_CCC_ENABLE:
            {
                gatt_dsc_ccc_enable_t *_arg = (gatt_dsc_ccc_enable_t*)arg;
                uint16_t conhdl = ble_get_conhdl_by_conidx(_arg->peer);
                app_ble_msg_t param;
                bool all_written = true; 
                BR_LOG_DEBUG("%s, result:%x\r\n",__func__,_arg->result);
                if(_arg->result == 0x00) {
                   for(uint8_t i=DTSC_SRV_NTF_CLI_CFG; i<DTSC_SRV_DESC_MAX; i++){
                        ///update desc_value
                        if(dtsc_env.dtsc_content[conhdl].descs[i] == _arg->dsc)
                            dtsc_env.dtsc_content[conhdl].desc_value[i] = _arg->val;
                        ///check all dsc value
                        if((dtsc_env.dtsc_content[conhdl].descs[i] != NULL) && (dtsc_env.dtsc_content[conhdl].desc_value[i] != GATT_SEND_VAL_NOTIFICATION))
                            all_written = false;    
                   }
                   BR_LOG_DEBUG("%s, all_written:%x\r\n",__func__,all_written);
                   if(all_written == true) {
                        param.ccc_cmp.conhdl = conhdl;
                        param.ccc_cmp.status = _arg->result;
                        ble_event_callback(BLE_GATTC_TRANS_CCCD_CMP, &param);
                   }
                }
                /// Requirement for encryption 
                else if(_arg->result == GATT_RES_INSUFFICIENT_AUTHENTICATION || _arg->result == GATT_RES_INSUFFICIENT_ENCRYPTION){
                    param.insuff_auth.conhdl = conhdl;
                    param.insuff_auth.status = _arg->result;
                    ble_event_callback(BLE_GATTC_TRANS_INSUFF, &param);
                }
                /// Disconnection not occurred
                else if(_arg->result != GATT_RES_CONNECT_FAILED){
                    param.ccc_failed.conhdl = conhdl;
                    param.ccc_failed.status = _arg->result;
                    ble_event_callback(BLE_GATTC_TRANS_CCCD_FAILED, &param);
                }
            }
            break;
        case GATTC_EV_CHR_VALUE_NOTIFIED:
            {
                gatt_chr_val_notif_t *_arg = (gatt_chr_val_notif_t *)arg;
                BR_LOG_DEBUG("peer:%p, chr:%p\r\n",_arg->peer,_arg->chr);
                uint8_t conhdl = ble_get_conhdl_by_conidx(_arg->peer) & 0xFF;
                app_ble_msg_t param;
                gatt_chr_t arg_chr = (gatt_chr_t)((uint32_t)(dtsc_env.dtsc_content[conhdl].chars[DTSC_SRV_NTF_CHAR]) & 0xFFFF);
                app_ble_evt_t evt = (_arg->chr == arg_chr) ? BLE_GATTC_TRANS_REPORT : BLE_GATTC_TRANS_CREDIT;
            	param.data_rcv.conhdl = conhdl;
            	param.data_rcv.len = _arg->val_len;
            	param.data_rcv.data = _arg->val;
                ble_event_callback(evt, &param);
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Handles confirmation of enabling the DTSC profile.
 *
 * This function handles the confirmation of enabling the DTSC profile and reports any errors.
 *
 * @param conhdl The connection handle.
 * @param err The error code indicating the result of the operation.
 */
static void prf_dtsc_enable_cfm_handler(uint16_t conhdl, prf_disc_err_t err)
{
    BR_LOG_DEBUG("%s, err:%x\r\n",__func__, err);
    app_ble_msg_t param;
    param.targe.conhdl = conhdl;
    app_ble_evt_t evt = (err == PRF_DISC_ERR_NO ? BLE_GATTC_TARGET_READY : BLE_GATTC_NO_TARGET_SVC);
    ble_event_callback(evt, &param);
    if(evt == BLE_GATTC_TARGET_READY)
        prf_dtsc_dsc_ccc_enable(conhdl);
}

/**
 * @brief Handles write indications for characteristic values.
 *
 * This function is a callback handler for write indications received for characteristic values.
 *
 * @param ev The GATT event type.
 * @param ctx The context of the event.
 * @param arg Additional argument containing the write response.
 */
static void prf_dtsc_chr_val_write_ind_handler(gatt_event_t ev, void *ctx, gatt_chr_val_write_rsp_t* arg)
{
    app_ble_msg_t param = {0};
    ///Active memory release is required when sending flow control data.
    if(ctx != NULL) FREE(ctx);
    param.ntf_cmp.conhdl = ble_get_conhdl_by_conidx(arg->peer);
	ble_event_callback(BLE_GATTC_TRANS_CMP, &param);
}

/**
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void prf_dtsc_enable_prf(uint16_t conhdl)
{
    ///Clearing transparent transmission service.
    memset(&dtsc_env.dtsc_content[conhdl], 0x00, sizeof(dtsc_dts_content_t));
    prf_content_t context = {0};
    context.svc = &dtsc_env.dtsc_content[conhdl].svc;
    context.chars = dtsc_env.dtsc_content[conhdl].chars;
    context.descs = dtsc_env.dtsc_content[conhdl].descs;
    ///Discovering services.
    prf_disc_start(conhdl, &dtsc_env.dtsc_cfg, &context, prf_dtsc_enable_cfm_handler);
}

void prf_dtsc_dsc_ccc_enable(uint16_t conhdl)
{
    gatt_ccc_val_t val = GATT_SEND_VAL_NOTIFICATION;
    for(uint8_t i=DTSC_SRV_NTF_CLI_CFG; i<DTSC_SRV_DESC_MAX; i++){
        gatt_dsc_t dsc = dtsc_env.dtsc_content[conhdl].descs[i];
        uint8_t char_idx = dtsc_env.dtsc_cfg.desc[i].char_idx;
        gatt_chr_t chr = dtsc_env.dtsc_content[conhdl].chars[char_idx];
        if(dsc != NULL && dtsc_env.dtsc_content[conhdl].desc_value[i] != val) {
            gattc_dsc_ccc_enable(ble_get_conidx_by_conhdl(conhdl), chr, dsc, val, (gatt_cbk_t *)prf_dtsc_cbk, NULL);
        }
    }
}

//If the data points to a ring buffer or the heap, internal active memory release is not required.
uint32_t prf_dtsc_send_data(uint16_t conhdl, uint8_t *data, uint16_t len)
{
    bool ret = GATT_RES_INVALID_HANDLE;
    void  *conidx = ble_get_conidx_by_conhdl(conhdl);
    gatt_chr_t chr = dtsc_env.dtsc_content[conhdl].chars[DTSC_SRV_WRITE_CHAR];
    if(chr != NULL) {
        uint8_t *ctx_data = NEW(len);
        if(ctx_data != NULL) {
            memcpy(ctx_data, data, len);
            gattc_chr_val_write_cmd(conidx, chr, ctx_data, len, (gatt_cbk_t *)prf_dtsc_chr_val_write_ind_handler, ctx_data);
            ret = GATT_OK;
        }
    }
    return ret; 
}

#ifdef CFG_BLE_FC
//Memory needs to be released after sending is complete.
uint32_t prf_dtsc_send_fc(uint16_t conhdl, uint8_t *data, uint16_t len) 
{
    bool ret = GATT_RES_INVALID_HANDLE;
    void  *conidx = ble_get_conidx_by_conhdl(conhdl);
    gatt_chr_t chr = dtsc_env.dtsc_content[conhdl].chars[DTSC_SRV_FC_CHAR];
    if(chr != NULL) {
        uint8_t *ctx_data = NEW(len);
        if(ctx_data != NULL) {
            memcpy(ctx_data, data, len);
            gattc_chr_val_write_cmd(conidx, chr, ctx_data, len, (gatt_cbk_t *)prf_dtsc_chr_val_write_ind_handler, ctx_data);
            ret = GATT_OK;
        }
    }
    return ret; 
}
#endif

int prf_dtsc_init(void)
{
    memset(&dtsc_env, 0x00, sizeof(dtsc_env_t));
    dtsc_env.dtsc_cfg.svc_uuid = &dtsc_uuid128;
    dtsc_env.dtsc_cfg.chr = dtsc_char;
    dtsc_env.dtsc_cfg.desc = dtsc_desc;
    dtsc_env.dtsc_cfg.chr_nb = DTSC_SRV_CHAR_MAX;
    dtsc_env.dtsc_cfg.desc_nb = DTSC_SRV_DESC_MAX;
    ///Populate the target uuid
    cfg_get(CFG_TYPE_GATTC_SVCUUID, &dtsc_uuid128, sizeof(uuid_t));
    cfg_get(CFG_TYPE_GATTC_NTFCHAR_UUID, &dtsc_char[DTSC_SRV_NTF_CHAR].uuid, sizeof(uuid_t));
    cfg_get(CFG_TYPE_GATTC_WRTCHAR_UUID, &dtsc_char[DTSC_SRV_WRITE_CHAR].uuid, sizeof(uuid_t));
#ifdef CFG_BLE_FC   
    cfg_get(CFG_TYPE_GATTC_FCCHAR_UUID, &dtsc_char[DTSC_SRV_FC_CHAR].uuid, sizeof(uuid_t));
#endif
    return 0;
}

