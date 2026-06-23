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
 * @file br_timer.h
 * @author Barrot Firmware Team
 * @version v1.0.1
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

/** @addtogroup 
 * @{
 */

#ifndef _BR_TIMER_H_
#define _BR_TIMER_H_
#include "stdint.h"
/* Includes ------------------------------------------------------------------*/
/* Public typedef -----------------------------------------------------------*/
/// Timer callback function type definition
typedef void (*timer_callback_t)(uint8_t event, void *arg);
/* Public define ------------------------------------------------------------*/    
/// Max timer delay 41943sec (41943000ms)
#define KE_TIMER_DELAY_MAX          (41943000)
/// Value indicating an invalide timer operation
#define BR_TIMER_INVALID_HANDLER    (0xFF)
/* Public constants ---------------------------------------------------------*/
/* Public function prototypes -----------------------------------------------*/
uint32_t sdk_timer_del(uint32_t hdl);
uint32_t sdk_timer_restart(uint32_t hdl,uint32_t millisec);
uint32_t sdk_timer_add(uint32_t millisec, uint8_t event, timer_callback_t cb, void *arg);
uint32_t sdk_timer_get_current_ms(void);
#endif // _BR_TIMER_H_

/**
 * @}
 */
