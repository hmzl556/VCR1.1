/**
 * @file    heartbeat.c
 * @brief   服务器心跳实现 — 周期唤醒 MCU + CAT1 上电建链报活
 *
 * 计时: app_now_ms() (sdk_timer_get_current_ms) 单调毫秒计数,
 *       在 br_sleep 深睡期间由 SDK 做睡眠补偿持续推进 (待机态 90s
 *       超时已验证), 休眠态噪音唤醒(~1-2s)时检查 — 见 heartbeat.h。
 */

#include "heartbeat.h"
#include "gmem.h"            /* app_now_ms()                        */
#include "rc_cat1_bridge.h"  /* rc_cat1_power_up / is_powered       */
#include "cat1_skt_udp.h"    /* cat1_modem_is_busy()                */
#include "udp_ota.h"         /* udp_ota_session_busy()              */
#include "remote_reports.h"  /* remote_queue_tag0A_heartbeat()      */
#include "pm_state.h"        /* pm_state_get — 待机态禁止执行业务   */
#include "co_printf.h"

/** 当前心跳周期 (ms; RAM): 出厂默认值初始化, 服务器响应可热更新; 0=关闭 */
static uint32_t s_period_ms;

/** 下次心跳到期时刻 (app_now_ms 值; RAM 时间戳, 复位后重新计时) */
static uint32_t s_next_due_ms;

/** 到期未发标志: 到期时 CAT1/OTA 忙则挂起, 业务一结束立刻补发 */
static uint8_t s_pending;

void heartbeat_init(void)
{
    s_period_ms = HEARTBEAT_DEFAULT_PERIOD_MS;
    s_pending = 0u;

    if (s_period_ms == 0u) {
        /* 出厂状态: 心跳关闭, 等服务器下发间隔后由
         * heartbeat_set_period_s() 开启并排程 */
        co_printf("[HB] init, disabled (period=0)\r\n");
        return;
    }

    s_next_due_ms = app_now_ms() + s_period_ms;
    co_printf("[HB] init, period=%u ms\r\n", (unsigned)s_period_ms);
}

void heartbeat_set_period_s(uint16_t period_s)
{
    /* 服务器下发心跳间隔 (响应数据ID 6, 单位秒; 0=关闭) */
    uint32_t new_period_ms = (uint32_t)period_s * 1000u;

    if (new_period_ms == s_period_ms) {
        /* ★ 同值不重排: 规范要求所有上行响应的 DataID6 都返回间隔,
         * 若每次收到都 next=now+N, 倒计时会被任意上报(按键/状态)
         * 反复清零, 心跳被无限推迟 (实测: 不从 Tag0A 而是从任意
         * 上报重新计时)。只有间隔变化时才更新并重排。 */
        return;
    }

    s_period_ms = new_period_ms;

    /*
     * ★ 间隔变化不清 s_pending: 本次心跳已欠发, 间隔更新只影响下一次
     * 排程; 若清掉, 正在等待建链的心跳会失去强制 poll 保护
     * (needs_cat1_poll 依赖 pending), 建链期间 BLE 活跃时 poll 被饿死
     * → 卡 boot delay。仅服务器显式关闭(0)时取消待发。
     */
    if (s_period_ms == 0u) {
        s_pending = 0u;
        co_printf("[HB] disabled by server (interval=0)\r\n");
        return;
    }

    s_next_due_ms = app_now_ms() + s_period_ms;
    co_printf("[HB] period updated: %u s\r\n", (unsigned)period_s);
}

uint32_t heartbeat_get_period_s(void)
{
    return s_period_ms / 1000u;
}

uint8_t heartbeat_due(void)
{
    if (s_period_ms == 0u) {
        return 0u;   /* 关闭状态: 永不到期 */
    }
    /* 32 位回绕安全: 有符号差值 >= 0 即到期 */
    return ((int32_t)(app_now_ms() - s_next_due_ms) >= 0) ? 1u : 0u;
}

