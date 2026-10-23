/**
 * @file udp_ota.c
 * @brief UDP OTA §3.1 协议时序状态机
 *
 * 严格一问一答（与 ota_session.py 一致）：
 *   Tag11 → 发 req_seq=N → 收 chunk N → 写 Flash → 再发 req_seq=N+1
 *   最后一片写完 → 发 req_seq=total+1（不等服务器下行）→ SKTSEND 发出即本地校验 → 复位
 */
#include "udp_ota.h"
#include "ota_frame.h"
#include "ota_crc.h"
#include "udp_bootload.h"
#include "flash_layout.h"
#include "userhard.h"
#include "cat1_skt_udp.h"
#include "cat1_udp_config.h"
#include "remote_reports.h"
#include "br_timer.h"
#include "co_printf.h"
#include "br1601x.h"
#include "ble_adv.h"
#include "ble_gap.h"
#include "ble_hid_kbd.h"
#include "pm_state.h"
#include "app_voice_ctrl.h"
#include "app_audio_pcm.h"
#include "ir_key_map.h"
#include "ir_learn.h"
#include <string.h>

/** @brief §3.1 下载时序阶段（与 s_expect_seq 配合） */
typedef enum {
	OTA_DL_IDLE = 0,       /**< 未升级 */
	OTA_DL_NEED_SEND,      /**< 可发 uplink，req_seq = s_expect_seq */
	OTA_DL_WAIT_CHUNK,     /**< 已发 req，等 downlink packet_seq == s_expect_seq */
	OTA_DL_VERIFY,         /**< 已发 req_seq=total+1，本地整包校验 */
	OTA_DL_RESET_PENDING,  /**< 校验通过，即将复位 */
} ota_dl_phase_t;

static ota_dl_phase_t s_dl_phase;
static BOOTLOAD_PARA_STRUCT s_para;
static uint16_t s_total_packets;
static uint16_t s_new_version;
static uint16_t s_saved_checksum;
static uint32_t s_prog_start;
static uint32_t s_prog_end;

/** 当前轮次：上行 req_seq 且下行期望 packet_seq（1..total，或 total+1 仅上行） */
static uint16_t s_expect_seq;
/** 已成功写入 Flash 的最大片序号 */
static uint16_t s_written_seq;

static uint8_t  s_uplink_built;
static uint8_t  s_uplink_buf[64];
static uint16_t s_uplink_len;
static ota_uplink_info_t s_pending_uplink;

static uint8_t  s_chunk_pending;
static uint16_t s_chunk_seq;
static uint8_t  s_chunk_need_prepare;

static uint8_t s_led_blink_active;
static uint32_t s_led_blink_start_ms;
static uint32_t s_led_blink_last_ms;
static uint8_t s_led_blink_on;

static uint32_t s_dl_led_last_ms;
static uint8_t  s_dl_led_on;
static uint32_t s_wait_chunk_since_ms;
static uint16_t s_chunk_retry_count;
static uint16_t s_stall_watch_written;
static uint32_t s_stall_watch_since_ms;
static uint8_t  s_pause_event;
static uint32_t s_pause_done_ms;
/** Tag11 启动至 fail/复位前的 OTA 会话标志（PAUSE 不清） */
static uint8_t  s_ota_session;

#if OTA_DEBUG_DUMP_RX || OTA_DEBUG_DUMP_TX
static void ota_debug_dump_hex_lines(const char *tag, const uint8_t *pdu, uint16_t len)
{
	uint16_t i;

	if (pdu == NULL || len == 0u) {
		return;
	}

	co_printf("[OTA  ] %s len=%u\r\n", tag, (unsigned)len);
	for (i = 0u; i < len; i++) {
		co_printf(" %02X", (unsigned)pdu[i]);
		if (((i + 1u) % 16u) == 0u) {
			co_printf("\r\n");
		}
	}
	if ((len % 16u) != 0u) {
		co_printf("\r\n");
	}
}
#endif

#if OTA_DEBUG_DUMP_TX
static uint8_t s_debug_tx_req1_logged;

static void ota_debug_reset_tx_req1_log(void)
{
	s_debug_tx_req1_logged = 0u;
}

static void ota_debug_dump_tx_req1(const ota_uplink_info_t *info,
                                   const uint8_t *pdu, uint16_t len)
{
	if (info == NULL || pdu == NULL || len == 0u) {
		return;
	}
	if (info->req_seq != 1u || s_debug_tx_req1_logged != 0u) {
		return;
	}

	s_debug_tx_req1_logged = 1u;
	co_printf("[OTA  ] uplink req1 tx ver=0x%04X req_addr=0x%08lX total=%u\r\n",
	          (unsigned)info->version, (unsigned long)info->req_addr,
	          (unsigned)info->total_packets);
	ota_debug_dump_hex_lines("uplink req1 dump", pdu, len);
}
#else
static void ota_debug_reset_tx_req1_log(void)
{
}
#endif

