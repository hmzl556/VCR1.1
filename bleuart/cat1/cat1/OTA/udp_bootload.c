/**
 * @file udp_bootload.c
 * @brief OTA 备份区 Flash 擦写与 BOOTLOAD_PARA 管理实现
 */
#include "udp_bootload.h"
#include "qflash.h"
#include "iwdg.h"
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
	if (addr < FLASH_APP2_START || end < addr || end >= (FLASH_APP2_START + FLASH_APP2_SIZE)) {
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
		IWDG_ReloadKey(); /* Feed only after a sector made forward progress. */
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
    uint8_t check[UDP_FLASH_SUBWRITE];

	for (offset = 0u; offset < (uint32_t)len; offset += UDP_FLASH_SUBWRITE) {
		uint32_t sub = (uint32_t)len - offset;

		if (sub > UDP_FLASH_SUBWRITE) {
			sub = UDP_FLASH_SUBWRITE;
		}
		if (Qflash_Write(abs_addr + offset, s_flash_wr_buf + offset, sub)
		    != FlashOperationSuccess) {
			return -1;
		}
        if (Qflash_Read(abs_addr + offset, check, sub) != FlashOperationSuccess ||
            memcmp(check, s_flash_wr_buf + offset, sub)) return -1;
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
	BOOTLOAD_PARA_STRUCT cfg, check;

	if (para == NULL || para->prog_store_addr != FLASH_APP2_START ||
        para->prog_write_addr != FLASH_APP1_START || !para->prog_length ||
        para->prog_length > FLASH_APP1_SIZE || para->prog_length > FLASH_APP2_SIZE) {
		return -1;
	}

	memcpy(&cfg, para, sizeof(cfg));
	cfg.prog_success_flag = 0xffffffffu;

	if (Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) {
		return -1;
	}
	if (Qflash_Write(FLASH_OTA_CFG_START + FLASH_OTA_PARA_OFFSET,
	                 (uint8_t *)&cfg, (uint32_t)sizeof(cfg)) != FlashOperationSuccess) {
		return -1;
	}
    if (udp_bootload_read_para(&check) != 0 || memcmp(&cfg, &check, sizeof(cfg))) return -1;
    cfg.prog_success_flag = OTA_UPGRADE_FLAG;
    if (Qflash_Write(FLASH_OTA_CFG_START + 16u, (uint8_t *)&cfg.prog_success_flag, 4u) != FlashOperationSuccess ||
        udp_bootload_read_para(&check) != 0 || memcmp(&cfg, &check, sizeof(cfg))) return -1;
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

/* Append-only 32-byte records inside the existing OTA configuration sector.
 * Each record has a CRC and a last-written commit word. NOR writes only clear
 * bits: overwriting written_seq in place cannot persist successive checkpoints.
 * The bootloader parameter (+0) and completion marker (+0x100) stay unchanged.
 */
#define CP_COMMIT 0x43504a31u
#define CP_RECORD_BYTES 32u
#define CP_RECORD_COUNT ((FLASH_OTA_CFG_SIZE - FLASH_OTA_CHECKPOINT_OFFSET) / CP_RECORD_BYTES)
typedef struct {
    OTA_CHECKPOINT_STRUCT cp;
    uint32_t crc;
    uint32_t reserved[2];
    uint32_t commit;
} checkpoint_record_t;
typedef char cp_size_check[(sizeof(checkpoint_record_t) == CP_RECORD_BYTES) ? 1 : -1];

static uint32_t checkpoint_crc(const OTA_CHECKPOINT_STRUCT *cp)
{
    const uint8_t *p = (const uint8_t *)cp;
    uint32_t crc = 0xffffffffu;
    unsigned i, bit;
    for (i = 0u; i < sizeof(*cp); ++i) {
        crc ^= p[i];
        for (bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return ~crc;
}

static int checkpoint_fields_valid(const OTA_CHECKPOINT_STRUCT *cp)
{
    if (!cp || cp->version == 0u || cp->total_packets == 0u ||
        cp->total_packets > FLASH_APP2_SIZE / OTA_CHUNK_SIZE ||
        cp->written_seq >= cp->total_packets) return 0;
    if (cp->written_seq > 0u &&
        (!cp->prog_length || cp->prog_length > FLASH_APP2_SIZE ||
         (cp->prog_length + OTA_CHUNK_SIZE - 1u) / OTA_CHUNK_SIZE != cp->total_packets)) return 0;
    return 1;
}

static int checkpoint_cfg_erasable(void)
{
    BOOTLOAD_PARA_STRUCT para;
    if (udp_bootload_read_para(&para) != 0) return 0;
    /* Never erase a committed image that the bootloader must install. */
    return para.prog_success_flag != OTA_UPGRADE_FLAG;
}

int udp_bootload_write_checkpoint(const OTA_CHECKPOINT_STRUCT *cp)
{
    checkpoint_record_t rec, check;
    uint32_t i, j, addr = 0u;
    if (!checkpoint_fields_valid(cp)) return -1;
    for (i = 0u; i < CP_RECORD_COUNT; ++i) {
        uint32_t candidate = FLASH_OTA_CHECKPOINT_ADDR + i * CP_RECORD_BYTES;
        const uint8_t *bytes = (const uint8_t *)&check;
        if (Qflash_Read(candidate, (uint8_t *)&check, sizeof(check)) != FlashOperationSuccess) return -1;
        for (j = 0u; j < sizeof(check) && bytes[j] == 0xffu; ++j) {}
        if (j == sizeof(check)) { addr = candidate; break; }
    }
    if (!addr) {
        /* A rollover can lose resume progress on power failure, but cannot
         * commit an incomplete image: the next download then starts fresh. */
        if (!checkpoint_cfg_erasable() || Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) return -1;
        addr = FLASH_OTA_CHECKPOINT_ADDR;
    }
    memset(&rec, 0xff, sizeof(rec));
    rec.cp = *cp;
    rec.cp.magic = OTA_CHECKPOINT_MAGIC;
    rec.crc = checkpoint_crc(&rec.cp);
    if (Qflash_Write(addr, (uint8_t *)&rec, sizeof(rec) - 4u) != FlashOperationSuccess) return -1;
    if (Qflash_Read(addr, (uint8_t *)&check, sizeof(check)) != FlashOperationSuccess ||
        memcmp(&rec, &check, sizeof(rec) - 4u)) return -1;
    rec.commit = CP_COMMIT;
    if (Qflash_Write(addr + sizeof(rec) - 4u, (uint8_t *)&rec.commit, 4u) != FlashOperationSuccess) return -1;
    if (Qflash_Read(addr, (uint8_t *)&check, sizeof(check)) != FlashOperationSuccess || memcmp(&rec, &check, sizeof(rec))) return -1;
    return 0;
}

uint8_t udp_bootload_read_checkpoint(OTA_CHECKPOINT_STRUCT *cp)
{
    checkpoint_record_t rec;
    uint32_t i;
    uint8_t found = 0u;
    if (!cp) return 0u;
    for (i = 0u; i < CP_RECORD_COUNT; ++i) {
        if (Qflash_Read(FLASH_OTA_CHECKPOINT_ADDR + i * CP_RECORD_BYTES,
                        (uint8_t *)&rec, sizeof(rec)) != FlashOperationSuccess) return 0u;
        if (rec.commit == CP_COMMIT && rec.cp.magic == OTA_CHECKPOINT_MAGIC &&
            checkpoint_fields_valid(&rec.cp) && rec.crc == checkpoint_crc(&rec.cp)) {
            *cp = rec.cp;
            found = 1u;
        }
    }
    return found;
}

void udp_bootload_clear_checkpoint(void)
{
    /* A successful write_cfg already erased the journal. Keep its install
     * flag intact. During a failed/fresh download only this OTA sector clears. */
    if (checkpoint_cfg_erasable()) (void)Qflash_Erase_Sector(FLASH_OTA_CFG_START);
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
    if (udp_bootload_copy_app2_to_app1(&para) != 0) {
        return -3; /* Keep the install flag so a reboot can retry. */
    }
    if (Qflash_Erase_Sector(FLASH_OTA_CFG_START) != FlashOperationSuccess) {
        return -2;
    }
	if (udp_bootload_write_done_marker() != 0) {
		return -4;
	}
	return 0;
}
