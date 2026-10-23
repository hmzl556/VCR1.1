#ifndef OTA_CRC_H
#define OTA_CRC_H

#include <stdint.h>

/**
 * @brief  计算 OTA 帧 CRC（PDF 附录 A Get_CRC）
 * @param  ptr  待校验数据
 * @param  len  字节长度
 * @return 1 字节二进制 CRC
 */
uint8_t ota_frame_get_crc(const uint8_t *ptr, uint32_t len);

/**
 * @brief  将 Get_CRC 结果编码为帧 CS（1 字节 BCD，十进制后两位）
 */
uint8_t ota_frame_crc_to_cs(uint8_t crc_raw);

/**
 * @brief  程序校验字：16 位字节累加和（§2 说明事项 2）
 * @param  data  固件数据
 * @param  len   字节长度
 * @return 累加和（自然 uint16 溢出截断）
 */
uint16_t ota_prog_checksum16(const uint8_t *data, uint32_t len);

/** 从 Flash 流式计算程序校验字（整包比对） */
uint16_t ota_prog_checksum16_flash(uint32_t flash_addr, uint32_t len);

/* Returns -1 on a failed read; never accepts stale buffer bytes as firmware. */
int ota_prog_checksum16_flash_checked(uint32_t flash_addr, uint32_t len, uint16_t *out);

#endif /* OTA_CRC_H */
