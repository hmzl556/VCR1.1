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
 * @file prf_search.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include <stdio.h>
#include "porting.h"
#include "prf_search.h"
#include "btlist.h"
#include "app_ble.h"
//#define BR_DEBUG_LEVEL  0xFF
#include "log.h"

/*
 * TYPEDEFS
 ****************************************************************************************
 */

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define PRF_CFG_PTR(_cfg)                           ((prf_db_cfg_t *)(_cfg))
#define PRF_CFG_CHR_PTR(_cfg, _idx)                 (&PRF_CFG_PTR(_cfg)->chr[_idx])        
#define PRF_CFG_DSC_PTR(_cfg, _idx)                 (&PRF_CFG_PTR(_cfg)->desc[_idx])
#define PRF_CFG_SVC_PTR(_cfg)                       (PRF_CFG_PTR(_cfg)->svc_uuid)
#define PRF_CFG_CHR_NB(_cfg)                        (PRF_CFG_PTR(_cfg)->chr_nb)
#define PRF_CFG_DSC_NB(_cfg)                        (PRF_CFG_PTR(_cfg)->desc_nb)

#define PRF_CTXT_PTR(_context)                      ((prf_content_t *)(_context))
#define PRF_CTXT_SVC(_context)                      (*(PRF_CTXT_PTR(_context)->svc))
#define PRF_CTXT_CHR(_context, _idx)                (PRF_CTXT_PTR(_context)->chars[_idx])
#define PRF_CTXT_DSC(_context, _idx)                (PRF_CTXT_PTR(_context)->descs[_idx])
#define PRF_CTXT_NBSVC(_context)                    (PRF_CTXT_PTR(_context)->nb_svc)
#define PRF_CTXT_STATE(_context)                    (PRF_CTXT_PTR(_context)->disc_state)
#define PRF_CTXT_LASTCHR(_context)                  (PRF_CTXT_PTR(_context)->last_char)

/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */

/**
 * LOCAL FUNCTIONS DECLARATIONS
 ****************************************************************************************
 */
static void prf_disc_char_desc_ind_handler(gatt_event_t ev, void *ctx, void *arg);

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Sends a service discovery confirmation message.
 *
 * This function is responsible for sending a confirmation message back to the application
 * or higher layers indicating the status of the service discovery process.
 *
 * @param ctx Pointer to the profile search environment structure that contains the context of the discovery process.
 * @param status The status of the service discovery process, indicating success or the type of error encountered.
 */
static void prf_disc_cfm_send(prf_search_env_t *ctx, uint8_t status)
{
    BR_LOG_DEBUG("%s, status:%d\r\n",__func__, status);
    prf_content_t *prf_content = &ctx->prf_content;
    PRF_CTXT_STATE(prf_content) = (status == PRF_DISC_ERR_NO ? PRF_STATE_DISC_COMPLETE : PRF_STATE_IDLE);

    if(ctx->cbk) ctx->cbk(ctx->conhdl, (prf_disc_err_t)status);
    FREE(ctx);
}

/**
 * @brief Requests the discovery of characteristic descriptors.
 *
 * After discovering all the characteristics of a service, this function is called to start
 * the discovery of their associated descriptors.
 *
 * @param ctx Pointer to the profile search environment structure that contains the context of the discovery process.
 */
static void prf_disc_desc_req(prf_search_env_t *ctx)
{ 
    prf_db_cfg_t  *prf_cfg = ctx->prf_cfg;
    prf_content_t *prf_content = &ctx->prf_content;
    for(uint8_t i = PRF_CTXT_LASTCHR(prf_content); i<PRF_CFG_DSC_NB(prf_cfg); i++) {
        uint8_t mandatory = PRF_CFG_DSC_PTR(prf_cfg, i)->mandatory;
        gatt_chr_t chr = PRF_CTXT_CHR(prf_content, PRF_CFG_DSC_PTR(prf_cfg, i)->char_idx);
        if(mandatory == TRUE && chr == NULL){
            //report err
            prf_disc_cfm_send(ctx, PRF_DISC_ERR_NODESC);
            return;
        }
        else if(chr != NULL) {
            BR_LOG_DEBUG("%s, peer:%p, chr:%p \r\n",__func__, ble_get_conidx_by_conhdl(ctx->conhdl), chr);
            gattc_des_discover(ble_get_conidx_by_conhdl(ctx->conhdl), chr, NULL, (gatt_cbk_t *)prf_disc_char_desc_ind_handler, ctx);
            return;
        }
    }
    ///All characteristic descriptors have been discovered.
    prf_disc_cfm_send(ctx, PRF_DISC_ERR_NO);
}

