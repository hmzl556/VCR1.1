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
 * @file prf_gap.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <string.h>
#include "prf_gap.h"
#include "gatt.h"
#include "ble_gap.h"
#include "app_ble.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define GATT_UUID_GENERIC_ACCESS                                0x1800
#define GATT_UUID_DEVICE_NAME                                   0x2A00
#define GATT_UUID_APPEARANCE                                    0x2A01
#define GATT_UUID_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS    0x2A04
#define GATT_UUID_CTL_ADDR_RESOL_SUPP                           0x2AA6
#define GATT_UUID_RSLV_PRIV_ADDR_ONLY                           0x2AC9

enum {
    GAP_IDX_CHAR_DEVNAME,
    GAP_IDX_CHAR_ICON,
    GAP_IDX_CHAR_SLAVE_PREF_PARAM,
    GAP_IDX_CAHR_CNT_ADDR_RESOL,
    GAP_IDX_CHAR_RSLV_PRIV_ADDR_ONLY,
    GAP_IDX_GAP_SVC,
    GAP_IDX_NB,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_chr_t gap_chr[GAP_IDX_NB] = {INST_INVALID};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Callback function for handling GATT events in the Generic Access Profile (GAP).
 *
 * This function is invoked in response to various GATT events related to the GAP.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param ctx The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */
static void prf_gap_cbk(gatt_event_t event, void *ctx, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE:
        case GATTS_EV_CHR_VALUE_WRITE_REQ:
        {
#if 0  ///If you need to change the name through 0x2A00, then enable this macro.
            gatt_chr_val_write_req_t *write = (gatt_chr_val_write_req_t *)param;
            if (write->chr == gap_chr[GAP_IDX_CHAR_DEVNAME]) {
                ble_gap_name_set((const char *)write->val+write->val_off, write->val_len);
                prf_gap_name_set((uint8_t *)write->val+write->val_off, write->val_len);
                if(event == GATTS_EV_CHR_VALUE_WRITE_REQ)
                    gatts_chr_val_write_rsp(write->peer, write->chr, GATT_OK);
            } 
#endif
        }
        break;
        
        case GATTS_EV_CHR_VALUE_READ_REQ: 
        {
            gatt_chr_val_read_req_t *read = (gatt_chr_val_read_req_t *)param;
            do {
                if (read->chr == gap_chr[GAP_IDX_CHAR_DEVNAME]) {
                    uint8_t name[APP_DEVICE_NAME_MAX_LEN] = {0};
                    uint8_t name_len = 0;
                    ble_gap_name_get(name, &name_len);
                    gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, (uint8_t *)name, name_len);
                    break;
                } 
                if (read->chr == gap_chr[GAP_IDX_CHAR_ICON]) {
                    uint16_t appreance = 0;
                    ble_gap_appreance_get(&appreance);
                    gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, (uint8_t *)&appreance, 2);
                    break;
                }
                if (read->chr == gap_chr[GAP_IDX_CHAR_SLAVE_PREF_PARAM]) {
                    ble_conn_param_t conn_par;
                    ble_gap_conn_param_get(&conn_par);
                    gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, (uint8_t *)&conn_par, sizeof(ble_conn_param_t));
                    break;
                }
#ifdef GAP_PRF_TESET
                if (read->chr == gap_chr[GAP_IDX_CAHR_CNT_ADDR_RESOL]) {
                    gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, (uint8_t *)NULL, 0);
                    break;
                }
                if (read->chr == gap_chr[GAP_IDX_CHAR_RSLV_PRIV_ADDR_ONLY]) {
                    gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, (uint8_t *)NULL, 0);
                    break;
                }