#if OTA_DEBUG_DUMP_RX
static const char *ota_debug_parse_fail_str(uint8_t reason)
{
	switch (reason) {
	case OTA_PARSE_FAIL_PARAM:
		return "param";
	case OTA_PARSE_FAIL_START_END:
		return "start/end";
	case OTA_PARSE_FAIL_CTRL_AFN:
		return "ctrl/afn";
	case OTA_PARSE_FAIL_TOTAL_LEN:
		return "total_len";
	case OTA_PARSE_FAIL_CRC:
		return "crc";
	case OTA_PARSE_FAIL_HDR_ONLY_LEN:
		return "hdr_only_len";
	case OTA_PARSE_FAIL_CHUNK_LEN:
		return "chunk_len";
	default:
		return "?";
	}
}

static void ota_debug_dump_rx_on_fail(const uint8_t *pdu, uint16_t pdu_len, uint8_t reason)
{
	uint16_t head_len;

	co_printf("[OTA  ] downlink parse fail reason=%s(%u)\r\n",
	          ota_debug_parse_fail_str(reason), (unsigned)reason);
	if (reason == OTA_PARSE_FAIL_CRC) {
		uint8_t crc_raw;
		uint8_t crc_cs;
		uint8_t crc_frame;

		ota_frame_get_parse_crc(&crc_raw, &crc_cs, &crc_frame);
		co_printf("[OTA  ] crc_raw=0x%02X crc_cs=0x%02X crc_frame=0x%02X\r\n",
		          (unsigned)crc_raw, (unsigned)crc_cs, (unsigned)crc_frame);
	}

	if (pdu == NULL || pdu_len == 0u) {
		return;
	}

#if OTA_DEBUG_DUMP_FULL
	ota_debug_dump_hex_lines("dump full", pdu, pdu_len);
#else
	head_len = OTA_DEBUG_DUMP_HEAD_BYTES;
	if (head_len > pdu_len) {
		head_len = pdu_len;
	}
	ota_debug_dump_hex_lines("dump head", pdu, head_len);
	if (pdu_len >= 2u) {
		co_printf("[OTA  ] dump tail: %02X %02X\r\n",
		          (unsigned)pdu[pdu_len - 2u], (unsigned)pdu[pdu_len - 1u]);
	}
#endif
}
#endif /* OTA_DEBUG_DUMP_RX */

static int udp_ota_chunk_prog_valid(uint32_t prog_start, uint32_t prog_end)
{
	uint32_t prog_len;

	if (prog_start != FLASH_APP2_START) {
		return -1;
	}
	if (prog_end < prog_start) {
		return -1;
	}
	prog_len = prog_end - prog_start + 1u;
	if (prog_len == 0u || prog_len > FLASH_APP2_SIZE ||
        (prog_len + OTA_CHUNK_SIZE - 1u) / OTA_CHUNK_SIZE != s_total_packets) {
		return -1;
	}
	return 0;
}

static int udp_bootload_addr_in_app2_check(uint32_t addr, uint32_t len)
{
	uint32_t end;

	if (len == 0u) {
		return -1;
	}
	end = addr + len - 1u;
	if (addr < FLASH_APP2_START || end >= (FLASH_APP2_START + FLASH_APP2_SIZE)) {
		return -1;
	}
	return 0;
}

static int udp_ota_chunk_seq_addr_match(uint16_t packet_seq, uint32_t chunk_addr)
{
	uint32_t expect;

	if (packet_seq == 0u) {
		return 0;
	}
	expect = FLASH_APP2_START + (uint32_t)(packet_seq - 1u) * OTA_CHUNK_SIZE;
	return (chunk_addr == expect) ? 0 : -1;
}

static uint32_t udp_ota_now_ms(void)
{
	return sdk_timer_get_current_ms();
}

static void udp_ota_clear_uplink_cache(void)
{
	s_uplink_built = 0u;
	s_uplink_len = 0u;
}

static void udp_ota_build_uplink_info(void)
{
	s_pending_uplink.total_packets = s_total_packets;
	s_pending_uplink.req_seq = s_expect_seq;
	s_pending_uplink.version = s_new_version;
	s_pending_uplink.status = 0u;
	if (s_expect_seq <= s_total_packets) {
		s_pending_uplink.req_addr = FLASH_APP2_START
			+ (uint32_t)(s_expect_seq - 1u) * OTA_CHUNK_SIZE;
	} else {
		s_pending_uplink.req_addr = 0u;
	}
}

