/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 1999-2020 Barrot Technology Limited
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/**
 * @file    co_printf.h
 * @brief   This file provides all the STDOUT functions.
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * @date    11-Augest-2018
 * @version V1.0.0
 * @author  Asher
*/

#ifndef _CM_PRINTF_H
#define _CM_PRINTF_H

#include <stdint.h>

#define DBG_TYPE_NONE                   (0x0000)    //0x0000
#define DBG_TYPE_ERROR                  (1 << 0)    //0x0001
#define DBG_TYPE_WARNING                (1 << 1)    //0x0002
#define DBG_TYPE_INFO                   (1 << 2)    //0x0004
#define DBG_TYPE_DRIVER                 (1 << 3)    //0x0008
#define DBG_TYPE_CS                     (1 << 4)    //0x0010
#define DBG_TYPE_APP                    (1 << 8)    //0x0100
#define DBG_TYPE_HCI                    (1 << 9)    //0x0100

#define DBG_TYPE_ALL                    (0xFFFF)    //0xFFFF

#define DBG_TYPE_ON                     (0)

void delay_cycles(uint32_t ui32Cycles);
void delay_n_10us(uint32_t val);
void delay_n_ms(uint32_t val);
void delay_n_us(uint32_t val);
void Delay_ms(uint32_t count);
void Delay(uint32_t count);
void br_sleep(void);
uint8_t br_sleep_lock_acquire(void);
uint8_t br_sleep_lock_release(void);
void app_sleep_prepare_proc(void);
void app_sleep_resume_proc(void);

void co_log_init(void);
void co_log_set_baud(uint32_t baud);
void co_log_deinit(void);
int co_printf(const char *fmt, ...);
int co_printf_type(uint16_t dbg_type, const char *fmt, ...);

void log_buff(uint8_t *data, int len);

#endif /* _CM_PRINTF_H */

