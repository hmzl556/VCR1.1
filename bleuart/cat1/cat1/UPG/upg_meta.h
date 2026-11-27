/**
 * @file upg_meta.h
 * @brief 遥控器硬件信息 / 版本信息 / 认证密钥（GY/T 407-2024 E.3.2、E.3.3、E.1.2）
 *
 * 说明：本模块**不缓存**密钥，每次按需从 Flash 读取，避免占用静态 RAM。
 */
#ifndef _UPG_META_H_
#define _UPG_META_H_

#include <stdint.h>
#include <stdbool.h>

#define UPG_IDCODE_LEN   32u
#define UPG_KEY_LEN      16u
#define UPG_KEY_MAX      4u

/** 数据类型标识（8 字节 ASCII，不足补 0） */
#define UPG_TYPE_REL0    "rel0"
#define UPG_TYPE_USR1    "usr1"
#define UPG_TYPE_USR2    "usr2"
#define UPG_TYPE_IRTV    "irtv"
#define UPG_TYPE_IRSTB   "irstb"
#define UPG_TYPE_BLESTB  "blestb"

/**
 * @brief  取认证识别码
 * @return true 表示已由产测写入真实密钥；false 表示使用了开发占位值
 */
bool upg_meta_idcode(uint8_t out[UPG_IDCODE_LEN]);

/**
 * @brief  取认证密钥
 * @return true 表示已由产测写入真实密钥；false 表示使用了开发占位值
 */
bool upg_meta_key(uint8_t index, uint8_t out[UPG_KEY_LEN]);

/**
 * @brief  生成 rel0 硬件信息文本："idcode:<32 字符>\n"
 * @return 写入长度；0 表示缓冲不足
 */
uint16_t upg_meta_build_rel0(uint8_t *buf, uint16_t cap);

/**
 * @brief  生成 usr1 升级数据版本信息：CRC16(2) + 长度(2) + 键值文本，均高字节在前
 * @return 写入长度；0 表示缓冲不足
 */
uint16_t upg_meta_build_usr1(uint8_t *buf, uint16_t cap);

/**
 * @brief  把已下载 usr1 里的 `fw:` 行强制同步成当前固件版本
 * @param  buf   usr1 内容（含 4 字节 CRC/长度头），原地修改
 * @param  total 当前总长（含头）
 * @param  cap   buf 容量
 * @return 同步后的总长；内容异常时原样返回
 * @note   `fw` 是**设备固件版本**，必须与编译期 RC_READ_VAL_DEVICE_SW_VERSION
 *         始终相等，不能被 APP 下载的内容覆盖（否则固件升级后 APP 里看到的
 *         版本号永远不变）。其余字段（品牌/型号 id、各键值文件版本）仍以
 *         覆盖层里 APP 写入的为准，符合 E.3.3「回读反映最新版本」。
 */
uint16_t upg_meta_usr1_sync_fw(uint8_t *buf, uint16_t total, uint16_t cap);

/** @brief 附录 J 的 CRC16（多项式 x16+x12+x5+1，反射式 0xA001） */
uint16_t upg_crc16(const uint8_t *buf, uint32_t len, uint16_t crc_last);

#endif /* _UPG_META_H_ */
