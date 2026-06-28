/**
 * @file    pm_standby.c
 * @brief   待机态管理 — 进入/退出待机态时的外设控制
 * @note    待机态是"已连接但无业务"的低功耗状态：
 *          1. 切换 BLE 连接参数为省电配置（50ms 间隔 / latency 100）
 *          2. 关闭非必要外设（LED、CAT1 TX 等）
 *          3. 保持按键检测（KEYSCAN）和射频链路（BLE 连接保活）
 *
 * 平均电流目标：≤ 65μA（GY/T 407-2024 §5.1.2.3）
 *
 * 依赖关系：
 *   - gap.h          → le_conn_update()  连接参数更新
 *   - ble_gap.h      → ble_gap_get_peer() 获取当前连接对端
 *   - userhard.h     → LED 控制宏
 *   - br1601x.h      → USART2 寄存器访问（CAT1 TX 控制）
 */

#include "pm_config.h"
#include "gap.h"            /* le_conn_update_t, le_conn_update()    */
#include "ble_gap.h"        /* ble_gap_get_peer()                    */
#include "userhard.h"       /* SETLED1/CLRLED1/SETLED2/CLRLED2      */
#include "br1601x.h"        /* USART2, USART_CTRL1_TXEN              */

/*===========================================================================
 * 一、CAT1 UART TX 控制（辅助函数）
 *
 * 说明：待机态下关闭 CAT1 USART2 的 TX 发送器（仅保留 RX 接收能力），
 *       以节省 UART 驱动功耗。通过操作 USART_CTRL1 寄存器的 TXEN 位实现。
 *===========================================================================*/

/**
 * @brief  关闭 CAT1 USART2 的 TX 发送器（保留 RX 接收）
 * @note   调用后 CAT1 只能收不能发，收到数据通过 RX 中断通知上层，
 *         此时应触发 pm_activity_notify(PM_ACTIVITY_CAT1) 切回工作态。
 */
static void pm_cat1_tx_disable(void)
{
    /* 清除 USART2_CTRL1 的 TXEN 位（0x0008），保留 RXEN 等其他位 */
    USART2->CTRL1 &= (uint16_t)(~(USART_CTRL1_TXEN));
    PM_LOG("CAT1 UART TX disabled (RX kept)");
}

/**
 * @brief  恢复 CAT1 USART2 的 TX 发送器（恢复正常收发）
 * @note   退出待机态时调用，恢复 CAT1 全双工通信能力。
 */
static void pm_cat1_tx_enable(void)
{
    /* 置位 USART2_CTRL1 的 TXEN 位（0x0008） */
    USART2->CTRL1 |= USART_CTRL1_TXEN;
    PM_LOG("CAT1 UART TX enabled");
}

/*===========================================================================
 * 二、LED 控制（辅助函数）
 *===========================================================================*/

/**
 * @brief  关闭所有 LED 指示灯
 * @note   待机态下 LEDs 无意义，关断节省约 2~4mA（取决于限流电阻）。
 */
static void pm_led_all_off(void)
{
    CLRLED1;
    CLRLED2;
}

/*===========================================================================
 * 三、BLE 连接参数切换
 *===========================================================================*/

/**
 * @brief  将 BLE 连接参数切换为待机态省电配置
 * @note   调用 le_conn_update() 向 STB（机顶盒主机）发起参数更新请求。
 *         参数：50ms 连接间隔 / latency 100（GY/T 407-2024 合法上限）。
 *         此请求由 STB 最终裁决，若 STB 拒绝，参数保持不变（不影响连接）。
 *
 * @warning 调用前必须确保 g_peer != NULL（已连接），否则 le_conn_update 无效。
 */
static void pm_conn_params_set_standby(void)
{
    peer_t peer = ble_gap_get_peer();

    if (peer == NULL) {
        /* 未连接时无法更新参数，静默返回 */
        return;
    }

    le_conn_update_t par;
    par.peer        = peer;
    par.intv_min    = PM_STANDBY_CONN_INTV_MIN;   /* 50ms (0x0028 * 1.25ms) */
    par.intv_max    = PM_STANDBY_CONN_INTV_MAX;   /* 50ms                    */
    par.latency_max = PM_STANDBY_CONN_LATENCY;    /* 100 个连接事件          */
    par.timeout     = PM_STANDBY_CONN_TIMEOUT;    /* 800 × 10ms = 8s         */
    par.ce_len_min  = 0;  /* 不指定 CE 长度，由控制器选择最优值 */
    par.ce_len_max  = 0;

    le_conn_update(&par);
    PM_LOG("Req standby conn params (50ms / lat 100)");
}

/**
 * @brief  将 BLE 连接参数恢复为工作态标准配置
 * @note   退出待机态时调用，恢复低延迟参数以保证按键/HID 响应速度。
 *         参数：10ms 连接间隔 / latency 40（当前工作态默认值）。
 */
static void pm_conn_params_set_working(void)
{
    peer_t peer = ble_gap_get_peer();

    if (peer == NULL) {
        return;
    }

    le_conn_update_t par;
    par.peer        = peer;
    par.intv_min    = PM_WORK_CONN_INTV_MIN;      /* 10ms (0x0008 * 1.25ms) */
    par.intv_max    = PM_WORK_CONN_INTV_MAX;      /* 10ms                    */
    par.latency_max = PM_WORK_CONN_LATENCY;       /* 40 个连接事件           */
    par.timeout     = PM_WORK_CONN_TIMEOUT;       /* 800 × 10ms = 8s         */
    par.ce_len_min  = 0;
    par.ce_len_max  = 0;

    le_conn_update(&par);
    //PM_LOG("已请求切换工作态连接参数 (10ms / latency 40)");
    PM_LOG("Req working conn params (10ms / lat 40)");
}

/*===========================================================================
 * 四、公共入口
 *===========================================================================*/

/**
 * @brief  进入待机态
 * @note   由 pm_state.c 的 pm_enter_standby() 调用。
 *         按顺序执行：连接参数切换 → 外设关闭 → LED 关闭。
 *         连接参数更新是异步的（STB 可能延迟响应或拒绝），
 *         但外设关闭是即时的，确保尽快进入省电状态。
 */
void pm_standby_enter(void)
{
    //PM_LOG("待机态：开始切换省电配置...");
    PM_LOG("Standby enter: switching to power-saving...");
    /* 1. 切换 BLE 连接参数为省电配置 */
    pm_conn_params_set_standby();

    /* 2. 关闭 CAT1 UART TX（RX 保持，可唤醒） */
    pm_cat1_tx_disable();

    /* 3. 关闭所有 LED 指示灯 */
    pm_led_all_off();

    //PM_LOG("待机态：省电配置已完成");
    PM_LOG("Standby enter: power-saving config done");
}

/**
 * @brief  退出待机态
 * @note   由 pm_state.c 的 pm_exit_standby() 调用。
 *         按顺序执行：恢复连接参数 → 恢复外设。
 *         连接参数恢复是异步的，但 CAT1 TX 立即恢复，
 *         确保待机态→工作态切换后按键/HID 立即可用。
 */
void pm_standby_exit(void)
{
    //PM_LOG("待机态：恢复工作配置...");
    PM_LOG("Standby exit: restoring working config...");

    /* 1. 恢复 BLE 连接参数为工作态配置 */
    pm_conn_params_set_working();

    /* 2. 恢复 CAT1 UART TX */
    pm_cat1_tx_enable();

    /* LED 恢复由各业务模块自行控制（如按键 LED 指示），此处不强行点亮 */

    //PM_LOG("待机态：工作配置已恢复");
    PM_LOG("Standby exit: working config restored");
}