/**
 * @brief Handles indications related to characteristic descriptor discovery.
 *
 * This function is a callback handler for indications received during the discovery of
 * characteristic descriptors. It processes the received indication and updates the profile
 * content or takes necessary actions based on the indication.
 *
 * @param ev The GATT event type, indicating the nature of the indication.
 * @param ctx The context of the event, typically a pointer to the GATT profile structure.
 * @param arg Additional argument, which may contain the indication data or other relevant information.
 */
static void prf_disc_char_desc_ind_handler(gatt_event_t ev, void *ctx, void *arg)
{
    prf_search_env_t *context = (prf_search_env_t *)ctx;
    prf_db_cfg_t  *prf_cfg = context->prf_cfg;
    prf_content_t *prf_content = &context->prf_content;
    gatt_chr_dsc_discovered_t *param = (gatt_chr_dsc_discovered_t *)arg;

    BR_LOG_DEBUG("%s, peer:%p char:%p ret:%x\r\n", __func__,
                                                   param->peer,
                                                   param->chr,
                                                   param->result);
    BR_LOG_DEBUG_HEX("prf_disc_char_desc_ind_handler",&param->uuid, sizeof(uuid_t));
    uint8_t last_char =  PRF_CTXT_LASTCHR(prf_content);
    prf_char_desc_def_t *cfg_dsc = PRF_CFG_DSC_PTR(prf_cfg, last_char);
    if(ev == GATTC_EV_CHR_DSC_DISCOVERED) {
        ///Recording descriptor handle.
        gatt_chr_t chr = PRF_CTXT_CHR(prf_content, cfg_dsc->char_idx);
        if(param->dsc && chr == param->chr && hlp_uuid_comp(&param->uuid, &cfg_dsc->uuid)) {
            PRF_CTXT_DSC(prf_content, last_char) = param->dsc;
        }
    }
    else if(ev == GATTC_EV_CHR_DSC_DISCOVER_CMPL){
        //Checking if the current descriptor is for forced discovery.
        if(PRF_CTXT_DSC(prf_content, last_char) || cfg_dsc->mandatory == false) {
            /// Discovering the descriptor of the next characteristic.
            PRF_CTXT_LASTCHR(prf_content)++;
            prf_disc_desc_req(context);
        }
        else {
            /// Description not found
            prf_disc_cfm_send(context, PRF_DISC_ERR_NODESC);
        }
    }
}

/**
 * @brief Handles indications related to characteristic discovery.
 *
 * This function is a callback handler for indications received during the discovery of
 * characteristics. It processes the received indication and updates the profile content
 * or takes necessary actions based on the indication.
 *
 * @param ev The GATT event type, indicating the nature of the indication.
 * @param ctx The context of the event, typically a pointer to the GATT profile structure.
 * @param arg Additional argument, which may contain the indication data or other relevant information.
 *
 * @note This function is typically registered as a callback with the GATT stack to handle
 *       characteristic indications during the service discovery process. It should be
 *       implemented to handle the specific logic of the profile for which it is intended.
 */
static void prf_disc_char_ind_handler(gatt_event_t ev, void *ctx, void *arg)
{
    prf_search_env_t *context = (prf_search_env_t *)ctx;
    prf_db_cfg_t  *prf_cfg = context->prf_cfg;
    prf_content_t *prf_content = &context->prf_content;
    gatt_chr_discovered_t *param = (gatt_chr_discovered_t *)arg;
    uint8_t conhdl = ble_get_conhdl_by_conidx(param->peer) & 0xFF;

    BR_LOG_DEBUG("%s, peer:%p svc:%p char:%p props:%x ret:%x\r\n", __func__,
                                                                   param->peer,
                                                                   param->svc,
                                                                   param->chr,
                                                                   param->props,
                                                                   param->result);
    BR_LOG_DEBUG_HEX("dtsc_disc_char_ind_handler uuid",&param->uuid, sizeof(uuid_t));
    ///Record the characteristic value handle and the end handle of the characteristic
    if(ev == GATTC_EV_CHARACTERISTIC_DISCOVERED) {/// The last feature is reported through feature discovery
        for(uint8_t i=0; i<PRF_CFG_CHR_NB(prf_cfg); i++) {
            prf_char_def_t *cfg_chr = PRF_CFG_CHR_PTR(prf_cfg, i);
            if(hlp_uuid_comp(&param->uuid, &cfg_chr->uuid) 
                && ((param->props & cfg_chr->prop) == cfg_chr->prop))
            {
                PRF_CTXT_CHR(prf_content, i) = param->chr;  
                BR_LOG_DEBUG("%s, char match\r\n",__func__);
                break;
            }
        }
    }
    ///characteristic discovery complete. Ready to discover descriptors
    else{
        ///Check for missing characteristic
        uint8_t match = true;
        for(uint8_t i=0;i<PRF_CFG_CHR_NB(prf_cfg);i++) {
            prf_char_def_t *cfg_chr = PRF_CFG_CHR_PTR(prf_cfg, i);
            if((cfg_chr->mandatory) && (!PRF_CTXT_CHR(prf_content, i))) {
                match = false;
                break;
            }
        }
        BR_LOG_DEBUG("%s, match:%d\r\n",__func__,match);
        if(match) { 
            ///discover desc
            PRF_CTXT_STATE(prf_content) = PRF_STATE_DISC_DESC;
            PRF_CTXT_LASTCHR(prf_content) = 0;
            prf_disc_desc_req(context);
        }
        else { 
            //report no char
            prf_disc_cfm_send(context, PRF_DISC_ERR_NOCHAR);
        }
    }
}

