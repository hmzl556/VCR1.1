/**
 * @file    combo_key.h
 * @brief   组合键管理器接口
 * @note    统一管理多组组合键（配对、解配），与工厂复位独立但共享硬件快照。
 *          仅支持"双键同时长按→触发动作"模式的简单组合键。
 *          工厂复位（含确认/取消交互）不在此管理器中，仍在 task_keyscan.c。
 */

#ifndef _COMBO_KEY_H_
#define _COMBO_KEY_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 一、时间常量（ms）
 *===========================================================================*/

/** @brief 组合键长按触发时间 */
#define COMBO_KEY_HOLD_MS          3000U

/** @brief 解配 LED 最短显示时间（避免触发瞬间灭灯用户无感知） */
#define COMBO_UNPAIR_LED_MIN_MS    2000U

/** @brief 解配最长等待时间（断连或超时） */
#define COMBO_UNPAIR_TIMEOUT_MS    30000U

/** @brief LED 闪烁周期 */
#define COMBO_LED_FLASH_MS         200U

/*===========================================================================
 * 二、公共 API
 *===========================================================================*/

/** @brief 组合键管理器初始化（在主循环启动前调用一次） */
void combo_key_init(void);

/**
 * @brief  每主循环周期调用，处理所有简单组合键
 * @param  now_ms       当前时间戳（app_now_ms()）
 * @param  hw_snapshot  由调用方一次性读取的硬件按键快照（2 个 uint32_t）
 * @note   调用方负责互斥：仅在 factory_rst_state == IDLE 时才调用此函数
 */
void combo_key_process(uint32_t now_ms, const uint32_t hw_snapshot[5]);

/**
 * @brief  查询所有简单组合键是否都处于 IDLE 状态
 * @return true = 全部空闲，false = 至少有一个活跃
 * @note   供 task_keyscan_process() 互斥判断：简单组合键活跃时工厂复位不响应
 */
bool combo_key_is_idle(void);

/**
 * @brief  查询解配流程是否正在进行
 * @return true = 解配中（含等断连），false = 其他
 * @note   供 ble_gap_process_disconnect_pending() 判断是否跳过广播重启
 */
bool combo_key_unpair_in_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* _COMBO_KEY_H_ */
