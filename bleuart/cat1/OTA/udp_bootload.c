/**
 * @file udp_bootload.c
 * @brief OTA 备份区 Flash 擦写与 BOOTLOAD_PARA 管理实现
 */
#include "udp_bootload.h"
#include "qflash.h"
#include <string.h>

/** Flash 单次关中断写入粒度；片间 pump 以接收异步 +SKTRECV */
#define UDP_FLASH_SUBWRITE  128u

/**
 * @brief  Flash 擦写间隙泵 UART（仅 APP OTA 需要）
 * @note   Boot 不链接 cat1_skt_udp.c，weak 空实现由链接器保留；
 *         APP 中 cat1_skt_udp.c 提供强符号覆盖本桩。
 */
__weak void cat1_skt_udp_pump_urc(void)
{
}

/** @brief 唯一 Flash 写缓冲（512B，与协议片长一致） */
static uint8_t s_flash_wr_buf[OTA_CHUNK_SIZE];
static uint32_t s_staged_addr;
static uint16_t s_staged_len;
static uint8_t  s_staged_valid;

/**
 * @brief  检查地址区间是否完全落在 APP2 备份区内
 * @return 0 合法；-1 非法
 */
static int udp_bootload_addr_in_app2(uint32_t addr, uint32_t len)
{
	uint32_t end;

	if (len == 0u) {
		return 0;
	}
	end = addr + len - 1u;
	if (addr < FLASH_APP2_START || end >= (FLASH_APP2_START + FLASH_APP2_SIZE)) {
		return -1;
	}
	return 0;
}

/**
 * @brief  按扇区步进擦除指定长度范围
 * @return 0 成功；-1 任一扇区擦除失败
 */
static int udp_bootload_erase_range(uint32_t start, uint32_t len)
{
	uint32_t addr;

	for (addr = start; addr < (start + len); addr += FLASH_SECTOR_SIZE) {
		if (Qflash_Erase_Sector(addr) != FlashOperationSuccess) {
			return -1;
		}
		cat1_skt_udp_pump_urc();
	}
	return 0;
}

/**
 * @brief  初始化 Qflash 驱动
 */
void udp_bootload_init(void)
{
	Qflash_Init();
}

/**
 * @brief  校验参数并按 prog_length 预擦除 APP2 备份区
 * @param  para  升级参数
 * @return 0 成功；-1 失败
 */
int udp_bootload_prepare_backup(const BOOTLOAD_PARA_STRUCT *para)
{
	if (para == NULL) {
		return -1;
	}
	if (para->prog_store_addr != FLASH_APP2_START
		|| para->prog_write_addr != FLASH_APP1_START) {
		return -1;
	}
	if (para->prog_length == 0u || para->prog_length > FLASH_APP2_SIZE) {
		return -1;
	}
	return udp_bootload_erase_range(para->prog_store_addr, para->prog_length);
}

/**
 * @brief  将 s_flash_wr_buf 写入 Flash（分段 + pump）
 */
static int udp_bootload_write_buf(uint32_t abs_addr, uint16_t len)
{
	uint32_t offset;

	for (offset = 0u; offset < (uint32_t)len; offset += UDP_FLASH_SUBWRITE) {
		uint32_t sub = (uint32_t)len - offset;

		if (sub > UDP_FLASH_SUBWRITE) {
			sub = UDP_FLASH_SUBWRITE;
		}
		if (Qflash_Write(abs_addr + offset, s_flash_wr_buf + offset, sub)
		    != FlashOperationSuccess) {
			return -1;
		}
		cat1_skt_udp_pump_urc();
	}
	return 0;
}

int udp_bootload_stage_chunk(uint32_t abs_addr, const uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0u || len > OTA_CHUNK_SIZE) {
		return -1;
	}
	if (udp_bootload_addr_in_app2(abs_addr, len) != 0) {
		return -1;
	}

	memcpy(s_flash_wr_buf, data, len);
	if (len < OTA_CHUNK_SIZE) {
		memset(s_flash_wr_buf + len, 0xFF, (size_t)(OTA_CHUNK_SIZE - len));
	}
	s_staged_addr = abs_addr;
	s_staged_len = len;
	s_staged_valid = 1u;
	return 0;
}

