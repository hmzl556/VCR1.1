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
static struct {
	uint8_t press_pending;
	uint16_t press_key;
	uint8_t release_pending;
	uint8_t release_consumer;
} s_hid_at_defer;

static void task_keyscan_hid_flush_deferred(void)
{
	if (cat1_modem_is_busy() != 0u) {
		return;
	}
	if (s_hid_at_defer.press_pending != 0u) {
		uint16_t key_id = s_hid_at_defer.press_key;

		s_hid_at_defer.press_pending = 0u;
		s_hid_at_defer.press_key = 0u;
		if (ble_gap_get_peer() != NULL && key_id != 0u) {
			co_printf("\r\nKeyboard press KEY_%d, send via HID (deferred).\r\n",
			          (unsigned)key_id);
			hid_kbd_send_key_id_current(key_id);
		}
	}
	if (s_hid_at_defer.release_pending != 0u) {
		uint8_t is_consumer = s_hid_at_defer.release_consumer;

		s_hid_at_defer.release_pending = 0u;
		s_hid_at_defer.release_consumer = 0u;
		if (ble_gap_get_peer() != NULL) {
			if (is_consumer != 0u) {
				hid_kbd_send_consumer_release_current();
			} else {
				hid_kbd_send_release_current();
			}
		}
	}
}

static void task_keyscan_hid_send_press(uint16_t key_id)
{
	if (ble_gap_get_peer() == NULL || key_id == 0u) {
		return;
	}
	if (cat1_modem_is_busy() != 0u) {
		s_hid_at_defer.press_pending = 1u;
		s_hid_at_defer.press_key = key_id;
		/* 新按下作废尚未发出的释放 */
		s_hid_at_defer.release_pending = 0u;
		return;
	}
	co_printf("\r\nKeyboard press KEY_%d, send via HID.\r\n", (unsigned)key_id);
	hid_kbd_send_key_id_current(key_id);
}

static void task_keyscan_hid_send_release(uint8_t is_consumer)
{
	if (ble_gap_get_peer() == NULL) {
		return;
	}
	if (cat1_modem_is_busy() != 0u) {
		s_hid_at_defer.release_pending = 1u;
		s_hid_at_defer.release_consumer = is_consumer;
		return;
	}
	if (is_consumer != 0u) {
		hid_kbd_send_consumer_release_current();
	} else {
		hid_kbd_send_release_current();
	}
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
                CLRLED1;
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
* @brief 重置按键扫描模块，当 BLE 断开连接时调用
*/
void task_keyscan_reset_on_ble_disconnect(void)
{
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
	s_hid_at_defer.press_pending = 0u;
	s_hid_at_defer.press_key = 0u;
	s_hid_at_defer.release_pending = 0u;
	s_hid_at_defer.release_consumer = 0u;
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

	if (key_active == TASK_KEYSCAN_KEY15) {
		key_type = 0x03u;
	} else {
		key_type = key_active_is_consumer ? 0x02u : 0x01u;
	}
	rc_key_input_event(key_type, action, key_active_usage, key_active);
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
	if (key_id != 0) {
		key_last_event_ms = now_ms;
	}



	// ====== 硬件按键快照（每周期一次，所有组合键共享） ======
	uint32_t hw_snapshot[5];
	KEYSCAN_ReadKeyData(hw_snapshot);


	// ====== 组合键互斥调度 ======
	// 工厂复位空闲时，简单组合键（配对/解配）才工作
	if (factory_rst_state == FACTORY_RST_IDLE) {
		combo_key_process(now_ms, hw_snapshot);
	}
	// 简单组合键空闲时，工厂复位才工作
	if (combo_key_is_idle()) {
		factory_reset_process(now_ms, key_id, hw_snapshot);
	}

	// ====== 任一组合键活跃时，跳过正常按键处理 ======
    if (factory_rst_state != FACTORY_RST_IDLE || !combo_key_is_idle())
    {
        // 重置正常按键状态机，避免残留状态干扰；同时清除 LED 防止常亮
        if (key_state != KEY_STATE_IDLE)
        {
            key_state = KEY_STATE_IDLE;
            key_active = 0;
            key_active_is_consumer = false;
            CLRLED2;
            CLRLED1;
        }
        return;
    }

	switch (key_state) {
		case KEY_STATE_IDLE:
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
				key_active = key_candidate;
				key_active_is_consumer = false;
				key_active_usage = 0;
				{
					uint16_t usage_tmp = 0;
					bool is_consumer_tmp = false;
					if (hid_kbd_map_key_to_usage(key_active, &usage_tmp, &is_consumer_tmp)) {
						key_active_is_consumer = is_consumer_tmp;
						key_active_usage = usage_tmp;
					}
				}

				// 短按按下即发：确认稳定按下后立刻发送
				key_state = KEY_STATE_PRESSED;

				// 电源键 + 回连态：切换为唤醒广播，通过广播中的 STB MAC 地址唤醒机顶盒
				if (key_active == TASK_KEYSCAN_KEY1
				    && pm_state_get() == PM_STATE_RECONNECTING) {
					co_printf("KEY1 pressed in RECONNECTING, switch to wakeup broadcast\r\n");
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
				if (ble_gap_get_peer() != NULL) {
					SETLED2;
				} else {
					SETLED1;
				}

				if (key_active != 0) {
					task_keyscan_hid_send_press(key_active);
					if (udp_ota_session_busy() != 0u) {
						cs_schedule();
					}
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

			// 中途按键值变化，重新回到消抖状态
			if (hw_key == key_active) {
				key_last_event_ms = now_ms;
			} else if (hw_key != 0 && hw_key != key_active) {
				key_id = hw_key;
			}

			// 长按判定
			if (key_id != 0 && key_id != key_active) {
				key_candidate = key_id;
				key_state_ts_ms = now_ms;
				key_state = KEY_STATE_DEBOUNCE;
				CLRLED2;
				CLRLED1;
				break;
			}

			// 长按判定：仅触发一次
			if (!key_long_triggered && (now_ms - key_press_start_ms) >= KEY_LONG_PRESS_MS) {
				key_long_triggered = true;
				/* OTA 会话期间禁止 KEY_15 长按启动语音采集 */
				if (key_active == TASK_KEYSCAN_KEY15) {
					if (udp_ota_session_busy() == 0u && !key15_voice_started) {
						key15_voice_started = true;
						(void)voice_ctrl_post_start();
						key15_rc_notify_defer(0x01u);
						co_printf("Voice capture started by KEY_15 (press)\r\n");
					}
				}
			}

			if (hw_key == 0 && (now_ms - key_last_event_ms) >= KEY_RELEASE_TIMEOUT_MS) {
				task_keyscan_hid_send_release(key_active_is_consumer ? 1u : 0u);
				if (udp_ota_session_busy() != 0u) {
					cs_schedule();
				}

				if (key_active == TASK_KEYSCAN_KEY15) {
					if (key15_voice_started) {
						key15_voice_started = false;
						(void)voice_ctrl_post_stop();
						key15_rc_notify_defer(0x00u);
						co_printf("Voice capture stopped by KEY_15 (release)\r\n\r\n");
					}
				} else {
					task_keyscan_rc_notify(0x00u);
				}
				CLRLED2;	
				CLRLED1;								// 清除 LED2（蓝） 状态为 OFF
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
