/**
 * @file app_task.h
 * @author Barrot Firmware Team
 * @brief This file contains the interface definition of the main business logic
 * @version 0.1
 * @date 2023-09-19
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __APP__TASK__H__
#define __APP__TASK__H__

#include "app_ble.h"
#include "atprocess.h"
#include "rbuf_data.h"
#include "app_power.h"

/**
 * EXTERNS
 ****************************************************************************************
 */ 
extern uint32_t ble_sleep_lock_mask;

/**
 * DEFINES
 ****************************************************************************************
 */
#define BLE_SLEEP_LOCK_MASK_SET(b)      do{_ENTER_CRITICAL();(ble_sleep_lock_mask |= 1<<(b));_LEAVE_CRITICAL();}while(0) 
#define BLE_SLEEP_LOCK_MASK_CLEAR(b)    do{_ENTER_CRITICAL();(ble_sleep_lock_mask &= ~(1<<(b)));_LEAVE_CRITICAL();}while(0)  
#define BLE_SLEEP_LOCK_MASK_CHECK(b)    (ble_sleep_lock_mask & (1<<b))
#define BLE_SLEEP_LOCK_MASK_GET()       (ble_sleep_lock_mask)

/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct{
    uint8_t addr_type;
    uint8_t addr[BD_ADDR_LEN];
    uint16_t conhdl;
    uint16_t ccc_enable;    /// master: send enable flag
    uint16_t mtu;
    uint8_t role;       
    uint8_t fc_cmp;         /// fc uuid Send complete mark
    uint8_t credit_max;     /// max credit,const
    uint8_t credit;         /// Current credit
    uint8_t rcv_credit;     /// receive credit
    uint8_t smp_state; 
    uint8_t *bond_info;     /// Used in autounlock mode
    bool svc_disc_cmp;   /// Service discovery complete
    ble_addr_t public_bd;
}conn_record_t;

extern trans_uart_init_t trans_init;

/**
 * @brief ble事件回调函数
 * 
 * @param evt 事件类型，参考app_ble_evt_t
 * @param param 回调参数指针，参考app_ble_msg_t
 */
void user_ble_event_cb(app_ble_evt_t evt, app_ble_msg_t *param);

/**
 * @brief at指令模块回调
 * 
 * @param evt 事件类型，参考cmd_change_evt_t
 * @param arg 回调参数
 * @return int 事件处理结果
 */
int user_atcmd_evt_ck(cmd_change_evt_t evt, void* arg);


/**
 * @brief ble低功耗控制回调
 * 
 * @param evt 事件类型
 * @param arg 回调参数指针
 */
void user_power_event_cb(app_power_evt_t evt, void* arg);

/**
 * @brief 串口事件回调
 * 
 * @param evt 回调参数
 */
void user_rbuf_event_cb(const trans_uart_ev_t* evt);

/**
 * @brief ble唤醒任务
 * 
 * @param
 */
void user_ble_wakeup_task(void);

/**
 * @brief ble睡眠任务
 * 
 */
void user_ble_sleep_task(void);

/**
* @brief 创建透传服务流控任务
 * 
 */
void user_create_credit_task(void);

#endif