int udp_bootload_commit_staged(void)
{
	int rc;

	if (s_staged_valid == 0u) {
		return -1;
	}
	rc = udp_bootload_write_buf(s_staged_addr, s_staged_len);
	s_staged_valid = 0u;
	return rc;
}

uint32_t udp_bootload_staged_addr(void)
{
	return s_staged_addr;
}

/**
 * @brief  写一片固件到 APP2 绝对地址
 * @param  abs_addr  本片起始地址
 * @param  data      代码数据
 * @param  len       长度（≤ OTA_CHUNK_SIZE）
 * @return 0 成功；-1 失败
 */
int udp_bootload_write_chunk(uint32_t abs_addr, const uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0u || len > OTA_CHUNK_SIZE) {
		return -1;
	}
	if (udp_bootload_addr_in_app2(abs_addr, len) != 0) {
		return -1;
	}

	memcpy(s_flash_wr_buf, data, len);
	if (len < OTA_CHUNK_SIZE) {
		memset(s_flash_wr_buf + len, 0xFF, (size_t)(OTA_CHUNK_SIZE - len));
	}

	return udp_bootload_write_buf(abs_addr, len);
}

/**
 * @brief  步骤 9：写 BOOTLOAD_PARA 并置 prog_success_flag=0x12345678
 * @param  para  升级参数
 * @return 0 成功；-1 失败
 */
int udp_bootload_write_cfg(const BOOTLOAD_PARA_STRUCT *para)
{
	BOOTLOAD_PARA_STRUCT cfg;

	if (para == NULL) {
		return -1;
	}

	memcpy(&cfg, para, sizeof(cfg));
	cfg.prog_success_flag = OTA_UPGRADE_FLAG;

	if (Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) {
		return -1;
	}
	if (Qflash_Write(FLASH_OTA_CFG_START + FLASH_OTA_PARA_OFFSET,
	                 (uint8_t *)&cfg, (uint32_t)sizeof(cfg)) != FlashOperationSuccess) {
		return -1;
	}
	return 0;
}

/**
 * @brief  从 Flash 读取 BOOTLOAD_PARA
 * @param  para  出参
 * @return 0 成功；-1 失败
 */
int udp_bootload_read_para(BOOTLOAD_PARA_STRUCT *para)
{
	if (para == NULL) {
		return -1;
	}
	if (Qflash_Read(FLASH_OTA_CFG_START + FLASH_OTA_PARA_OFFSET,
	                (uint8_t *)para, (uint32_t)sizeof(*para)) != FlashOperationSuccess) {
		return -1;
	}
	return 0;
}

/**
 * @brief  Bootloader：APP2→APP1 逐 512B 复制
 * @param  para  升级参数
 * @return 0 成功；-1 失败（最多 10 次重试）
 */
int udp_bootload_copy_app2_to_app1(const BOOTLOAD_PARA_STRUCT *para)
{
	uint32_t offset;
	uint32_t retry;

	if (para == NULL || para->prog_length == 0u) {
		return -1;
	}
	if (para->prog_store_addr != FLASH_APP2_START
		|| para->prog_write_addr != FLASH_APP1_START) {
		return -1;
	}
	if (para->prog_length > FLASH_APP1_SIZE || para->prog_length > FLASH_APP2_SIZE) {
		return -1;
	}

	for (retry = 0u; retry < 10u; retry++) {
		if (udp_bootload_erase_range(FLASH_APP1_START, para->prog_length) != 0) {
			continue;
		}

		for (offset = 0u; offset < para->prog_length; offset += OTA_CHUNK_SIZE) {
			uint32_t chunk = para->prog_length - offset;
			if (chunk > OTA_CHUNK_SIZE) {
				chunk = OTA_CHUNK_SIZE;
			}
			if (Qflash_Read(para->prog_store_addr + offset, s_flash_wr_buf, chunk)
			    != FlashOperationSuccess) {
				goto copy_retry;
			}
			if (Qflash_Write(para->prog_write_addr + offset, s_flash_wr_buf, chunk)
			    != FlashOperationSuccess) {
				goto copy_retry;
			}
		}
		return 0;

copy_retry:
		;
	}
	return -1;
}

