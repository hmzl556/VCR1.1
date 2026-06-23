/**
 * @file    pm_state.c
 * @brief   电源管理状态机核心实现
 * @note    此文件是 PMU 模块的核心，实现五状态机（配对/回连/工作/待机/休眠）
 *          的全部状态转换逻辑。
 *
 * 时间基准：使用全局变量 voiceUptimeMs（gmem.h），每 8ms 递增一次。
 * 调用周期：pm_state_process() 在 BLE 主循环中每 ~10ms 调用一次。
 *
 * 内部设计原则：
 *   1. 所有状态变量 static，外部不可直接访问
 *   2. 每个状态的 enter/exit 操作集中处理
 *   3. 空闲计时器基于"最后活动时间戳"而非递减计数
 *   4. 所有对外通知通过回调机制，不直接操控外设
 */

#include "pm_state.h"
#include "pm_config.h"
#include "gmem.h"       /* voiceUptimeMs 全局毫秒计数器 */
#include "gap.h"        /* peer_bonded_count() 绑定数量查询 */

/*===========================================================================
 * 一、内部静态变量
 *===========================================================================*/

/** @brief 当前电源状态，初始为配对态（后续由绑定检查修正） */
static pm_state_t   g_pm_state              = PM_STATE_PAIRING;

/** @brief 最后一次业务活动发生的时间戳（voiceUptimeMs 值） */
static uint32_t     g_pm_last_activity_ms   = 0U;

/** @brief 回连广播开始时间戳，用于超时判断 */
static uint32_t     g_pm_reconnect_start_ms = 0U;

/** @brief 进入待机态的时间戳，用于待机→休眠超时判断 */
static uint32_t     g_pm_standby_start_ms   = 0U;

/** @brief 进入休眠态前记录唤醒原因：0=未定义, 1=按键, 2=RTC */
static uint8_t      g_pm_wake_reason        = 0U;

/*===========================================================================
 * 二、内部辅助函数声明
 *===========================================================================*/

/**
 * @brief  检查当前是否存在绑定记录
 * @return true = 有绑定记录（应回连），false = 无绑定（应配对）
 * @note   封装 peer_bonded_count()，便于后续替换或 mock 测试
 */
static bool pm_has_bonding(void)
{
    return (peer_bonded_count() > 0);
}

/**
 * @brief  刷新活动时间戳为当前时刻
 * @note   每次有用户交互（按键/语音/CAT1/对端数据）时调用，
 *         推迟进入待机态/休眠态的时间
 */
static void pm_refresh_activity(void)
{
    g_pm_last_activity_ms = voiceUptimeMs;
}

/**
 * @brief  计算自上次活动以来的空闲时长（毫秒）
 * @return 空闲毫秒数
 */
static uint32_t pm_idle_duration_ms(void)
{
    return (voiceUptimeMs - g_pm_last_activity_ms);
}

/*===========================================================================
 * 三、各状态进入/退出操作（内部函数）
 *
 *  备注：以下函数当前为框架实现，具体外设控制逻辑
 *        将在第4步（pm_standby.c）和第6步（pm_sleep.c）中完善。
 *===========================================================================*/

/**
 * @brief  进入工作态的操作
 * @note   从待机态切回时：恢复工作态连接参数、恢复外设
 *         从配对/回连首次进入时：由 GAP 层处理，此处仅记录日志
 */
static void pm_enter_working(void)
{
    PM_LOG("进入工作态 WORKING");
    /* 工作态连接参数将在第4步 pm_standby_exit() 中自动恢复 */

    /* 刷新活动时间戳，确保不会立即又进入待机态 */
    pm_refresh_activity();
}

/**
 * @brief  进入待机态的操作
 * @note   切换为大连接间隔+最大延迟数以降低功耗
 *         关闭非必要外设（LED、CAT1 TX 等）
 *         TODO: 第4步实现详细外设控制
 */
static void pm_enter_standby(void)
{
    PM_LOG("进入待机态 STANDBY — 切换省电参数");
    g_pm_standby_start_ms = voiceUptimeMs;

    /* 第4步将实现：
     *   1. 发送连接参数更新请求 (50ms / latency 100)
     *   2. 关闭 LED
     *   3. 关闭 CAT1 UART TX，保持 RX
     *   4. 可选：关闭 ADC、关闭非必要时钟
     */
}

/**
 * @brief  退出待机态的操作
 * @note   恢复工作态连接参数，重新开启外设
 *         TODO: 第4步实现
 */
static void pm_exit_standby(void)
{
    PM_LOG("退出待机态 STANDBY — 恢复工作参数");

    /* 第4步将实现：
     *   1. 发送连接参数更新请求 (10ms / latency 40)
     *   2. 恢复 CAT1 UART TX
     *   3. 恢复 LED 指示功能
     */
}

