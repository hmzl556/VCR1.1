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
 * @file prf_gatt.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "porting.h"
#include "prf_gatt.h"
#include "gatt.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define GATT_UUID_SERVICE_CHANGED                   0x2A05
#define GATT_UUID_GENERIC_ATTRIBUTE                 0x1801
#define GATT_UUID_CHAR_CLI_SUP_FEAT                 0x2B29
#define GATT_UUID_CHAR_DB_HASH                      0x2B2A

enum {
    GATT_IDX_CHAR_SVC_CHANGED,
    GATT_IDX_CHAR_CLI_SUP_FEAT,
    GATT_IDX_CHAR_DB_HASH,
    GATT_IDX_SVC,
    GATT_IDX_NB,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_svc_t gatt_chr[GATT_IDX_NB] = {INST_INVALID};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Callback function for handling GATT events in the Generic Attribute Profile Service (0x1801).
 *
 * This function is invoked in response to various GATT events related to the GATT Service.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param context The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_gatt_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event)
    {
        case GATTS_EV_CHR_VALUE_SEND_START: {
            /* If the current state is IDLE and a notification is sent on the audio characteristic, update the state to READY and start the audio if necessary */
            /*gatt_chr_val_send_state_t *chr_val_send_state = (gatt_chr_val_send_state_t *)param;
            if (chr_val_send_state->chr == chr_svc_change) {
                if (chr_val_send_state->type & GATT_SEND_VAL_NOTIFICATION) {
                  
                }
            }*/
        }
        break;
    
    default:
        /* For all other events, do nothing */
        break;
    }
}

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
gatt_rst_t prf_gatt_svc_change(svc_change_t *svc_change)
{
    uint8_t val[4] = {0};

    ENCODE2BYTE_LITTLE(val, svc_change->start_hdl);
    ENCODE2BYTE_LITTLE(val + 2, svc_change->end_hdl);
    return gatts_chr_val_indicate(svc_change->peer, gatt_chr[GATT_IDX_CHAR_SVC_CHANGED], val, 4);
}

int prf_gatt_init(void)
{
    gatt_chr_add_params_t char_add_params[] ={
        [GATT_IDX_CHAR_SVC_CHANGED]  = {&gatt_chr[GATT_IDX_CHAR_SVC_CHANGED], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_SERVICE_CHANGED)},
                                        0, 0, GATT_CHR_PROP_INDICATE, 0, prf_gatt_cbk, NULL},
#ifdef GATT_PRF_TESET
        [GATT_IDX_CHAR_CLI_SUP_FEAT] = {&gatt_chr[GATT_IDX_CHAR_CLI_SUP_FEAT], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_CHAR_CLI_SUP_FEAT)},
                                        GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE, 0, 0, 0, prf_gatt_cbk, NULL},
        [GATT_IDX_CHAR_DB_HASH]      = {&gatt_chr[GATT_IDX_CHAR_DB_HASH], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_CHAR_DB_HASH)},
                                        GATT_CHR_PROP_READ, 0, 0, 0, prf_gatt_cbk, NULL},
#endif
    };
    
    gatt_svc_create_params_t svc_create_params[] = 
    {
        [0] = {&gatt_chr[GATT_IDX_SVC], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_GENERIC_ATTRIBUTE)},
                NULL,NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t), char_add_params},
    };

    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }

	return 0;
}
