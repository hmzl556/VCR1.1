#include <string.h>
#include <stdio.h>
#include "cfg_user.h"
#include "br1601x.h"
#include "br_timer.h"
#include "cs_core.h"
#include "co_printf.h"
#include "ble_adv.h"
#include "ble_gap.h"
#include "ble_dtss.h"
#include "gmem.h"
#include "userhard.h"
#include "gpio.h"
#include "app_keyscan.h"
#include "task_ble.h"
#include "ble_hid_kbd.h"
#include "ble_diss.h"
#include "ble_bas.h"
#include "ble_rc_svc.h"
#include "rc_key_input.h"
#include "rc_cat1_bridge.h"
#include "rc_protocol.h"
#include "app_rtos.h"
#include "pm_state.h"       /* 电源管理状态机 — 休眠/待机态控制 */
#include "combo_key.h"      /* 组合键管理器（配对/解配）           */
#include "heartbeat.h"      /* 服务器心跳定时器                     */
#include "iwdg.h"           /* IWDG 独立看门狗                     */
#include "FreeRTOS.h"
#include "task.h"

extern char __heap_base;		// 声明外部符号，表示 C 语言堆内存的起始地址，由链接器提供
extern char __heap_limit;		// 声明外部符号，表示 C 语言堆内存的结束地址，由链接器提供

void host_init(void);
void callBakSdkTimer(uint8_t ev, void *arg);
void setSdkTimer(uint16_t ms);
uint8_t myBleStartAdv(void);

// 全局用户配置结构体，包含扫描使能和低功耗时钟类型
user_cfg user_env = {
	.scan_en    = 0,
	.lsc_type   = LSCI_32000HZ,
};

// 蓝牙初始化完成回调，目前未使用参数，预留接口供未来扩展
void host_init_done(void *hdl, void *context, void *arg, uint16_t len)
{
	(void)hdl;
	(void)context;
	(void)arg;
	(void)len;
}

// 初始化 BLE 栈，设置本地蓝牙地址并完成相关初始化
int ble_stack_init(void)
{
	/*
	 * 长按唤醒时 KEYSCAN ISR 反复触发会打断 co_printf → STDOUT
	 * 的 LPUART TX 死等循环导致活锁。此处临时关断，出口恢复。
	 */
	NVIC_DisableIRQ(KEYSCAN_IRQn);

	local_bd_init_from_chip();	// 从芯片读取本地蓝牙地址并初始化相关变量
	cs_init(local_bd);			// 初始化核心模块，传入本地蓝牙地址以完成相关设置
	host_init();				// 初始化主机相关模块，准备好 BLE 栈的运行环境

	NVIC_EnableIRQ(KEYSCAN_IRQn);	/* 恢复按键中断 */
	return 0;					// 返回 0 表示 BLE 栈初始化成功
}

/* 待机暂停标志: 修复 sdk_timer_pause() 删除与回调重装之间的竞态。
 * 竞态: del 之后 s_sdk_hdl=0 之前, 若已排队的中断回调恰好触发,
 * callBakSdkTimer 开头 setSdkTimer(8) 会重建一个"孤儿"8ms 定时器
 * (句柄随后被清 0, 无人管理), 导致待机态每 8ms 被唤醒 ≈500µA。
 * 修复: pause 先置标志, 在途回调见到标志直接返回, 不重装。 */
static volatile uint8_t s_sdk_timer_paused;

// SDK 定时器回调函数，每 8ms 被调用一次
void callBakSdkTimer(uint8_t ev, void *arg)
{
	(void)ev;
	(void)arg;

	if (s_sdk_timer_paused != 0u) {
		/* 暂停期间的回调: 不重装定时器, 不推进 FreeRTOS tick */
		return;
	}

	setSdkTimer(8U);

	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		(void)xTaskIncrementTick();
	}
}

static uint32_t s_sdk_hdl;

// 设置 SDK 定时器，单位为毫秒
void setSdkTimer(uint16_t ms)
{
	s_sdk_hdl = sdk_timer_add(ms, 2, callBakSdkTimer, NULL);
}

/* 待机态: 删掉 SDK 定时器, br_sleep() 无 pending 定时器可深睡 */
void sdk_timer_pause(void)
{
	s_sdk_timer_paused = 1u;  /* 先置标志: 在途回调不再重装 */
	if (s_sdk_hdl != 0u) { sdk_timer_del(s_sdk_hdl); s_sdk_hdl = 0u; }
}

