/**
 * @file flash_layout.h
 * @brief UDP OTA 远程备份 Flash 分区地址定义
 *
 * 布局：16K Boot @ 0x01000000 → 220K APP1 → 220K APP2 → 4K OTA 配置
 *       → 4K BLE 绑定 → 4K 用户云策略 → 12K 学习波形 A/B
 *       → 4K 按键键值覆盖层 → 4K 认证密钥 → 12K 预留。
 *
 * v2 变更（220K）：APP1/APP2 由 200K 扩到 220K，为 GY/T 407-2024 键值下载
 * 功能腾出代码空间；其后所有数据分区整体后移。Bootloader 必须同步重编，
 * 且本版固件与 v1 布局引导程序不兼容（已出货设备需返厂或产线重烧引导程序）。
 *
 * 扇区大小使用 qflash.h 的 FLASH_SECTOR_SIZE，此处不重复定义以免与 SDK 冲突。
 */
#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

#define FLASH_LAYOUT_VERSION         2u      /* 2 = 220K APP，键值下载版 */

#define FLASH_BOOT_START             0x01000000u
#define FLASH_BOOT_SIZE              0x00004000u   /* 16KB */

#define FLASH_APP1_START             0x01004000u
#define FLASH_APP1_SIZE              0x00037000u   /* 220KB */

#define FLASH_APP2_START             0x0103B000u
#define FLASH_APP2_SIZE              0x00037000u   /* 220KB */

#define FLASH_OTA_CFG_START          0x01072000u
#define FLASH_OTA_CFG_SIZE           0x00001000u   /* 4KB */

#define FLASH_OTA_PARA_OFFSET          0x000u
#define FLASH_OTA_DONE_MARKER_OFFSET 0x100u
#define FLASH_OTA_DONE_MARKER_ADDR   (FLASH_OTA_CFG_START + FLASH_OTA_DONE_MARKER_OFFSET)
#define FLASH_OTA_CHECKPOINT_OFFSET    0x200u
#define FLASH_OTA_CHECKPOINT_ADDR      (FLASH_OTA_CFG_START + FLASH_OTA_CHECKPOINT_OFFSET)

#define OTA_CHECKPOINT_MAGIC           0x4F544048u   /* 'OTPH' 断点续传有效标记 */

#define BLE_BONDING_STORE_ADDR       (FLASH_OTA_CFG_START + FLASH_OTA_CFG_SIZE)

/** Tag0x11 云策略持久化（按键采集 / 上报间隔 / 数据量阈值），独立 4KB 扇区 */
#define FLASH_USER_CFG_START         (BLE_BONDING_STORE_ADDR + 0x1000u)  /* 0x01074000 */
#define FLASH_USER_CFG_SIZE          0x00001000u
#define APP_CLOUD_POLICY_MAGIC       0x5243504Cu   /* 'RCPL' */
#define APP_CLOUD_POLICY_VER         1u

/* Waveform learning A/B: three 4KB sectors per bank, directly after cloud
 * policy. Legacy NEC banks are removed; no migration from previous addresses.
 * Existing keys must be learned again after installing this layout. */
#define FLASH_IR_RAW_START (FLASH_USER_CFG_START + FLASH_USER_CFG_SIZE) /* 0x01075000 */
#define FLASH_IR_RAW_BACKUP (FLASH_IR_RAW_START + FLASH_IR_RAW_BANK_SIZE) /* 0x01078000 */
#define FLASH_IR_RAW_BANK_SIZE 0x3000u

/* 按键键值下载覆盖层（GY/T 407-2024）：下载值记录，恢复出厂即擦除。
 * A/B 双扇区轮换：写 B → 回读校验 → B 标记有效 → 擦 A，断电最坏丢一次下载，
 * 不会出现"正在写的那一份坏掉、旧的也擦没了"的两头失效。 */
#define FLASH_KEYSTORE_A_START (FLASH_IR_RAW_BACKUP + FLASH_IR_RAW_BANK_SIZE) /* 0x0107B000 */
#define FLASH_KEYSTORE_SIZE    0x00001000u   /* 4KB */
#define FLASH_KEYSTORE_B_START (FLASH_KEYSTORE_A_START + FLASH_KEYSTORE_SIZE) /* 0x0107C000 */
#define FLASH_KEYSTORE_START   FLASH_KEYSTORE_A_START   /* 兼容旧引用：默认主份 */
#define KEYSTORE_MAGIC       0x4B53544Fu   /* 'KSTO' */
#define KEYSTORE_VER         1u

/* 认证识别码 idcode + 多组 key（GY/T 407-2024 附录 E.1.2），产测写入 */
#define FLASH_AUTHKEY_START  (FLASH_KEYSTORE_B_START + FLASH_KEYSTORE_SIZE)   /* 0x0107D000 */
#define FLASH_AUTHKEY_SIZE   0x00001000u   /* 4KB */
#define AUTHKEY_MAGIC        0x41554B59u   /* 'AUKY' */
#define AUTHKEY_VER          1u

/* 剩余分区保留至 Flash 末尾 0x01080000 */
/* 键值下载升级标志（GY/T 407-2024 表 E.14）：独立扇区，掉电保持，
 * 与 OTA 配置区分离，避免设置标志时擦掉 OTA 断点/参数 */
#define FLASH_UPG_FLAG_START    (FLASH_AUTHKEY_START + FLASH_AUTHKEY_SIZE) /* 0x0107E000 */
#define FLASH_UPG_FLAG_SIZE     0x00001000u   /* 4KB */
#define UPG_FLAG_MAGIC          0x55504746u   /* 'UPGF' */
#define UPG_FLAG_VALID          0xF5F5F500u   /* 表 E.13/E.14：已设置有效升级标志 */
#define UPG_FLAG_NONE           0x00000000u   /* 未设置 */

#define FLASH_UNALLOCATED_START (FLASH_UPG_FLAG_START + FLASH_UPG_FLAG_SIZE) /* 0x0107F000 */
#define FLASH_UNALLOCATED_SIZE  0x00001000u   /* 4KB */

#define OTA_UPGRADE_FLAG           0x12345678u
#define OTA_SUCCESS_FLAG           OTA_UPGRADE_FLAG
#define OTA_BOOT_DONE_MARKER       OTA_UPGRADE_FLAG

#define OTA_CHUNK_SIZE             512u

#endif /* FLASH_LAYOUT_H */