#endif
            }while(0);
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
int prf_gap_init(void)
{
    gatt_chr_add_params_t char_add_params[] =
    {
        [GAP_IDX_CHAR_DEVNAME]              = {&gap_chr[GAP_IDX_CHAR_DEVNAME], {UUID_LEN_16,UUID16_VAL_SET(GATT_UUID_DEVICE_NAME)}, 
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_gap_cbk, NULL},
        [GAP_IDX_CHAR_ICON]                 = {&gap_chr[GAP_IDX_CHAR_ICON], {UUID_LEN_16,UUID16_VAL_SET(GATT_UUID_APPEARANCE)}, 
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_gap_cbk, NULL},
        [GAP_IDX_CHAR_SLAVE_PREF_PARAM]     = {&gap_chr[GAP_IDX_CHAR_SLAVE_PREF_PARAM], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS)}, 
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_gap_cbk, NULL},
#ifdef GAP_PRF_TESET
        [GAP_IDX_CAHR_CNT_ADDR_RESOL]       = {&gap_chr[GAP_IDX_CAHR_CNT_ADDR_RESOL], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_CTL_ADDR_RESOL_SUPP)}, 
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_gap_cbk, NULL},
        [GAP_IDX_CHAR_RSLV_PRIV_ADDR_ONLY]  = {&gap_chr[GAP_IDX_CHAR_RSLV_PRIV_ADDR_ONLY], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_RSLV_PRIV_ADDR_ONLY)}, 
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_gap_cbk, NULL},
#endif
    };
    
    gatt_svc_create_params_t svc_create_params[] = 
    {
        [0] = {&gap_chr[GAP_IDX_GAP_SVC], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_GENERIC_ACCESS)}, 
                NULL, NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t),char_add_params},
    };

    *svc_create_params[0].psvc = gatts_svc_create(&svc_create_params[0].uuid, svc_create_params[0].cbk, svc_create_params[0].ctx);
    for (int i = 0; i < svc_create_params[0].chr_num; i++) {
        if (svc_create_params[0].chrs[i].props & GATT_CHR_PROP_READ) {
            svc_create_params[0].chrs[i].secu |= ATT_PERM_READ;
        }
        if (svc_create_params[0].chrs[i].props & GATT_CHR_PROP_WRITE) {
            svc_create_params[0].chrs[i].secu |= ATT_PERM_WRITE;
        }
        if(i == GAP_IDX_CHAR_DEVNAME) {
            uint8_t name[APP_DEVICE_NAME_MAX_LEN];
            uint8_t name_len;
            ble_gap_name_get(name, &name_len);
            *svc_create_params[0].chrs[i].pchr = gatts_chr_ro_add(*svc_create_params[0].psvc, 
                                                                &svc_create_params[0].chrs[i].uuid, 
                                                                svc_create_params[0].chrs[i].props, 
                                                                svc_create_params[0].chrs[i].secu, 
                                                                svc_create_params[0].chrs[i].cbk, 
                                                                svc_create_params[0].chrs[i].ctx,
                                                                name, name_len);
        }
        else {
            *svc_create_params[0].chrs[i].pchr = gatts_chr_add(*svc_create_params[0].psvc, 
                                                            &svc_create_params[0].chrs[i].uuid, 
                                                            svc_create_params[0].chrs[i].props, 
                                                            svc_create_params[0].chrs[i].secu, 
                                                            svc_create_params[0].chrs[i].cbk, 
                                                            svc_create_params[0].chrs[i].ctx);
        }
        if (svc_create_params[0].chrs[i].ccc_props) {
            svc_create_params[0].chrs[i].ccc_secu |= (ATT_PERM_READ | ATT_PERM_WRITE);
            gatts_dsc_ccc_add(*svc_create_params[0].chrs[i].pchr, 
                              svc_create_params[0].chrs[i].ccc_props, 
                              svc_create_params[0].chrs[i].ccc_secu);
        }
    }
    gatts_svc_start(*svc_create_params[0].psvc);

    return 0;
}

void prf_gap_name_set(uint8_t *name, uint16_t name_len)
{
    gap_chr[GAP_IDX_CHAR_DEVNAME] = gatts_chr_ro_update(gap_chr[GAP_IDX_GAP_SVC], gap_chr[GAP_IDX_CHAR_DEVNAME], (uint8_t *)name, name_len);
}

