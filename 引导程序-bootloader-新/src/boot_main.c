/**
 * @file boot_main.c
 * @brief 16KB Bootloader @ 0x01000000：读 OTA 标志 → APP2→APP1 复制 → 写 marker → 跳转 APP1
 */
#include "br1601x.h"
#include "qflash.h"
#include "udp_bootload.h"
#include "flash_layout.h"
#include "co_printf.h"
#include "usart.h"
#include <string.h>

typedef void (*boot_func_t)(void);

/**
 * @brief  跳转 APP 前关中断并关闭 Boot 使用的外设
 */
static void boot_deinit_before_jump(void)
{
	co_log_deinit();
	LPUART_DeInit(LPUART1);
	RCC_EnableLpuartClk(DISABLE);
	__disable_irq();
}

/**
 * @brief  跳转到指定 Flash 地址的 ARM 应用程序
 * @param  address  向量表起始地址（APP1 = FLASH_APP1_START）
 * @note   读取 SP 与 Reset_Handler 后直接跳转，不返回
 */
static void boot_jump_to_app(uint32_t address)
{
	uint32_t sp = *(__IO uint32_t *)address;
	uint32_t pc = *(__IO uint32_t *)(address + 4u);
	boot_func_t fn = (boot_func_t)pc;

	boot_deinit_before_jump();
	__set_MSP(sp);
	fn();
}

/** @brief FreeRTOS 向量占位（Bootloader 不启用 RTOS，避免链接 startup 缺符号） */
void vPortSVCHandler(void);
void xPortPendSVHandler(void);
void SysTick_Handler(void);

void vPortSVCHandler(void) { for (;;) { } }
void xPortPendSVHandler(void) { for (;;) { } }
void SysTick_Handler(void) { for (;;) { } }

/**
 * @brief  Bootloader 入口
 * 流程：
 * 1. 读 BOOTLOAD_PARA，prog_success_flag != 0x12345678 → 直接跳 APP1
 * 2. APP2→APP1 复制（失败保留 flag，下次 Boot 重试）
 * 3. 复制成功后擦 OTA 配置扇区并写完成 marker @ 0x01068100
 * 4. 跳转 APP1
 */
int main(void)
{
	//定义BOOTLOAD_PARA_STRUCT结构体
	BOOTLOAD_PARA_STRUCT para;

	//初始化日志
	co_log_init();
	//打印 bootloader 开始日志
	co_printf("\r\n[BOOT] bootloader start...\r\n");

	//等待50ms，等待电源稳定
	delay_n_10us(5000u);
	//初始化Qflash
	Qflash_Init();

	//读取BOOTLOAD_PARA，如果prog_success_flag != 0x12345678 → 直接跳 APP1
	if (udp_bootload_read_para(&para) != 0
	    || para.prog_success_flag != OTA_UPGRADE_FLAG) {
		co_printf("[BOOT] no upgrade flag, jump APP1\r\n");
		boot_jump_to_app(FLASH_APP1_START);
	}

	//检查参数是否合法，如果不合法直接跳转APP1
	if (para.prog_store_addr != FLASH_APP2_START
	    || para.prog_write_addr != FLASH_APP1_START
	    || para.prog_length == 0u
	    || para.prog_length > FLASH_APP1_SIZE) {
		boot_jump_to_app(FLASH_APP1_START);
	}

	//将APP2备份区复制到APP1，如果复制失败，则跳转APP1
	if (udp_bootload_copy_app2_to_app1(&para) != 0) {
		co_printf("[BOOT] copy fail, flag kept, jump APP1\r\n");
		boot_jump_to_app(FLASH_APP1_START);
	}

	//擦除OTA配置扇区，如果擦除失败，则跳转APP1
	if (Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) {
		co_printf("[BOOT] cfg erase fail, jump APP1\r\n");
		boot_jump_to_app(FLASH_APP1_START);
	}

	//写入完成marker，如果写入失败，则跳转APP1
	if (udp_bootload_write_done_marker() != 0) {
		co_printf("[BOOT] marker fail, jump APP1\r\n");
		boot_jump_to_app(FLASH_APP1_START);
	}

	//打印复制成功日志
	co_printf("[BOOT] copy OK, jump APP1...\r\n");

	//跳转APP1
	boot_jump_to_app(FLASH_APP1_START);

	//返回0
	return 0;
}