static void udp_ota_fail(uint8_t status)
{
	(void)status;
	udp_ota_session_exit();
	udp_bootload_clear_checkpoint();
	CLRLED1;
	s_chunk_pending = 0u;
	s_chunk_need_prepare = 0u;
	s_dl_phase = OTA_DL_IDLE;
	s_expect_seq = 1u;
	s_written_seq = 0u;
	s_chunk_retry_count = 0u;
	s_wait_chunk_since_ms = 0u;
	s_pause_done_ms = 0u;
	udp_ota_clear_uplink_cache();
	ota_debug_reset_tx_req1_log();
	co_printf("[OTA  ] fail status=%u\r\n", (unsigned)status);
}

/**
 * @brief  暂停 OTA：写 Flash 断点；退避后由 poll_auto_resume 直接发 0x41 续传
 */
static void udp_ota_pause(uint8_t reason)
{
	OTA_CHECKPOINT_STRUCT cp;

	if (s_dl_phase == OTA_DL_IDLE || s_dl_phase == OTA_DL_VERIFY
	    || s_dl_phase == OTA_DL_RESET_PENDING) {
		return;
	}

	cat1_skt_udp_force_fail_active();

	if (s_total_packets > 0u && s_new_version != 0u
	    && s_written_seq < s_total_packets) {
		memset(&cp, 0, sizeof(cp));
		cp.version = s_new_version;
		cp.total_packets = s_total_packets;
		cp.written_seq = s_written_seq;
		cp.prog_checksum = s_saved_checksum;
		cp.prog_length = s_para.prog_length;
		if (udp_bootload_write_checkpoint(&cp) != 0) {
			co_printf("[OTA  ] checkpoint write fail\r\n");
		}
	}

	co_printf("[OTA  ] PAUSE reason=%u ver=0x%04X total=%u written=%u expect=%u retries=%u\r\n",
	          (unsigned)reason,
	          (unsigned)s_new_version, (unsigned)s_total_packets,
	          (unsigned)s_written_seq, (unsigned)s_expect_seq,
	          (unsigned)s_chunk_retry_count);

	CLRLED1;
	s_chunk_pending = 0u;
	s_chunk_need_prepare = 0u;
	s_dl_phase = OTA_DL_IDLE;
	s_chunk_retry_count = 0u;
	s_wait_chunk_since_ms = 0u;
	udp_ota_clear_uplink_cache();
	s_pause_event = 1u;
	s_pause_done_ms = udp_ota_now_ms();
	s_stall_watch_written = s_written_seq;
	s_stall_watch_since_ms = 0u;
}

static void udp_ota_check_retry_limit(void)
{
	if (s_chunk_retry_count >= CAT1_OTA_CHUNK_RETRY_MAX) {
		udp_ota_pause(1u);
	}
}

static void udp_ota_sync_checkpoint(void)
{
	OTA_CHECKPOINT_STRUCT cp;

	if (s_new_version == 0u || s_total_packets == 0u) {
		return;
	}
	if (s_written_seq == 0u || s_written_seq >= s_total_packets) {
		return;
	}
	if (s_para.prog_length == 0u || (s_written_seq % 8u) != 0u) {
		return;
	}

	memset(&cp, 0, sizeof(cp));
	cp.version = s_new_version;
	cp.total_packets = s_total_packets;
	cp.written_seq = s_written_seq;
	cp.prog_checksum = s_saved_checksum;
	cp.prog_length = s_para.prog_length;
	(void)udp_bootload_write_checkpoint(&cp);
}

/**
 * @brief  Tag11 与 Flash 断点一致时从 written_seq+1 续传（不擦 APP2）
 * @return 1 已续传；0 需全新下载
 */
static uint8_t udp_ota_try_resume_from_checkpoint(uint16_t sw_version_bcd,
                                                  uint16_t total_packets)
{
	OTA_CHECKPOINT_STRUCT cp;

	if (udp_bootload_read_checkpoint(&cp) == 0u) {
		return 0u;
	}
	if (cp.version != sw_version_bcd || cp.total_packets != total_packets) {
		co_printf("[OTA  ] Tag11 checkpoint mismatch (cp ver=0x%04X pkts=%u), fresh\r\n",
		          (unsigned)cp.version, (unsigned)cp.total_packets);
		return 0u;
	}
	if (cp.written_seq == 0u) {
		co_printf("[OTA  ] Tag11 checkpoint written=0, fresh start\r\n");
		return 0u;
	}

	s_total_packets = total_packets;
	s_new_version = sw_version_bcd;
	s_saved_checksum = cp.prog_checksum;
	s_prog_start = FLASH_APP2_START;
	s_prog_end = FLASH_APP2_START + cp.prog_length - 1u;
	memset(&s_para, 0, sizeof(s_para));
	s_para.prog_store_addr = FLASH_APP2_START;
	s_para.prog_write_addr = FLASH_APP1_START;
	s_para.prog_length = cp.prog_length;
	s_para.prog_checksum = (uint32_t)cp.prog_checksum;

	s_written_seq = cp.written_seq;
	s_expect_seq = (uint16_t)(cp.written_seq + 1u);
	s_chunk_pending = 0u;
	s_chunk_need_prepare = 0u;
	s_chunk_retry_count = 0u;
	udp_ota_clear_uplink_cache();
	s_dl_phase = OTA_DL_NEED_SEND;
	SETLED1;
	udp_ota_session_enter();

	co_printf("[OTA  ] Tag11 RESUME ver=0x%04X written=%u/%u, req chunk %u\r\n",
	          (unsigned)sw_version_bcd, (unsigned)cp.written_seq,
	          (unsigned)total_packets, (unsigned)s_expect_seq);
	return 1u;
}

