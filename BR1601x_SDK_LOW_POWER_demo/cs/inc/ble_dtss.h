/*****************************************************************************
 * Copyright (c) 2024, Barrot Technology Co.,Ltd..
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
 * @file ble_dtss.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2024, Barrot Technology Co.,Ltd.. All rights reserved.
 */

 /** @addtogroup
 * @{
 */
#ifndef _BLE_DTSS_H
#define _BLE_DTSS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>

#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "ble_adv.h"
#include "ble_gap.h"

/* Define -------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef uint8_t (*ble_send_to_uart_cbk)(const uint8_t *data, uint16_t len);
typedef struct {
    peer_t     peer;
    gatt_svc_t dtss_svc;
    gatt_chr_t dtss_chr0;
    gatt_chr_t dtss_chr1;
    ble_send_to_uart_cbk ble_send_to_uart;
}ble_dtss_env_t;

/* Public define ------------------------------------------------------------*/

/* Public variables ---------------------------------------------------------*/

/* Public function prototypes -----------------------------------------------*/


int ble_dtss_init(void);
void ble_send_to_peer(uint8_t *data, uint16_t len);
void ble_send_to_uart_cbk_reg(ble_send_to_uart_cbk ble_send_to_uart);


#ifdef __cplusplus
}
#endif

#endif //_BLE_DTSS_H
