#include "ir_wave_store.h"
#include "ir_key_policy.h"
#include <stdbool.h>
#include <stdint.h>
#include "task_keyscan.h"
#include "co_printf.h"
#include "app_voice_ctrl.h"
#include "app_keyscan.h"
#include "ble_hid_kbd.h"
#include "ble_gap.h"
#include "gmem.h"
#include "rc_key_input.h"
#include "pm_state.h"       /* 电源管理 — 按键活动通知 */
#include "combo_key.h"      /* 组合键管理器（配对/解配）     */
#include "udp_ota.h"
#include "ble_adv.h"        /* ble_adv_start_wakeup          */
#include "cs_core.h"
#include "userhard.h"
#include "cat1_skt_udp.h"   /* cat1_modem_is_busy：HID/AT 错峰 */
#include "ir_key_map.h"     /* ir_send_key — BLE 未连接时发红外  */
#include "irc.h"            /* irc_send_repeat — 长按重复码       */
#include "ir_learn.h"       /* 红外学习模式（长按电源键触发）     */

/* OTA owns LED1 red for the whole session, including pause/retry.
 * Preserve key handling; only suppress this module's ordinary LED writes. */
static void task_keyscan_red_led(bool on)
{
    if (udp_ota_session_busy() != 0U) return;
    if (on) SETLED1; else CLRLED1;
}

volatile uint16_t key_receive = 0;
volatile uint32_t g_last_ble_ms = 0U;  /* BLE/CAT1 串行化：BLE 最后活动时间戳 */

typedef enum
{
	KEY_STATE_IDLE = 0,
	KEY_STATE_DEBOUNCE,
	KEY_STATE_PRESSED
} key_state_t;

static key_state_t key_state = KEY_STATE_IDLE;
static uint16_t key_candidate = 0;
static uint16_t key_active = 0;
static uint16_t key_active_usage = 0;
static uint32_t key_state_ts_ms = 0;
static uint32_t key_last_event_ms = 0;
static uint32_t key_press_start_ms = 0;
static bool key_long_triggered = false;
static bool key_active_is_consumer = false;
static bool key15_voice_started = false;

/** Current physical press was accepted by the IR queue. */
static bool s_ir_key_routed;

/* ====== 红外学习模式状态机 ====== */
typedef enum {
    IR_LEARN_MODE_OFF = 0,
    IR_LEARN_MODE_WAIT_KEY,   /* 已长按右侧电视机电源键 5s，等待选目标键 */
    IR_LEARN_MODE_CAPTURING,  /* 已选目标键，正在捕获 */
    IR_LEARN_MODE_FEEDBACK,
    IR_LEARN_MODE_SAVING,
} ir_learn_mode_t;

/** @brief 长按右侧电视机电源键进入学习模式的触发时长 */
#define IR_LEARN_TRIGGER_MS       5000U
#define IR_TV_POWER_LED_MS        100U
/** @brief 学习模式无操作超时 */
#define IR_LEARN_MODE_TIMEOUT_MS  15000U

static ir_learn_mode_t s_ir_learn_mode = IR_LEARN_MODE_OFF;
static uint32_t s_ir_learn_mode_ts_ms = 0U;
static uint32_t s_ir_learn_led_ts_ms = 0U;
static bool     s_ir_learn_led_on = false;
static uint8_t s_ir_feedback_left;
static uint32_t s_ir_feedback_period;
static bool s_ir_wait_release;
static bool s_ir_suppress_until_release;


