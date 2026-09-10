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
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAYS OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file app_gpio.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef __APP_GPIO_H__
#define __APP_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "hal_gpio.h"

#ifdef i2616e_u1
#define BLE_DISC_PIN            HAL_GPIO_PIN_3      //PA3
#define BLE_DISC_DIR            HAL_GPIO_PU_IN
#define BLE_DISC_INT            HAL_GPIO_INT_FALLING_EDGE 

#define BLE_CONN_IND_PIN        HAL_GPIO_PIN_18     //PB11
#define BLE_CONN_IND_DIR        HAL_GPIO_PP_OUT
#define BLE_CONN_IND_INT        HAL_GPIO_INT_DISABLE

#define BLE_READY_IND_PIN       HAL_GPIO_PIN_3      //PA3
#define BLE_READY_IND_DIR       HAL_GPIO_PP_OUT
#define BLE_READY_IND_INT       HAL_GPIO_INT_DISABLE

#define BLE_ADV_IND_PIN         HAL_GPIO_PIN_11     //PB4
#define BLE_ADV_IND_DIR         HAL_GPIO_PP_OUT
#define BLE_ADV_IND_INT         HAL_GPIO_INT_DISABLE
#elif i2616e
#define BLE_DISC_PIN            HAL_GPIO_PIN_9      //PB2
#define BLE_DISC_DIR            HAL_GPIO_PU_IN
#define BLE_DISC_INT            HAL_GPIO_INT_FALLING_EDGE

#define BLE_CONN_IND_PIN        HAL_GPIO_PIN_18     //PB11
#define BLE_CONN_IND_DIR        HAL_GPIO_PP_OUT
#define BLE_CONN_IND_INT        HAL_GPIO_INT_DISABLE

#define BLE_READY_IND_PIN       HAL_GPIO_PIN_3      //PA3
#define BLE_READY_IND_DIR       HAL_GPIO_PP_OUT
#define BLE_READY_IND_INT       HAL_GPIO_INT_DISABLE

#define BLE_ADV_IND_PIN         HAL_GPIO_PIN_11     //PB4
#define BLE_ADV_IND_DIR         HAL_GPIO_PP_OUT
#define BLE_ADV_IND_INT         HAL_GPIO_INT_DISABLE
#endif

#ifdef CFG_CONN_LED 
#define LED_CONN_IND_ON()       hal_gpio_write_bit(BLE_CONN_IND_PIN,  BIT_SET)
#define LED_CONN_IND_OFF()      hal_gpio_write_bit(BLE_CONN_IND_PIN,  BIT_RESET)
#else 
#define LED_CONN_IND_ON()       
#define LED_CONN_IND_OFF()      
#endif

#ifdef CFG_READY_LED 
#define LED_READY_IND_ON()      hal_gpio_write_bit(BLE_READY_IND_PIN,  BIT_RESET)
#define LED_READY_IND_OFF()     hal_gpio_write_bit(BLE_READY_IND_PIN,  BIT_SET)
#else 
#define LED_READY_IND_ON()      
#define LED_READY_IND_OFF()     
#endif

#ifdef CFG_ADV_LED
#define LED_ADV_IND_ON()        hal_gpio_write_bit(BLE_ADV_IND_PIN,  BIT_SET)
#define LED_ADV_IND_BLINK()     hal_gpio_toggle_pin(BLE_ADV_IND_PIN)
#define LED_ADV_IND_OFF()       hal_gpio_write_bit(BLE_ADV_IND_PIN,  BIT_RESET)
#else 
#define LED_ADV_IND_ON()        
#define LED_ADV_IND_BLINK()     
#define LED_ADV_IND_OFF()       
#endif

/**
 * @brief 
 * 
 */
void ble_disc_io_init(void);

/**
 * @brief 
 * 
 */
void ble_conn_io_init(void);

/**
 * @brief 
 * 
 */
void ble_adv_io_init(void);

/**
 * @brief 
 * 
 */
void ble_adv_io_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_GPIO_H__ */
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
