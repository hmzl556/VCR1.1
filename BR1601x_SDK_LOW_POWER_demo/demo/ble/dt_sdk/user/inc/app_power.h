/**
 * @file app_power.h
 * @author Barrot Firmware Team
 * @brief This file contains ble's sleep reminder as well as the interface to wake up mcu
 * @version 0.1
 * @date 2023-05-25
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __APP__POWER__H__
#define __APP__POWER__H__

/**
 * INCLUDE FILES
 *******************************************************************************
 */

#include "hal_gpio.h"
#include "porting.h"

/**
 * DEFINES
 ****************************************************************************************
 */
#ifdef i2616e_u1
#define MCU_WAKEUP_PIN			HAL_GPIO_PIN_11             //PB4
#define MCU_WAKEUP_DIR			HAL_GPIO_PP_OUT
#define MCU_WAKEUP_INT          HAL_GPIO_INT_DISABLE

#define BLE_SLEEP_PIN 		    HAL_GPIO_PIN_12             //PB5
#define BLE_SLEEP_DIR		    HAL_GPIO_PD_IN
#define BLE_SLEEP_INT		    HAL_GPIO_INT_RISING_EDGE 
#elif i2616e
#define MCU_WAKEUP_PIN			HAL_GPIO_PIN_20             //PB13
#define MCU_WAKEUP_DIR			HAL_GPIO_PP_OUT
#define MCU_WAKEUP_INT          HAL_GPIO_INT_DISABLE

#define BLE_SLEEP_PIN 		    HAL_GPIO_PIN_10             //PB3
#define BLE_SLEEP_DIR		    HAL_GPIO_PD_IN
#define BLE_SLEEP_INT		    HAL_GPIO_INT_RISING_EDGE 
#endif

#define HAL_WAKEUP_MCU(LEVEL)       hal_gpio_write_bit(MCU_WAKEUP_PIN, LEVEL)
#define BLE_SLEEP_ENABLE_LEVEL      BIT_SET

/**
 * TYPEDEF
 ****************************************************************************************
 */

typedef enum{
    POWER_MCU_WAKE_END_IND = 0,
    POWER_MCU_WAKE_IND,
    POWER_MCU_SLEEP_IND,
    POWER_MCU_WAKE_DELAY_IND,
    POWER_MCU_WAKE_TIMEOUT_IND,
    POWER_BLE_WAKE_IND,
    POWER_BLE_SLEEP_IND,
}app_power_evt_t;

typedef void (power_evt_cb_t)(app_power_evt_t, void*);

/**
 * GLOBAL FUNCTION DECLARATION
 ****************************************************************************************
 */

/**
 * @brief 根据指定的方式初始化睡眠使能引脚，唤醒引脚，以及注册回调函数
 * 
 * @param fn 回调函数指针，参考power_evt_cb_t
 */
void app_power_init(power_evt_cb_t * fn);

/**
 * @brief 唤醒对端mcu，可重入
 * 
 */
void app_power_mcu_wakeup(void);

/**
 * @brief 执行结束唤醒动作
 * 
 */ 
void app_power_mcu_wakeup_end_exec(void);

/**
 * @brief 更新唤醒和睡眠相关参数
 * 
 */
void app_power_param_update(void);

/**
 * @brief 更新mcu睡眠延时
 *
 */
void app_power_mcu_sleep_timer_update(void);

/**
 * @brief 初始化睡眠使能引脚中断触发方式
 * 
 */
void app_power_sleep_pin_init(hal_gpio_direction_t dir, hal_gpio_interrupt_t interrupt_mode);
#endif
