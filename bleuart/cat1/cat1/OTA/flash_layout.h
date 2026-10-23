/**
 * @file flash_layout.h
 * @brief UDP OTA 远程备份 Flash 分区地址定义
 *
 * 布局：16K Boot @ 0x01000000 → 200K APP1 → 200K APP2 → 4K OTA 配置
 *       → 4K BLE 绑定 → 4K 用户云策略配置。
 * 扇区大小使用 qflash.h 的 FLASH_SECTOR_SIZE，此处不重复定义以免与 SDK 冲突。
 */
#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

#define FLASH_BOOT_START             0x01000000u
#define FLASH_BOOT_SIZE              0x00004000u   /* 16KB */

#define FLASH_APP1_START             0x01004000u
#define FLASH_APP1_SIZE              0x00032000u   /* 200KB */

#define FLASH_APP2_START             0x01036000u
#define FLASH_APP2_SIZE              0x00032000u   /* 200KB */

#define FLASH_OTA_CFG_START          0x01068000u
#define FLASH_OTA_CFG_SIZE           0x00001000u   /* 4KB */

#define FLASH_OTA_PARA_OFFSET          0x000u
#define FLASH_OTA_DONE_MARKER_OFFSET 0x100u
#define FLASH_OTA_DONE_MARKER_ADDR   (FLASH_OTA_CFG_START + FLASH_OTA_DONE_MARKER_OFFSET)
#define FLASH_OTA_CHECKPOINT_OFFSET    0x200u
#define FLASH_OTA_CHECKPOINT_ADDR      (FLASH_OTA_CFG_START + FLASH_OTA_CHECKPOINT_OFFSET)

#define OTA_CHECKPOINT_MAGIC           0x4F544048u   /* 'OTPH' 断点续传有效标记 */

#define BLE_BONDING_STORE_ADDR       (FLASH_OTA_CFG_START + FLASH_OTA_CFG_SIZE)

/** Tag0x11 云策略持久化（按键采集 / 上报间隔 / 数据量阈值），独立 4KB 扇区 */
#define FLASH_USER_CFG_START         (BLE_BONDING_STORE_ADDR + 0x1000u)  /* 0x0106A000 */
#define FLASH_USER_CFG_SIZE          0x00001000u
#define APP_CLOUD_POLICY_MAGIC       0x5243504Cu   /* 'RCPL' */
#define APP_CLOUD_POLICY_VER         1u

/* Waveform learning A/B: three 4KB sectors per bank, directly after cloud
 * policy. Legacy NEC banks are removed; no migration from previous addresses.
 * Existing keys must be learned again after installing this layout. */
#define FLASH_IR_RAW_START (FLASH_USER_CFG_START + FLASH_USER_CFG_SIZE) /* 0x0106B000 */
#define FLASH_IR_RAW_BACKUP (FLASH_IR_RAW_START + FLASH_IR_RAW_BANK_SIZE) /* 0x0106E000 */
#define FLASH_IR_RAW_BANK_SIZE 0x3000u
#define FLASH_UNALLOCATED_START (FLASH_IR_RAW_BACKUP + FLASH_IR_RAW_BANK_SIZE) /* 0x01071000 */

#define OTA_UPGRADE_FLAG           0x12345678u
#define OTA_SUCCESS_FLAG           OTA_UPGRADE_FLAG
#define OTA_BOOT_DONE_MARKER       OTA_UPGRADE_FLAG

#define OTA_CHUNK_SIZE             512u

#endif /* FLASH_LAYOUT_H */