/**
 * @brief  进入休眠态的操作
 * @note   停止广播、断开连接、配置唤醒源、进入 Sleep 模式
 *         TODO: 第6步实现详细休眠流程
 */
static void pm_enter_sleep(void)
{
    PM_LOG("进入休眠态 SLEEP — 准备进入 Sleep 模式");

    /* 第6步将实现：
     *   1. 停止 BLE 广播
     *   2. 断开 BLE 连接（如果有）
     *   3. 关闭全部非必要外设
     *   4. 配置 KEYSCAN EXTI 唤醒源
     *   5. 配置 RTC 定时唤醒
     *   6. 调用 PWR_EnterSLEEPMode()
     */
}

/**
 * @brief  进入配对态的操作
 * @note   启动可发现可连接广播（类型1）
 */
static void pm_enter_pairing(void)
{
    PM_LOG("进入配对态 PAIRING");
    pm_refresh_activity();

    /* 广播启动由 ble_adv_start_pairing() 完成，
       在 pm_state_transition() 调用后由上层触发 */
}

/**
 * @brief  进入回连态的操作
 * @note   启动不可发现可连接广播（类型2），开始重试计时
 */
static void pm_enter_reconnecting(void)
{
    PM_LOG("进入回连态 RECONNECTING");
    g_pm_reconnect_start_ms = voiceUptimeMs;
    pm_refresh_activity();

    /* 广播启动由 ble_adv_start_reconnect() 完成，
       在 pm_state_transition() 调用后由上层触发 */
}

/*===========================================================================
 * 四、公共 API 实现
 *===========================================================================*/

/**
 * @brief  电源状态机初始化
 */
void pm_state_init(void)
{
    /* 根据绑定状态决定初始状态 */
    if (pm_has_bonding()) {
        g_pm_state = PM_STATE_RECONNECTING;
        PM_LOG("PM 初始化：检测到绑定记录，初始状态 = 回连态");
    } else {
        g_pm_state = PM_STATE_PAIRING;
        PM_LOG("PM 初始化：无绑定记录，初始状态 = 配对态");
    }

    /* 初始化活动时间戳为当前时刻，避免立即触发超时 */
    pm_refresh_activity();

    /* 第6步将在此处配置 RTC 唤醒定时器以备休眠态使用 */
}

/**
 * @brief  获取当前电源状态
 */
pm_state_t pm_state_get(void)
{
    return g_pm_state;
}

/**
 * @brief  状态转换
 * @param  new_state  目标状态
 * @note   此函数会先执行旧状态的退出操作，再执行新状态的进入操作。
 *         如果新旧状态相同则直接返回（幂等）。
 */
void pm_state_transition(pm_state_t new_state)
{
    pm_state_t old_state = g_pm_state;

    /* 幂等保护：相同状态不重复转换 */
    if (old_state == new_state) {
        return;
    }

    PM_LOG("状态转换: %s → %s",
           pm_state_name(old_state),
           pm_state_name(new_state));

    /* ----- 退出旧状态 ----- */
    switch (old_state) {
        case PM_STATE_STANDBY:
            pm_exit_standby();
            break;
        /* 其他状态当前无需特殊退出操作 */
        default:
            break;
    }

    /* ----- 更新状态 ----- */
    g_pm_state = new_state;

    /* ----- 进入新状态 ----- */
    switch (new_state) {
        case PM_STATE_PAIRING:
            pm_enter_pairing();
            break;
        case PM_STATE_RECONNECTING:
            pm_enter_reconnecting();
            break;
        case PM_STATE_WORKING:
            pm_enter_working();
            break;
        case PM_STATE_STANDBY:
            pm_enter_standby();
            break;
        case PM_STATE_SLEEP:
            pm_enter_sleep();
            break;
        default:
            /* 非法状态，回退 */
            PM_LOG("错误：非法状态 %d，回退到配对态", (int)new_state);
            g_pm_state = PM_STATE_PAIRING;
            break;
    }
}

/**
 * @brief  通知 PM 状态机有业务活动发生
 */
void pm_activity_notify(pm_activity_t activity)
{
    /* 首先刷新活动时间戳（无论当前状态，推迟进入待机/休眠） */
    pm_refresh_activity();

    /* 如果在待机态，立即切回工作态 */
    if (g_pm_state == PM_STATE_STANDBY) {
        PM_LOG("待机态下检测到活动(%d)，切回工作态", (int)activity);
        pm_state_transition(PM_STATE_WORKING);
    }

    /* 休眠态下 CPU 停止，此函数不可达 */
    /* 其他状态下仅刷新时间戳即可 */
}

/**
 * @brief  通知 PM 状态机：BLE 连接已建立
 */
