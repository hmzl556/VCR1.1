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
 * @file ble_adv.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _BLE_ADV_H_
#define _BLE_ADV_H_

#include "stdlib.h"
#include "app_ble.h"

#ifndef ADV_DATA_MAX_SIZE
#define ADV_DATA_MAX_SIZE                31
#endif

typedef struct{
    uint8_t adv_data_len;       /*广播数据长度*/
    uint8_t adv_data[ADV_DATA_MAX_SIZE];    /*广播数据payload*/
}ble_adv_data_t;

typedef struct {
    uint8_t enable;
    /// Advertising duration (in unit of 10ms). 0 means that advertising continues
    /// until the host disable it
    uint16_t duration;
    /// advertising interval (in unit of 625us). Must be greater than 20ms
    uint16_t adv_intv;
}adv_time_t;

typedef struct {    
    uint8_t adv_data[ADV_DATA_MAX_SIZE];
    uint8_t adv_data_len;
    uint8_t scan_rsp_data[ADV_DATA_MAX_SIZE];
    uint8_t scan_rsp_data_len;
    uint8_t attach_appearance;
    uint8_t attach_name;
    uint8_t adv_phy;
#if 0
    uint8_t  ex_adv_enable;
    uint8_t* ex_adv_p_data;
    uint8_t  ex_adv_data_len;
#endif
    adv_time_t adv_time;
}ble_adv_params_t;

void ble_adv_cbk(gap_event_t ev, void *ctx, void *par);
void ble_adv_init(ble_adv_params_t *adv_init);
void ble_adv_deinit(void);
int ble_adv_start(void);
int ble_adv_stop(void);
bool ble_adv_state_get(void);
void ble_adv_intv_set(uint16_t intv);
void ble_scan_rsp_data_update(void);
uint32_t ble_adv_data_set(ble_adv_data_t *adv_data);
uint32_t ble_scan_rsp_data_set(ble_adv_data_t *rsp_data);
void ble_adv_data_get(uint8_t *p_data, uint8_t *len);
void ble_scan_rsp_data_get(uint8_t *p_data, uint8_t *len);

#endif