bool task_keyscan_ir_busy(void)
{
    return s_ir_learn_mode != IR_LEARN_MODE_OFF || ir_tx_pending();
}
void task_keyscan_ir_learn_result(ir_learn_evt_t evt, uint8_t user, uint8_t cmd)
{
    bool ok;
    (void)user; (void)cmd;
    if (s_ir_learn_mode != IR_LEARN_MODE_CAPTURING) return;
    if (evt == IR_LEARN_EVT_TIMEOUT || evt == IR_LEARN_EVT_CANCELED) {
        s_ir_learn_mode = IR_LEARN_MODE_OFF;
        task_keyscan_red_led(false);
        return;
    }
    if (evt == IR_LEARN_EVT_DONE && cat1_modem_is_busy()) {

        s_ir_learn_mode = IR_LEARN_MODE_SAVING;
        s_ir_learn_mode_ts_ms = app_now_ms();
        return;
    }
    ok = evt == IR_LEARN_EVT_DONE &&
         ir_wave_store_save(ir_learn_get_target(), ir_learn_wave());
    ir_learn_discard();
    s_ir_feedback_left = ok ? 6u : 10u;
    s_ir_feedback_period = ok ? 500u : 100u;
    s_ir_learn_mode_ts_ms = app_now_ms();
    s_ir_learn_led_ts_ms = app_now_ms();
    s_ir_learn_led_on = true;
    task_keyscan_red_led(true);
    s_ir_learn_mode = IR_LEARN_MODE_FEEDBACK;
    co_printf("[IR] KEY%u %s\r\n", (unsigned)ir_learn_get_target(), ok ? "saved" : "failed");
}

/* KEY_15 语音与 RC-KEY notify 分开发送，避免同周期多次 FSM_Write */
static struct {
	uint8_t pending;
	uint8_t action;
	uint16_t usage;
	uint16_t key_id;
} s_key15_rc_defer;

/*
 * CAT1 AT 忙时延后 HID：ECSORF 收包窗口内发 BLE notify 会拖垮 UART RX 消费。
 */
#define HID_DEFER_CAPACITY 16u
#define HID_DEFER_PRESS 0x0100u
#define HID_DEFER_CONSUMER_RELEASE 0x0200u
static uint16_t s_hid_events[HID_DEFER_CAPACITY];
static uint8_t s_hid_head, s_hid_count, s_hid_reset_pending;

static void hid_defer_clear(void)
{ s_hid_head = s_hid_count = s_hid_reset_pending = 0u; }

static void hid_defer_push(uint16_t event)
{
    if (s_hid_count == HID_DEFER_CAPACITY) {
        /* Bounded overflow: release both report types before retaining the
         * latest event. Never leave a previously delivered key stuck down. */
        s_hid_head = s_hid_count = 0u;
        s_hid_reset_pending = 1u;
        co_printf("[HID] deferred queue full; release recovery pending\r\n");
    }
    s_hid_events[(s_hid_head + s_hid_count) % HID_DEFER_CAPACITY] = event;
    ++s_hid_count;
}

static void hid_defer_cancel_presses(void)
{
    uint8_t i, kept = 0u;
    for (i = 0u; i < s_hid_count; ++i) {
        uint16_t event = s_hid_events[(s_hid_head + i) % HID_DEFER_CAPACITY];
        if (!(event & HID_DEFER_PRESS))
            s_hid_events[(s_hid_head + kept++) % HID_DEFER_CAPACITY] = event;
    }
    s_hid_count = kept;
}

static void task_keyscan_hid_flush_deferred(void)
{
    unsigned budget = 4u;
    if (cat1_modem_is_busy()) return;
    if (ble_gap_get_peer() == NULL) { hid_defer_clear(); return; }
    if (s_hid_reset_pending) {
        hid_kbd_send_release_current();
        hid_kbd_send_consumer_release_current();
        s_hid_reset_pending = 0u;
    }
    while (s_hid_count && budget--) {
        uint16_t event = s_hid_events[s_hid_head];
        s_hid_head = (s_hid_head + 1u) % HID_DEFER_CAPACITY;
        --s_hid_count;
        if (event & HID_DEFER_PRESS) hid_kbd_send_key_id_current(event & 0xffu);
        else if (event & HID_DEFER_CONSUMER_RELEASE) hid_kbd_send_consumer_release_current();
        else hid_kbd_send_release_current();
    }
}

static void task_keyscan_hid_send_press(uint16_t key_id)
{
    if (key_id == TASK_KEYSCAN_TV_POWER || !key_id || ble_gap_get_peer() == NULL) return;
    if (key_id == TASK_KEYSCAN_STB_POWER) key_id = TASK_KEYSCAN_KEY1;
    if (cat1_modem_is_busy() || s_hid_count || s_hid_reset_pending)
        hid_defer_push((uint16_t)(HID_DEFER_PRESS | key_id));
    else hid_kbd_send_key_id_current(key_id);
}

