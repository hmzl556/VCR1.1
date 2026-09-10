/**
 * @file app_power.c
 * @author Barrot Firmware Team
 * @brief This file implements the interface of waking up the peer mcu and reporting sleep and wake up events
 * @version 0.1
 * @date 2023-05-25
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "app_power.h"
#include "cfg.h"
#include "task.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define MCU_WAKE_END_DELAY  			2

#define MCU_WAKE_DELAY	    			s_tx_delay.wake_delay	
#define MCU_SLEEP_DELAY	    			s_tx_delay.sleep_delay

#define MCU_WAKE_TYPE					s_wake_act.type
#define MCU_WAKE_LEVEL					s_wake_act.level
#define MCU_WAKE_KEEP_TIME				s_wake_act.keep_time

#define MCU_SLEEP_STATE_SET(_e)	        s_wakeup_mcu_state = (_e)
#define MCU_SLEEP_STATE_GET()			s_wakeup_mcu_state

enum {
	MCU_WAKE_TYPE_PULSE = 0,
	MCU_WAKE_TYPE_LEVEL = 1,
};
	
enum {
 	MCU_WAKE_LEVEL_LOW = 0,
 	MCU_WAKE_LEVEL_HIGH,
};

enum {
    MCU_WAKEUP_STATE_START,
    MCU_WAKEUP_STATE_TIMEOUT,
    MCU_WAKEUP_STATE_END,
};

/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */

static cmd_tx_delay_t		s_tx_delay;				 /*CFG_TYPE_TX_DELAY*/
static cmd_wake_act_t		s_wake_act;				 /*CFG_TYPE_WAKEACT*/
#ifdef CFG_WKUP_MCU
static uint8_t s_wakeup_mcu_state = MCU_WAKEUP_STATE_END;
#endif
static tid_t sleep_tid = NULL;
static power_evt_cb_t * s_power_cb;

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */

/**
 * @brief 调用注册的回调函数
 * 
 * @param evt 事件类型，参考power_evt_t
 * @param arg 参数指针
 */
static void power_event_cb(app_power_evt_t evt, void* arg)
{
    if(s_power_cb)
        s_power_cb(evt, arg);
}

/**
 * @brief 上报mcu使能ble睡眠使能事件
 * 
 */
#ifdef CFG_SLEEP_IO
static void power_ble_sleep_callback(void)
{
    if(hal_gpio_read_bit(BLE_SLEEP_PIN) == BLE_SLEEP_ENABLE_LEVEL)
        power_event_cb(POWER_BLE_SLEEP_IND, NULL); 
    else
        power_event_cb(POWER_BLE_WAKE_IND, NULL);
}
#endif

/**
 * @brief mcu进入睡眠
 * 
 */
static void power_mcu_sleep_start()
{
    /// 上报mcu进入睡眠事件
    power_event_cb(POWER_MCU_SLEEP_IND, NULL);
}

/**
 * @brief io唤醒mcu结束
 * 
 */
#ifdef CFG_WKUP_MCU
static void power_mcu_wakeup_timeout()
{
    /// 设置唤醒引脚
    if(MCU_WAKE_TYPE == MCU_WAKE_TYPE_PULSE)
        HAL_WAKEUP_MCU(!MCU_WAKE_LEVEL);   
}
#endif

static void power_mcu_wakeup_end_delay(void)
{
#ifdef CFG_WKUP_MCU
    /// 设置唤醒引脚
    if(MCU_WAKE_TYPE == MCU_WAKE_TYPE_LEVEL)
        HAL_WAKEUP_MCU(!MCU_WAKE_LEVEL);
    MCU_SLEEP_STATE_SET(MCU_WAKEUP_STATE_END);
#endif
    power_event_cb(POWER_MCU_WAKE_END_IND, NULL);
}

/**
 * @brief 通知允许发送数据
 * 
 */
static void power_mcu_wake_delay_ind()
{
    power_event_cb(POWER_MCU_WAKE_DELAY_IND, NULL);   
}

/**
 * @brief 初始化唤醒io、睡眠使能io
 * 
 */
