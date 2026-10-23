/**
 * @file    pm_state.c
 * @brief   电源管理状态机核心实现
 * @note    此文件是 PMU 模块的核心，实现六状态机（静默/配对/回连/工作/待机/休眠）
 *          的全部状态转换逻辑。
 *
 * 时间基准：app_now_ms()（sdk_timer 单调毫秒计数）。
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
#include "gmem.h"       /* app_now_ms() */
#include "gap.h"        /* peer_bonded_count() 绑定数量查询 */
#include "rc_cat1_bridge.h"
#include "udp_ota.h"
#include "cat1_skt_udp.h"
#include "heartbeat.h"
#include "task_keyscan.h"
#include "ble_adv.h"    /* ble_adv_start_by_bonding() — 唤醒后复用栈重启广播 */

extern volatile uint8_t g_audio_pcm_enabled;
extern void sdk_timer_pause(void);   /* main.c: 删 8ms 定时器让 WFI 深睡 */
extern void sdk_timer_resume(void);  /* main.c: 恢复 8ms 定时器 (FreeRTOS tick) */
extern volatile bool g_pairing_active; /* ble_gap.c: 组合键配对广播标志 */

/*===========================================================================
 * 一、内部静态变量
 *===========================================================================*/

/** @brief 当前电源状态，初始为静默态（后续由 pm_state_init 根据绑定修正） */
static pm_state_t   g_pm_state              = PM_STATE_SILENT;

/** @brief 最后一次业务活动发生的时间戳（app_now_ms 值） */
static uint32_t     g_pm_last_activity_ms   = 0U;

/** @brief 回连广播开始时间戳，用于超时判断 */
static uint32_t     g_pm_reconnect_start_ms = 0U;

/** @brief 进入配对态的时间戳，用于配对超时判断 */
static uint32_t     g_pm_pairing_start_ms   = 0U;

/** @brief 进入静默态的时间戳，用于静默超时判断 */
static uint32_t     g_pm_silent_start_ms    = 0U;

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
    g_pm_last_activity_ms = app_now_ms();
}

/**
 * @brief  计算自上次活动以来的空闲时长（毫秒）
 * @return 空闲毫秒数
 */
static uint32_t pm_idle_duration_ms(void)
{
    /* Unsigned subtraction remains correct across the 32-bit ms wrap. */
    return app_now_ms() - g_pm_last_activity_ms;
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

    /* 配对会话随休眠结束: 清除组合键配对标志, 否则唤醒后广播重启
     * 会误发配对广播 (实测: 配对态进休眠, 唤醒后直接回到配对广播) */
    g_pairing_active = false;

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
     * 休眠唤醒后：CAT1 UART/模组状态已在 sleep 流程中关闭，需重新初始化；
     * pm_wakeup_entry() 按绑定关系切到 SILENT/RECONNECTING 并刷新 PM 计时。
     * bleAdvStat 保持 0xa6 (见 pm_sleep.c): 复用现有 BLE 栈重启广播,
     * 不重建栈 (重建=二次 malloc → HardFault)。
     */
    rc_cat1_init();
    pm_wakeup_entry();
    /* 恢复 8ms sdk 定时器 (FreeRTOS tick): 休眠入口已暂停, 否则唤醒后
     * tick 一直冻结 (vTaskDelay 等会挂死) */
    sdk_timer_resume();
    ble_adv_start_by_bonding();

    (void)wake_reason;
}

/**
 * @brief  进入静默态的操作
 * @note   上电/唤醒后无绑定时进入。CPU 运行但不发任何 BLE 广播，
 *         等待用户按 KEY14+KEY3 组合键触发配对或超时进入休眠。
 *         计时器从进入此刻开始，与后续配对态的计时器独立。
 */
static void pm_enter_silent(void)
{
    PM_LOG("Enter SILENT state - waiting for combo key");
    g_pm_silent_start_ms = app_now_ms();
    pm_refresh_activity();
}

/**
 * @brief  进入配对态的操作
 * @note   启动可发现可连接广播（类型1）
 */
static void pm_enter_pairing(void)
{
    PM_LOG("Enter PAIRING state");
    g_pm_pairing_start_ms = app_now_ms();
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
    g_pm_reconnect_start_ms = app_now_ms();
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
    extern volatile bool g_pairing_active;

    /* 根据绑定状态决定初始状态 */
    if (pm_has_bonding()) {
        g_pm_state = PM_STATE_RECONNECTING;
        PM_LOG("PM init: bonded, start=RECONNECTING");
    } else {
        /* 无绑定: 上电后进静默态, 等 KEY14+KEY3 组合键触发配对 */
        g_pm_state = PM_STATE_SILENT;
        g_pm_silent_start_ms = app_now_ms();
        PM_LOG("PM init: no bond, start=SILENT");
    }

    /* 初始化活动时间戳为当前时刻，避免立即触发超时 */
    pm_refresh_activity();
}

/**
 * @brief  BLE 栈初始化完成后的状态同步
 * @note   pm_state_init() 调用时 BLE 栈尚未启动，peer_bonded_count() 返回 0，
 *         导致初始状态始终为 SILENT。本函数在 myBleStartAdv() 之后调用，
 *         此时绑定数据已从 Flash 恢复到 RAM，可正确判断。
 *         仅当"PM认为是SILENT或PAIRING但实际有绑定"时才修正为 RECONNECTING。
 */