static void task_keyscan_hid_send_release(uint8_t is_consumer)
{
    if (key_active == TASK_KEYSCAN_TV_POWER || ble_gap_get_peer() == NULL) return;
    if (cat1_modem_is_busy() || s_hid_count || s_hid_reset_pending)
        hid_defer_push(is_consumer ? HID_DEFER_CONSUMER_RELEASE : 0u);
    else if (is_consumer) hid_kbd_send_consumer_release_current();
    else hid_kbd_send_release_current();
}

static void key15_rc_notify_defer(uint8_t action)
{
	s_key15_rc_defer.pending = 1u;
	s_key15_rc_defer.action = action;
	s_key15_rc_defer.usage = key_active_usage;
	s_key15_rc_defer.key_id = key_active;
}


// ====== 恢复出厂设置状态机 ======
typedef enum
{
    FACTORY_RST_IDLE = 0,        // 空闲，正常按键处理
    FACTORY_RST_COMBO_DETECT,    // 检测到 KEY8 + KEY14 同时按下，计时 3s
    FACTORY_RST_PENDING,         // 待恢复出厂模式（10s 窗口），LED2 快闪
    FACTORY_RST_CONFIRMED,       // 已确认恢复出厂，LED2 亮 2s 后熄灭
    FACTORY_RST_CANCELLED        // 已取消，LED1 亮 2s 后熄灭
} factory_rst_state_t;

static factory_rst_state_t factory_rst_state = FACTORY_RST_IDLE;
static uint32_t factory_rst_state_ts_ms = 0;   // 进入当前状态的时间戳
static uint32_t factory_rst_led_ts_ms = 0;     // LED 闪烁/切换时间戳
static bool factory_rst_led2_state = false;     // LED2 当前亮灭状态
static bool pending_keys_released = false;      // PENDING 下组合键是否已松开

