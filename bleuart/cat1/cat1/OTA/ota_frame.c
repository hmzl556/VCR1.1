#include <string.h>
#include "ota_frame.h"
#include "ota_crc.h"
#include "flash_layout.h"
#include "br1601x.h"

static uint8_t s_parse_fail_reason;
static uint8_t s_parse_crc_raw;
static uint8_t s_parse_crc_cs;
static uint8_t s_parse_crc_frame;

uint8_t ota_frame_get_parse_fail_reason(void)
{
	return s_parse_fail_reason;
}

void ota_frame_get_parse_crc(uint8_t *crc_raw, uint8_t *crc_cs, uint8_t *crc_frame)
{
	if (crc_raw != NULL) {
		*crc_raw = s_parse_crc_raw;
	}
	if (crc_cs != NULL) {
		*crc_cs = s_parse_crc_cs;
	}
	if (crc_frame != NULL) {
		*crc_frame = s_parse_crc_frame;
	}
}

static uint16_t ota_read_be16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t ota_read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24)
	     | ((uint32_t)p[1] << 16)
	     | ((uint32_t)p[2] << 8)
	     | (uint32_t)p[3];
}

static void ota_write_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)((v >> 8) & 0xFFu);
	p[1] = (uint8_t)(v & 0xFFu);
}

static void ota_write_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)((v >> 24) & 0xFFu);
	p[1] = (uint8_t)((v >> 16) & 0xFFu);
	p[2] = (uint8_t)((v >> 8) & 0xFFu);
	p[3] = (uint8_t)(v & 0xFFu);
}

static void ota_encode_length(uint8_t *p, uint16_t len)
{
	p[0] = (uint8_t)(((len / 1000u) % 10u) << 4) | (uint8_t)((len / 100u) % 10u);
	p[1] = (uint8_t)(((len / 10u) % 10u) << 4) | (uint8_t)(len % 10u);
}

static int ota_decode_length(const uint8_t *p, uint16_t *len_out)
{
	uint16_t v;

	if (p == NULL || len_out == NULL) {
		return -1;
	}

	if ((p[0] >> 4) > 9u || (p[0] & 15u) > 9u ||
        (p[1] >> 4) > 9u || (p[1] & 15u) > 9u) return -1;
	v = (uint16_t)(((p[0] >> 4) & 0x0Fu) * 1000u
	             + (p[0] & 0x0Fu) * 100u
	             + ((p[1] >> 4) & 0x0Fu) * 10u
	             + (p[1] & 0x0Fu));
	*len_out = v;
	return 0;
}
void ota_frame_fill_addr_bcd(uint8_t addr_bcd[6])
{
	const uint8_t *mac;
	uint8_t i;

	if (addr_bcd == NULL) {
		return;
	}

	mac = SystemGetMacAddr();
	if (mac == NULL) {
		memset(addr_bcd, 0, 6u);
		return;
	}

	for (i = 0u; i < 6u; i++) {
		uint8_t hi = (uint8_t)((mac[i] >> 4) & 0x0Fu);
		uint8_t lo = (uint8_t)(mac[i] & 0x0Fu);
		addr_bcd[i] = (uint8_t)((hi << 4) | lo);
	}
}

