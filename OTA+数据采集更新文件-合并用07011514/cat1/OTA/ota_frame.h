/**
 * @file ota_frame.h
 * @brief 0x41 OTA 帧组包/解包（AFN 固定 0x60，裸 UDP 无 AES）
 *
 * 多字节整型字段统一大端（含总包数/序号/地址/版本/校验/片长）。
 * 下行 prog_start/prog_end 为 APP2 备份区逻辑起止（FLASH_APP2_START）。
 * 帧体长度 2 字节为 BCD（如 542→05 42）；6 字节终端地址亦为 BCD；
 * 帧校验 CS 为 1 字节 BCD（Get_CRC 结果十进制后两位，如 0x8E→142→0x42）。
 */
#ifndef OTA_FRAME_H
#define OTA_FRAME_H

#include <stdint.h>

#define OTA_FRAME_START              0x41u
#define OTA_FRAME_END                0x16u
#define OTA_AFN                      0x60u
#define OTA_CTRL_DOWN                0x03u
#define OTA_CTRL_UP                  0x13u

#define OTA_DOWNLINK_HDR_SIZE        22u
#define OTA_UPLINK_DATA_SIZE         11u

typedef struct {	uint16_t total_packets;
	uint16_t packet_seq;
	uint32_t prog_start;
	uint32_t prog_end;
	uint16_t new_version;
	uint16_t prog_checksum;
	uint32_t chunk_addr;
	uint16_t chunk_len;
	const uint8_t *code_data;
} ota_downlink_info_t;

typedef struct {
	uint16_t total_packets;
	uint16_t req_seq;
	uint32_t req_addr;
	uint16_t version;
	uint8_t  status;
} ota_uplink_info_t;

#define OTA_PARSE_FAIL_NONE           0u
#define OTA_PARSE_FAIL_PARAM          1u
#define OTA_PARSE_FAIL_START_END      2u
#define OTA_PARSE_FAIL_CTRL_AFN       3u
#define OTA_PARSE_FAIL_TOTAL_LEN      4u
#define OTA_PARSE_FAIL_CRC            5u
#define OTA_PARSE_FAIL_HDR_ONLY_LEN   6u
#define OTA_PARSE_FAIL_CHUNK_LEN      7u

int ota_frame_parse_downlink(const uint8_t *frame, uint16_t frame_len,
                             ota_downlink_info_t *info);

/** 最近一次 ota_frame_parse_downlink 失败原因（成功时清零） */
uint8_t ota_frame_get_parse_fail_reason(void);

/** 最近一次下行 CRC：raw=Get_CRC，cs=BCD 帧字节，frame=收到 CS */
void ota_frame_get_parse_crc(uint8_t *crc_raw, uint8_t *crc_cs, uint8_t *crc_frame);

uint16_t ota_frame_build_uplink(const ota_uplink_info_t *info,
                                uint8_t *out, uint16_t out_cap);

void ota_frame_fill_addr_bcd(uint8_t addr_bcd[6]);

#endif /* OTA_FRAME_H */