// 恢复出厂设置状态机处理（与正常按键处理并行运行）
// hw_snapshot 由调用方一次读取传入，避免重复读硬件
static void factory_reset_process(uint32_t now_ms, uint16_t key_id,
                                  const uint32_t hw_snapshot[5])
{
    switch (factory_rst_state)
    {
        case FACTORY_RST_IDLE:
        {
            bool k8 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY8);
            bool k14 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY14);
            if (k8 && k14)
            {
                factory_rst_state = FACTORY_RST_COMBO_DETECT;
                factory_rst_state_ts_ms = now_ms;
                co_printf("Factory reset: KEY8+KEY14 combo detected, counting 3s...\r\n");
            }
            break;
        }

        case FACTORY_RST_COMBO_DETECT:
        {
            bool k8 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY8);
            bool k14 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY14);
            if (!(k8 && k14))
            {
                factory_rst_state = FACTORY_RST_IDLE;
                co_printf("Factory reset: combo released before 3s, cancelled.\r\n");
            }
            else if ((now_ms - factory_rst_state_ts_ms) >= FACTORY_RST_COMBO_HOLD_MS)
            {
                factory_rst_state = FACTORY_RST_PENDING;
                factory_rst_state_ts_ms = now_ms;
                factory_rst_led_ts_ms = now_ms;
                factory_rst_led2_state = false;
                pending_keys_released = false;
                CLRLED2;
                co_printf("Factory reset: pending mode entered! LED2 fast flash.\r\n");
            }
            break;
        }

        case FACTORY_RST_PENDING:
        {
            if ((now_ms - factory_rst_led_ts_ms) >= LED_FAST_FLASH_MS)
            {
                factory_rst_led_ts_ms = now_ms;
                factory_rst_led2_state = !factory_rst_led2_state;
                if (factory_rst_led2_state) SETLED2; else CLRLED2;
            }

            if ((now_ms - factory_rst_state_ts_ms) >= FACTORY_RST_PENDING_MS)
            {
                CLRLED2;
                factory_rst_state = FACTORY_RST_CANCELLED;
                factory_rst_state_ts_ms = now_ms;
                co_printf("Factory reset: 10s timeout, cancelled.\r\n");
                break;
            }

            if (!pending_keys_released)
            {
                bool k8 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY8);
                bool k14 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY14);
                if (!k8 && !k14)
                {
                    pending_keys_released = true;
                    co_printf("Factory reset: both combo keys released, key check active.\r\n");
                }
            }
            else
            {
                if (key_id != 0)
                {
                    if (key_id == TASK_KEYSCAN_KEY4)
                    {
                        /* OTA 会话期间不执行工厂复位清绑定/进休眠 */
                        if (udp_ota_session_busy() != 0u) {
                            CLRLED2;
                            factory_rst_state = FACTORY_RST_IDLE;
                            co_printf("Factory reset: ignored, OTA session busy.\r\n");
                            break;
                        }
                        CLRLED2;
                        factory_rst_state = FACTORY_RST_CONFIRMED;
                        factory_rst_state_ts_ms = now_ms;
                        SETLED2;
                        co_printf("Factory reset: KEY4 confirmed! Erasing bonding...\r\n");
                        if (!ir_wave_store_clear()) {
                            CLRLED2;
                            factory_rst_state = FACTORY_RST_CANCELLED;
                            co_printf("Factory reset: IR flash clear failed\r\n");
                            break;
                        }
                        ir_user_reset();
                        ble_gap_clear_bonding_info();
                    }
                    else
                    {
                        CLRLED2;
                        factory_rst_state = FACTORY_RST_CANCELLED;
                        factory_rst_state_ts_ms = now_ms;
                        co_printf("Factory reset: key %d pressed, cancelled.\r\n", key_id);
                    }
                }
            }
            break;
        }

        case FACTORY_RST_CONFIRMED:
            if ((now_ms - factory_rst_state_ts_ms) >= FACTORY_RST_LED_ON_MS)
            {
                CLRLED2;
                factory_rst_state = FACTORY_RST_IDLE;
                /* OTA 会话期间工厂复位完成也不进 SLEEP */
                if (udp_ota_session_busy() != 0u) {
                    co_printf("Factory reset: complete ignored, OTA session busy.\r\n");
                    break;
                }
                // 工厂复位后进入休眠，用户按 KEY14+KEY5 可重新配对
                pm_state_transition(PM_STATE_SLEEP);
                co_printf("Factory reset: complete, entering SLEEP.\r\n");
            }
            break;

        case FACTORY_RST_CANCELLED:
            if ((now_ms - factory_rst_state_ts_ms) >= FACTORY_RST_LED_ON_MS)
            {
                task_keyscan_red_led(false);
                factory_rst_state = FACTORY_RST_IDLE;
                co_printf("Factory reset: cancelled, returned to IDLE.\r\n");
            }
            break;

        default:
            factory_rst_state = FACTORY_RST_IDLE;
            break;
    }
}

/**
 * @brief  红外学习模式状态机（长按右侧电视机电源键 5s 进入）
 * @param  now_ms 当前时间戳
 * @param  key_id 本周期按键事件（0 = 无）
 * @note   WAIT_KEY：LED 常亮，等待选目标键；
 *         CAPTURING：LED 闪烁，驱动 ir_learn_process() 解码。
 */
