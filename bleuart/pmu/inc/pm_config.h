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
 * @brief  工作态 → 待机态：无业务空闲超时（8 秒）
 * @note   业务包括：按键操作、语音开始/停止、对端数据下发、CAT1 数据
 *         当前取值偏保守（常规遥控器方案），可后续根据功耗测试结果调整
 *         范围建议：5000 ~ 15000 ms
 *
 *         【测试模式】改为 10000ms，方便验证状态切换
 */
//#define PM_WORK_TO_STANDBY_MS         8000U
#define PM_WORK_TO_STANDBY_MS         4500000U   /* 测试用：10秒 */

/**
 * @brief  最后一次活动 → 休眠态：总空闲超时（120 秒）
 * @note   从最后一次业务活动开始计时，跨工作态和待机态连续计算
 *         实际待在待机态的时长 = PM_STANDBY_TO_SLEEP_MS - PM_WORK_TO_STANDBY_MS
 */
//#define PM_STANDBY_TO_SLEEP_MS       45000U
#define PM_STANDBY_TO_SLEEP_MS       9000000U   /* 测试用：120秒 */

/**
 * @brief  回连态 → 休眠态：持续广播无连接超时（45 秒）
 * @note   回连广播期间若无 STB 连接，超时后进入休眠节省功耗
 *         范围建议：30000 ~ 60000 ms
 *
 *         【测试模式】改为 45000ms，方便验证状态切换
 */
//#define PM_RECONNECT_TIMEOUT_MS      45000U
#define PM_RECONNECT_TIMEOUT_MS      9000000U   /* 测试用：45秒 */

/**
 * @brief  配对态 → 休眠态：广播无连接超时（30 秒）
 */
#define PM_PAIRING_TIMEOUT_MS         9000000U

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
 * @note   100 个连接事件 × 50ms = 最大 5 秒才响应一次
 *         取最大值以最大程度降低功耗
 *         范围：40 ~ 100（GY/T 407-2024 附录 C.2）
 */
#define PM_STANDBY_CONN_LATENCY         100U

/**
 * @brief  待机态连接超时（单位：10ms）
 * @note   800 × 10ms = 8s，确保待机态下链路不会轻易断开
 */
#define PM_STANDBY_CONN_TIMEOUT         800U

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
 * 四、休眠态 RTC 参数
 *===========================================================================*/

/**
 * @brief  RTC 定时唤醒间隔（秒）
 * @note   休眠态下 RTC 每隔此秒数唤醒一次，检查是否需要恢复广播
 *         当前设为 5 秒（使用 LSI 32KHz，精度 ±10% 可接受）
 */
#define PM_SLEEP_RTC_WAKEUP_SEC          5U

/**
 * @brief  RTC 唤醒时钟分频选择
 * @note   使用 RTC_WKUPCLK_RTCCLK_DIV16（LSI 32KHz / 16 = 2KHz）
 *         计数器 = 2KHz × 5s = 10000
 */
#define PM_SLEEP_RTC_WKUP_CLOCK    RTC_WKUPCLK_RTCCLK_DIV16
#define PM_SLEEP_RTC_WKUP_COUNT    ((PM_SLEEP_RTC_WAKEUP_SEC) * (32000U / 16U))

/*===========================================================================
 * 五、调试开关
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