void udp_ota_init(void)
{
	s_ota_session = 0u;
	s_dl_phase = OTA_DL_IDLE;
	s_chunk_pending = 0u;
	s_led_blink_active = 0u;
	s_dl_led_on = 0u;
	s_wait_chunk_since_ms = 0u;
	s_stall_watch_written = 0u;
	s_stall_watch_since_ms = 0u;
	s_pause_event = 0u;
	s_pause_done_ms = 0u;
	udp_bootload_init();

	{
		OTA_CHECKPOINT_STRUCT cp;

		if (udp_bootload_read_checkpoint(&cp) != 0u) {
			co_printf("[OTA  ] checkpoint on boot: ver=0x%04X written=%u/%u\r\n",
			          (unsigned)cp.version,
			          (unsigned)cp.written_seq,
			          (unsigned)cp.total_packets);
		}
	}

	if (udp_bootload_is_done_marker_set() != 0u) {
		s_led_blink_active = 1u;
		s_led_blink_start_ms = udp_ota_now_ms();
		s_led_blink_last_ms = s_led_blink_start_ms;
		s_led_blink_on = 0u;
		co_printf("[OTA  ] boot marker detected, LED1 blink 3s\r\n");
	}
}

void udp_ota_on_udp_ready(void)
{
	(void)s_dl_phase;
}

void udp_ota_on_tag11_task(uint16_t sw_version_bcd, uint16_t total_packets)
{
	uint16_t local_ver;

	if (sw_version_bcd == 0u && total_packets == 0u) {
		co_printf("[OTA  ] Tag11 no upgrade task\r\n");
		return;
	}

	if (s_dl_phase != OTA_DL_IDLE) {
		co_printf("[OTA  ] Tag11 ignored, phase=%u\r\n", (unsigned)s_dl_phase);
		return;
	}

	if (total_packets == 0u || total_packets > FLASH_APP2_SIZE / OTA_CHUNK_SIZE) {
		co_printf("[OTA  ] Tag11 invalid total_packets\r\n");
		return;
	}

	if (remote_get_local_sw_version_bcd(&local_ver) != 0) {
		co_printf("[OTA  ] Tag11 local version read fail\r\n");
		return;
	}

	if (sw_version_bcd < local_ver) {
		co_printf("[OTA  ] Tag11 old version=0x%04X downgrade rejected srv=0x%04X\r\n",
		          (unsigned)local_ver, (unsigned)sw_version_bcd);
		return;
	}

	if (sw_version_bcd == local_ver) {
		co_printf("[OTA  ] Tag11 same version skip srv=0x%04X local=0x%04X\r\n",
		          (unsigned)sw_version_bcd, (unsigned)local_ver);
		return;
	}

	if (udp_ota_try_resume_from_checkpoint(sw_version_bcd, total_packets) != 0u) {
		return;
	}

	udp_ota_session_enter();
	udp_bootload_clear_checkpoint();

	s_total_packets = total_packets;
	s_new_version = sw_version_bcd;
	s_saved_checksum = 0u;
	s_prog_start = 0u;
	s_prog_end = 0u;
	memset(&s_para, 0, sizeof(s_para));
	s_para.prog_store_addr = FLASH_APP2_START;
	s_para.prog_write_addr = FLASH_APP1_START;

	s_written_seq = 0u;
	s_expect_seq = 1u;
	s_chunk_pending = 0u;
	s_chunk_need_prepare = 0u;
	s_chunk_retry_count = 0u;
	udp_ota_clear_uplink_cache();
	ota_debug_reset_tx_req1_log();
	s_dl_phase = OTA_DL_NEED_SEND;

	co_printf("\r\n[OTA  ] Tag11 start OTA ver=0x%04X pkts=%u, req chunk 1\r\n\r\n",
	          (unsigned)sw_version_bcd, (unsigned)total_packets);
}