/**
 * @brief Handles service discovery indication events.
 *
 * This function is a callback handler for service discovery indication events.
 * It processes the received indication event and updates the profile content accordingly.
 *
 * @param ev The GATT event type.
 * @param ctx The context of the event, typically a pointer to the GATT profile structure.
 * @param arg Additional argument, which may contain the indication data or other relevant information.
 *
 * @note This function is typically registered as a callback with the GATT stack to handle service discovery indications.
 *       It should be implemented to handle the specific logic of the profile for which it is intended.
 */
static void prf_disc_svc_ind_handler(gatt_event_t ev, void *ctx, void *arg)
{
    prf_search_env_t *context = (prf_search_env_t *)ctx;
    prf_db_cfg_t  *prf_cfg = context->prf_cfg;
    prf_content_t *prf_content = &context->prf_content;
    gatt_svc_discovered_t *param = (gatt_svc_discovered_t *)arg;//todo

    BR_LOG_DEBUG("%s, conidx:%p\r\n",__func__, param->peer);
    BR_LOG_DEBUG_HEX("dtsc svc uuid",&param->uuid, sizeof(uuid_t));
    
    if(ev == GATTC_EV_SERVICE_DISCOVERED) {
        if(hlp_uuid_comp(&param->uuid, PRF_CFG_SVC_PTR(prf_cfg))){
            PRF_CTXT_NBSVC(prf_content)++;
            PRF_CTXT_SVC(prf_content) = param->svc;
        }
    }
    else if(ev == GATTC_EV_SERVICE_DISCOVER_CMPL){ //服务发现完成,
        BR_LOG_DEBUG("%s, nb svc:%d\r\n",__func__, PRF_CTXT_NBSVC(prf_content));
        if(PRF_CTXT_NBSVC(prf_content) != 0) {
            PRF_CTXT_STATE(prf_content) = PRF_STATE_DISC_CHAR,
            gattc_chr_discover(param->peer, PRF_CTXT_SVC(prf_content), NULL, (gatt_cbk_t *)prf_disc_char_ind_handler, context);
        }
        else {
            //report no svc
            prf_disc_cfm_send(context, PRF_DISC_ERR_NOSVC);
        }
    }
}

/**
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void prf_disc_start(uint16_t conhdl, prf_db_cfg_t  *prf_cfg, prf_content_t *prf_content, prf_cbk_t cbk)
{
    ///recording parameters.
    prf_search_env_t *context = NEW(sizeof(prf_search_env_t));
    context->conhdl = conhdl;
    context->prf_cfg = prf_cfg;
    context->cbk = cbk;
    memcpy(&context->prf_content, prf_content, sizeof(prf_content_t));
    ///Clearing transparent transmission service.
    memset(prf_content->svc, 0x00, sizeof(gatt_svc_t));
    memset(prf_content->chars, 0x00, prf_cfg->chr_nb * sizeof(gatt_chr_t));
    memset(prf_content->descs, 0x00, prf_cfg->desc_nb * sizeof(gatt_dsc_t));
    ///Discovering services.
    prf_content->disc_state = PRF_STATE_DISC_SVC;
    gattc_svc_discover(ble_get_conidx_by_conhdl(conhdl), NULL/*PRF_CFG_SVC_PTR(prf_cfg)*/, (gatt_cbk_t *)prf_disc_svc_ind_handler, context);
}

