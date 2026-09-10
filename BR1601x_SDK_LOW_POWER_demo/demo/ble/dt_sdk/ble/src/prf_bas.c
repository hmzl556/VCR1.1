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
 * @file prf_bas.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "gap.h"
#include "gatt.h"
#include "prf_bas.h"
#include "app_adc.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define GATT_UUID_BATTERY_SERVICE                   0x180F
#define GATT_UUID_BATTERY_LEVEL                     0x2A19

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_svc_t svc_bas = INST_INVALID;
static gatt_chr_t chr_bat_lev = INST_INVALID;

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Callback function for the Battery Service (BAS) profile.
 *
 * This function is called in response to various GATT events related to the Battery Service profile.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param ctx The context associated with the event.
 * @param param The parameters related to the event.
 */
static void prf_bas_cbk(gatt_event_t event, void *ctx, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *read = (gatt_chr_val_read_req_t *)param;
            uint32_t voltage = adc_read_battery_voltage();
            uint8_t bat_level = adc_voltage_to_percentage(voltage);
            gatts_chr_val_read_rsp(read->peer, read->chr, GATT_OK, &bat_level, sizeof(uint8_t));
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
void prf_bas_bat_level_send(peer_t peer, uint8_t bat_level)
{
    if (INST_INVALID == chr_bat_lev) return;
        
    gatts_chr_val_notif(peer, chr_bat_lev, &bat_level, sizeof(uint8_t));
}

int prf_bas_init(void)
{
    gatt_chr_add_params_t char_add_params[] ={
        [0] = {&chr_bat_lev, {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_BATTERY_LEVEL)}, 
                GATT_CHR_PROP_READ, 0, GATT_CHR_PROP_NOTIFY, 0, prf_bas_cbk, NULL},
    };
    
    gatt_svc_create_params_t svc_create_params[] = {
        [0] = {&svc_bas, {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_BATTERY_SERVICE)}, 
                NULL, NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t),char_add_params},
    };

    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }
    
	return 0;
}


