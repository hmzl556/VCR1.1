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
#include "iwdg.h"           /* IWDG 独立看门狗                     */
#include "irc.h"            /* IRC 红外发送驱动                     */
#include "ir_learn.h"       /* 红外学习（TIM3 捕获解码）            */
#include "ir_learn_store.h" /* 学习键值落库                          */
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

/**
 * @brief  红外学习结果回调
 * @param  evt  学习事件
 * @param  user 学习到的 NEC 用户码（DONE 时有效）
 * @param  cmd  学习到的 NEC 数据码（DONE 时有效）
 * @note   学习成功后把结果保存到目标按键的 NV 存储（学习值优先）。
 */
static void ir_learn_result(ir_learn_evt_t evt, uint8_t user, uint8_t cmd)
{
	switch (evt) {
	case IR_LEARN_EVT_DONE:
		co_printf("[IR   ] learn result user=0x%02X cmd=0x%02X\r\n",
		          (unsigned)user, (unsigned)cmd);
		(void)ir_learn_store_save(ir_learn_get_target(), user, cmd);
		break;
	case IR_LEARN_EVT_FAILED:
		co_printf("[IR   ] learn failed\r\n");
		break;
	case IR_LEARN_EVT_TIMEOUT:
		co_printf("[IR   ] learn timeout\r\n");
		break;
	case IR_LEARN_EVT_CANCELED:
		co_printf("[IR   ] learn canceled\r\n");
		break;
	default:
		break;
	}
}

// 主函数，程序入口点
int main(void)
{
	delay_n_10us(50U * 100U);	// 上电后延时 50ms，等待电源稳定
	initialGmem();				// 初始化全局内存与变量
	initialGpio();				// 初始化 GPIO，主要是 LED
	delay_n_10us(2000U * 100U); // 上电后延时 2000ms，预留SWD烧录窗口，之后PA4/PA5切为USART1
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
	irc_init();				// 初始化 IRC 红外发送
	ir_learn_init(ir_learn_result);	// 初始化红外学习（PB5 = TIM3_CH2）
	rc_cat1_init();				// 初始化 Cat.1 模块
	app_rtos_init();			// 初始化 RTOS

	pm_state_init();			// 初始化电源管理状态机
	combo_key_init();			// 初始化组合键管理器

	(void)xTaskCreateStatic(task_ble_entry, "ble", 512, NULL, 3, s_ble_stack, &s_ble_tcb);
	vTaskStartScheduler();		// 启动 FreeRTOS 调度器

	for (;;) {
	}
}
