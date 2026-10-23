#include <stddef.h>
#include "ota_crc.h"
#include "qflash.h"

/**
 * @brief  计算 OTA 帧 CRC（PDF 附录 A Get_CRC）
 */
uint8_t ota_frame_get_crc(const uint8_t *ptr, uint32_t len)
{
	uint8_t crc = 0u;
	uint8_t i;

	if (ptr == NULL) {
		return 0u;
	}

	while (len-- != 0u) {
		for (i = 0x80u; i != 0u; i = (uint8_t)(i >> 1)) {
			if ((crc & 0x80u) != 0u) {
				crc = (uint8_t)(crc << 1);
				crc ^= 0x5Du;
			} else {
				crc = (uint8_t)(crc << 1);
			}
			if ((*ptr & i) != 0u) {
				crc ^= 0x5Du;
			}
		}
		ptr++;
	}
	return crc;
}

uint8_t ota_frame_crc_to_cs(uint8_t crc_raw)
{
	uint8_t v;

	v = (uint8_t)(crc_raw % 100u);
	return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/**
 * @brief  程序校验字：16 位字节累加和
 */
uint16_t ota_prog_checksum16(const uint8_t *data, uint32_t len)
{
	uint32_t i;
	uint16_t sum = 0u;

	if (data == NULL) {
		return 0u;
	}

	for (i = 0u; i < len; i++) {
		sum = (uint16_t)(sum + data[i]);
	}
	return sum;
}

/**
 * @brief  从 Flash 流式计算程序校验字（整包比对）
 * @param  flash_addr  Flash 地址
 * @param  len         Flash 长度
 * @return 累加和（自然 uint16 溢出截断）
 */
int ota_prog_checksum16_flash_checked(uint32_t flash_addr, uint32_t len, uint16_t *out)
{
    uint8_t buf[256];
    uint32_t off = 0u, i, chunk;
    uint16_t sum = 0u;
    if (!out || !len || flash_addr > 0xffffffffu - (len - 1u)) return -1;
    while (off < len) {
        chunk = len - off;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);
        if (Qflash_Read(flash_addr + off, buf, chunk) != FlashOperationSuccess) return -1;
        for (i = 0u; i < chunk; ++i) sum = (uint16_t)(sum + buf[i]);
        off += chunk;
    }
    *out = sum;
    return 0;
}

uint16_t ota_prog_checksum16_flash(uint32_t flash_addr, uint32_t len)
{
    uint16_t sum = 0u;
    (void)ota_prog_checksum16_flash_checked(flash_addr, len, &sum);
    return sum;
}
