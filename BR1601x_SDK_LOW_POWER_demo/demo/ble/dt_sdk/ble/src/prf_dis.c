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
 * @file prf_dis.c
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
#include "prf_dis.h"
#include "gap.h"
#include "gatt.h"
#include "log.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
#define GATT_UUID_DEVICE_INFORMATION                                        0x180A
#define GATT_UUID_SYSTEM_ID                                                 0x2A23
#define GATT_UUID_MODEL_NUMBER_STRING                                       0x2A24
#define GATT_UUID_SERIAL_NUMBER_STRING                                      0x2A25
#define GATT_UUID_FIRMWARE_REVISION_STRING                                  0x2A26                                         
#define GATT_UUID_HARDWARE_REVISION_STRING                                  0x2A27                                        
#define GATT_UUID_SOFTWARE_REVISION_STRING                                  0x2A28                               
#define GATT_UUID_MANUFACTURER_NAME_STRING                                  0x2A29                               
#define GATT_UUID_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST       0x2A2A
#define GATT_UUID_PNP_ID                                                    0x2A50

enum {
    DIS_IDX_CHAR_SYSTEM_ID,
    DIS_IDX_CHAR_MODEL_NB_STR,           
    DIS_IDX_CHAR_SERIAL_NB_STR,
    DIS_IDX_CHAR_FIRM_REV_STR,
    DIS_IDX_CHAR_HARD_REV_STR,
    DIS_IDX_CHAR_SW_REV_STR,
    DIS_IDX_CHAR_MANUFACTURER_NAME,
    DIS_IDX_CHAR_IEEE,
    DIS_IDX_CHAR_PNP_ID_CHARL,
    DIS_IDX_SVC,
    DIS_IDX_NB,
};

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
static gatt_chr_t dis_chr[DIS_IDX_NB] = {INST_INVALID};

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Callback function for handling GATT events in the Device Information Service (DIS).
 *
 * This function is invoked in response to various GATT events related to the DIS.
 * It processes the events and performs the necessary actions based on the event type and parameters.
 *
 * @param event The GATT event that triggered the callback.
 * @param ctx The context associated with the event, which may include the connection handle or other relevant data.
 * @param param The parameters related to the event, which may include details of the event such as data or status codes.
 */ 
static void prf_dis_cbk(gatt_event_t event, void *ctx, void *param)
{
    BR_LOG_DEBUG("%s, evt: %x\r\n", __func__, event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t *val = NULL;
            uint8_t val_len = 0;
            do {
                 if (in->chr == dis_chr[DIS_IDX_CHAR_SYSTEM_ID]) {
                    val = (uint8_t *)APP_DIS_SYSTEM_ID;
                    val_len = APP_DIS_SYSTEM_ID_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_MODEL_NB_STR]) {
                    val = (uint8_t *)APP_DIS_MODEL_NB_STR;
                    val_len = APP_DIS_MODEL_NB_STR_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_SERIAL_NB_STR]) {
                    val = (uint8_t *)APP_DIS_SERIAL_NB_STR;
                    val_len = APP_DIS_SERIAL_NB_STR_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_FIRM_REV_STR]) {
                    val = (uint8_t *)APP_DIS_FIRM_REV_STR;
                    val_len = APP_DIS_FIRM_REV_STR_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_HARD_REV_STR]) {
                    val = (uint8_t *)APP_DIS_HARD_REV_STR;
                    val_len = APP_DIS_HARD_REV_STR_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_SW_REV_STR]) {
                    val = (uint8_t *)APP_DIS_SW_REV_STR;
                    val_len = APP_DIS_SW_REV_STR_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_MANUFACTURER_NAME]) {
                    val = (uint8_t *)APP_DIS_MANUFACTURER_NAME;
                    val_len = APP_DIS_MANUFACTURER_NAME_LEN;
                    break;
                 }
                 if (in->chr == dis_chr[DIS_IDX_CHAR_IEEE]) {
                    val = (uint8_t *)APP_DIS_IEEE;
                    val_len = APP_DIS_IEEE_LEN;
                    break;
                 }if (in->chr == dis_chr[DIS_IDX_CHAR_PNP_ID_CHARL]) {
                    val = (uint8_t *)APP_DIS_PNP_ID;
                    val_len = APP_DIS_PNP_ID_LEN;
                    break;
                 }
            }while(0);
            /// read rsp
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, val_len);
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
int prf_dis_init(void)
{
    gatt_chr_add_params_t char_add_params[] ={
        [DIS_IDX_CHAR_SYSTEM_ID]            = {&dis_chr[DIS_IDX_CHAR_SYSTEM_ID], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_SYSTEM_ID)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_MODEL_NB_STR]         = {&dis_chr[DIS_IDX_CHAR_MODEL_NB_STR], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_MODEL_NUMBER_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_SERIAL_NB_STR]        = {&dis_chr[DIS_IDX_CHAR_SERIAL_NB_STR], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_SERIAL_NUMBER_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_FIRM_REV_STR]         = {&dis_chr[DIS_IDX_CHAR_FIRM_REV_STR], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_FIRMWARE_REVISION_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_HARD_REV_STR]         = {&dis_chr[DIS_IDX_CHAR_HARD_REV_STR], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_HARDWARE_REVISION_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_SW_REV_STR]           = {&dis_chr[DIS_IDX_CHAR_SW_REV_STR], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_SOFTWARE_REVISION_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_MANUFACTURER_NAME]    = {&dis_chr[DIS_IDX_CHAR_MANUFACTURER_NAME], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_MANUFACTURER_NAME_STRING)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_IEEE]                 = {&dis_chr[DIS_IDX_CHAR_IEEE], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
        [DIS_IDX_CHAR_PNP_ID_CHARL]         = {&dis_chr[DIS_IDX_CHAR_PNP_ID_CHARL], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_PNP_ID)},
                                                GATT_CHR_PROP_READ, 0, 0, 0, prf_dis_cbk, NULL},
    };

    gatt_svc_create_params_t svc_create_params[] = 
    {
        [0] = {&dis_chr[DIS_IDX_SVC], {UUID_LEN_16, UUID16_VAL_SET(GATT_UUID_DEVICE_INFORMATION)}, 
                NULL, NULL, sizeof(char_add_params)/sizeof(gatt_chr_add_params_t), char_add_params},
    };

    gatts_svc_create_ex(sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t), svc_create_params);

    for (int i = 0; i < sizeof(svc_create_params) / sizeof(gatt_svc_create_params_t); i++) {
        gatts_svc_start(*svc_create_params[i].psvc);
    }

	return 0;
}