static void ir_learn_mode_process(uint32_t now_ms, uint16_t key_id, const uint32_t hw_snapshot[5])
{
    uint8_t hw = app_keyscan_read_key_id();
    bool new_press = false;
    (void)key_id;
    if (s_ir_learn_mode == IR_LEARN_MODE_OFF) return;
    if (udp_ota_session_busy()) {
        s_ir_learn_mode = IR_LEARN_MODE_OFF;
        ir_learn_stop();
        task_keyscan_red_led(false);
        return;
    }
    pm_activity_notify(PM_ACTIVITY_KEY);
    if (hw_snapshot[0] == 0u && hw_snapshot[1] == 0u) s_ir_wait_release = false;
    else if (hw && !s_ir_wait_release) {
        s_ir_wait_release = true;
        new_press = true;
        s_ir_learn_mode_ts_ms = now_ms;
    }
    if (now_ms - s_ir_learn_mode_ts_ms >= IR_LEARN_MODE_TIMEOUT_MS) {
        s_ir_learn_mode = IR_LEARN_MODE_OFF;
        ir_learn_stop();
        task_keyscan_red_led(false);
        return;
    }
    if (s_ir_learn_mode == IR_LEARN_MODE_SAVING) {
        /* qflash masks interrupts. Do not erase/write across an active CAT1
         * UART transaction; keep scheduling BLE, UART and watchdog meanwhile. */
        task_keyscan_red_led(true);
        if (!cat1_modem_is_busy()) {
            s_ir_learn_mode = IR_LEARN_MODE_CAPTURING;
            task_keyscan_ir_learn_result(IR_LEARN_EVT_DONE, 0, 0);
        }
        return;
    }
    if (s_ir_learn_mode == IR_LEARN_MODE_FEEDBACK) {
        if (now_ms - s_ir_learn_led_ts_ms >= s_ir_feedback_period) {
            s_ir_learn_led_ts_ms = now_ms;
            if (--s_ir_feedback_left == 0u) {
                s_ir_learn_mode = IR_LEARN_MODE_WAIT_KEY;
                task_keyscan_red_led(true);
            } else {
                s_ir_learn_led_on = !s_ir_learn_led_on;
                if (s_ir_learn_led_on) task_keyscan_red_led(true); else task_keyscan_red_led(false);
            }
        }
        return;
    }
    if (new_press && ir_key_learnable(hw)) {
        /* Only four policy-approved keys are learnable. Stop old capture
         * before retargeting; a cancellation must not exit the UI session. */
        s_ir_learn_mode = IR_LEARN_MODE_WAIT_KEY;
        ir_learn_stop();
        ir_learn_set_target(hw);
        if (ir_learn_start()) {
            s_ir_learn_mode = IR_LEARN_MODE_CAPTURING;
            s_ir_learn_led_ts_ms = now_ms;
            s_ir_learn_led_on = true;
        }
    }
    if (s_ir_learn_mode == IR_LEARN_MODE_CAPTURING) {
        if (now_ms - s_ir_learn_led_ts_ms >= 200u) {
            s_ir_learn_led_ts_ms = now_ms;
            s_ir_learn_led_on = !s_ir_learn_led_on;
            if (s_ir_learn_led_on) task_keyscan_red_led(true); else task_keyscan_red_led(false);
        }
        ir_learn_process();
    } else task_keyscan_red_led(true);
}

/**
* @brief 重置按键扫描模块，当 BLE 断开连接时调用
*/
void task_keyscan_reset_on_ble_disconnect(void)
{
	ir_tx_release();
    s_ir_key_routed = false;
    key_state = KEY_STATE_IDLE;
	key_candidate = 0;
	key_active = 0;
	key_active_usage = 0;
	key_state_ts_ms = 0;
	key_last_event_ms = 0;
	key_press_start_ms = 0;
	key_long_triggered = false;
	key_active_is_consumer = false;
	key15_voice_started = false;
	s_key15_rc_defer.pending = 0u;
    hid_defer_clear();
	key_receive = 0;
	//CLRLED2;							// 清除 LED2（蓝） 状态为 OFF
}

/**
* @brief 获取当前时间，单位为毫秒
* @return 当前时间，单位为毫秒
*/
static uint32_t task_keyscan_get_now_ms(void)
{
	return app_now_ms();
}

/**
* @brief 发送 RC 通知
* @param action 动作
*/
static void task_keyscan_rc_notify(uint8_t action)
{
	uint8_t key_type;
    /* The right TV power is IR-only; the left STB power keeps the existing
     * external power key identity in HID/RC/Tag10 despite its physical ID18. */
    if (key_active == TASK_KEYSCAN_TV_POWER) return;

	if (key_active == TASK_KEYSCAN_KEY15) {
		key_type = 0x03u;
	} else {
		key_type = key_active_is_consumer ? 0x02u : 0x01u;
	}
	rc_key_input_event(key_type, action, key_active_usage,
                       key_active == TASK_KEYSCAN_STB_POWER ? TASK_KEYSCAN_KEY1 : key_active);
}

void task_keyscan_flush_deferred_hid(void)
{
	task_keyscan_hid_flush_deferred();
}

