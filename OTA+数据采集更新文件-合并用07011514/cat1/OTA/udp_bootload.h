/**
 * @file udp_bootload.h
 * @brief OTA 备份区 Flash 擦写与 BOOTLOAD_PARA 管理（参考 FM33 远程备份）
 */
#ifndef UDP_BOOTLOAD_H
#define UDP_BOOTLOAD_H

#include <stdint.h>
#include "flash_layout.h"

/**
 * @brief  OTA 升级参数（20 字节，存 @ FLASH_OTA_CFG_START + 0x000）
 */
typedef struct {
	uint32_t prog_store_addr;   /**< APP2 备份区基址（FLASH_APP2_START） */
	uint32_t prog_length;       /**< 程序长度 = prog_end - prog_start + 1 */
	uint32_t prog_checksum;     /**< 程序校验字（16 位累加和，非 CRC16） */
	uint32_t prog_write_addr;   /**< APP1 用户区基址（FLASH_APP1_START） */
	uint32_t prog_success_flag; /**< 0x12345678 表示整包下载校验通过 */
} BOOTLOAD_PARA_STRUCT;

/**
 * @brief  OTA 断点（@ FLASH_OTA_CHECKPOINT_ADDR，暂停续传用）
 * @note   written_seq 为已成功写入 APP2 的最大片序号；续传从 written_seq+1 请求
 */
typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t total_packets;
	uint16_t written_seq;
	uint16_t prog_checksum;
	uint32_t prog_length;
} OTA_CHECKPOINT_STRUCT;

/**
 * @brief  初始化 Qflash 驱动（APP/Bootloader 上电各调用一次）
 */
void udp_bootload_init(void);

/**
 * @brief  校验参数并按 prog_length 预擦除 APP2 备份区（首片写入前）
 * @param  para  升级参数（store/write 地址须符合 flash_layout.h）
 * @return 0 成功；-1 参数非法或擦除失败
 */
int udp_bootload_prepare_backup(const BOOTLOAD_PARA_STRUCT *para);

/**
 * @brief  写一片固件到绝对 Flash 地址
 * @param  abs_addr  本片起始地址（须在 APP2 范围内）
 * @param  data      代码数据
 * @param  len       长度（≤ OTA_CHUNK_SIZE）
 * @return 0 成功；-1 参数非法或写入失败
 */
int udp_bootload_write_chunk(uint32_t abs_addr, const uint8_t *data, uint16_t len);

/**
 * @brief  暂存一片到内部 512B 缓冲（与 s_cat1_tlv 复用，避免额外 RAM）
 * @return 0 成功；-1 失败
 */
int udp_bootload_stage_chunk(uint32_t abs_addr, const uint8_t *data, uint16_t len);

/** @brief  将 stage_chunk 暂存的内容写入 Flash */
int udp_bootload_commit_staged(void);

/** @brief  当前暂存片起始地址（commit 前有效，供日志） */
uint32_t udp_bootload_staged_addr(void);

/**
 * @brief  步骤 9：擦 OTA 配置扇区并写入 BOOTLOAD_PARA（prog_success_flag=0x12345678）
 * @param  para  升级参数（success_flag 由本函数强制写入）
 * @return 0 成功；-1 失败
 */
int udp_bootload_write_cfg(const BOOTLOAD_PARA_STRUCT *para);

/**
 * @brief  从 Flash 读取 BOOTLOAD_PARA
 * @param  para  出参
 * @return 0 成功；-1 读失败或 para 为 NULL
 */
int udp_bootload_read_para(BOOTLOAD_PARA_STRUCT *para);

/**
 * @brief  Bootloader：将 APP2 备份区复制到 APP1（最多 10 次重试，不二次校验）
 * @param  para  升级参数（须已校验 prog_success_flag）
 * @return 0 成功；-1 失败
 * @note   不擦除 BLE_BONDING_STORE_ADDR 所在扇区
 */
int udp_bootload_copy_app2_to_app1(const BOOTLOAD_PARA_STRUCT *para);

/**
 * @brief  Bootloader：在 OTA 配置扇区 +0x100 写入完成 marker（0x12345678）
 * @return 0 成功；-1 写入失败
 * @note   调用前须已擦除配置扇区；marker 供新 APP 读闪灯指示
 */
int udp_bootload_write_done_marker(void);

/**
 * @brief  新 APP 启动时检查完成 marker 是否有效
 * @return 1 有效（OTA_BOOT_DONE_MARKER）；0 无效或未设置
 */
uint8_t udp_bootload_is_done_marker_set(void);

/**
 * @brief  清除完成 marker（LED 指示结束后调用，写 0）
 */
void udp_bootload_clear_done_marker(void);

/** @brief  写入断点（暂停 OTA 时调用） */
int udp_bootload_write_checkpoint(const OTA_CHECKPOINT_STRUCT *cp);

/** @brief  读取断点；@return 1 有效；0 无效或未设置 */
uint8_t udp_bootload_read_checkpoint(OTA_CHECKPOINT_STRUCT *cp);

/** @brief  清除断点（升级完成/失败/全新开始时） */
void udp_bootload_clear_checkpoint(void);

/** Bootloader 入口：检测标志、复制 APP2→APP1、写 marker */
int udp_bootload_run_bootloader(void);

#endif /* UDP_BOOTLOAD_H */
