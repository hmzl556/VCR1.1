/**
 * @file    pm_config.h
 * @brief   电源管理（PMU）可配置参数
 * @note    所有超时、连接参数均通过此文件的宏定义管理，
 *           调试阶段可快速调整而不影响逻辑代码。
 *           最终定版后可将这些值固化。
 */

#ifndef _PM_CONFIG_H_
#define _PM_CONFIG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 一、超时参数（单位：毫秒）
 *===========================================================================*/

/**
 * @brief  工作态 → 待机态：无业务空闲超时（测试阶段暂时 5 分钟）
 * @note   业务包括：按键操作、语音开始/停止、对端数据下发、CAT1 数据
 *         当前取值偏保守（常规遥控器方案），可后续根据功耗测试结果调整
 *         范围建议：5000 ~ 15000 ms
 *
 *         临时调整 2026-10-23：原值30000U；恢复见暂时改动文档.md。
 */
//#define PM_WORK_TO_STANDBY_MS         4500000U
#define PM_WORK_TO_STANDBY_MS         300000U  /* 暂时5分钟；原值30000U */

/**
 * @brief  最后一次活动 → 休眠态：总空闲超时（测试阶段暂时 30 分钟）
 * @note   从最后一次业务活动开始计时，跨工作态和待机态连续计算
 *         实际待在待机态的时长 = PM_STANDBY_TO_SLEEP_MS - PM_WORK_TO_STANDBY_MS
 */
//#define PM_STANDBY_TO_SLEEP_MS       9000000U
#define PM_STANDBY_TO_SLEEP_MS       1800000U /* 暂时30分钟；原值90000U */

/**
 * @brief  回连态 → 休眠态：持续广播无连接超时（45 秒）
 * @note   回连广播期间若无 STB 连接，超时后进入休眠节省功耗
 *         范围建议：30000 ~ 60000 ms
 *
 *         【测试模式】改为 45000ms，方便验证状态切换
 */
//#define PM_RECONNECT_TIMEOUT_MS      9000000U
#define PM_RECONNECT_TIMEOUT_MS      60000U   /* 60秒 */

/**
 * @brief  静默态 → 休眠态：上电/唤醒后无按键超时（暂时 5 分钟）
 * @note   上电或休眠唤醒后若无绑定，进入静默态等待用户按组合键。
 *         超时无操作则进入休眠节省功耗。
 *         范围建议：30000 ~ 120000 ms
 */
/* 临时调整 2026-10-22：原值 60000U；恢复说明见 暂时改动文档.md。 */
#define PM_SILENT_TO_SLEEP_MS          300000U

/**
 * @brief  配对态 → 休眠态：广播无连接超时（120 秒）
 * @note   从 KEY14+KEY3 组合键触发配对广播时开始计时，
 *         与静默态超时独立，确保配对窗口完整。
 */
#define PM_PAIRING_TIMEOUT_MS         30000U

/*===========================================================================
 * 二、待机态 BLE 连接参数
 *===========================================================================*/

/**
 * @brief  待机态连接间隔（单位：1.25ms）
 * @note   50ms = 0x0028 * 1.25ms
 *         取 GY/T 407-2024 §5.1.2.3 规定的上限值（5ms~50ms），
 *         与工作态 10ms 形成 5:1 的功耗梯度
 */
#define PM_STANDBY_CONN_INTV_MIN     0x0028U  /* 50ms */
#define PM_STANDBY_CONN_INTV_MAX     0x0028U  /* 50ms */

/**
 * @brief  待机态从机延迟（单位：连接事件个数）
 * @note   100 个事件 × 50ms = 最大 5 秒不用监听
 *         监管超时: (1+100)×50ms×2 = 10.1s → timeout 需 >1010
 */
#define PM_STANDBY_CONN_LATENCY         100U

/**
 * @brief  待机态监管超时（单位：10ms）
 * @note   1200 × 10ms = 12s > (1+100)×50ms×2 = 10.1s
 */
#define PM_STANDBY_CONN_TIMEOUT        1200U

/*===========================================================================
 * 三、工作态 BLE 连接参数（从待机态切回时使用）
 *===========================================================================*/

/**
 * @brief  工作态连接间隔（单位：1.25ms）
 * @note   10ms，符合 GY/T 407-2024 §5.1.2.3 要求
 */
#define PM_WORK_CONN_INTV_MIN        0x0008U  /* 10ms */
#define PM_WORK_CONN_INTV_MAX        0x0008U  /* 10ms */

/**
 * @brief  工作态从机延迟
 * @note   40 个连接事件，符合 GY/T 407-2024 附录 C.2
 */
#define PM_WORK_CONN_LATENCY            40U

/**
 * @brief  工作态连接超时（单位：10ms）
 */
#define PM_WORK_CONN_TIMEOUT           800U

/*===========================================================================
 * 四、调试开关
 *===========================================================================*/

/**
 * @brief  PMU 调试日志开关
 * @note   1 = 使能状态转换日志，便于开发和调试
 *         0 = 关闭日志，量产时使用
 */
#define PM_DEBUG_LOG_ENABLE             1U   /* 默认关闭，调试时改为 1U */

#if PM_DEBUG_LOG_ENABLE
    /* 注意：开启后会 #include "co_printf.h"，可能与部分文件的编译环境冲突。
       调试完后请恢复为 0U，避免量产固件中引入不必要的依赖。 */
    #include "co_printf.h"
    #define PM_LOG(fmt, ...)   co_printf("[PM] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define PM_LOG(fmt, ...)   ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PM_CONFIG_H_ */