/* 待机态唤醒: 重建 SDK 定时器 */
void sdk_timer_resume(void)
{
	if (s_sdk_hdl == 0u) { s_sdk_hdl = sdk_timer_add(8U, 2, callBakSdkTimer, NULL); }
	s_sdk_timer_paused = 0u;  /* 后清标志: 重建完成后再允许重装 */
}

// 初始化 BLE 栈与 GATT/GAP（不启动广播），供 OTA 会话并行 HID 使用
uint8_t ble_stack_init_only(void)
{
	ble_stack_init();			// 初始化 BLE 栈（host_init 初始化 GATT 层）

	// GATT 服务必须在 ble_gap_init() 之前创建，使 peer_recover()
	// 恢复绑定后协议栈能追踪 GATT 变更，向 STB 发送 Service Changed
	ble_device_info_init();		// 初始化设备信息服务
	hid_kbd_init();				// 初始化 HID 键盘服务
	ble_dtss_init();			// 初始化 DTSS 服务
	ble_rc_svc_init();			// 初始化 RC 服务
	ble_bas_init();				// 初始化 BAS 电池服务

	ble_gap_init();				// GAP 初始化 + peer_recover 绑定恢复
	ble_adv_init();				// 初始化 BLE 广播（不调用 start，避免干扰 OTA）
	rc_key_input_init();		// 初始化遥控按键输入模块
	return 0xa6U;				// 返回状态码 0xa6U 表示 BLE 栈已成功初始化
}

// 启动 BLE 广播并完成相关初始化，返回状态码 0xa6U 表示成功
uint8_t myBleStartAdv(void)
{
	(void)ble_stack_init_only();// 初始化 BLE 栈与 GATT/GAP（不启动广播），供 OTA 会话并行 HID 使用
	ble_adv_start_by_bonding();	// 根据绑定状态选择广播模式
	return 0xa6U;				// 返回状态码 0xa6U 表示 BLE 广播已成功启动
}

static StaticTask_t s_ble_tcb;			// BLE 任务控制块
static StackType_t s_ble_stack[512];	// 512 字节的任务栈空间，根据实际需求调整大小

// 主函数，程序入口点
int main(void)
{
	delay_n_10us(50U * 100U);	// 上电后延时 50ms，等待电源稳定
	initialGmem();				// 初始化全局内存与变量
	initialGpio();				// 初始化 GPIO，主要是 LED
	delay_n_10us(1000U * 100U); // 上电后延时 1000ms，同步时间需要CAT1稳定下来
	co_log_init();				// 初始化日志模块，后续可使用 co_printf 输出日志

	/*
	 * 初始化 IWDG 独立看门狗：LSI 32KHz / 256 = 125Hz, 1250 ticks = 10 秒超时。
	 * 休眠期间 LSI 停止，IWDG 随之停止，不会误触发。
	 */
	RCC->CTRLSTS |= RCC_CTRLSTS_RMRSTF;        /* 清除残留复位标志          */
	IWDG_WriteConfig(IWDG_WRITE_ENABLE);
	IWDG_SetPrescalerDiv(IWDG_PRESCALER_DIV256);
	IWDG_CntReload(1250U);                     /* 10 秒超时                 */
	IWDG_ReloadKey();
	IWDG_Enable();

	co_printf("\r\n\r\nInitialization complete, starting FreeRTOS...\r\n");
	co_printf("[BR REMOTE] SW version: %s\r\n", RC_READ_VAL_DEVICE_SW_VERSION);
	co_printf("C heap %u bytes\r\n", (unsigned)((char *)&__heap_limit - (char *)&__heap_base));

	ble_keyscan_init();			// 初始化按键扫描模块
	rc_cat1_init();				// 初始化 Cat.1 模块
	app_rtos_init();			// 初始化 RTOS

	pm_state_init();			// 初始化电源管理状态机
	combo_key_init();			// 初始化组合键管理器
	heartbeat_init();			// 初始化服务器心跳定时器

	(void)xTaskCreateStatic(task_ble_entry, "ble", 512, NULL, 3, s_ble_stack, &s_ble_tcb);
	vTaskStartScheduler();		// 启动 FreeRTOS 调度器

	for (;;) {
	}
}