void pm_state_sync_after_ble_init(void)
{
    /*
     * pm_state_init() 先于 BLE 栈初始化，此时 peer_bonded_count() 必为 0，
     * 因此初始状态固定为 SILENT。BLE 栈就绪后，绑定已从 Flash 恢复，
     * 若实际有绑定，需通过正规状态转换切换到 RECONNECTING，
     * 确保 pm_enter_reconnecting() 被调用（初始化回连计时器+刷新活动时间戳）。
     */
    if ((g_pm_state == PM_STATE_SILENT || g_pm_state == PM_STATE_PAIRING)
        && pm_has_bonding()) {
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

    /* OTA 会话期间禁止进入待机/休眠（combo/工厂复位等可能绕过 process 直接 transition） */
    if ((udp_ota_session_busy() != 0u || task_keyscan_ir_busy() || g_audio_pcm_enabled ||
         rc_cat1_has_work() || heartbeat_needs_cat1_poll())
        && (new_state == PM_STATE_STANDBY || new_state == PM_STATE_SLEEP)) {
        PM_LOG("OTA/IR busy, reject transition to %s",
               pm_state_name(new_state));
        return;
    }

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
             * 无论目标是 WORKING 还是 SLEEP 都完整退出待机:
             * - 目标 SLEEP 时不恢复的话, pm_clk_off 门控掉的
             *   TIM1/SPI1/SPI2/USART1 时钟会一直缺到休眠唤醒之后,
             *   BLE 栈重建时挂死 → 看门狗复位 (实测: 每次休眠后自动重启);
             * - 进 SLEEP 前需再次暂停 sdk 定时器: WFI 有 pending 定时器
             *   会被每 8ms 唤醒 (休眠瞬间自动唤醒的根源)。
             */
            pm_exit_standby();
            if (new_state == PM_STATE_SLEEP) {
                sdk_timer_pause();
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
        case PM_STATE_SILENT:
            pm_enter_silent();
            break;
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
            PM_LOG("ERROR: invalid state %d, fallback SILENT", (int)new_state);
            g_pm_state = PM_STATE_SILENT;
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
    if(g_pm_state==PM_STATE_SILENT && activity==PM_ACTIVITY_KEY)
        g_pm_silent_start_ms=app_now_ms();

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

    /* 休眠前的主动断连(le_conn_destory): 忽略, 唤醒后由 pm_wakeup_entry 决定去向 */
    if (g_pm_state == PM_STATE_SLEEP) {
        PM_LOG("Disconnect during SLEEP (pre-sleep destroy), ignore");
        return;
    }

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

    /* OTA 会话期间跳过 PM 超时检查，仅刷新活动时间，避免 session 结束后立即误休眠 */
    if (udp_ota_session_busy() != 0u) {
        pm_refresh_activity();
        return;
    }

    if (task_keyscan_ir_busy() || g_audio_pcm_enabled) {
        pm_refresh_activity();
        g_pm_reconnect_start_ms = app_now_ms();
        g_pm_pairing_start_ms = app_now_ms();
        g_pm_silent_start_ms = app_now_ms();
        return;
    }
    switch (g_pm_state) {
        case PM_STATE_SILENT:
            /* 检查静默态 → 休眠态超时 */
            {
                uint32_t now_ms = app_now_ms();
                if ((now_ms - g_pm_silent_start_ms) >= PM_SILENT_TO_SLEEP_MS) {
                    co_printf("SILENT timeout %lu ms -> SLEEP",
                              (unsigned long)(now_ms - g_pm_silent_start_ms));
                    pm_state_transition(PM_STATE_SLEEP);
                }
            }
            break;

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
            {
                uint32_t now_ms = app_now_ms();
                if ((now_ms - g_pm_reconnect_start_ms) >= PM_RECONNECT_TIMEOUT_MS) {
                    co_printf("RECONNECT timeout %lu ms -> SLEEP",
                              (unsigned long)(now_ms - g_pm_reconnect_start_ms));
                    pm_state_transition(PM_STATE_SLEEP);
                }
            }
            break;

        case PM_STATE_PAIRING:
            /* 配对态超时：30s 无人连接则休眠 */
            {
                uint32_t now_ms = app_now_ms();
                if ((now_ms - g_pm_pairing_start_ms) >= PM_PAIRING_TIMEOUT_MS) {
                    co_printf("PAIRING timeout %lu ms -> SLEEP",
                              (unsigned long)(now_ms - g_pm_pairing_start_ms));
                    pm_state_transition(PM_STATE_SLEEP);
                }
            }
            break;

        case PM_STATE_SLEEP:
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
        PM_LOG("Wakeup: no bond -> SILENT");
        pm_state_transition(PM_STATE_SILENT);
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
        case PM_STATE_SILENT:       return "SILENT";
        case PM_STATE_PAIRING:      return "PAIRING";
        case PM_STATE_RECONNECTING: return "RECONNECTING";
        case PM_STATE_WORKING:      return "WORKING";
        case PM_STATE_STANDBY:      return "STANDBY";
        case PM_STATE_SLEEP:        return "SLEEP";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief  获取当前空闲时长（调试用）
 */
uint32_t pm_get_idle_ms(void)
{
    return pm_idle_duration_ms();
}
