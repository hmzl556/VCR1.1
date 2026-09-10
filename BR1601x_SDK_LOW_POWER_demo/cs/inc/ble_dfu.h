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
 * @file ble_dfu.h
 * @author Tian Leixin
 * @version v1.1
 *
 * @copyright Copyright (c) 2026, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#ifndef _BLE_DFU_H_
#define _BLE_DFU_H_
#include <stdint.h>
#include <stdbool.h>
#include "br_dfu_boot.h"
/* Public define ------------------------------------------------------------*/
/* Public typedef -----------------------------------------------------------*/
typedef enum {
    DFU_EVT_START,
    DFU_EVT_SUCCESS,
    DFU_EVT_ERROR,
    DFU_EVT_CANCEL
}ble_dfu_evt;

typedef void(*p_cb_t)(ble_dfu_evt evt);

int ble_dfu_init(void);
void ble_dfu_cb_reg(p_cb_t cb);
void ble_dfu_action_init(void);
bool ble_dfu_is_finished(void);
#endif
