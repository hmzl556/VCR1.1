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
 * @file ble_non_standard_protocol.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2024, Barrot Technology Co.,Ltd.. All rights reserved.
 */

 /** @addtogroup 
 * @{ 
 */ 
#ifndef _BLE_NON_STD_H_
#define _BLE_NON_STD_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Define -------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef enum le_scn_conn_ch_sel{
    //USE ALL CHANNEL
    LE_SCN_CONN_CH_ALL = 0,
    //CHANNEL 37
    LE_SCN_CONN_CH_37 = 37,
    //CHANNEL 38
    LE_SCN_CONN_CH_38 = 38,
    //CHANNEL 39
    LE_SCN_CONN_CH_39 = 39,
}le_scn_conn_ch_sel_t;

/* Public define ------------------------------------------------------------*/

/* Public variables ---------------------------------------------------------*/

/* Public function prototypes -----------------------------------------------*/

/**
 * @brief get scan channel
 * @param NULL
 * @return le_scn_conn_ch_sel_t
 */
le_scn_conn_ch_sel_t le_scn_ch_get(void);

/**
 * @brief get conn channel
 * @param NULL
 * @return le_scn_conn_ch_sel_t
 */
le_scn_conn_ch_sel_t le_conn_ch_get(void);

/**
 * @brief set scan channel
 * @param {le_scn_conn_ch_sel_t} ch
 * @return {*}
 */
void le_scan_set_ch(le_scn_conn_ch_sel_t ch);

/**
 * @brief set conn channel
 * @param {le_scn_conn_ch_sel_t} ch
 * @return {*}
 */
void le_conn_set_ch(le_scn_conn_ch_sel_t ch);


#ifdef __cplusplus
}
#endif

#endif //_FSM_FT_H