/**
 * @brief  是否有待发的心跳业务 (待机分支据此切回工作态执行)
 */
uint8_t heartbeat_pending(void)
{
    return (s_pending != 0u) ? 1u : 0u;
}

/**
 * @brief  心跳是否开启 (周期非 0)
 */
uint8_t heartbeat_enabled(void)
{
    return (s_period_ms != 0u) ? 1u : 0u;
}

void heartbeat_poll(void)
{
    if (heartbeat_due() != 0u) {
        /* 到期: 周期节拍照常排程下一次, 本次标为待发 */
        s_next_due_ms = app_now_ms() + s_period_ms;
        if (s_pending == 0u) s_pending = 1u;
        co_printf("[HB] due, waiting for CAT1 idle...\r\n");
    }

    /*
     * ★ 待机态禁止执行业务: 待机分支没有 rc_cat1_poll, 若在待机态
     * power_up, 模组开着机进 boot delay 却无人推进 → 永远卡在
     * "boot delay 2000 ms" (实测)。由待机分支的 due/pending 检查
     * 先切回工作态, 下一轮再执行。
     */
    if (pm_state_get() == PM_STATE_STANDBY) {
        return;
    }

    /* A bounded setup failure powered LTE off. Retry at the next period,
     * rather than keep a permanent sleep lock with no modem to advance. */
    if (s_pending == 2u && rc_cat1_is_powered() == 0u) s_pending = 0u;
    if (s_pending != 1u) {
        /* 无待发, 或已上电等 UDP ready (由 heartbeat_on_udp_ready 处理) */
        return;
    }

    /* OTA / CAT1 业务忙: 不打断, 挂起等业务结束 (不丢本次心跳) */
    if (udp_ota_session_busy() != 0u || cat1_modem_is_busy() != 0u) {
        return;
    }

    if (rc_cat1_udp_is_ready() != 0u) {
        /* 模组已在线: 直接组帧入队, tx 步骤随即发出 */
        if(remote_queue_tag0A_heartbeat()==REMOTE_QUEUE_OK){
            s_pending=0u;
            co_printf("[HB] heartbeat tick: queued Tag0x0A (modem ready)\r\n");
        }
        return;
    }

    /*
     * 模组未在线: 上电建链。★ 不能此刻入队 — tx 步骤不看 UDP 就绪,
     * 建链期间会把队列里的帧取出发送 → 无 socket → "UDP send fail!"
     * → 帧丢失 (实测服务器收不到)。改为 UDP ready 时补发。
     */
    s_pending = 2u;
    co_printf("[HB] heartbeat tick: power up CAT1, queue at UDP ready\r\n");
    if (rc_cat1_is_powered() == 0u) rc_cat1_power_up();
}

/**
 * @brief  UDP 就绪回调 (rc_cat1_on_setup_ok 调用): 补发挂起的心跳帧
 */
void heartbeat_on_udp_ready(void)
{
    if (s_pending != 2u) {
        return;
    }
    if(remote_queue_tag0A_heartbeat()==REMOTE_QUEUE_OK){
        s_pending=0u;
        co_printf("[HB] UDP ready: queued Tag0x0A\r\n");
    }else s_pending=1u; /* Keep it pending and retry once queue space returns. */
}

/**
 * @brief  心跳待发期间是否需强制推进 CAT1 poll
 * @note   主循环的 rc_cat1_poll 有 BLE 活动门控 (g_last_ble_ms 50ms),
 *         HID 频繁活动时会把建链饿死 (实测: 心跳上电后卡在
 *         boot delay, setup 永不开始)。待发期间绕过该门控。
 */
uint8_t heartbeat_needs_cat1_poll(void)
{
    if (s_pending == 0u) {
        return 0u;
    }
    if (udp_ota_session_busy() != 0u) {
        return 0u;
    }
    if (rc_cat1_udp_is_ready() != 0u) {
        return 0u;
    }
    return 1u;
}