void pm_notify_connected(void)
{
    PM_LOG("收到连接成功通知，当前状态=%s", pm_state_name(g_pm_state));

    switch (g_pm_state) {
        case PM_STATE_PAIRING:
        case PM_STATE_RECONNECTING:
            /* 配对/回连态连接成功 → 进入工作态 */
            pm_state_transition(PM_STATE_WORKING);
            break;

        case PM_STATE_WORKING:
        case PM_STATE_STANDBY:
            /* 已连接状态，不应再收到连接通知（可能是重连或异常） */
            PM_LOG("警告：已在连接态下收到连接通知，忽略");
            break;

        case PM_STATE_SLEEP:
            /* 休眠态 CPU 停止，此函数不可达 */
            break;

        default:
            break;
    }
}

/**
 * @brief  通知 PM 状态机：BLE 连接已断开
 */
void pm_notify_disconnected(void)
{
    PM_LOG("收到断连通知，当前状态=%s", pm_state_name(g_pm_state));

    /* 根据是否有绑定记录决定去向 */
    if (pm_has_bonding()) {
        /* 有绑定 → 回到回连态，尝试重连 */
        pm_state_transition(PM_STATE_RECONNECTING);
    } else {
        /* 无绑定 → 回到配对态，等待新配对 */
        pm_state_transition(PM_STATE_PAIRING);
    }
}

/**
 * @brief  每主循环周期调用，检查超时条件
 */
void pm_state_process(void)
{
    uint32_t idle_ms;

    switch (g_pm_state) {
        case PM_STATE_WORKING:
            /* 检查工作态 → 待机态超时 */
            idle_ms = pm_idle_duration_ms();
            if (idle_ms >= PM_WORK_TO_STANDBY_MS) {
                PM_LOG("工作态空闲 %lu ms 超时，进入待机态", idle_ms);
                pm_state_transition(PM_STATE_STANDBY);
            }
            break;

        case PM_STATE_STANDBY:
            /* 检查待机态 → 休眠态超时 */
            idle_ms = pm_idle_duration_ms();
            if (idle_ms >= PM_STANDBY_TO_SLEEP_MS) {
                PM_LOG("待机态空闲 %lu ms 超时，进入休眠态", idle_ms);
                pm_state_transition(PM_STATE_SLEEP);
            }
            break;

        case PM_STATE_RECONNECTING:
            /* 检查回连态 → 休眠态超时 */
            if ((voiceUptimeMs - g_pm_reconnect_start_ms) >= PM_RECONNECT_TIMEOUT_MS) {
                PM_LOG("回连超时 %lu ms，进入休眠态",
                       (unsigned long)(voiceUptimeMs - g_pm_reconnect_start_ms));
                pm_state_transition(PM_STATE_SLEEP);
            }
            break;

        case PM_STATE_PAIRING:
        case PM_STATE_SLEEP:
            /* 配对态不超时（用户可能在操作配对流程） */
            /* 休眠态 CPU 停止，process 不可达 */
            break;

        default:
            break;
    }
}

/**
 * @brief  休眠唤醒入口
 */
void pm_wakeup_entry(void)
{
    PM_LOG("========== 休眠唤醒入口 ==========");

    /* 判断唤醒原因：
     *   - KEYSCAN EXTI：按键唤醒
     *   - RTC 定时器：定时唤醒
     * 第6步将实现具体唤醒源判断逻辑
     */

    /* 根据绑定状态决定去向 */
    if (pm_has_bonding()) {
        PM_LOG("唤醒后检测到绑定记录，进入回连态");
        pm_state_transition(PM_STATE_RECONNECTING);
    } else {
        PM_LOG("唤醒后无绑定记录，进入配对态");
        pm_state_transition(PM_STATE_PAIRING);
    }
}

/**
 * @brief  检查当前是否处于可通信状态（已连接）
 */
bool pm_is_connected(void)
{
    return (g_pm_state == PM_STATE_WORKING ||
            g_pm_state == PM_STATE_STANDBY);
}

/**
 * @brief  获取状态名称字符串（调试用）
 */
const char* pm_state_name(pm_state_t state)
{
    switch (state) {
        case PM_STATE_PAIRING:      return "配对态(PAIRING)";
        case PM_STATE_RECONNECTING: return "回连态(RECONNECTING)";
        case PM_STATE_WORKING:      return "工作态(WORKING)";
        case PM_STATE_STANDBY:      return "待机态(STANDBY)";
        case PM_STATE_SLEEP:        return "休眠态(SLEEP)";
        default:                    return "未知(UNKNOWN)";
    }
}

/**
 * @brief  获取当前空闲时长（调试用）
 */
uint32_t pm_get_idle_ms(void)
{
    return pm_idle_duration_ms();
}
