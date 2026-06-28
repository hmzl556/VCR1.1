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
#include "pm_standby.h"  /* pm_standby_enter() / pm_standby_exit() */
#include "pm_sleep.h"    /* pm_sleep_enter()                     */
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
    /*
     * 防护：若 voiceUptimeMs 意外小于 g_pm_last_activity_ms（可能原因：
     * ① g_pm_last_activity_ms 未在关键路径被刷新  ② 变量被异常改写）
     * 减法会下溢得到一个极大值（~49 天），导致立即误触发超时。
     * 此处检测下溢并返回 0，同时印出警告便于调试。
     */
    if (voiceUptimeMs < g_pm_last_activity_ms) {
        PM_LOG("WARN: idle timer underflow! voice=%lu last_act=%lu",
               (unsigned long)voiceUptimeMs, (unsigned long)g_pm_last_activity_ms);
        /* 修复：将时间戳拉到当前值，恢复计时器 */
        g_pm_last_activity_ms = voiceUptimeMs;
        return 0U;
    }
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
    PM_LOG("Enter WORKING state");
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
    PM_LOG("Enter STANDBY state - power saving");
    g_pm_standby_start_ms = voiceUptimeMs;

    /* 执行待机态硬件操作：连接参数切换 + CAT1 TX关闭 + LED关闭 */
    pm_standby_enter();
}


/**
 * @brief  退出待机态的操作
 * @note   恢复工作态连接参数，重新开启外设
 *         TODO: 第4步实现
 */
static void pm_exit_standby(void)
{
    PM_LOG("Exit STANDBY - restore working params");

    /* 执行工作态硬件恢复：连接参数 + CAT1 TX */
    pm_standby_exit();
}

/**
 * @brief  进入休眠态的操作
 * @note   停止广播、断开连接、配置唤醒源、进入 Sleep 模式
 *         TODO: 第6步实现详细休眠流程
 */
static void pm_enter_sleep(void)
{
    uint8_t wake_reason;

    PM_LOG("Enter SLEEP state - prep sleep mode");

    /*
     * pm_sleep_enter() handles all sleep-mode hardware operations:
     *   1. Stop BLE advertising
     *   2. Reset bleAdvStat (main loop will restart BLE after wake)
     *   3. Shutdown non-essential peripherals (LED, CAT1 UART, etc.)
     *   4. Configure RTC wakeup timer (LSI 2KHz, N-second interval)
     *   5. Enter PWR_EnterSLEEPMode(WFI) — CPU stops here
     *
     * On wakeup (KEYSCAN EXTI or RTC), this function returns the wake reason.
     */
    wake_reason = pm_sleep_enter();

    /*
     * After waking from Sleep mode:
     * - bleAdvStat was reset to 0, so the main loop will call myBleStartAdv()
     *   which re-initializes BLE stack + services + starts advertising
     * - Determine destination state based on bonding and wake reason
     * - If woken by KEYSCAN: user pressed a key, restart PAIRING or RECONNECTING
     * - If woken by RTC: periodic check-in, restart RECONNECTING if bonded
     */
    if (pm_has_bonding()) {
        PM_LOG("Sleep wake: bonded, entering RECONNECTING");
        g_pm_state = PM_STATE_RECONNECTING;
    } else {
        PM_LOG("Sleep wake: no bond, entering PAIRING");
        g_pm_state = PM_STATE_PAIRING;
    }

    /* Refresh activity timer so we don't immediately time out again */
    pm_refresh_activity();

    (void)wake_reason;
}

/**
 * @brief  进入配对态的操作
 * @note   启动可发现可连接广播（类型1）
 */
static void pm_enter_pairing(void)
{
    PM_LOG("Enter PAIRING state");
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
    PM_LOG("Enter RECONNECTING state");
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
        PM_LOG("PM init: bonded, start=RECONNECTING");
    } else {
        g_pm_state = PM_STATE_PAIRING;
        PM_LOG("PM init: no bond, start=PAIRING");
    }

    /* 初始化活动时间戳为当前时刻，避免立即触发超时 */
    pm_refresh_activity();
}

