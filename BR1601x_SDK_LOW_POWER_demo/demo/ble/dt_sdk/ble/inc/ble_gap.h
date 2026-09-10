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
 * @file ble_gap.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _BLE_GAP_H_
#define _BLE_GAP_H_

#include "stdint.h"
#include "porting.h"
//#include "app_ble_cfg.h"

#define APP_DEVICE_NAME_MAX_LEN     29

typedef struct{
    uint8_t addr[BD_ADDR_LEN];
}gap_addr_t;

typedef struct{
    uint16_t intv_min; /*连接参数最小值*/
    uint16_t intv_max; /*连接参数最大值*/
    uint16_t latency;      /*连接允许从机最多忽略连接事件数*/
    uint16_t time_out;      /*连接丢失超时时间*/
}ble_conn_param_t;

typedef struct{
    /// Device MAC address type (enum gapm_own_addr)
    uint8_t mac_addr_type;
    /// Device MAC address
    gap_addr_t  mac_addr;
    /// Device GATT MTU
    uint16_t mtu;
    /// Maximum number of connections
    uint8_t conn_num_max;   
    /// Device appearance
    uint16_t appearance;
    /// Device Name length
    uint8_t dev_name_len;
    /// Device Name
    uint8_t dev_name[APP_DEVICE_NAME_MAX_LEN];
    /// Device connection parameters
    ble_conn_param_t dev_conn_param;    
}ble_gap_params_t;

uint32_t ble_gap_init(ble_gap_params_t *gap_params);
void ble_gap_name_get(uint8_t dev_name[APP_DEVICE_NAME_MAX_LEN], uint8_t *dev_name_len);
void ble_gap_appreance_get(uint16_t *appreance);
void ble_gap_conn_param_get(ble_conn_param_t *param);

#endif