int ota_frame_parse_downlink(const uint8_t *frame, uint16_t frame_len,
                             ota_downlink_info_t *info)
{
	uint16_t body_len;
	uint16_t data_len;
	uint16_t expect_total;
	uint8_t crc_raw;
	uint8_t crc_cs;
	uint8_t crc_frame;
	const uint8_t *data;

	s_parse_fail_reason = OTA_PARSE_FAIL_NONE;

	if (frame == NULL || info == NULL || frame_len < 15u) {
		s_parse_fail_reason = OTA_PARSE_FAIL_PARAM;
		return -1;
	}
	if (frame[0] != OTA_FRAME_START || frame[frame_len - 1u] != OTA_FRAME_END) {
		s_parse_fail_reason = OTA_PARSE_FAIL_START_END;
		return -1;
	}
	if (ota_decode_length(frame + 1, &body_len) != 0) {
		s_parse_fail_reason = OTA_PARSE_FAIL_PARAM;
		return -1;
	}
	if (frame[3] != OTA_CTRL_DOWN || frame[10] != OTA_AFN) {
		s_parse_fail_reason = OTA_PARSE_FAIL_CTRL_AFN;
		return -1;
	}

	expect_total = (uint16_t)(1u + 2u + body_len + 1u + 1u);
	if (frame_len != expect_total) {
		s_parse_fail_reason = OTA_PARSE_FAIL_TOTAL_LEN;
		return -1;
	}

    /* Verify the complete fixed header before accessing any data fields. */
    if (body_len < 8u + OTA_DOWNLINK_HDR_SIZE) {
        s_parse_fail_reason = OTA_PARSE_FAIL_HDR_ONLY_LEN;
        return -1;
    }
	data = frame + 11;
	data_len = (uint16_t)(body_len - 1u - 6u - 1u);
	crc_raw = ota_frame_get_crc(frame + 3, (uint32_t)body_len);
	crc_cs = ota_frame_crc_to_cs(crc_raw);
	crc_frame = frame[11u + data_len];
	s_parse_crc_raw = crc_raw;
	s_parse_crc_cs = crc_cs;
	s_parse_crc_frame = crc_frame;
	if (crc_cs != crc_frame) {
		s_parse_fail_reason = OTA_PARSE_FAIL_CRC;
		return -1;
	}

	memset(info, 0, sizeof(*info));
	info->total_packets = ota_read_be16(data + 0);
	info->packet_seq = ota_read_be16(data + 2);
	info->prog_start = ota_read_be32(data + 4);
	info->prog_end = ota_read_be32(data + 8);
	info->new_version = ota_read_be16(data + 12);
	info->prog_checksum = ota_read_be16(data + 14);
	info->chunk_addr = ota_read_be32(data + 16);
	info->chunk_len = ota_read_be16(data + 20);

	if (info->packet_seq == 0u) {
		if (data_len != OTA_DOWNLINK_HDR_SIZE) {
			s_parse_fail_reason = OTA_PARSE_FAIL_HDR_ONLY_LEN;
			return -1;
		}
		info->code_data = NULL;
		return 0;
	}

	if (data_len != (uint16_t)(OTA_DOWNLINK_HDR_SIZE + OTA_CHUNK_SIZE)
	    || info->chunk_len != OTA_CHUNK_SIZE) {
		s_parse_fail_reason = OTA_PARSE_FAIL_CHUNK_LEN;
		return -1;
	}
	info->code_data = data + OTA_DOWNLINK_HDR_SIZE;
	return 0;
}

uint16_t ota_frame_build_uplink(const ota_uplink_info_t *info,
                                uint8_t *out, uint16_t out_cap)
{
	uint8_t addr_bcd[6];
	uint16_t body_len;
	uint16_t pos;
	uint8_t crc_raw;
	uint8_t crc_cs;

	if (info == NULL || out == NULL || out_cap < 32u) {
		return 0u;
	}

	body_len = (uint16_t)(1u + 6u + 1u + OTA_UPLINK_DATA_SIZE);
	if ((uint16_t)(1u + 2u + body_len + 1u + 1u) > out_cap) {
		return 0u;
	}

	pos = 0u;
	out[pos++] = OTA_FRAME_START;
	ota_encode_length(out + pos, body_len);
	pos = (uint16_t)(pos + 2u);
	out[pos++] = OTA_CTRL_UP;
	ota_frame_fill_addr_bcd(addr_bcd);
	memcpy(out + pos, addr_bcd, 6u);
	pos = (uint16_t)(pos + 6u);
	out[pos++] = OTA_AFN;
	ota_write_be16(out + pos, info->total_packets);
	pos = (uint16_t)(pos + 2u);
	ota_write_be16(out + pos, info->req_seq);
	pos = (uint16_t)(pos + 2u);
	ota_write_be32(out + pos, info->req_addr);
	pos = (uint16_t)(pos + 4u);
	ota_write_be16(out + pos, info->version);
	pos = (uint16_t)(pos + 2u);
	out[pos++] = info->status;

	crc_raw = ota_frame_get_crc(out + 3, (uint32_t)body_len);
	crc_cs = ota_frame_crc_to_cs(crc_raw);
	out[pos++] = crc_cs;
	out[pos++] = OTA_FRAME_END;
	return pos;
}