int udp_ota_on_rx(const uint8_t *pdu, uint16_t pdu_len)
{
	ota_downlink_info_t info;
	int rc;

	if (pdu == NULL || pdu_len < 15u || pdu[0] != OTA_FRAME_START) {
		return -1;
	}

	rc = ota_frame_parse_downlink(pdu, pdu_len, &info);
	if (rc != 0) {
#if OTA_DEBUG_DUMP_RX
		ota_debug_dump_rx_on_fail(pdu, pdu_len, ota_frame_get_parse_fail_reason());
#else
		co_printf("[OTA  ] downlink parse fail\r\n");
#endif
		return 0;
	}

	if (info.packet_seq == 0u) {
		return 0;
	}

	if (info.packet_seq <= s_written_seq) {
		co_printf("[OTA  ] dup chunk %u ignored (written)\r\n",
		          (unsigned)info.packet_seq);
		return 0;
	}

	if (s_dl_phase != OTA_DL_WAIT_CHUNK) {
		if (s_dl_phase == OTA_DL_NEED_SEND && info.packet_seq == s_expect_seq) {
			co_printf("[OTA  ] late chunk %u in NEED_SEND, accept\r\n",
			          (unsigned)info.packet_seq);
			s_chunk_retry_count = 0u;
		} else {
			co_printf("[OTA  ] unexpected chunk seq=%u phase=%u expect=%u\r\n",
			          (unsigned)info.packet_seq, (unsigned)s_dl_phase,
			          (unsigned)s_expect_seq);
			return 0;
		}
	} else if (info.packet_seq != s_expect_seq) {
		co_printf("[OTA  ] seq mismatch got=%u expect=%u\r\n",
		          (unsigned)info.packet_seq, (unsigned)s_expect_seq);
		return 0;
	}

	if (info.packet_seq < 1u || info.packet_seq > s_total_packets) {
		udp_ota_fail(2u);
		return 0;
	}
	if (info.chunk_len != OTA_CHUNK_SIZE || info.code_data == NULL) {
		udp_ota_fail(2u);
		return 0;
	}
	if (udp_bootload_addr_in_app2_check(info.chunk_addr, info.chunk_len) != 0) {
		udp_ota_fail(2u);
		return 0;
	}
	if (udp_ota_chunk_seq_addr_match(info.packet_seq, info.chunk_addr) != 0) {
		co_printf("[OTA  ] seq/addr mismatch seq=%u addr=0x%08lX\r\n",
		          (unsigned)info.packet_seq, (unsigned long)info.chunk_addr);
		udp_ota_fail(2u);
		return 0;
	}

	if (s_chunk_pending != 0u) {
		return 0;
	}

	if (info.packet_seq == 1u) {
		if (info.total_packets != s_total_packets
		    || info.new_version != s_new_version) {
			udp_ota_fail(2u);
			return 0;
		}
		if (udp_ota_chunk_prog_valid(info.prog_start, info.prog_end) != 0) {
			udp_ota_fail(2u);
			return 0;
		}
		s_saved_checksum = info.prog_checksum;
		s_prog_start = info.prog_start;
		s_prog_end = info.prog_end;
		s_para.prog_length = s_prog_end - s_prog_start + 1u;
		s_para.prog_checksum = (uint32_t)s_saved_checksum;
		s_chunk_need_prepare = 1u;
		SETLED1;
		co_printf("\r\n[OTA  ] chunk1 csum=0x%04X len=%u LED1 ON\r\n\r\n",
		          (unsigned)s_saved_checksum, (unsigned)s_para.prog_length);
	} else if (info.total_packets != s_total_packets
	           || info.new_version != s_new_version) {
		udp_ota_fail(2u);
		return 0;
	}

	if (udp_bootload_stage_chunk(info.chunk_addr, info.code_data, info.chunk_len) != 0) {
		udp_ota_fail(1u);
		return 0;
	}
	s_chunk_seq = info.packet_seq;
	s_chunk_pending = 1u;
	return 1;
}

