#include "task_ble.h"
#include "cs_core.h"
#include "gmem.h"
#include "userhard.h"
#include "ble_adv.h"
#include "ble_gap.h"
#include "task_keyscan.h"
#include "app_keyscan.h"    /* app_keyscan_read_key_id — WDT 测试 */
#include "task_voice.h"
#include "app_rtos.h"
#include "rc_cat1_bridge.h"
#include "cat1_skt_udp.h"
#include "co_printf.h"
#include "pm_state.h"       /* 电源管理状态机 — 空闲超时检查 */
#include "iwdg.h"            /* IWDG 独立看门狗 — 喂狗            */
#include "exti.h"            /* EXTI_ClrITPendBit — 清中断挂起位   */
#include "udp_ota.h"
#include "ir_wave_store.h" /* 学习键值加载（须在 BLE 栈/Qflash 就绪后） */
#include "heartbeat.h"      /* 服务器心跳 — 工作/待机循环检查 */
#include "ble_bas.h"
#include "bat_adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pwr.h"            /* PWR_EnterSTANDBYMode */

extern uint8_t myBleStartAdv(void);     // 声明外部函数，启动 BLE 广播并完成相关初始化
extern void setSdkTimer(uint16_t ms);   // 声明外部函数，设置 SDK 定时器，单位为毫秒
extern volatile uint16_t key_receive;

/**
 * @brief  BLE 任务入口函数，负责处理 BLE 相关的事件和状态
 * @param  pvParameters 任务参数
 */
void task_ble_entry(void *pvParameters)
{
	(void)pvParameters;

	for (;;) {
		/*
		 * OTA 会话混合循环（udp_ota_session_busy）：
		 *   - 每轮 burst 内穿插 cs_schedule / 按键 HID（避免 Cat.1 阻塞饿死 BLE）
		 *   - 已连接时不重建 BLE 栈；仅关广播/语音/Tag10 采集
		 *   - 全力轮询 OTA 拉片（rc_cat1_poll_ota_exclusive）
		 * PAUSE 退避期间 session 仍为 busy，本分支持续生效。
		 */
		if (udp_ota_session_busy() != 0u) {
			unsigned ota_iter;

		/* 已连接时禁止 ble_stack_init_only，避免重建栈导致 HID 断链 */
			if (bleAdvStat != 0xa6U) {
				if (ble_gap_get_peer() == NULL) {
					bleAdvStat = ble_stack_init_only();
					pm_state_sync_after_ble_init();
					ir_wave_store_load();
				} else {
					bleAdvStat = 0xa6U;
				}
			}

		/* OTA burst 内穿插 BLE 调度与按键，避免 Cat.1 阻塞导致 HID 无响应 */
			for (ota_iter = 0u; ota_iter < 24u; ota_iter++) {
				cs_schedule();
				ble_gap_process_disconnect_pending();
				task_keyscan_process();
				rc_cat1_poll_ota_exclusive();
				udp_ota_download_led_poll();
				cs_schedule();
				taskYIELD();
			}

			cat1_ble_process_pending();
			udp_ota_boot_indicate_poll();
			IWDG_ReloadKey();
			setSdkTimer(8U);
			taskYIELD();
			continue;
		}

		/*
		 * 电源管理状态机处理（每循环周期调用一次，约 10ms）
		 * 检查以下超时条件并执行状态转换：
		 *   - 工作态空闲超时 → 待机态
		 *   - 待机态空闲超时 → 休眠态
		 *   - 回连态持续超时 → 休眠态
		 * 其他状态（配对/休眠）下本函数为空操作。
		 */
		pm_state_process();

		// 如果 BLE 广播尚未启动，则尝试启动 BLE 广播并设置 SDK 定时器以继续检查状态
		if (bleAdvStat != 0xa6U) {
			bleAdvStat = myBleStartAdv();
			cs_schedule();
			setSdkTimer(8);

			/*
			 * BLE 栈初始化完成后，重新同步 PM 状态机。
			 * 此时 peer_bonded_count() 已有正确的绑定数据（从 Flash 恢复），
			 * 若原先是 PAIRING（默认值）但实际有绑定，修正为 RECONNECTING。
			 */
			pm_state_sync_after_ble_init();
			ir_wave_store_load();
		}

		/* 服务器心跳: 到期则给 CAT1 上电建链报活 (见 heartbeat.c) */
		heartbeat_poll();

	if (pm_state_get() == PM_STATE_STANDBY) {
        IWDG_ReloadKey();
		cs_schedule();
		ble_gap_process_disconnect_pending();
		if (key_receive || app_keyscan_any_pressed())
		{ pm_activity_notify(PM_ACTIVITY_KEY); continue; }

		/* 心跳到期或有待发业务: 切回工作态执行 (待机态不跑 CAT1 poll) */
		if (heartbeat_due() || heartbeat_pending())
		{ pm_activity_notify(PM_ACTIVITY_KEY); continue; }

		/*
		 * 厂商低功耗 demo 的核心: 睡眠前清理 KEYSCAN 残留中断。
		 * 残留的 pending 位会让 br_sleep() 一进去就被唤醒(dt=0ms),
		 * 永远进不了深睡。
		 */
		KEYSCAN_InfoClear();
		KEYSCAN_ClearInterrupt();
		EXTI_ClrITPendBit(EXTI_LINE13);

		br_sleep();

		if (key_receive) { pm_activity_notify(PM_ACTIVITY_KEY); }
		continue;
	}

		// 处理 BLE 相关的事件和状态
		cs_schedule();

		// 处理 BLE GAP 层的断开连接事件
		ble_gap_process_disconnect_pending();

		// 处理 KEYSCAN 相关的事件和状态
		task_keyscan_process();

		// 处理 VOICE 相关的事件和状态，确保语音输入能够被及时捕获和处理
		task_voice_process_once();
        bat_adc_poll(); /* sample only in safe ADC/modem windows; readers use cache */
		/* KEY_15 语音 GATT 与 RC-KEY notify 错开，避免同周期多次 FSM_Write */
		task_keyscan_flush_deferred_voice_rc();

		// BLE/CAT1 串行化：
		// - AT 进行中必须继续 rc_cat1_poll（消费 USART RX / 推进 ECSORF）
		// - 空闲时仍要求距上次 HID/BLE 活动 ≥50ms，再开 CAT1 新业务
		cs_schedule();
		{
			extern volatile uint32_t g_last_ble_ms;
			if (cat1_modem_is_busy() != 0u
                || (rc_cat1_has_work() != 0u && rc_cat1_udp_is_ready() == 0u)
			    || (app_now_ms() - g_last_ble_ms) >= 50U
			    || heartbeat_needs_cat1_poll() != 0u) {
				rc_cat1_poll();
			}
			/* AT 刚结束则立刻补发延后 HID */
			task_keyscan_flush_deferred_hid();
		}

		udp_ota_boot_indicate_poll();

		// 处理 Cat.1 模块事件队列中的事件，确保系统能够及时响应来自 Cat.1 模块的事件和数据
		cat1_ble_process_pending();

		// 更新 BLE GAP 层的 LED 指示状态，提供用户可见的连接状态反馈
		ble_gap_led_indicate();

		/* BAS：CCC 已订阅时每 60s Notify 一次电池电量 */
		ble_bas_poll();

		/* Idempotent retry if the SDK timer table was temporarily full in a callback. */
        setSdkTimer(8U);
		// IWDG 喂狗
		IWDG_ReloadKey();
		taskYIELD();
	}
}
