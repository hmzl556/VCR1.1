/**
 * @file    pm_state.h
 * @brief   电源管理状态机 — 类型定义与公共 API
 * @note    此模块独立于现有业务逻辑，通过接口注入实现松耦合。
 *          状态转换不直接修改现有全局变量，而是通过回调/通知机制。
 *
 * 状态机概览：
 *
 *   上电(无绑定)                    上电(有绑定)
 *        ↓                               ↓
 *     SILENT ──超时无按键──→ SLEEP    RECONNECTING
 *        │                      │         │
 *   KEY14+KEY3 组合键      按键唤醒   连接成功/超时→SLEEP
 *        │                 (无绑定)        ↓
 *        ↓                      │      WORKING ←──按键/数据── STANDBY
 *    PAIRING ←─────────────────┘        │    \────无业务──→/    │
 *        │                         断连(有绑定)           无业务超时
 *   配对成功/超时                        ↓                       ↓
 *        ↓                         RECONNECTING              SLEEP
 *     WORKING                            │                       │
 *                                   超时无连接              按键/RTC唤醒
 *                                        ↓                  /          \
 *                                     SLEEP          有绑定→RECONNECTING
 *                                                    无绑定→SILENT
 */

#ifndef _PM_STATE_H_
#define _PM_STATE_H_

#include <stdint.h>
#include <stdbool.h>
#include "pm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 一、状态枚举
 *===========================================================================*/

/**
 * @brief  电源管理状态枚举
 * @note   六个显式状态，覆盖遥控器全生命周期
 */
typedef enum {
    PM_STATE_PAIRING      = 0,  /* 配对态：可发现可连接广播，等待新设备配对     */
    PM_STATE_RECONNECTING = 1,  /* 回连态：不可发现可连接广播，等待已知设备回连 */
    PM_STATE_WORKING      = 2,  /* 工作态：已连接，全功能运行，小连接间隔        */
    PM_STATE_STANDBY      = 3,  /* 待机态：已连接但无业务，大连接间隔省电        */
    PM_STATE_SLEEP        = 4,  /* 休眠态：无连接无广播，Sleep 模式极低功耗      */
    PM_STATE_SILENT       = 5,  /* 静默态：CPU运行无广播，等待组合键触发配对/回连 */
} pm_state_t;

/*===========================================================================
 * 二、活动类型枚举
 *===========================================================================*/

/**
 * @brief  业务活动类型
 * @note   各业务模块在产生用户交互事件时，调用 pm_activity_notify()
 *         通知 PM 状态机"有活动"，用于重置空闲计时器。
 *         待机态下收到任何活动通知将立即切回工作态。
 */
typedef enum {
    PM_ACTIVITY_KEY        = 0,  /* 按键操作（按下/释放，含语音键 KEY15） */
    PM_ACTIVITY_VOICE      = 1,  /* 语音传输开始（保留，当前由按键覆盖）  */
    PM_ACTIVITY_PEER_DATA  = 2,  /* 对端（STB）数据下发，如 DTSS 写入    */
    PM_ACTIVITY_CAT1       = 3,  /* CAT1 模块收到网络数据                */
} pm_activity_t;

/*===========================================================================
 * 三、公共 API
 *===========================================================================*/

/**
 * @brief  电源状态机初始化
 * @note   在系统启动时调用一次（建议在 app_rtos_init() 之后）。
 *         初始化状态：有绑定→RECONNECTING，无绑定→SILENT。。
 *         同时配置 RTC 唤醒定时器以备休眠态使用。
 */
void pm_state_init(void);

/**
 * @brief  获取当前电源状态
 * @return 当前 pm_state_t 枚举值
 */
pm_state_t pm_state_get(void);

/**
 * @brief  状态转换（内部函数，也可由外部强制触发）
 * @param  new_state  目标状态
 * @note   此函数会执行进入/退出状态的硬件操作
 *         （如待机态切换连接参数、休眠态进入 Sleep 模式等）
 */
void pm_state_transition(pm_state_t new_state);

/**
 * @brief  通知 PM 状态机有业务活动发生
 * @param  activity  活动类型
 * @note   各业务模块（按键/语音/CAT1/对端数据）在产生事件时调用。
 *         功能：
 *         1. 重置空闲计时器（推迟进入待机态/休眠态的时间）
 *         2. 若当前处于待机态，立即切回工作态
 *         3. 若当前处于休眠态，此调用无效（休眠态下 CPU 停止）
 */
void pm_activity_notify(pm_activity_t activity);

/**
 * @brief  通知 PM 状态机：BLE 连接已建立
 * @note   在 GAP 连接完成回调（ble_gap_connected）中调用。
 *         根据当前状态执行对应动作：
 *         - PAIRING/RECONNECTING → WORKING
 *         - 其他状态 → 保持（异常保护）
 */
void pm_notify_connected(void);

/**
 * @brief  通知 PM 状态机：BLE 连接已断开
 * @note   在 GAP 断连回调（ble_gap_disconnected）中调用。
 *         自动根据绑定状态决定进入 PAIRING 或 RECONNECTING。
 */
void pm_notify_disconnected(void);

/**
 * @brief  BLE 栈初始化完成后调用，修正 PM 初始状态
 * @note   必须在 myBleStartAdv() 成功后调用一次。
 *         因为 pm_state_init() 在 BLE 栈启动前调用，此时 peer_bonded_count()
 *         返回 0（绑定数据尚未从 Flash 恢复），导致初始状态固定为 SILENT。
 *         本函数在 BLE 栈就绪后重新检查绑定，若实际有绑定则修正为 RECONNECTING。
 */
void pm_state_sync_after_ble_init(void);

/**
 * @brief  每主循环周期调用一次
 * @note   在 BLE 任务主循环（task_ble_entry）开头调用。
 *         检查以下超时条件并执行状态转换：
 *         1. SILENT 超时 → SLEEP
 *         2. PAIRING 超时 → SLEEP
 *         3. WORKING 空闲超时 → STANDBY
 *         4. STANDBY 空闲超时 → SLEEP
 *         5. RECONNECTING 超时 → SLEEP
 *         调用频率：约 10ms 一次（与主循环周期一致）
 */
void pm_state_process(void);

/**
 * @brief  休眠唤醒入口
 * @note   CPU 从 Sleep 模式唤醒后调用（在 main.c 或启动代码中）。
 *         判断唤醒原因（KEYSCAN vs RTC），恢复外设，
 *         并根据绑定状态进入静默态或回连态。
 */
void pm_wakeup_entry(void);

/**
 * @brief  检查当前是否处于"可发送 HID/语音数据"的状态
 * @return true = 已连接且可发送， false = 未连接或休眠
 * @note   供各业务模块在发送前快速判断，避免无效操作。
 *         工作态 + 待机态返回 true（两者都有 BLE 连接）。
 */
bool pm_is_connected(void);

/**
 * @brief  获取状态名称字符串（调试用）
 * @param  state  状态枚举值
 * @return 对应的中文名称字符串
 */
const char* pm_state_name(pm_state_t state);

/**
 * @brief  获取当前空闲时长（调试用）
 * @return 距离上次活动的毫秒数
 */
uint32_t pm_get_idle_ms(void);

/**
 * @brief  补偿低功耗期间的时间流逝
 * @param  elapsed_ms  流逝的毫秒数（从空闲计时器中减去）
 * @note   供 Standby 模式唤醒后调用，使超时检测能正确工作
 */
void pm_activity_compensate_time(uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* _PM_STATE_H_ */
