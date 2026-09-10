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
 * @file co_timer.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2024, Barrot Technology Co.,Ltd.. All rights reserved.
 */

 /** @addtogroup 
 * @{ 
  */ 
#ifndef _CO_TIMER_H
#define _CO_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Define -------------------------------------------------------------------*/
/// Max timer delay 41943sec (41943000ms)
#define KE_TIMER_DELAY_MAX          (41943000)
/// Value indicating an invalide timer operation
#define BR_TIMER_INVALID_HANDLER    (0xFF)

/* Typedef ------------------------------------------------------------------*/
typedef void* co_timer_handle_t;


/* Public define ------------------------------------------------------------*/
typedef enum co_timer_mode 
{
    CO_TIMER_ONE_SHOT = 0,          //one shot
    CO_TIMER_REPET,                 //repet
    CO_TIMER_ONE_SHOT_AUTO_FREE,    //one shot and auto free
} co_timer_mode_t;

/**
 * @brief: timer callback
 * @param {void*} param
 * @return {*}
 */
typedef void (*co_timer_cb_t)(void* param);

/* Public variables ---------------------------------------------------------*/
typedef struct
{
    /// Integer part of the time (in half-slot)
    uint32_t hs;
    /// Fractional part of the time (in half-us) (range: 0-624)
    uint32_t hus;
} co_timer_clk_t;

/* Public function prototypes -----------------------------------------------*/

/**
 * @brief co tiamer init
 * @return {*}
 */
void co_timer_init(void);

/**
 * @brief create a software timer, will malloc memory for timer(max num is 64)
 * @param {co_timer_handle_t*} timer_handle, timer handle pointer
 * @param {co_timer_cb_t} cb, callback function
 * @param {co_timer_mode_t} mode, timer mode @see co_timer_mode_t
 * @return {*}true: success, false: failed
 */
bool co_timer_create(co_timer_handle_t* timer_handle, co_timer_cb_t cb, co_timer_mode_t mode);

/**
 * @brief start the timer
 * @param {co_timer_handle_t*} timer_handle, timer handle pointer
 * @param {void*} arg, callback function param
 * @param {uint64_t} period, timer period
 * @return {*}true: success, false: failed
 */
bool co_timer_start(co_timer_handle_t* timer_handle, void* arg, uint64_t period);

/**
 * @brief stop the timer
 * @param {co_timer_handle_t*} timer_handle, timer handle pointer
 * @return {*}true: success, false: failed
 */
bool co_timer_stop(co_timer_handle_t* timer_handle);

/**
 * @brief delete the timer
 * @param {co_timer_handle_t*} timer_handle, timer handle pointer
 * @return {*}true: success, false: failed
 */
bool co_timer_delete(co_timer_handle_t* timer_handle);

/**
 * @brief get the number of milliseconds that have elapsed since the system was started.
 * @return {*}unit: ms
 */
uint32_t co_timer_get_current_ms(void);

/**
 * @brief get ip souse clock
 * @return {*}@see co_timer_clk_t
 */
co_timer_clk_t co_timer_get_souse_clock(void);


#ifdef __cplusplus
}
#endif

#endif //_CO_TIMER_H