void task_keyscan_flush_deferred_voice_rc(void)
{
	if (s_key15_rc_defer.pending == 0u) {
		return;
	}

	/* 先让语音 chr2 indicate / chr0 notify 入队完成，再发 RC-KEY notify */
	cs_schedule();
	rc_key_input_event(0x03u, s_key15_rc_defer.action,
	                   s_key15_rc_defer.usage, s_key15_rc_defer.key_id);
	s_key15_rc_defer.pending = 0u;
}

/**
* @brief 按键扫描处理
*/
void task_keyscan_process(void)
{
	uint16_t key_id = 0;

	/* AT 空闲时先吐出延后 HID，避免与下一笔 CAT1 再叠在一起 */
	task_keyscan_hid_flush_deferred();

	//static uint16_t _ks = 0; _ks++; if ((_ks % 500) == 0) co_printf("K");
	if (key_receive) {
		key_id = key_receive;
		key_receive = 0;
	}

	uint32_t now_ms = task_keyscan_get_now_ms();
    ir_tx_process();
	if (key_id != 0) {
		key_last_event_ms = now_ms;
	}



	// ====== 硬件按键快照（每周期一次，所有组合键共享） ======
	uint32_t hw_snapshot[5];
	KEYSCAN_ReadKeyData(hw_snapshot);


	// ====== 组合键互斥调度（学习模式活跃时暂停，避免与选目标键冲突） ======
	// 工厂复位空闲时，简单组合键（配对/解配）才工作
	if (factory_rst_state == FACTORY_RST_IDLE && s_ir_learn_mode == IR_LEARN_MODE_OFF) {
        bool combo_was_idle = combo_key_is_idle();
        combo_key_process(now_ms, hw_snapshot);
        /* Release the ordinary/wake red LED at combo takeover. Special
         * indication starts on later iterations, so it is not overwritten. */
        if (combo_was_idle && !combo_key_is_idle()) task_keyscan_red_led(false);
	}
	// 简单组合键空闲时，工厂复位才工作
	if (combo_key_is_idle() && s_ir_learn_mode == IR_LEARN_MODE_OFF) {
		factory_reset_process(now_ms, key_id, hw_snapshot);
	}

	// ====== 红外学习模式处理（长按电源键 5s 触发） ======
	bool was_learning = s_ir_learn_mode != IR_LEARN_MODE_OFF;
	ir_learn_mode_process(now_ms, key_id, hw_snapshot);

	// ====== 任一组合键或学习模式活跃时，跳过正常按键处理 ======
    if (factory_rst_state != FACTORY_RST_IDLE || !combo_key_is_idle()
        || s_ir_learn_mode != IR_LEARN_MODE_OFF || was_learning)
    {
        ir_tx_release();
        s_ir_key_routed = false;
        s_ir_suppress_until_release = true;
        // 重置正常按键状态机，避免残留状态干扰；同时清除 LED 防止常亮
        if (key_state != KEY_STATE_IDLE)
        {
            if (key_state == KEY_STATE_PRESSED) {
                hid_defer_cancel_presses();
                task_keyscan_hid_send_release(key_active_is_consumer ? 1u : 0u);
                if (key15_voice_started) {
                    key15_voice_started = false;
                    (void)voice_ctrl_post_stop();
                    key15_rc_notify_defer(0x00u);
                } else if (key_active != TASK_KEYSCAN_KEY15) task_keyscan_rc_notify(0x00u);
            }
            key_state = KEY_STATE_IDLE;
            key_active = 0;
            key_active_is_consumer = false;
        }
        return;
    }

    if (s_ir_suppress_until_release) {
        if (hw_snapshot[0] != 0u || hw_snapshot[1] != 0u) return;
        s_ir_suppress_until_release = false;
        task_keyscan_red_led(false); /* Cancelled/released combos must not leave wake red latched. */
        return;
    }
	switch (key_state) {
		case KEY_STATE_IDLE:
            if (key_id == 0u && hw_snapshot[0] == 0u && hw_snapshot[1] == 0u) task_keyscan_red_led(false);
			if (key_id != 0) {
				key_candidate = key_id;
				key_state_ts_ms = now_ms;
				key_state = KEY_STATE_DEBOUNCE;
			}
			break;

		// 消抖状态：处理中途按键值乱跳和消抖完成两件事
		case KEY_STATE_DEBOUNCE:
			if (key_id != 0 && key_id != key_candidate) {
				key_candidate = key_id;
				key_state_ts_ms = now_ms;
			}
			if ((now_ms - key_state_ts_ms) >= KEY_DEBOUNCE_MS) {
                if (!keyscan_is_key_pressed(hw_snapshot, (uint8_t)key_candidate)) {
                    task_keyscan_red_led(false);
                    key_state = KEY_STATE_IDLE;
                    break;
                }
				key_active = key_candidate;
				key_active_is_consumer = false;
				key_active_usage = 0;
				{
					uint16_t usage_tmp = 0;
					bool is_consumer_tmp = false;
					if (hid_kbd_map_key_to_usage(key_active == TASK_KEYSCAN_STB_POWER ? TASK_KEYSCAN_KEY1 : key_active, &usage_tmp, &is_consumer_tmp)) {
						key_active_is_consumer = is_consumer_tmp;
						key_active_usage = usage_tmp;
					}
				}

				// 短按按下即发：确认稳定按下后立刻发送
				key_state = KEY_STATE_PRESSED;

				// 电源键 + 回连态：切换为唤醒广播，通过广播中的 STB MAC 地址唤醒机顶盒
				if (key_active == TASK_KEYSCAN_STB_POWER
				    && pm_state_get() == PM_STATE_RECONNECTING) {
					co_printf("STB power KEY18 pressed in RECONNECTING, switch to wakeup broadcast\r\n");
					ble_adv_start_wakeup();
				}

				// 通知电源管理状态机：有按键活动
				// - 待机态下此调用会立即切回工作态
				// - 工作态下此调用会重置空闲计时器，推迟进入待机态
				pm_activity_notify(PM_ACTIVITY_KEY);

				//SETLED2;									// 设置 LED2（蓝） 状态为 ON
				key_press_start_ms = now_ms;
				key_long_triggered = false;

				// 按键 LED 指示：按下亮，松开灭
				if (key_active == TASK_KEYSCAN_TV_POWER) {
                    CLRLED2;
                    task_keyscan_red_led(true);
                } else if (ble_gap_get_peer() != NULL) {
					SETLED2;
				} else {
					task_keyscan_red_led(true);
				}

                s_ir_key_routed = false;
                if (key_active != 0u) {
                    /* Learning changes IR codes only, never the BLE mapping.
                     * TV power remains addressed to the TV, not the STB peer.
                     * Voice is BLE-only and cannot replay legacy learning. */
                    if (key_active != TASK_KEYSCAN_TV_POWER &&
                        key_active != TASK_KEYSCAN_KEY15 && ble_gap_get_peer() == NULL)
                        s_ir_key_routed = ir_send_key((uint8_t)key_active);
                    if (ble_gap_get_peer() != NULL && key_active != TASK_KEYSCAN_TV_POWER)
                        task_keyscan_hid_send_press(key_active);
                    if (udp_ota_session_busy()) cs_schedule();
                }

				if (key_active != TASK_KEYSCAN_KEY15) {
					task_keyscan_rc_notify(0x01u);
				}
			}
			break;
		
		// 稳定按下状态：处理中途按键值变化、长按判定和释放判定
		case KEY_STATE_PRESSED:
		{
			uint8_t hw_key = app_keyscan_read_key_id();

            /* A brief red pulse distinguishes the TV power press from the
             * solid red indication after the 5-second learning threshold. */
            if (key_active == TASK_KEYSCAN_TV_POWER &&
                now_ms - key_press_start_ms >= IR_TV_POWER_LED_MS) {
                task_keyscan_red_led(false);
            }

			/* Right TV power KEY1 long hold 5s learns; left STB power KEY18 remains normal. */
			if (key_active == TASK_KEYSCAN_TV_POWER && hw_key == TASK_KEYSCAN_TV_POWER
                && udp_ota_session_busy() == 0u && !ir_tx_pending()
			    && (now_ms - key_press_start_ms) >= IR_LEARN_TRIGGER_MS) {
				s_ir_learn_mode = IR_LEARN_MODE_WAIT_KEY;
                s_ir_wait_release = true;
                ir_tx_release();
                s_ir_key_routed = false;
                task_keyscan_hid_send_release(key_active_is_consumer ? 1u : 0u);
				s_ir_learn_mode_ts_ms = now_ms;
				CLRLED2;
				task_keyscan_red_led(true);  /* LED 常亮 */
				co_printf("[IR] learn mode entered\r\n");
				/* 模拟电源键释放，避免 STB 认为按键一直按住 */
				task_keyscan_rc_notify(0x00u);
				/* 重置按键状态机，学习模式接管 */
				key_state = KEY_STATE_IDLE;
				key_active = 0;
				key_active_usage = 0;
				key_active_is_consumer = false;
				break;
			}

			// 中途按键值变化，重新回到消抖状态
			if (hw_key == key_active) {
				key_last_event_ms = now_ms;
			} else if (hw_key != 0 && hw_key != key_active) {
				key_id = hw_key;
			}

			// 长按判定
			if (key_id != 0 && key_id != key_active) {
                ir_tx_release(); s_ir_key_routed = false;
                task_keyscan_hid_send_release(key_active_is_consumer ? 1u : 0u);
                if (key15_voice_started) {
                    key15_voice_started = false;
                    (void)voice_ctrl_post_stop();
                    key15_rc_notify_defer(0x00u);
                } else if (key_active != TASK_KEYSCAN_KEY15) task_keyscan_rc_notify(0x00u);
				key_candidate = key_id;
				key_state_ts_ms = now_ms;
				key_state = KEY_STATE_DEBOUNCE;
				CLRLED2;
				task_keyscan_red_led(false);
				break;
			}

			// 长按判定：仅触发一次
			if (!key_long_triggered && (now_ms - key_press_start_ms) >= KEY_LONG_PRESS_MS) {
				key_long_triggered = true;
				/* OTA 会话期间禁止 KEY_15 长按启动语音采集 */
				if (key_active == TASK_KEYSCAN_KEY15) {
					if (udp_ota_session_busy() == 0u && !key15_voice_started && !s_ir_key_routed &&
                        ble_gap_get_peer() != NULL) {
						key15_voice_started = true;
						(void)voice_ctrl_post_start();
						key15_rc_notify_defer(0x01u);
						co_printf("[VOICE] KEY15 start requested\r\n");
					}
				}
			}

            if (hw_key == key_active) {
                pm_activity_notify(PM_ACTIVITY_KEY);
                if (s_ir_key_routed && now_ms-key_press_start_ms>=400u &&
                    (ir_wave_store_has(key_active) || key_active==5u || key_active==6u ||
                     key_active==7u || key_active==11u || key_active==12u ||
                     key_active==13u || key_active==14u || key_active==16u))
                    ir_tx_repeat((uint8_t)key_active);
            } else ir_tx_release();

            if (hw_key == 0 && (now_ms - key_last_event_ms) >= KEY_RELEASE_TIMEOUT_MS) {
                if(key_active==TASK_KEYSCAN_TV_POWER && now_ms-key_press_start_ms<IR_LEARN_TRIGGER_MS)
                    (void)ir_send_key(TASK_KEYSCAN_TV_POWER);
                ir_tx_release();
                s_ir_key_routed = false;

				task_keyscan_hid_send_release(key_active_is_consumer ? 1u : 0u);
				if (udp_ota_session_busy() != 0u) {
					cs_schedule();
				}

				if (key_active == TASK_KEYSCAN_KEY15) {
					if (key15_voice_started) {
						key15_voice_started = false;
						(void)voice_ctrl_post_stop();
						key15_rc_notify_defer(0x00u);
						co_printf("[VOICE] KEY15 stop requested\r\n\r\n");
					}
				} else {
					task_keyscan_rc_notify(0x00u);
				}
				CLRLED2;	
				task_keyscan_red_led(false);								// 清除 LED2（蓝） 状态为 OFF
				key_state = KEY_STATE_IDLE;
				key_active = 0;
				key_active_usage = 0;
				key_active_is_consumer = false;
			}
			break;
		}

		default:
			key_state = KEY_STATE_IDLE;
			break;
	}
}
