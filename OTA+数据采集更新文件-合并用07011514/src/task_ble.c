#include "task_ble.h"
#include "cs_core.h"
#include "gmem.h"
#include "userhard.h"
#include "ble_adv.h"
#include "ble_gap.h"
#include "task_keyscan.h"
#include "task_voice.h"
#include "app_rtos.h"
#include "rc_cat1_bridge.h"
#include "udp_ota.h"
#include "co_printf.h"
#include "FreeRTOS.h"
#include "task.h"

extern uint8_t myBleStartAdv(void);     // 声明外部函数，启动 BLE 广播并完成相关初始化
extern void setSdkTimer(uint16_t ms);   // 声明外部函数，设置 SDK 定时器，单位为毫秒

/**
 * @brief  BLE 任务入口函数，负责处理 BLE 相关的事件和状态
 * @param  pvParameters 任务参数
 */
void task_ble_entry(void *pvParameters)
{
	uint8_t runLedStat = 0;				// 记录 LED 状态，初始值为 0 表示 LED 关闭

	(void)pvParameters;

	// 主循环，持续处理 BLE 相关的事件和状态，同时更新 LED 状态和定时器
	//static uint16_t loop_diag_cnt = 0;

	for (;;) {
		if (udp_ota_is_active() != 0u) {
			unsigned ota_iter;

			for (ota_iter = 0u; ota_iter < 24u; ota_iter++) {
				rc_cat1_poll_ota_exclusive();
				udp_ota_download_led_poll();
				taskYIELD();
			}
			setSdkTimer(8U);
			taskYIELD();
			continue;
		}

		//loop_diag_cnt++;
		// if ((loop_diag_cnt % 500) == 0) {
		// 	co_printf("[LOOP] iter=%u uptime=%u\r\n", loop_diag_cnt, voiceUptimeMs);
		// }

		// 如果 BLE 广播尚未启动，则尝试启动 BLE 广播并设置 SDK 定时器以继续检查状态
		if (bleAdvStat != 0xa6U) {
			bleAdvStat = myBleStartAdv();
			cs_schedule();
			setSdkTimer(8);
			//co_printf("BLE stack ready, advertising started.\r\n");
		}

		// 处理 BLE 相关的事件和状态
		//if ((loop_diag_cnt % 500) == 0) co_printf("[LOOP] pre cs_sched\r\n");
		cs_schedule();
		//if ((loop_diag_cnt % 500) == 0) co_printf("[LOOP] post cs_sched\r\n");

		// 处理 BLE GAP 层的断开连接事件
		ble_gap_process_disconnect_pending();

		// 处理 KEYSCAN 相关的事件和状态
		//if ((loop_diag_cnt % 500) == 0) co_printf("[LOOP] pre keyscan\r\n");
		task_keyscan_process();
		//if ((loop_diag_cnt % 500) == 0) co_printf("[LOOP] post keyscan\r\n");
			

		// 处理 VOICE 相关的事件和状态，确保语音输入能够被及时捕获和处理
		task_voice_process_once();

		// Cat.1 轮询放后：UDP AT 可能阻塞数秒，不应挡住按键/LED
		rc_cat1_poll();

		udp_ota_boot_indicate_poll();

		// 处理 Cat.1 模块事件队列中的事件，确保系统能够及时响应来自 Cat.1 模块的事件和数据
		cat1_ble_process_pending();

		// 更新 BLE GAP 层的 LED 指示状态，提供用户可见的连接状态反馈
		ble_gap_led_indicate();

		// 让出 CPU，允许其他任务执行，确保系统的响应性和多任务协作
		taskYIELD();

	}
}