void udp_ota_poll_work(void)
{
	if (s_chunk_pending == 0u) {
		return;
	}

	cat1_skt_udp_pump_urc();

	if (s_chunk_need_prepare != 0u) {
		if (udp_bootload_prepare_backup(&s_para) != 0) {
			s_chunk_pending = 0u;
			s_chunk_need_prepare = 0u;
			udp_ota_fail(1u);
			return;
		}
		s_chunk_need_prepare = 0u;
		cat1_skt_udp_pump_urc();
	}

	if (udp_bootload_commit_staged() != 0) {
		s_chunk_pending = 0u;
		udp_ota_fail(1u);
		return;
	}

	cat1_skt_udp_pump_urc();
	co_printf("[OTA  ] chunk %u/%u @ 0x%08lX\r\n",
	          (unsigned)s_chunk_seq, (unsigned)s_total_packets,
	          (unsigned long)udp_bootload_staged_addr());

	s_chunk_pending = 0u;
	s_written_seq = s_chunk_seq;
	s_wait_chunk_since_ms = 0u;
	s_chunk_retry_count = 0u;
	udp_ota_clear_uplink_cache();
	udp_ota_sync_checkpoint();

	if (s_written_seq < s_total_packets) {
		s_expect_seq = (uint16_t)(s_written_seq + 1u);
		s_dl_phase = OTA_DL_NEED_SEND;
	} else {
		s_expect_seq = (uint16_t)(s_total_packets + 1u);
		s_dl_phase = OTA_DL_NEED_SEND;
	}
}

int udp_ota_poll_tx_get(const uint8_t **frame, uint16_t *len)
{
	if (s_dl_phase == OTA_DL_VERIFY) {
		uint16_t calc;

		if (ota_prog_checksum16_flash_checked(FLASH_APP2_START, s_para.prog_length, &calc) != 0) {
            co_printf("[OTA  ] verify flash read fail\r\n");
            udp_ota_fail(1u);
            return -2;
        }
		if (calc != s_saved_checksum) {
			co_printf("[OTA  ] checksum fail calc=0x%04X expect=0x%04X\r\n",
			          (unsigned)calc, (unsigned)s_saved_checksum);
			udp_ota_fail(2u);
		} else if (udp_bootload_write_cfg(&s_para) != 0) {
			co_printf("[OTA  ] write cfg fail\r\n");
			udp_ota_fail(1u);
		} else {
			co_printf("[OTA  ] verify OK, reset\r\n");
			udp_bootload_clear_checkpoint();
			s_dl_phase = OTA_DL_RESET_PENDING;
			delay_n_10us(1000u);
			NVIC_SystemReset();
		}
		return -2;
	}

	if (s_dl_phase != OTA_DL_NEED_SEND || s_chunk_pending != 0u
	    || frame == NULL || len == NULL) {
		return -2;
	}

	if (s_uplink_built == 0u) {
		udp_ota_build_uplink_info();
		s_uplink_len = ota_frame_build_uplink(&s_pending_uplink, s_uplink_buf,
		                                      (uint16_t)sizeof(s_uplink_buf));
		if (s_uplink_len == 0u) {
			return -2;
		}
		s_uplink_built = 1u;
		co_printf("\r\n[OTA  ] uplink req_seq=%u status=%u len=%u\r\n",
		          (unsigned)s_pending_uplink.req_seq,
		          (unsigned)s_pending_uplink.status,
		          (unsigned)s_uplink_len);
#if OTA_DEBUG_DUMP_TX
		ota_debug_dump_tx_req1(&s_pending_uplink, s_uplink_buf, s_uplink_len);
#endif
	}

	*frame = s_uplink_buf;
	*len = s_uplink_len;
	return 1;
}

void udp_ota_on_uplink_issued(void)
{
	if (s_dl_phase != OTA_DL_NEED_SEND) {
		return;
	}
	if (s_expect_seq <= s_total_packets) {
		s_dl_phase = OTA_DL_WAIT_CHUNK;
		s_wait_chunk_since_ms = udp_ota_now_ms();
	} else {
		co_printf("[OTA  ] final uplink issued, verify\r\n");
		s_dl_phase = OTA_DL_VERIFY;
	}
}

void udp_ota_on_uplink_sent(void)
{
	if (s_dl_phase == OTA_DL_VERIFY || s_dl_phase == OTA_DL_RESET_PENDING) {
		return;
	}
	udp_ota_clear_uplink_cache();
	if (s_expect_seq > s_total_packets) {
		return;
	}
	/* SKTSEND 已 OK 但本片未 staging：必须回到 NEED_SEND，否则永久 WAIT_CHUNK */
	if (s_chunk_pending == 0u && s_written_seq < s_expect_seq) {
		s_dl_phase = OTA_DL_NEED_SEND;
		s_wait_chunk_since_ms = 0u;
		s_chunk_retry_count++;
		co_printf("[OTA  ] uplink OK no chunk, retry req_seq=%u (#%u)\r\n",
		          (unsigned)s_expect_seq, (unsigned)s_chunk_retry_count);
		udp_ota_check_retry_limit();
	}
}

