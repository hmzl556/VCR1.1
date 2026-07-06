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
#include "ble_rc_svc.h"
#include "rc_key_input.h"
#include "rc_cat1_bridge.h"
#include "rc_protocol.h"
#include "app_rtos.h"
#include "pm_state.h"       /* 电源管理状态机 — 休眠/待机态控制 */
#include "combo_key.h"      /* 组合键管理器（配对/解配）           */
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
	local_bd_init_from_chip();	// 从芯片读取本地蓝牙地址并初始化相关变量
	cs_init(local_bd);			// 初始化核心模块，传入本地蓝牙地址以完成相关设置

	/*
	 * 深度睡眠唤醒后 Flash 需要额外建立时间才能稳定读取。
	 * host_init() 会通过 BtInitStack 读取 NVDS 配置项，
	 * 此处增加延时确保 Flash 模拟电路完全稳定。
	 * delay_n_10us() 是纯指令循环，不依赖任何外设/调度器，
	 * 100 个单位 = 1ms，相比整体启动时间 (~2500ms) 可忽略。
	 */
	delay_n_10us(100U);			/* 100 × 10μs = 1ms Flash 稳定延时 */

	host_init();				// 初始化主机相关模块，准备好 BLE 栈的运行环境
	return 0;					// 返回 0 表示 BLE 栈初始化成功
}

// SDK 定时器回调函数，每 8ms 被调用一次
void callBakSdkTimer(uint8_t ev, void *arg)
{
	(void)ev;
	(void)arg;

	setSdkTimer(8U);			// 重新设置定时器，确保每 8ms 调用一次此回调函数

	// 如果 FreeRTOS 调度器正在运行，调用 xTaskIncrementTick 来更新系统时间和触发任务切换
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		(void)xTaskIncrementTick();
	}
}

// 设置 SDK 定时器，单位为毫秒
void setSdkTimer(uint16_t ms)
{
	// 定时器接口，设置一个定时器在 ms 毫秒后触发，触发时调用 callBakSdkTimer 函数
	sdk_timer_add(ms, 2, callBakSdkTimer, NULL);
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
	delay_n_10us(50U * 1000U);	// 上电后延时 50ms，等待电源稳定
	initialGmem();				// 初始化全局内存与变量
	initialGpio();				// 初始化 GPIO，主要是 LED
	co_log_init();				// 初始化日志模块，后续可使用 co_printf 输出日志
	ble_keyscan_init();			// 初始化按键扫描模块

	// 注意：BLE 栈初始化较慢，必须在启动 FreeRTOS 之前完成
	co_printf("\r\n\r\nInitialization complete, starting FreeRTOS...\r\n");
	
	// 输出固件的软件版本号
	co_printf("[BOOT ] SW version: %s\r\n", RC_READ_VAL_DEVICE_SW_VERSION);

	// 输出 C 语言堆内存大小，供调试参考
	co_printf("C heap %u bytes\r\n", (unsigned)((char *)&__heap_limit - (char *)&__heap_base));

	rc_cat1_init();				// 初始化 Cat.1 模块

	app_rtos_init();			// 初始化 RTOS 相关资源，如信号量、消息队列等

	/*
	 * 初始化电源管理状态机（PMU）
	 * - 根据绑定记录数量自动决定初始状态：有绑定→回连态，无绑定→配对态
	 * - 必须在 app_rtos_init() 之后（后续休眠态可能使用 RTOS 资源）
	 * - 必须在 BLE 任务启动之前，确保任务内首次 pm_state_process() 调用安全
	 */

	pm_state_init();			// 初始化电源管理状态机
	
	combo_key_init();			// 初始化组合键管理器
	
	(void)xTaskCreateStatic(task_ble_entry, "ble", 512, NULL, 3, s_ble_stack, &s_ble_tcb);

	vTaskStartScheduler();		// 启动 FreeRTOS 调度器，开始执行任务
	
	for (;;) {					// 理论上不应该执行到这里
	}
}