/**
 * @brief  写 OTA 完成 marker @ FLASH_OTA_DONE_MARKER_ADDR
 * @return 0 成功；-1 失败
 */
int udp_bootload_write_done_marker(void)
{
	uint32_t marker = OTA_BOOT_DONE_MARKER;

	if (Qflash_Write(FLASH_OTA_DONE_MARKER_ADDR,
	                 (uint8_t *)&marker, (uint32_t)sizeof(marker)) != FlashOperationSuccess) {
		return -1;
	}
	return 0;
}

/**
 * @brief  读完成 marker 是否为 OTA_BOOT_DONE_MARKER
 * @return 1 有效；0 无效
 */
uint8_t udp_bootload_is_done_marker_set(void)
{
	uint32_t marker = 0u;

	(void)Qflash_Read(FLASH_OTA_DONE_MARKER_ADDR,
	                  (uint8_t *)&marker, (uint32_t)sizeof(marker));
	return (uint8_t)(marker == OTA_BOOT_DONE_MARKER);
}

/**
 * @brief  清除完成 marker（写 0）
 */
void udp_bootload_clear_done_marker(void)
{
	uint32_t marker = 0u;

	(void)Qflash_Write(FLASH_OTA_DONE_MARKER_ADDR,
	                   (uint8_t *)&marker, (uint32_t)sizeof(marker));
}

int udp_bootload_write_checkpoint(const OTA_CHECKPOINT_STRUCT *cp)
{
	OTA_CHECKPOINT_STRUCT out;

	if (cp == NULL || cp->version == 0u || cp->total_packets == 0u) {
		return -1;
	}
	if (cp->written_seq >= cp->total_packets) {
		return -1;
	}
	if (cp->written_seq > 0u
	    && (cp->prog_length == 0u || cp->prog_length > FLASH_APP2_SIZE)) {
		return -1;
	}

	memcpy(&out, cp, sizeof(out));
	out.magic = OTA_CHECKPOINT_MAGIC;
	if (Qflash_Write(FLASH_OTA_CHECKPOINT_ADDR,
	                 (uint8_t *)&out, (uint32_t)sizeof(out)) != FlashOperationSuccess) {
		return -1;
	}
	return 0;
}

uint8_t udp_bootload_read_checkpoint(OTA_CHECKPOINT_STRUCT *cp)
{
	OTA_CHECKPOINT_STRUCT in;

	if (cp == NULL) {
		return 0u;
	}
	if (Qflash_Read(FLASH_OTA_CHECKPOINT_ADDR,
	                (uint8_t *)&in, (uint32_t)sizeof(in)) != FlashOperationSuccess) {
		return 0u;
	}
	if (in.magic != OTA_CHECKPOINT_MAGIC) {
		return 0u;
	}
	if (in.version == 0u || in.total_packets == 0u) {
		return 0u;
	}
	if (in.written_seq >= in.total_packets) {
		return 0u;
	}
	if (in.written_seq > 0u
	    && (in.prog_length == 0u || in.prog_length > FLASH_APP2_SIZE)) {
		return 0u;
	}
	memcpy(cp, &in, sizeof(*cp));
	return 1u;
}

void udp_bootload_clear_checkpoint(void)
{
	OTA_CHECKPOINT_STRUCT zero;

	memset(&zero, 0, sizeof(zero));
	(void)Qflash_Write(FLASH_OTA_CHECKPOINT_ADDR,
	                   (uint8_t *)&zero, (uint32_t)sizeof(zero));
}

int udp_bootload_run_bootloader(void)
{
	BOOTLOAD_PARA_STRUCT para;

	if (udp_bootload_read_para(&para) != 0) {
		return -1;
	}
	if (para.prog_success_flag != OTA_UPGRADE_FLAG) {
		return -1;
	}
	if (Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) {
		return -2;
	}
	if (udp_bootload_copy_app2_to_app1(&para) != 0) {
		return -3;
	}
	if (udp_bootload_write_done_marker() != 0) {
		return -4;
	}
	return 0;
}