void udp_ota_on_uplink_timeout(void)
{
	if (s_dl_phase == OTA_DL_VERIFY || s_dl_phase == OTA_DL_RESET_PENDING) {
		return;
	}
	if (s_dl_phase == OTA_DL_WAIT_CHUNK) {
		cat1_skt_udp_force_fail_active();
		udp_ota_clear_uplink_cache();
		s_dl_phase = OTA_DL_NEED_SEND;
		s_wait_chunk_since_ms = 0u;
		s_chunk_retry_count++;
		co_printf("[OTA  ] chunk recv timeout, retry req_seq=%u (#%u)\r\n",
		          (unsigned)s_expect_seq, (unsigned)s_chunk_retry_count);
		cat1_skt_udp_log_recv_diag(s_expect_seq);
		udp_ota_check_retry_limit();
	}
}

uint8_t udp_ota_uplink_waits_chunk(void)
{
	return (uint8_t)(s_expect_seq <= s_total_packets);
}

void udp_ota_confirm_tx(void)
{
	udp_ota_clear_uplink_cache();
}

uint8_t udp_ota_is_active(void)
{
	return (uint8_t)(s_dl_phase != OTA_DL_IDLE);
}

/**
 * @brief  OTA 会话是否仍占用系统（下载 + PAUSE 退避 + VERIFY）
 * @note   与 udp_ota_is_active 不同：PAUSE 时 phase=IDLE 但 session 仍为 busy
 */
uint8_t udp_ota_session_busy(void)
{
	return s_ota_session;
}

/**
 * @brief  进入 OTA 会话隔离：显式关广播、停语音采集；已连接时保持 HID
 * @note   幂等；Tag11 fresh start 与 checkpoint 续传各调用一次
 */
void udp_ota_session_enter(void)
{
	peer_t peer;

	if (s_ota_session != 0u) {
		return;
	}
	s_ota_session = 1u;
    ir_tx_cancel();
    ir_learn_stop();
	(void)ble_adv_stop();
	(void)voice_ctrl_post_stop();
	if (g_audio_pcm_enabled != 0u) {
		Audio_Pcm_Stop();
	}

	peer = ble_gap_get_peer();
	if (peer != NULL) {
		hid_kbd_force_cccd_enable(peer);
		pm_activity_notify(PM_ACTIVITY_KEY);
		co_printf("[OTA  ] session enter: keep HID peer=%08X\r\n", (unsigned)peer);
	}
	co_printf("[OTA  ] session enter: adv/voice off\r\n");
}

/**
 * @brief  结束 OTA 会话，恢复正常广播/PM/数据采集路径
 * @note   校验 OK 复位前不必调用；udp_ota_fail 内会调用
 */
void udp_ota_session_exit(void)
{
	if (s_ota_session == 0u) {
		return;
	}
	s_ota_session = 0u;
    if (ble_gap_get_peer() == NULL) ble_adv_start_by_bonding();
	co_printf("[OTA  ] session exit\r\n");
}

uint8_t udp_ota_has_pending_uplink(void)
{
	return (uint8_t)(s_dl_phase == OTA_DL_NEED_SEND
	                 || s_dl_phase == OTA_DL_WAIT_CHUNK);
}

static uint8_t udp_ota_resume_after_pause(void)
{
	OTA_CHECKPOINT_STRUCT cp;
	uint8_t from_flash = 0u;

	if (s_dl_phase != OTA_DL_IDLE) {
		return 0u;
	}

	if (s_new_version != 0u && s_total_packets > 0u
	    && s_written_seq > 0u && s_written_seq < s_total_packets) {
		/* 同一次会话 PAUSE：RAM 状态仍有效 */
	} else if (udp_bootload_read_checkpoint(&cp) != 0u
	           && cp.written_seq > 0u && cp.written_seq < cp.total_packets) {
		s_total_packets = cp.total_packets;
		s_new_version = cp.version;
		s_saved_checksum = cp.prog_checksum;
		s_prog_start = FLASH_APP2_START;
		s_prog_end = FLASH_APP2_START + cp.prog_length - 1u;
		memset(&s_para, 0, sizeof(s_para));
		s_para.prog_store_addr = FLASH_APP2_START;
		s_para.prog_write_addr = FLASH_APP1_START;
		s_para.prog_length = cp.prog_length;
		s_para.prog_checksum = (uint32_t)cp.prog_checksum;
		s_written_seq = cp.written_seq;
		from_flash = 1u;
	} else {
		return 0u;
	}

	s_expect_seq = (uint16_t)(s_written_seq + 1u);
	s_chunk_pending = 0u;
	s_chunk_need_prepare = 0u;
	s_chunk_retry_count = 0u;
	s_wait_chunk_since_ms = 0u;
	udp_ota_clear_uplink_cache();
	s_dl_phase = OTA_DL_NEED_SEND;
	SETLED1;
	s_stall_watch_written = s_written_seq;
	s_stall_watch_since_ms = 0u;

	co_printf("[OTA  ] RESUME after PAUSE ver=0x%04X written=%u/%u req chunk %u src=%s\r\n",
	          (unsigned)s_new_version, (unsigned)s_written_seq,
	          (unsigned)s_total_packets, (unsigned)s_expect_seq,
	          from_flash != 0u ? "flash" : "ram");
	return 1u;
}