void app_power_pin_init(void)
{
#ifdef CFG_WKUP_MCU
	/// 初始化唤醒mcu引脚
    hal_gpio_init_t wkup_struct;
	wkup_struct.pin = MCU_WAKEUP_PIN;
	wkup_struct.dir = MCU_WAKEUP_DIR;
    wkup_struct.interrupt_mode = HAL_GPIO_INT_DISABLE;
	wkup_struct.arg = NULL;
	wkup_struct.interrupt_callback = (hal_gpio_callback_t *)NULL;
	hal_gpio_init(&wkup_struct);
	HAL_WAKEUP_MCU(!MCU_WAKE_LEVEL);
#endif
#ifdef CFG_SLEEP_IO
    /// 初始化使能ble睡眠引脚
    hal_gpio_init_t slp_struct;
	slp_struct.pin = BLE_SLEEP_PIN;
	slp_struct.dir = BLE_SLEEP_DIR;
	slp_struct.interrupt_mode = BLE_SLEEP_INT;
	slp_struct.arg = NULL;
	slp_struct.interrupt_callback = (hal_gpio_callback_t *)power_ble_sleep_callback;
	hal_gpio_init(&slp_struct);
    power_ble_sleep_callback(); //note: 更新睡眠使能状态
#else
    power_event_cb(POWER_BLE_WAKE_IND, NULL);
#endif
}

void app_power_mcu_wakeup(void)
{ 
#ifdef CFG_WKUP_MCU
     if(MCU_SLEEP_STATE_GET() == MCU_WAKEUP_STATE_END) { //需要唤醒动作
        /// Set the status to Wake up
        MCU_SLEEP_STATE_SET(MCU_WAKEUP_STATE_START);
        /// Wake mcu
        HAL_WAKEUP_MCU(MCU_WAKE_LEVEL);
        /// Notify the user layer that spin-down is not allowed
        power_event_cb(POWER_MCU_WAKE_IND, NULL); 
        /// Create a wake up pin delay timer
        if((MCU_WAKE_TYPE == MCU_WAKE_TYPE_PULSE) &&(MCU_WAKE_KEEP_TIME != 0)){
            TIMER_CREATE(MCU_WAKE_KEEP_TIME, NULL, (task_func *)power_mcu_wakeup_timeout);
        }
        else{
            power_mcu_wakeup_timeout();
        }
		/// Create mcu sleep timer
		app_power_mcu_sleep_timer_update();
        /// Create a delay send data timer   
        if(MCU_WAKE_DELAY != 0)
            TIMER_CREATE(MCU_WAKE_DELAY, NULL, (task_func *)power_mcu_wake_delay_ind);
        else
            power_mcu_wake_delay_ind();
    }
    else{
        /// Reset the mcu sleep timer
        if(sleep_tid){
            timer_restart(&sleep_tid);
        }
    }
#else 
    power_mcu_wake_delay_ind();
#endif
}


void app_power_init(power_evt_cb_t *p)
{
    s_power_cb = p;
    /// read sleep param
    app_power_param_update();
    app_power_pin_init();
}

void app_power_param_update(void)
{
    cfg_get(CFG_TYPE_TX_DELAY, &s_tx_delay, sizeof(cmd_tx_delay_t));
	cfg_get(CFG_TYPE_WAKEACT, &s_wake_act, sizeof(cmd_wake_act_t));
}

void app_power_mcu_sleep_timer_update(void)
{
    if(MCU_SLEEP_DELAY != 0){
        if(sleep_tid)
            timer_restart(&sleep_tid);
        else  
            TIMER_CREATE_ONESHOT(&sleep_tid, MCU_SLEEP_DELAY, NULL, (task_func *)power_mcu_sleep_start);
    }    
	else
        power_mcu_sleep_start();
}

void app_power_mcu_wakeup_end_exec(void)
{
#ifdef CFG_WKUP_MCU
    TIMER_CREATE(MCU_WAKE_END_DELAY, NULL, (task_func *)power_mcu_wakeup_end_delay);
#else 
    power_mcu_wakeup_end_delay();
#endif
}

void app_power_sleep_pin_init(hal_gpio_direction_t dir, hal_gpio_interrupt_t interrupt_mode)
{   
#ifdef CFG_SLEEP_IO
    hal_gpio_init_t gpio_struct;

    gpio_struct.pin = BLE_SLEEP_PIN;
	gpio_struct.dir = dir;
	gpio_struct.interrupt_mode = interrupt_mode;
	gpio_struct.arg = NULL;
	gpio_struct.interrupt_callback = (hal_gpio_callback_t *)power_ble_sleep_callback;
	hal_gpio_init(&gpio_struct);
#endif
}