/**
 * @brief  BLE 栈初始化完成后的状态同步
 * @note   pm_state_init() 调用时 BLE 栈尚未启动，peer_bonded_count() 返回 0，
 *         导致初始状态始终为 PAIRING。本函数在 myBleStartAdv() 之后调用，
 *         此时绑定数据已从 Flash 恢复到 RAM，可正确判断。
 *         仅当"PM认为是PAIRING但实际有绑定"时才修正为 RECONNECTING。
 */
void pm_state_sync_after_ble_init(void)
{
    /*
     * pm_state_init() 先于 BLE 栈初始化，此时 peer_bonded_count() 必为 0，
     * 因此初始状态固定为 PAIRING。BLE 栈就绪后，绑定已从 Flash 恢复，
     * 若实际有绑定，需通过正规状态转换切换到 RECONNECTING，
     * 确保 pm_enter_reconnecting() 被调用（初始化回连计时器+刷新活动时间戳）。
     */
    if (g_pm_state == PM_STATE_PAIRING && pm_has_bonding()) {
        pm_state_transition(PM_STATE_RECONNECTING);
        PM_LOG("PM sync after BLE init: corrected to RECONNECTING");
    }
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

    PM_LOG("State transition: %s -> %s",
           pm_state_name(old_state),
           pm_state_name(new_state));

    /* ----- 退出旧状态 ----- */
    switch (old_state) {
        case PM_STATE_STANDBY:
            /*
             * 从待机态退出：目标为 WORKING 时恢复外设；
             * 目标为 SLEEP 时跳过（马上要关外设睡觉，不需要恢复）
             */
            if (new_state != PM_STATE_SLEEP) {
                pm_exit_standby();
            }
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
            PM_LOG("ERROR: invalid state %d, fallback PAIRING", (int)new_state);
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
        PM_LOG("Activity(%d) in STANDBY, switch WORKING", (int)activity);
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
    PM_LOG("Connected notify, curr=%s", pm_state_name(g_pm_state));

    switch (g_pm_state) {
        case PM_STATE_PAIRING:
        case PM_STATE_RECONNECTING:
            /* 配对/回连态连接成功 → 进入工作态 */
            pm_state_transition(PM_STATE_WORKING);
            break;

        case PM_STATE_WORKING:
        case PM_STATE_STANDBY:
            /* 已连接状态，不应再收到连接通知（可能是重连或异常） */
            PM_LOG("WARN: connect notify while connected, ignore");
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
    PM_LOG("Disconnect notify, curr=%s", pm_state_name(g_pm_state));

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
                co_printf("WORKING idle %lu ms timeout -> STANDBY", idle_ms);
                pm_state_transition(PM_STATE_STANDBY);
            }
            break;

        case PM_STATE_STANDBY:
            /* 检查待机态 → 休眠态超时 */
            idle_ms = pm_idle_duration_ms();
            if (idle_ms >= PM_STANDBY_TO_SLEEP_MS) {
                co_printf("STANDBY idle %lu ms timeout -> SLEEP", idle_ms);
                pm_state_transition(PM_STATE_SLEEP);
            }
            break;

        case PM_STATE_RECONNECTING:
            /* 检查回连态 → 休眠态超时 */
            if ((voiceUptimeMs - g_pm_reconnect_start_ms) >= PM_RECONNECT_TIMEOUT_MS) {
                co_printf("RECONNECT timeout %lu ms -> SLEEP",
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
    PM_LOG("========== SLEEP WAKEUP ==========");

    /* 判断唤醒原因：
     *   - KEYSCAN EXTI：按键唤醒
     *   - RTC 定时器：定时唤醒
     * 第6步将实现具体唤醒源判断逻辑
     */

    /* 根据绑定状态决定去向 */
    if (pm_has_bonding()) {
        PM_LOG("Wakeup: bonded -> RECONNECTING");
        pm_state_transition(PM_STATE_RECONNECTING);
    } else {
        PM_LOG("Wakeup: no bond -> PAIRING");
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