uint8_t udp_ota_poll_auto_resume(void)
{
	uint32_t now_ms;
	uint32_t elapsed;

	if (s_pause_done_ms == 0u || s_dl_phase != OTA_DL_IDLE) {
		return 0u;
	}
	now_ms = udp_ota_now_ms();
	elapsed = now_ms - s_pause_done_ms;
	if (elapsed < UDP_OTA_RESUME_BACKOFF_MS) {
		return 0u;
	}
	s_pause_done_ms = 0u;
	co_printf("[OTA  ] auto RESUME backoff %u ms elapsed\r\n", (unsigned)elapsed);
	return udp_ota_resume_after_pause();
}

uint8_t udp_ota_poll_stall_pause(void)
{
	uint32_t now_ms;
	uint32_t elapsed;

	if (s_dl_phase == OTA_DL_IDLE || s_dl_phase == OTA_DL_VERIFY
	    || s_dl_phase == OTA_DL_RESET_PENDING) {
		s_stall_watch_written = s_written_seq;
		s_stall_watch_since_ms = 0u;
		return 0u;
	}
	if (s_written_seq != s_stall_watch_written) {
		s_stall_watch_written = s_written_seq;
		s_stall_watch_since_ms = 0u;
		return 0u;
	}
	now_ms = udp_ota_now_ms();
	if (s_stall_watch_since_ms == 0u) {
		s_stall_watch_since_ms = now_ms;
		return 0u;
	}
	elapsed = now_ms - s_stall_watch_since_ms;
	if (elapsed < UDP_OTA_STALL_PAUSE_MS) {
		return 0u;
	}
	co_printf("[OTA  ] stall %u ms written=%u, force PAUSE\r\n",
	          (unsigned)elapsed, (unsigned)s_written_seq);
	udp_ota_pause(2u);
	return 1u;
}

uint8_t udp_ota_take_pause_event(void)
{
	if (s_pause_event == 0u) {
		return 0u;
	}
	s_pause_event = 0u;
	return 1u;
}

void udp_ota_download_led_poll(void)
{
	uint32_t now_ms;

	if (s_dl_phase == OTA_DL_IDLE || s_led_blink_active != 0u) {
		return;
	}

	now_ms = udp_ota_now_ms();
	if ((now_ms - s_dl_led_last_ms) < OTA_LED_BLINK_PERIOD_MS) {
		return;
	}

	s_dl_led_last_ms = now_ms;
	s_dl_led_on = (uint8_t)(s_dl_led_on ^ 1u);
	if (s_dl_led_on != 0u) {
		SETLED1;
	} else {
		CLRLED1;
	}
}

uint8_t udp_ota_poll_watchdog(void)
{
	uint32_t now_ms;
	uint32_t elapsed;

	if (s_dl_phase != OTA_DL_WAIT_CHUNK) {
		return 0u;
	}
	/* 已 staging 本片，等 Flash commit，勿打断 */
	if (s_chunk_pending != 0u) {
		return 0u;
	}
	if (s_wait_chunk_since_ms == 0u) {
		s_wait_chunk_since_ms = udp_ota_now_ms();
		return 0u;
	}

	now_ms = udp_ota_now_ms();
	elapsed = now_ms - s_wait_chunk_since_ms;
	if (elapsed < CAT1_OTA_CHUNK_RETRY_MS) {
		return 0u;
	}

	co_printf("[OTA  ] chunk recv watchdog %u ms, retry req_seq=%u\r\n",
	          (unsigned)elapsed, (unsigned)s_expect_seq);
	udp_ota_on_uplink_timeout();
	return 1u;
}

void udp_ota_boot_indicate_poll(void)
{
	uint32_t now_ms;
	uint32_t elapsed;

	if (s_led_blink_active == 0u) {
		return;
	}

	now_ms = udp_ota_now_ms();
	elapsed = now_ms - s_led_blink_start_ms;
	if (elapsed >= OTA_LED_BLINK_DURATION_MS) {
		CLRLED1;
		udp_bootload_clear_done_marker();
		s_led_blink_active = 0u;
		co_printf("[OTA  ] boot indicate done\r\n");
		return;
	}

	if ((now_ms - s_led_blink_last_ms) >= OTA_LED_BLINK_PERIOD_MS) {
		s_led_blink_last_ms = now_ms;
		s_led_blink_on = (uint8_t)(s_led_blink_on ^ 1u);
		if (s_led_blink_on != 0u) {
			SETLED1;
		} else {
			CLRLED1;
		}
	}
}
