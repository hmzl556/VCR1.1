/**
 * @file cat1_skt_udp.c
 * @brief 模组 UDP AT（NT26 方案 B）：诊断 AT → CGATT/CGPADDR → ECSOCL → ECSOCR → ECSONMI → ECSOST；
 *        下行 +ECSONMI URC + ECSORF 主动读。
 */
#include "cat1_skt_udp.h"
#include "cat1_usart1.h"
#include "cat1_udp_config.h"
#include "co_printf.h"
#include "br_timer.h"
#include "cat1_time_sync.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef CAT1_AT_TOTAL_WAIT_MS
#define CAT1_AT_TOTAL_WAIT_MS  4000u
#endif

typedef enum {
	AT_PHASE_IDLE = 0,
	AT_PHASE_WAIT_OK,
	AT_PHASE_WAIT_DOWNLINK,
	AT_PHASE_WAIT_ECSORF,
} at_phase_t;

typedef enum {
	SETUP_PHASE_IDLE = 0,
	SETUP_PHASE_CSCON,
	SETUP_PHASE_CEREG,
	SETUP_PHASE_CSQ,
	SETUP_PHASE_ICCID_PENDING,
	SETUP_PHASE_ICCID,
	SETUP_PHASE_FREQ_PENDING,
	SETUP_PHASE_FREQ,
	SETUP_PHASE_CCLK_PENDING,
	SETUP_PHASE_CCLK,
	SETUP_PHASE_CGATT_PENDING,
	SETUP_PHASE_CGATT,
	SETUP_PHASE_CGATT_RETRY_WAIT, /* CGATT=0，间隔后再查 */
	SETUP_PHASE_CGPADDR_PENDING,
	SETUP_PHASE_CGPADDR,
	SETUP_PHASE_CLOSE_PENDING,
	SETUP_PHASE_CLOSE,
	SETUP_PHASE_CREATE_PENDING,
	SETUP_PHASE_CREATE,
	SETUP_PHASE_ECSONMI_PENDING,
	SETUP_PHASE_ECSONMI,
	SETUP_PHASE_DONE,
	SETUP_PHASE_FAIL,
} setup_phase_t;

static uint8_t  s_sock_id = 255u;
static char     s_modem_line[CAT1_AT_LINE_MAX];
static char     s_ecsonmi_line[CAT1_ECSONMI_LINE_MAX];
static char     s_modem_rsp[CAT1_MODEM_RSP_CAP];

/* ICCID (SIM卡号) 捕获: AT+ECICCID 响应原文(实测可含字母), 供心跳 Tag0x0A 使用 */
static uint8_t  s_iccid_raw[20];
static uint8_t  s_iccid_len;
static uint8_t  s_iccid_valid;
static unsigned s_urc_line_len;
static uint8_t  s_ota_uplink_wait;
static uint8_t  s_recv_allow = 1u;
static uint8_t *s_recv_target;
static uint16_t s_recv_target_cap;
static uint16_t s_recv_pdu_len;
static uint8_t  s_recv_pending;

static uint8_t  s_ecsonmi_pending;
static uint8_t  s_ecsonmi_extra;   /* pending 已占用时再来的 URC 计数 */
static uint16_t s_ecsonmi_len;
static uint32_t s_ecsonmi_at_ms;
static uint8_t  s_idle_ecsorf_active;
static uint8_t  s_ecsorf_empty_retry;
static uint8_t  s_ecsorf_mismatch_retry; /* mismatch 后再读次数（与 empty 分计） */
static uint8_t  s_ecsorf_wait_retry;
static uint8_t  s_ecsorf_line_overflow_logged;
static uint8_t  s_ecsorf_last_mismatch;
static uint32_t s_ecsorf_retry_at_ms;

static uint8_t           s_at_active;
static at_phase_t        s_at_phase;
static uint32_t          s_at_start_ms;
static uint32_t          s_at_ok_at_ms;
static uint8_t           s_at_allow_error;
static uint32_t          s_at_wait_after_ok_ms;
static uint32_t          s_at_total_wait_ms;
static char             *s_at_rsp;
static unsigned          s_at_rsp_cap;
static unsigned          s_at_rsp_len;
static uint8_t           s_at_got_ok;
static uint8_t           s_ecsost_uplink_ok;   /* ECSOST 已 OK（ECSORF 重试会清 s_at_got_ok） */
static uint8_t           s_uplink_ok_event;    /* 供 bridge 立刻打 send OK */
static uint8_t           s_downlink_fail_event;/* 上行已 OK 但 ECSORF 放弃 */
static uint8_t           s_downlink_idle_event;/* 上行已 OK 且下行窗口空等无 URC/PDU */
static uint8_t           s_socket_gone_event;  /* CME 406 等：本地 sock 已失效 */
static unsigned          s_at_line_len;

static setup_phase_t s_setup_phase;
static uint32_t      s_cgatt_wait_start_ms; /* 首次 CGATT=0 时刻；0=未进入等待 */
static uint32_t      s_cgatt_next_ms;       /* 下次允许发 CGATT? 的时刻 */

/* Fallback only: start normal 115200 CSCON immediately after existing boot
 * delay. Enter this recovery path only if that first command has no valid
 * response. All fallback timer waits return to the BLE task. */
#define CAT1_BAUD_SETTLE_MS       100U
#define CAT1_BAUD_RUN_WAIT_MS     300U
#define CAT1_BAUD_BOOT_WAIT_MS   1000U
#define CAT1_BAUD_IPR_WAIT_MS     300U
#define CAT1_BAUD_ROUNDS            3U

enum {
    BAUD_SYNC_INIT = 0, BAUD_SYNC_RUN_SETTLE, BAUD_SYNC_RUN_WAIT,
    BAUD_SYNC_BOOT_SETTLE, BAUD_SYNC_BOOT_WAIT, BAUD_SYNC_IPR_WAIT,
    BAUD_SYNC_VERIFY_SETTLE, BAUD_SYNC_VERIFY_WAIT, BAUD_SYNC_DONE,
    BAUD_SYNC_FAIL
};
static uint8_t s_baud_phase, s_baud_round;
static uint32_t s_baud_started_ms, s_baud_changed_ms;

static void cat1_ecso_issue_ecsorf(void);
static void cat1_ecso_try_start_ecsorf_read(void);
static void cat1_ecsonmi_pending_clear(void);
static void cat1_socket_mark_gone(const char *reason);
static int cat1_parse_cme_err(const char *line);

static uint32_t cat1_ms_elapsed(uint32_t since_ms)
{
	uint32_t now_ms = sdk_timer_get_current_ms();

	if (now_ms >= since_ms) {
		return now_ms - since_ms;
	}
	return (0xFFFFFFFFu - since_ms) + now_ms + 1u;
}

static void cat1_ecsonmi_pending_clear(void)
{
	s_ecsonmi_pending = 0u;
	s_ecsonmi_extra = 0u;
	s_ecsonmi_len = 0u;
	s_ecsonmi_at_ms = 0u;
}

/** NT26 方案 B：406=不能找到套接字；本地句柄失效并通知 bridge 重建 */
static void cat1_socket_mark_gone(const char *reason)
{
	co_printf("[CAT1 UDP] socket gone: %s (was sock=%u)\r\n",
	          (reason != NULL) ? reason : "?", (unsigned)s_sock_id);
	s_sock_id = 255u;
	cat1_ecsonmi_pending_clear();
	s_socket_gone_event = 1u;
}

static int cat1_parse_cme_err(const char *line)
{
	const char *p;

	if (line == NULL || strncmp(line, "+CME ERROR:", 11) != 0) {
		return -1;
	}
	p = line + 11;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p < '0' || *p > '9') {
		return -1;
	}
	return atoi(p);
}

static void bin_to_hex_upper(const uint8_t *in, uint16_t inlen, char *hex, unsigned hex_cap)
{
	static const char d[] = "0123456789ABCDEF";
	unsigned i;

	if (in == NULL || hex == NULL || (unsigned)inlen * 2u + 1u > hex_cap) {
		if (hex != NULL) {
			hex[0] = '\0';
		}
		return;
	}

	for (i = 0u; i < (unsigned)inlen; i++) {
		hex[2u * i]     = (char)d[in[i] >> 4];
		hex[2u * i + 1u] = (char)d[in[i] & 0x0Fu];
	}
	hex[2u * (unsigned)inlen] = '\0';
}

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

static int hex_to_bin(const char *hex, unsigned hex_len, uint8_t *out, uint16_t out_cap, uint16_t *out_len)
{
	unsigned i;
	unsigned bin_len;

	if (hex == NULL || out == NULL || out_len == NULL || (hex_len & 1u) != 0u) {
		return -1;
	}
	bin_len = hex_len / 2u;
	if (bin_len > (unsigned)out_cap) {
		return -1;
	}
	for (i = 0u; i < bin_len; i++) {
		int hi = hex_nibble(hex[2u * i]);
		int lo = hex_nibble(hex[2u * i + 1u]);

		if (hi < 0 || lo < 0) {
			return -1;
		}
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	*out_len = (uint16_t)bin_len;
	return 0;
}

static const char *cat1_find_nth_quote(const char *s, int n)
{
	int count = 0;

	if (s == NULL) {
		return NULL;
	}
	while (*s != '\0') {
		if (*s == '"') {
			count++;
			if (count == n) {
				return s;
			}
		}
		s++;
	}
	return NULL;
}

static const char *cat1_ecsorf_parse_line(const char *line)
{
	if (line == NULL || line[0] == '\0') {
		return NULL;
	}
	if (strncmp(line, "+ECSORF:", 8) == 0) {
		return line;
	}
	if (line[0] == '+') {
		return NULL;
	}
	return line;
}

static void cat1_ecsorf_log_parse_fail(const char *line, const char *reason,
                                       unsigned data_len, unsigned hex_len)
{
	char head[CAT1_ECSORF_PARSE_DIAG_HEAD + 1u];
	unsigned n;
	unsigned i;
	unsigned line_len;

	if (line == NULL) {
		line = "";
	}
	line_len = (unsigned)strlen(line);
	n = line_len;
	if (n > CAT1_ECSORF_PARSE_DIAG_HEAD) {
		n = CAT1_ECSORF_PARSE_DIAG_HEAD;
	}
	for (i = 0u; i < n; i++) {
		char c = line[i];

		head[i] = (char)((c >= 0x20 && c < 0x7f) ? c : '.');
	}
	head[n] = '\0';
	CAT1_UDP_LOG("[CAT1 UDP] ECSORF parse fail: %s line_len=%u data_len=%u hex_len=%u cap=%u head=%s\r\n",
	          reason, line_len, data_len, hex_len, (unsigned)s_recv_target_cap, head);
}

static int cat1_ecsorf_try_parse_data_line(const char *line)
{
	const char *parse_line;
	const char *hex_open;
	const char *hex_close;
	const char *after_ip;
	const char *len_start;
	unsigned data_len;
	unsigned hex_len;
	const char *hex;

	parse_line = cat1_ecsorf_parse_line(line);
	if (parse_line == NULL) {
		return 0;
	}

	hex_open = cat1_find_nth_quote(parse_line, 3);
	hex_close = cat1_find_nth_quote(parse_line, 4);
	if (hex_open == NULL || hex_close == NULL || hex_close <= hex_open) {
		/*
		 * 脏行少引号（UART 丢 1B 常见）：先按 "6868"+ECSONMI 长度抢救；
		 * 失败则标 mismatch，OK 后走 mismatch 重试（勿当 empty OK 刷 5 次）。
		 */
		if (s_ecsonmi_len >= 5u
		    && (unsigned)s_ecsonmi_len <= (unsigned)s_recv_target_cap) {
			const char *p6868 = strstr(parse_line, "6868");

			if (p6868 != NULL) {
				unsigned need = (unsigned)s_ecsonmi_len * 2u;
				unsigned avail = (unsigned)strlen(p6868);

				if (avail >= need
				    && hex_to_bin(p6868, need, s_recv_target, s_recv_target_cap,
				                  &s_recv_pdu_len) == 0
				    && s_recv_target != NULL
				    && s_recv_pdu_len == (uint16_t)s_ecsonmi_len) {
					CAT1_UDP_LOG("[CAT1 UDP] ECSORF recover 6868 %uB (no_data_quotes)\r\n",
					          (unsigned)s_ecsonmi_len);
					s_recv_pending = 1u;
					if (s_ecsonmi_extra > 0u) {
						s_ecsonmi_extra--;
						s_ecsonmi_pending = 1u;
						s_ecsonmi_at_ms = sdk_timer_get_current_ms();
					} else {
						s_ecsonmi_pending = 0u;
						s_ecsonmi_len = 0u;
						s_ecsonmi_at_ms = 0u;
					}
					s_ecsorf_empty_retry = 0u;
					s_ecsorf_mismatch_retry = 0u;
					s_ecsorf_wait_retry = 0u;
					s_ecsorf_last_mismatch = 0u;
					return 1;
				}
			}
		}
		if (parse_line[0] == '+' || (parse_line[0] >= '0' && parse_line[0] <= '9')) {
			cat1_ecsorf_log_parse_fail(parse_line, "no_data_quotes", 0u, 0u);
			s_ecsorf_last_mismatch = 1u;
		}
		return -1;
	}

	after_ip = cat1_find_nth_quote(parse_line, 2);
	if (after_ip == NULL) {
		cat1_ecsorf_log_parse_fail(parse_line, "no_ip_quote", 0u, 0u);
		s_ecsorf_last_mismatch = 1u;
		return -1;
	}
	len_start = after_ip + 1u;
	while (*len_start == ',') {
		len_start++;
	}
	while (*len_start != '\0' && *len_start != ',') {
		len_start++;
	}
	if (*len_start != ',') {
		cat1_ecsorf_log_parse_fail(parse_line, "bad_len_field", 0u, 0u);
		s_ecsorf_last_mismatch = 1u;
		return -1;
	}
	len_start++;
	while (*len_start == ' ' || *len_start == '\t') {
		len_start++;
	}
	data_len = (unsigned)atoi(len_start);

	hex = hex_open + 1u;
	hex_len = (unsigned)(hex_close - hex);
	/* 脏行偶发奇数 hex：去掉末尾半个 nibble 再对齐 */
	if ((hex_len & 1u) != 0u && hex_len >= 3u) {
		hex_len -= 1u;
	}
	if (hex_len != data_len * 2u) {
		/*
		 * 模组偶发声明长度与 hex 不一致（UART 并发串线）：
		 * 1) hex 与 ECSONMI 声明等长，或以 6868 开头且对齐 → 信任 hex；
		 * 2) 脏行中搜索 "6868 并按 ecsonmi_len 完整截取；
		 * 3) 其余（含 hex 少 1B）一律 mismatch → 重读，勿把半包当 recv OK。
		 */
		if ((hex_len & 1u) == 0u && hex_len >= 2u) {
			unsigned hex_bytes = hex_len / 2u;

			if (hex_bytes <= (unsigned)s_recv_target_cap && hex_bytes >= 5u
			    && s_ecsonmi_len != 0u && hex_bytes == (unsigned)s_ecsonmi_len) {
				CAT1_UDP_LOG("[CAT1 UDP] ECSORF len trust hex %uB (declared=%u)\r\n",
				          hex_bytes, data_len);
				data_len = hex_bytes;
			} else {
				const char *p6868 = strstr(hex, "6868");

				if (p6868 != NULL && s_ecsonmi_len >= 5u
				    && (unsigned)s_ecsonmi_len <= (unsigned)s_recv_target_cap) {
					unsigned need = (unsigned)s_ecsonmi_len * 2u;
					unsigned avail = hex_len - (unsigned)(p6868 - hex);

					if (avail >= need) {
						hex = p6868;
						hex_len = need;
						data_len = (unsigned)s_ecsonmi_len;
						CAT1_UDP_LOG("[CAT1 UDP] ECSORF recover 6868 %uB from dirty line\r\n",
						          data_len);
					} else {
						CAT1_UDP_LOG("[CAT1 UDP] ECSORF hex short %u/%u, reject\r\n",
						          hex_len, data_len * 2u);
						cat1_ecsorf_log_parse_fail(parse_line, "len_mismatch", data_len, hex_len);
						s_ecsorf_last_mismatch = 1u;
						return -1;
					}
				} else {
					CAT1_UDP_LOG("[CAT1 UDP] ECSORF hex short %u/%u, reject\r\n",
					          hex_len, data_len * 2u);
					cat1_ecsorf_log_parse_fail(parse_line, "len_mismatch", data_len, hex_len);
					s_ecsorf_last_mismatch = 1u;
					return -1;
				}
			}
		} else {
			cat1_ecsorf_log_parse_fail(parse_line, "len_mismatch", data_len, hex_len);
			s_ecsorf_last_mismatch = 1u;
			return -1;
		}
	}
	if (data_len == 0u || data_len > (unsigned)s_recv_target_cap) {
		cat1_ecsorf_log_parse_fail(parse_line, "len_oob", data_len, hex_len);
		s_ecsorf_last_mismatch = 1u;
		return -1;
	}
	if (s_recv_pending != 0u) {
		CAT1_UDP_LOG("[CAT1 UDP] ECSORF pending, drop new line\r\n");
		return 0;
	}
	if (s_recv_allow == 0u) {
		cat1_ecsorf_log_parse_fail(parse_line, "recv_disallowed", data_len, hex_len);
		return 0;
	}
	if (hex_to_bin(hex, hex_len, s_recv_target, s_recv_target_cap, &s_recv_pdu_len) != 0
		|| s_recv_target == NULL || s_recv_pdu_len == 0u
		|| s_recv_pdu_len != data_len) {
		cat1_ecsorf_log_parse_fail(parse_line, "hex_decode", data_len, hex_len);
		return -1;
	}

	s_recv_pending = 1u;
	if (s_ecsonmi_extra > 0u) {
		s_ecsonmi_extra--;
		s_ecsonmi_pending = 1u;
		s_ecsonmi_at_ms = sdk_timer_get_current_ms();
		/* s_ecsonmi_len 保留最近一次 URC 声明长度 */
	} else {
		s_ecsonmi_pending = 0u;
		s_ecsonmi_len = 0u;
		s_ecsonmi_at_ms = 0u;
	}
	s_ecsorf_empty_retry = 0u;
	s_ecsorf_mismatch_retry = 0u;
	s_ecsorf_wait_retry = 0u;
	s_ecsorf_last_mismatch = 0u;
	return 1;
}

static void cat1_on_ecsonmi(uint16_t len)
{
	if (len == 0u) {
		return;
	}
	if (s_ecsonmi_pending != 0u) {
		if (s_ecsonmi_extra < 8u) {
			s_ecsonmi_extra++;
			s_ecsonmi_len = len;
			s_ecsonmi_at_ms = sdk_timer_get_current_ms();
			CAT1_UDP_LOG("[CAT1 UDP] ECSONMI queued extra=%u len=%u\r\n",
			          (unsigned)s_ecsonmi_extra, (unsigned)len);
		} else {
			CAT1_UDP_LOG("[CAT1 UDP] ECSONMI pending, drop new URC\r\n");
		}
		return;
	}
	s_ecsonmi_pending = 1u;
	s_ecsonmi_len = len;
	s_ecsonmi_at_ms = sdk_timer_get_current_ms();
	s_ecsorf_empty_retry = 0u;
	s_ecsorf_mismatch_retry = 0u;
	s_ecsorf_wait_retry = 0u;
	s_ecsorf_last_mismatch = 0u;
	CAT1_UDP_LOG("[CAT1 UDP] ECSONMI recv sock=%u len=%u\r\n",
	          (unsigned)s_sock_id, (unsigned)len);
}

static int cat1_ecso_try_parse_ecsonmi_line(const char *line)
{
	const char *p;
	unsigned sock;
	unsigned len;

	if (line == NULL || strncmp(line, "+ECSONMI:", 9) != 0) {
		return 0;
	}

	p = line + 9;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	sock = (unsigned)atoi(p);
	while (*p != '\0' && *p != ',') {
		p++;
	}
	if (*p != ',') {
		return -1;
	}
	p++;
	len = (unsigned)atoi(p);
	if (sock != (unsigned)s_sock_id && s_sock_id != 255u) {
		CAT1_UDP_LOG("[CAT1 UDP] ECSONMI sock=%u expect=%u\r\n",
		          sock, (unsigned)s_sock_id);
		return 0;
	}
	cat1_on_ecsonmi((uint16_t)len);
	return 1;
}

/**
 * @brief  仅拼 +ECSONMI URC；匹配失败时把已吃字节交回命令行（见 s_urc_spill）
 * @return 1=本字节归 URC；0=归命令应答（可能先附带 spill）
 */
static char     s_urc_spill[12];
static uint8_t  s_urc_spill_len;

static uint8_t cat1_skt_udp_feed_char(char c)
{
	static const char s_ecsonmi_pref[] = "+ECSONMI:";

	if (c == '\r') {
		return (uint8_t)(s_urc_line_len > 0u ? 1u : 0u);
	}
	if (c == '\n') {
		if (s_urc_line_len > 0u) {
			s_ecsonmi_line[s_urc_line_len] = '\0';
			if (strncmp(s_ecsonmi_line, s_ecsonmi_pref, 9u) == 0) {
				(void)cat1_ecso_try_parse_ecsonmi_line(s_ecsonmi_line);
				s_urc_line_len = 0u;
				return 1u;
			}
			/* +CSCON URC: 0=idle/PSM-ready, 1=connected */
			if (strncmp(s_ecsonmi_line, "+CSCON:", 7) == 0) {
				co_printf("[CAT1 URC] %s\r\n", s_ecsonmi_line);
				s_urc_line_len = 0u;
				return 1u;
			}
			/* other + lines: spill to cmd response */
			if (s_urc_line_len < sizeof(s_urc_spill)) {
				memcpy(s_urc_spill, s_ecsonmi_line, s_urc_line_len);
				s_urc_spill_len = (uint8_t)s_urc_line_len;
			}
			s_urc_line_len = 0u;
			return 0u;
		}
		return 0u;
	}

	if (s_urc_line_len == 0u && c != '+') {
		return 0u;
	}

    /* Buffer only the two URCs. Ordinary +ECICCID/+CCLK/+ECSORF and
     * +CME ERROR lines must stream to the AT parser, including long bodies.
     * A 12-byte prefix spill cannot hold a complete 20-byte ICCID line. */
    if(s_urc_line_len<9u){
        uint8_t ecso=(c==s_ecsonmi_pref[s_urc_line_len]);
        uint8_t cscon=s_urc_line_len<7u ? c=="+CSCON:"[s_urc_line_len] :
            strncmp(s_ecsonmi_line,"+CSCON:",7u)==0;
        if(!ecso && !cscon){
            if(s_urc_line_len){memcpy(s_urc_spill,s_ecsonmi_line,s_urc_line_len);s_urc_spill_len=(uint8_t)s_urc_line_len;}
            s_urc_line_len=0;return 0u;
        }
    }

	if (s_urc_line_len + 1u < sizeof(s_ecsonmi_line)) {
		s_ecsonmi_line[s_urc_line_len++] = c;
	} else {
		co_printf("[CAT1 UDP] ECSONMI line overflow, drop\r\n");
		s_urc_line_len = 0u;
	}
	return 1u;
}

static void cat1_modem_finish(cat1_modem_result_t result)
{
	(void)result;
	s_at_active = 0u;
	s_at_phase = AT_PHASE_IDLE;
	s_idle_ecsorf_active = 0u;
	s_ecsorf_wait_retry = 0u;
	s_urc_spill_len = 0u;
}

static void cat1_modem_note_ecsost_ok(void)
{
	s_ecsost_uplink_ok = 1u;
	/* 业务 wait_after_ok>0：上报成功事件；OTA wait=0 由 bridge 在 complete 打日志 */
	if (s_at_wait_after_ok_ms != 0u) {
		s_uplink_ok_event = 1u;
	}
}

uint8_t cat1_skt_udp_take_uplink_ok_event(void)
{
	uint8_t ev = s_uplink_ok_event;
	s_uplink_ok_event = 0u;
	return ev;
}

uint8_t cat1_skt_udp_take_downlink_fail_event(void)
{
	uint8_t ev = s_downlink_fail_event;
	s_downlink_fail_event = 0u;
	return ev;
}

uint8_t cat1_skt_udp_take_downlink_idle_event(void)
{
	uint8_t ev = s_downlink_idle_event;
	s_downlink_idle_event = 0u;
	return ev;
}

uint8_t cat1_skt_udp_take_socket_gone_event(void)
{
	uint8_t ev = s_socket_gone_event;
	s_socket_gone_event = 0u;
	return ev;
}

static cat1_modem_result_t cat1_ecsorf_finish_give_up(void)
{
	cat1_ecsonmi_pending_clear();
	if (s_ecsost_uplink_ok != 0u) {
		s_downlink_fail_event = 1u;
		cat1_modem_finish(CAT1_MODEM_OK);
		return CAT1_MODEM_OK;
	}
	cat1_modem_finish(CAT1_MODEM_FAIL);
	return CAT1_MODEM_FAIL;
}

static void cat1_ecsorf_schedule_empty_retry(void)
{
	s_at_got_ok = 0u;
	s_at_line_len = 0u;
	s_at_phase = AT_PHASE_WAIT_DOWNLINK;
	s_ecsorf_retry_at_ms = sdk_timer_get_current_ms();
	s_ecsorf_wait_retry = 1u;
}

static cat1_modem_result_t cat1_ecsorf_handle_empty_ok(void)
{
	if (s_recv_pending != 0u) {
		cat1_modem_finish(CAT1_MODEM_OK);
		return CAT1_MODEM_OK;
	}
	/* mismatch：最多再试 MISMATCH_RETRY_MAX 次；其后 empty 直接放弃 */
	if (s_ecsorf_last_mismatch != 0u) {
		s_ecsorf_last_mismatch = 0u;
		if (s_ecsorf_mismatch_retry < CAT1_ECSORF_MISMATCH_RETRY_MAX) {
			s_ecsorf_mismatch_retry++;
			CAT1_UDP_LOG("[CAT1 UDP] ECSORF mismatch, retry %u/%u (ecsonmi=%u)\r\n",
			          (unsigned)s_ecsorf_mismatch_retry,
			          (unsigned)CAT1_ECSORF_MISMATCH_RETRY_MAX,
			          (unsigned)s_ecsonmi_len);
			cat1_ecsorf_schedule_empty_retry();
			return CAT1_MODEM_BUSY;
		}
		co_printf("[CAT1 UDP] ECSORF mismatch, give up (ecsonmi=%u)\r\n",
		          (unsigned)s_ecsonmi_len);
		return cat1_ecsorf_finish_give_up();
	}
	if (s_ecsorf_mismatch_retry > 0u) {
		co_printf("[CAT1 UDP] ECSORF empty after mismatch retry, give up (ecsonmi=%u)\r\n",
		          (unsigned)s_ecsonmi_len);
		return cat1_ecsorf_finish_give_up();
	}
	if (s_ecsorf_empty_retry < CAT1_ECSORF_EMPTY_RETRY_MAX) {
		s_ecsorf_empty_retry++;
		CAT1_UDP_LOG("[CAT1 UDP] ECSORF empty OK, retry %u/%u (ecsonmi=%u)\r\n",
		          (unsigned)s_ecsorf_empty_retry,
		          (unsigned)CAT1_ECSORF_EMPTY_RETRY_MAX,
		          (unsigned)s_ecsonmi_len);
		cat1_ecsorf_schedule_empty_retry();
		return CAT1_MODEM_BUSY;
	}
	co_printf("[CAT1 UDP] ECSORF no data after %u retries, ecsonmi_len=%u\r\n",
	          (unsigned)CAT1_ECSORF_EMPTY_RETRY_MAX, (unsigned)s_ecsonmi_len);
	return cat1_ecsorf_finish_give_up();
}

static uint8_t cat1_ecsorf_delay_elapsed(void)
{
	uint32_t need_ms;
	uint32_t since_ms;

	if (s_ecsorf_empty_retry > 0u || s_ecsorf_mismatch_retry > 0u) {
		if (s_ecsorf_wait_retry == 0u) {
			return 1u;
		}
		need_ms = CAT1_ECSORF_RETRY_DELAY_MS;
		since_ms = s_ecsorf_retry_at_ms;
	} else {
		need_ms = CAT1_ECSORF_FIRST_DELAY_MS;
		since_ms = s_ecsonmi_at_ms;
	}
	return (uint8_t)(cat1_ms_elapsed(since_ms) >= need_ms);
}

static void cat1_ecsorf_arm_delay(void)
{
	if (s_ecsorf_wait_retry != 0u) {
		return;
	}
	/* 首次读 FIRST_DELAY=0：不等待；仅空 OK 重试需要延时 */
	if (s_ecsorf_empty_retry == 0u) {
		return;
	}
	s_ecsorf_retry_at_ms = sdk_timer_get_current_ms();
	s_ecsorf_wait_retry = 1u;
	s_at_got_ok = 0u;
	s_at_line_len = 0u;
	if (s_at_phase == AT_PHASE_WAIT_ECSORF) {
		s_at_phase = AT_PHASE_WAIT_DOWNLINK;
	}
}

static void cat1_ecsorf_try_retry_after_delay(void)
{
	if (s_ecsorf_wait_retry == 0u || s_ecsonmi_pending == 0u || s_at_active == 0u) {
		return;
	}
	if (s_at_phase == AT_PHASE_WAIT_ECSORF) {
		return;
	}
	cat1_ecso_try_start_ecsorf_read();
}

static uint8_t s_wait_downlink_chunk;

static void cat1_modem_begin_wait(char *rsp, unsigned cap, uint8_t allow_error,
                                  uint32_t wait_after_ok_ms)
{
	s_at_active = 1u;
	s_at_phase = AT_PHASE_WAIT_OK;
	s_at_start_ms = sdk_timer_get_current_ms();
	s_at_ok_at_ms = 0u;
	s_at_allow_error = allow_error;
	s_at_wait_after_ok_ms = wait_after_ok_ms;
	s_at_total_wait_ms = (wait_after_ok_ms == 0u) ? CAT1_OTA_AT_TOTAL_WAIT_MS
	                                               : CAT1_AT_TOTAL_WAIT_MS;
	s_ota_uplink_wait = (uint8_t)(wait_after_ok_ms == 0u && s_wait_downlink_chunk != 0u
	                              && rsp == NULL);
	s_at_rsp = rsp;
	s_at_rsp_cap = cap;
	s_at_rsp_len = 0u;
	s_at_got_ok = 0u;
	s_ecsost_uplink_ok = 0u;
	s_uplink_ok_event = 0u;
	s_downlink_fail_event = 0u;
	s_downlink_idle_event = 0u;
	s_at_line_len = 0u;
	s_urc_spill_len = 0u;

	if (rsp != NULL && cap > 0u) {
		rsp[0] = '\0';
	}
}

static void cat1_ecso_issue_ecsorf(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+ECSORF=%u,%u",
	         (unsigned)s_sock_id, (unsigned)CAT1_ECSORF_READ_MAX);
	usart1_at_send(s_modem_line, (unsigned)sizeof(s_modem_line));
	s_at_phase = AT_PHASE_WAIT_ECSORF;
	s_at_got_ok = 0u;
	s_at_line_len = 0u;
	s_at_rsp = s_modem_rsp;
	s_at_rsp_cap = (unsigned)sizeof(s_modem_rsp);
	s_at_rsp_len = 0u;
	s_modem_rsp[0] = '\0';
	s_ecsorf_line_overflow_logged = 0u;
}

static void cat1_ecso_try_start_ecsorf_read(void)
{
	if (s_ecsonmi_pending == 0u || s_sock_id == 255u) {
		return;
	}
	if (s_at_active != 0u) {
		if (s_at_phase == AT_PHASE_WAIT_ECSORF) {
			return;
		}
		if (s_at_phase != AT_PHASE_WAIT_DOWNLINK && s_ota_uplink_wait == 0u) {
			return;
		}
	} else if (s_idle_ecsorf_active == 0u) {
		s_idle_ecsorf_active = 1u;
		s_at_active = 1u;
		s_at_start_ms = sdk_timer_get_current_ms();
		s_at_total_wait_ms = CAT1_AT_TOTAL_WAIT_MS;
		s_at_allow_error = 0u;
		s_at_wait_after_ok_ms = 0u;
		s_ota_uplink_wait = 0u;
	}
	if (cat1_ecsorf_delay_elapsed() == 0u) {
		cat1_ecsorf_arm_delay();
		return;
	}
	s_ecsorf_wait_retry = 0u;
	cat1_ecso_issue_ecsorf();
}

uint8_t cat1_modem_is_busy(void)
{
	return s_at_active;
}

void cat1_skt_udp_finish_active_ok(void)
{
	if (s_at_active != 0u) {
		cat1_modem_finish(CAT1_MODEM_OK);
	}
}

static cat1_modem_result_t cat1_modem_process_byte(uint8_t b)
{
	uint8_t urc_taken = cat1_skt_udp_feed_char((char)b);
	uint8_t i;

	/* 误判 URC 后吐回的前缀，并入命令应答行 */
	if (s_urc_spill_len > 0u) {
		for (i = 0u; i < s_urc_spill_len; i++) {
			if (s_at_line_len + 1u < CAT1_AT_LINE_MAX) {
				s_modem_line[s_at_line_len++] = s_urc_spill[i];
			}
		}
		s_urc_spill_len = 0u;
	}

	if (s_at_rsp != NULL && s_at_rsp_cap > 0u && s_at_rsp_len + 1u < s_at_rsp_cap) {
		s_at_rsp[s_at_rsp_len++] = (char)b;
		s_at_rsp[s_at_rsp_len] = '\0';
	}

	if (b == '\n') {
		if (urc_taken != 0u) {
			s_at_line_len = 0u;
			if (s_ecsonmi_pending != 0u
			    && (s_at_phase == AT_PHASE_WAIT_DOWNLINK || s_ota_uplink_wait != 0u)) {
				cat1_ecso_try_start_ecsorf_read();
			}
			cat1_ecsorf_try_retry_after_delay();
			if (s_at_got_ok != 0u && s_recv_pending != 0u && s_at_phase != AT_PHASE_WAIT_ECSORF) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			return CAT1_MODEM_BUSY;
		}
		s_modem_line[s_at_line_len] = '\0';
		if (s_at_phase == AT_PHASE_WAIT_ECSORF && s_at_line_len > 0u
		    && strncmp(s_modem_line, "OK", 2) != 0
		    && strncmp(s_modem_line, "ERROR", 5) != 0
		    && strncmp(s_modem_line, "+CME ERROR:", 11) != 0) {
			(void)cat1_ecsorf_try_parse_data_line(s_modem_line);
		}

		if (s_at_line_len == 2u && s_modem_line[0] == 'O' && s_modem_line[1] == 'K') {
			s_at_got_ok = 1u;
			s_at_ok_at_ms = sdk_timer_get_current_ms();
			if (s_at_phase == AT_PHASE_WAIT_ECSORF) {
				return cat1_ecsorf_handle_empty_ok();
			}
			if (s_at_phase == AT_PHASE_WAIT_OK) {
				cat1_modem_note_ecsost_ok();
			}
			if (s_at_wait_after_ok_ms == 0u && s_ota_uplink_wait != 0u) {
				if (s_recv_pending != 0u) {
					cat1_modem_finish(CAT1_MODEM_OK);
					return CAT1_MODEM_OK;
				}
				s_at_phase = AT_PHASE_WAIT_DOWNLINK;
				s_at_line_len = 0u;
				cat1_ecso_try_start_ecsorf_read();
				return CAT1_MODEM_BUSY;
			}
			if (s_at_wait_after_ok_ms == 0u) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			s_at_phase = AT_PHASE_WAIT_DOWNLINK;
		} else if (s_at_line_len == 5u && memcmp(s_modem_line, "ERROR", 5) == 0) {
			if (s_at_allow_error != 0u) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			if (s_at_phase == AT_PHASE_WAIT_ECSORF || s_idle_ecsorf_active != 0u) {
				co_printf("[CAT1 UDP] AT ERROR (ECSORF), give up pending\r\n");
				return cat1_ecsorf_finish_give_up();
			}
			if (s_at_got_ok == 0u) {
				cat1_ecsonmi_pending_clear();
			}
			co_printf("[CAT1 UDP] AT ERROR\r\n");
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		} else if (s_at_line_len >= 11u
				   && strncmp(s_modem_line, "+CME ERROR:", 11) == 0) {
			int cme_err;

			if (s_at_allow_error != 0u) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			cme_err = cat1_parse_cme_err(s_modem_line);
			co_printf("[CAT1 UDP] %s\r\n", s_modem_line);
			/* 406=不能找到套接字：失效本地句柄，禁止 ECSORF 死循环 */
			if (cme_err == 406) {
				cat1_socket_mark_gone("CME 406");
				cat1_modem_finish(CAT1_MODEM_FAIL);
				return CAT1_MODEM_FAIL;
			}
			if (s_at_phase == AT_PHASE_WAIT_ECSORF || s_idle_ecsorf_active != 0u) {
				return cat1_ecsorf_finish_give_up();
			}
			if (s_at_got_ok == 0u) {
				cat1_ecsonmi_pending_clear();
			}
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		}
		s_at_line_len = 0u;
	} else if (b != '\r' && urc_taken == 0u) {
		if (s_at_line_len + 1u < CAT1_AT_LINE_MAX) {
			s_modem_line[s_at_line_len++] = (char)b;
		} else if (s_at_phase == AT_PHASE_WAIT_ECSORF
		           && s_ecsorf_line_overflow_logged == 0u) {
			s_ecsorf_line_overflow_logged = 1u;
			co_printf("[CAT1 UDP] ECSORF line overflow cap=%u\r\n",
			          (unsigned)CAT1_AT_LINE_MAX);
		}
	} else if (urc_taken != 0u && s_urc_line_len == 1u) {
		/* 命令应答行被 URC 打断：丢弃半行，避免 ECSORF 脏数据 */
		s_at_line_len = 0u;
	}

	if (s_ecsonmi_pending != 0u
	    && (s_at_phase == AT_PHASE_WAIT_DOWNLINK || s_ota_uplink_wait != 0u)) {
		cat1_ecso_try_start_ecsorf_read();
	}

	cat1_ecsorf_try_retry_after_delay();

	if (s_at_got_ok != 0u && s_recv_pending != 0u && s_at_phase != AT_PHASE_WAIT_ECSORF) {
		cat1_modem_finish(CAT1_MODEM_OK);
		return CAT1_MODEM_OK;
	}

	return CAT1_MODEM_BUSY;
}

static cat1_modem_result_t cat1_modem_check_timeout(void)
{
	cat1_ecsorf_try_retry_after_delay();

	if (cat1_ms_elapsed(s_at_start_ms) >= s_at_total_wait_ms) {
        /* The first normal CSCON also checks UART reachability. The legacy
         * optional-diagnostic rule accepts an empty timeout; here silence
         * must trigger recovery. Complete OK/ERROR replies finish earlier. */
        if (s_setup_phase == SETUP_PHASE_CSCON && s_baud_phase == BAUD_SYNC_INIT &&
            s_at_got_ok == 0U) {
            cat1_ecsonmi_pending_clear();
            cat1_modem_finish(CAT1_MODEM_FAIL);
            return CAT1_MODEM_FAIL;
        }
		if (s_recv_pending != 0u) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
		if (s_at_allow_error != 0u
			&& (s_at_rsp == NULL || s_at_rsp[0] == '\0'
				|| strstr(s_at_rsp, "OK") != NULL || strstr(s_at_rsp, "ERROR") != NULL)) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
		if (s_ota_uplink_wait != 0u && s_recv_pending == 0u) {
			co_printf("[CAT1 UDP] ECSOST OK but no chunk\r\n");
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		}
		if (s_ecsost_uplink_ok == 0u && s_at_got_ok == 0u) {
			co_printf("[CAT1 UDP] AT timeout phase=%u\r\n", (unsigned)s_at_phase);
			/* 未拿到 OK 时不要用残留 ECSONMI 去 ECSORF，否则易刷 CME 406 */
			cat1_ecsonmi_pending_clear();
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		}
		cat1_modem_finish(CAT1_MODEM_OK);
		return CAT1_MODEM_OK;
	}

	if (s_ecsonmi_pending != 0u
	    && (s_at_phase == AT_PHASE_WAIT_DOWNLINK || s_ota_uplink_wait != 0u)) {
		cat1_ecso_try_start_ecsorf_read();
	}

	if (s_ecsost_uplink_ok != 0u || s_at_got_ok != 0u) {
		if (s_recv_pending != 0u) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
		if (s_ota_uplink_wait != 0u && s_at_ok_at_ms != 0u
		    && cat1_ms_elapsed(s_at_ok_at_ms) >= CAT1_OTA_CHUNK_RETRY_MS) {
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		}
		if (s_at_wait_after_ok_ms != 0u && s_at_ok_at_ms != 0u
			&& cat1_ms_elapsed(s_at_ok_at_ms) >= s_at_wait_after_ok_ms) {
			/* 窗口到点仍在读 / 已有 URC：不打断，改由总超时或 ECSORF 收尾 */
			if (s_at_phase == AT_PHASE_WAIT_ECSORF || s_ecsonmi_pending != 0u) {
				return CAT1_MODEM_BUSY;
			}
			if (s_ecsost_uplink_ok != 0u) {
				s_downlink_idle_event = 1u;
			}
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
	}

	return CAT1_MODEM_BUSY;
}

cat1_modem_result_t cat1_modem_step(void)
{
	uint8_t b;
	cat1_modem_result_t rc;

	if (s_at_active == 0u) {
		return CAT1_MODEM_IDLE;
	}

	while (USART1_CAT1_RxPop(&b) != 0U) {
		rc = cat1_modem_process_byte(b);
		if (rc != CAT1_MODEM_BUSY) {
			return rc;
		}
	}

	return cat1_modem_check_timeout();
}

void cat1_skt_udp_bind_recv_buf(uint8_t *buf, uint16_t cap)
{
	s_recv_target = buf;
	s_recv_target_cap = cap;
}

void cat1_skt_udp_set_recv_allow(uint8_t allow)
{
	s_recv_allow = (allow != 0u) ? 1u : 0u;
}

void cat1_skt_udp_pump_urc(void)
{
	uint8_t b;

	if (s_at_active != 0u) {
		return;
	}

	while (USART1_CAT1_RxPop(&b) != 0U) {
		cat1_skt_udp_feed_char((char)b);
	}
	/* 空闲且仍有 +ECSONMI：立刻起 ECSORF，不等下一笔业务发送 */
	if (s_ecsonmi_pending != 0u && s_at_active == 0u) {
		cat1_ecso_try_start_ecsorf_read();
	}
}

void cat1_skt_udp_drain_urc(void)
{
	cat1_skt_udp_pump_urc();
}

uint8_t cat1_skt_udp_recv_pending(void)
{
	return s_recv_pending;
}

void cat1_skt_udp_force_fail_active(void)
{
	if (s_at_active != 0u) {
		s_uplink_ok_event = 0u;
		s_downlink_fail_event = 0u;
		s_downlink_idle_event = 0u;
		cat1_modem_finish(CAT1_MODEM_FAIL);
	}
}

static cat1_modem_result_t cat1_idle_ecsorf_step(void)
{
	cat1_modem_result_t mr;

	if (s_idle_ecsorf_active == 0u) {
		return CAT1_MODEM_IDLE;
	}
	mr = cat1_modem_step();
	if (mr == CAT1_MODEM_BUSY) {
		return CAT1_MODEM_BUSY;
	}
	s_idle_ecsorf_active = 0u;
	return mr;
}

int cat1_skt_udp_poll_recv(uint8_t *pdu, uint16_t pdu_cap, uint16_t *pdu_len)
{
	cat1_modem_result_t mr;

	cat1_skt_udp_pump_urc();

	if (s_ecsonmi_pending != 0u && s_at_active == 0u) {
		cat1_ecso_try_start_ecsorf_read();
	}
	if (s_idle_ecsorf_active != 0u) {
		mr = cat1_idle_ecsorf_step();
		if (mr == CAT1_MODEM_BUSY) {
			return 0;
		}
		if (mr == CAT1_MODEM_FAIL) {
			if (s_sock_id != 255u && s_ecsonmi_extra > 0u) {
				s_ecsonmi_extra--;
				s_ecsonmi_pending = 1u;
				s_ecsonmi_at_ms = sdk_timer_get_current_ms();
			} else {
				cat1_ecsonmi_pending_clear();
			}
			return -1;
		}
	}

	if (s_recv_pending == 0u) {
		return 0;
	}
	if (pdu == NULL || pdu_len == NULL || s_recv_target == NULL) {
		return -1;
	}
	if (s_recv_pdu_len > pdu_cap || s_recv_pdu_len > s_recv_target_cap) {
		s_recv_pending = 0u;
		s_recv_pdu_len = 0u;
		return -1;
	}

	if (pdu != s_recv_target) {
		memcpy(pdu, s_recv_target, s_recv_pdu_len);
	}
	*pdu_len = s_recv_pdu_len;
	s_recv_pending = 0u;
	s_recv_pdu_len = 0u;
	return 1;
}

static void cat1_rx_discard_all(void)
{
	USART1_CAT1_RxFlush();
	s_urc_line_len = 0u;
}

static void cat1_send_at_line(char *line)
{
	USART1_CAT1_RxFlush();
	usart1_at_send(line, (unsigned)sizeof(s_modem_line));
}

static void cat1_baud_issue(const char *cmd, uint32_t timeout_ms)
{
    strncpy(s_modem_line, cmd, sizeof(s_modem_line)-1U);
    s_modem_line[sizeof(s_modem_line)-1U] = '\0';
    s_urc_line_len = 0U;
    cat1_send_at_line(s_modem_line);
    cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 0U, 0U);
    s_at_total_wait_ms = timeout_ms;
}

static int cat1_baud_fail(void)
{
    (void)USART1_CAT1_SetBaudrate(115200U);
    s_baud_phase = BAUD_SYNC_FAIL;
    co_printf("[CAT1] baud sync failed (115200/9600)\r\n");
    return CAT1_SETUP_FAIL;
}

static int cat1_baud_retry(void)
{
    if (++s_baud_round >= CAT1_BAUD_ROUNDS) return cat1_baud_fail();
    if (USART1_CAT1_SetBaudrate(115200U) != 0) return cat1_baud_fail();
    s_baud_changed_ms = sdk_timer_get_current_ms();
    s_baud_phase = BAUD_SYNC_RUN_SETTLE;
    return CAT1_SETUP_BUSY;
}

static int cat1_baud_sync_step(void)
{
    cat1_modem_result_t mr;
    uint32_t now = sdk_timer_get_current_ms();
    if (s_baud_phase == BAUD_SYNC_DONE) return CAT1_SETUP_OK;
    if (s_baud_phase == BAUD_SYNC_FAIL) return CAT1_SETUP_FAIL;
    if (s_baud_phase == BAUD_SYNC_INIT) {
        s_baud_started_ms = s_baud_changed_ms = now;
        s_baud_round = 0U;
        if (USART1_CAT1_SetBaudrate(9600U) != 0) return cat1_baud_fail();
        s_baud_phase = BAUD_SYNC_BOOT_SETTLE;
        return CAT1_SETUP_BUSY;
    }
    if (s_baud_phase == BAUD_SYNC_RUN_SETTLE || s_baud_phase == BAUD_SYNC_BOOT_SETTLE ||
        s_baud_phase == BAUD_SYNC_VERIFY_SETTLE) {
        if ((uint32_t)(now-s_baud_changed_ms) < CAT1_BAUD_SETTLE_MS) return CAT1_SETUP_BUSY;
        if (s_baud_phase == BAUD_SYNC_RUN_SETTLE) {
            cat1_baud_issue("AT",CAT1_BAUD_RUN_WAIT_MS);
            s_baud_phase = BAUD_SYNC_RUN_WAIT;
        } else if (s_baud_phase == BAUD_SYNC_BOOT_SETTLE) {
            cat1_baud_issue("AT",CAT1_BAUD_BOOT_WAIT_MS);
            s_baud_phase = BAUD_SYNC_BOOT_WAIT;
        } else {
            cat1_baud_issue("AT",CAT1_BAUD_BOOT_WAIT_MS);
            s_baud_phase = BAUD_SYNC_VERIFY_WAIT;
        }
        return CAT1_SETUP_BUSY;
    }
    mr = cat1_modem_step();
    if (mr == CAT1_MODEM_BUSY) return CAT1_SETUP_BUSY;
    if (s_baud_phase == BAUD_SYNC_RUN_WAIT || s_baud_phase == BAUD_SYNC_VERIFY_WAIT) {
        if (mr == CAT1_MODEM_OK) {
            s_baud_phase = BAUD_SYNC_DONE;
            co_printf("[CAT1] baud 115200 ready, sync=%u ms\r\n",
                      (unsigned)(now-s_baud_started_ms));
            return CAT1_SETUP_OK;
        }
        if (s_baud_phase == BAUD_SYNC_VERIFY_WAIT) return cat1_baud_retry();
        if (USART1_CAT1_SetBaudrate(9600U) != 0) return cat1_baud_fail();
        s_baud_changed_ms = now;
        s_baud_phase = BAUD_SYNC_BOOT_SETTLE;
    } else if (s_baud_phase == BAUD_SYNC_BOOT_WAIT) {
        if (mr != CAT1_MODEM_OK) return cat1_baud_retry();
        co_printf("[CAT1] baud 9600 confirmed, set 115200\r\n");
        cat1_baud_issue("AT+IPR=115200",CAT1_BAUD_IPR_WAIT_MS);
        s_baud_phase = BAUD_SYNC_IPR_WAIT;
    } else if (s_baud_phase == BAUD_SYNC_IPR_WAIT) {
        /* IPR may answer at either old/new baud: only the following AT at
         * 115200 proves success. An IPR OK alone is never sufficient. */
        if (USART1_CAT1_SetBaudrate(115200U) != 0) return cat1_baud_fail();
        s_baud_changed_ms = now;
        s_baud_phase = BAUD_SYNC_VERIFY_SETTLE;
    } else return cat1_baud_fail();
    return CAT1_SETUP_BUSY;
}

static void cat1_setup_log_rsp(const char *tag)
{
	if (s_modem_rsp[0] != '\0') {
		co_printf("[CAT1] %s rsp: %s\r\n", tag, s_modem_rsp);
	} else {
		co_printf("[CAT1] %s rsp: (empty)\r\n", tag);
	}
}

static int cat1_setup_parse_ecsocr_id(void)
{
	const char *p = s_modem_rsp;
	char *end;
	long id;

	while (p != NULL && *p != '\0') {
		while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		if (*p == 'O') {
			break;
		}
		id = strtol(p, &end, 10);
		if (end > p && id >= 0 && id <= 254) {
			s_sock_id = (uint8_t)((unsigned long)id);
			return 0;
		}
		p = end;
	}

	co_printf("[CAT1 UDP] bad ECSOCR id rsp: %s\r\n", s_modem_rsp);
	return -1;
}

static int cat1_setup_issue_cscon(void)
{
	strncpy(s_modem_line, "AT+CSCON=1", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CSCON;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_cereg(void)
{
	strncpy(s_modem_line, "AT+CEREG=0", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CEREG;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_csq(void)
{
	strncpy(s_modem_line, "AT+CSQ", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CSQ;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_iccid(void)
{
	strncpy(s_modem_line, "AT+ECICCID", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_ICCID;
	return CAT1_SETUP_BUSY;
}

/**
 * @brief  从 AT+ECICCID 响应中提取 20 位 SIM 卡号并转 BCD10 存储
 * @note   响应不完整时保留旧值; 心跳 Tag0x0A 通过 cat1_iccid_get_raw 读取
 */
static void cat1_iccid_capture_from_rsp(const char *rsp)
{
	const char *p;
	uint8_t digits[20];
	uint8_t nd = 0u;
	uint8_t i;

	if (rsp == NULL) {
		return;
	}
	p = strstr(rsp, "ECICCID");
	if (p == NULL) {
		co_printf("[CAT1] ICCID rsp has no ECICCID\r\n");
		return;
	}
	/*
	 * ★ 必须跳过关键字 "ECICCID" 本身(7字符): 关键字里的 E/C/C/C/D
	 * 也是合法十六进制字符, 从关键字开头收集会把它们当卡号收进去
	 * (实测上报 ECCCD898604F41525D20 — 头部多了 ECCCD, 尾部少 5 位)。
	 * 跳过后从冒号后的卡号原文开始收集。
	 */
	p += 7u;
	/*
	 * 卡号原文可含字母(实测含 F/D): 原样保存。
	 * 传输编码待与服务器协商: BCD10 带不了字母, 拟改 HEX,
	 * 协商确定前 Tag0x0A 的 SIM 字段暂填 0。
	 * 此处只收 [0-9A-Fa-f] 字符, 最长 20。
	 */
	while (*p != '\0' && *p != '\r' && *p != '\n' && nd < 20u) {
		char c = *p;

		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
		    || (c >= 'a' && c <= 'f')) {
			digits[nd++] = (uint8_t)c;
		}
		p++;
	}
	if (nd < 18u) {
		co_printf("[CAT1] ICCID capture incomplete (%u chars)\r\n", (unsigned)nd);
		return;   /* 不完整: 保留旧值 */
	}
	memcpy(s_iccid_raw, digits, nd);
	s_iccid_len = nd;
	s_iccid_valid = 1u;
	co_printf("[CAT1] ICCID captured raw (%u chars): ", (unsigned)nd);
	for (i = 0u; i < nd; i++) {
		co_printf("%c", (char)s_iccid_raw[i]);
	}
	co_printf("\r\n");
}

/**
 * @brief  读取 SIM 卡号原文 (setup 阶段 AT+ECICCID 捕获, 可含字母)
 * @return 0=成功; -1=尚未捕获
 */
int cat1_iccid_get_raw(uint8_t out[20], uint8_t *out_len)
{
	if (s_iccid_valid == 0u || out == NULL || out_len == NULL) {
		return -1;
	}
	memcpy(out, s_iccid_raw, s_iccid_len);
	*out_len = s_iccid_len;
	return 0;
}

static int cat1_setup_issue_cgatt(void)
{
	strncpy(s_modem_line, "AT+CGATT?", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 0u, 0u);
	s_setup_phase = SETUP_PHASE_CGATT;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_cgpaddr(void)
{
	strncpy(s_modem_line, "AT+CGPADDR", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CGPADDR;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_cgatt_attached(void)
{
	return (s_modem_rsp[0] != '\0' && strstr(s_modem_rsp, "+CGATT: 1") != NULL) ? 0 : -1;
}

static int cat1_setup_issue_close(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+ECSOCL=%u",
	         (unsigned)CAT1_ECSO_SOCKET_ID);
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CLOSE;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_create(void)
{
	strncpy(s_modem_line, "AT+ECSOCR=DGRAM,17,0,1", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 0u, 0u);
	s_setup_phase = SETUP_PHASE_CREATE;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_ecsonmi(void)
{
	strncpy(s_modem_line, "AT+ECSONMI=1", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 0u, 0u);
	s_setup_phase = SETUP_PHASE_ECSONMI;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_freq(void)
{
	strncpy(s_modem_line, "AT+ECFREQ?", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_FREQ;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_cclk(void)
{
	strncpy(s_modem_line, "AT+CCLK?", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_rsp, (unsigned)sizeof(s_modem_rsp), 1u, 0u);
	s_setup_phase = SETUP_PHASE_CCLK;
	return CAT1_SETUP_BUSY;
}

static void cat1_setup_on_diag_done(setup_phase_t phase, uint8_t ok)
{
	switch (phase) {
	case SETUP_PHASE_CSCON:
		cat1_setup_log_rsp("AT+CSCON=1");
		s_setup_phase = SETUP_PHASE_CEREG;
		break;
	case SETUP_PHASE_CEREG:
		cat1_setup_log_rsp("AT+CEREG=0");
		s_setup_phase = SETUP_PHASE_CSQ;
		break;
	case SETUP_PHASE_CSQ:
		cat1_setup_log_rsp("AT+CSQ");
		s_setup_phase = SETUP_PHASE_ICCID_PENDING;
		break;
	case SETUP_PHASE_ICCID:
		cat1_iccid_capture_from_rsp(s_modem_rsp);
		cat1_setup_log_rsp("AT+ECICCID");
		s_setup_phase = SETUP_PHASE_FREQ_PENDING;
		break;
	case SETUP_PHASE_FREQ:
		cat1_setup_log_rsp("AT+ECFREQ?");
		s_setup_phase = SETUP_PHASE_CCLK_PENDING;
		break;
	case SETUP_PHASE_CCLK:
		cat1_setup_log_rsp("AT+CCLK?");
		if (ok != 0u) {
			if (cat1_time_sync_from_cclk_rsp(s_modem_rsp) == 0) {
				co_printf("[CAT1] time synced from CCLK\r\n");
			} else {
				co_printf("[CAT1] CCLK time parse fail\r\n");
			}
		}
		s_setup_phase = SETUP_PHASE_CGATT_PENDING;
		break;
	case SETUP_PHASE_CGPADDR:
		cat1_setup_log_rsp("AT+CGPADDR");
		s_setup_phase = SETUP_PHASE_CLOSE_PENDING;
		break;
	case SETUP_PHASE_CLOSE:
		if (ok != 0u) {
			cat1_setup_log_rsp("AT+ECSOCL");
		}
		s_setup_phase = SETUP_PHASE_CREATE_PENDING;
		break;
	default:
		break;
	}
}

int cat1_skt_udp_setup_step(void)
{
	cat1_modem_result_t mr;

	if (s_setup_phase == SETUP_PHASE_DONE) {
		return CAT1_SETUP_OK;
	}
	if (s_setup_phase == SETUP_PHASE_FAIL) {
		return CAT1_SETUP_FAIL;
	}

    if (s_baud_phase != BAUD_SYNC_INIT && s_baud_phase != BAUD_SYNC_DONE) {
        int baud_rc = cat1_baud_sync_step();
        if (baud_rc == CAT1_SETUP_FAIL) s_setup_phase = SETUP_PHASE_FAIL;
        if (baud_rc != CAT1_SETUP_OK) return baud_rc;
        /* Verified 115200: restart normal setup, including CSCON. */
        s_setup_phase = SETUP_PHASE_IDLE;
    }

	if (cat1_modem_is_busy() != 0u) {
		mr = cat1_modem_step();
		if (mr == CAT1_MODEM_BUSY) {
			return CAT1_SETUP_BUSY;
		}
        if (mr == CAT1_MODEM_FAIL && s_setup_phase == SETUP_PHASE_CSCON &&
            s_baud_phase == BAUD_SYNC_INIT) {
            co_printf("[CAT1] 115200 no response, try 9600\r\n");
            return cat1_baud_sync_step();
        }
        /* A recognized OK/ERROR reply proves the current UART rate. Keep
         * existing CSCON error-tolerant behavior instead of changing baud. */
        if (mr == CAT1_MODEM_OK && s_setup_phase == SETUP_PHASE_CSCON &&
            s_baud_phase == BAUD_SYNC_INIT) s_baud_phase = BAUD_SYNC_DONE;
		if (mr == CAT1_MODEM_FAIL) {
			if (s_setup_phase == SETUP_PHASE_CSCON || s_setup_phase == SETUP_PHASE_CEREG
			    || s_setup_phase == SETUP_PHASE_CSQ || s_setup_phase == SETUP_PHASE_ICCID
			    || s_setup_phase == SETUP_PHASE_FREQ || s_setup_phase == SETUP_PHASE_CCLK
			    || s_setup_phase == SETUP_PHASE_CGPADDR || s_setup_phase == SETUP_PHASE_CLOSE) {
				cat1_setup_on_diag_done(s_setup_phase, 0u);
				return CAT1_SETUP_BUSY;
			}
			if (s_setup_phase == SETUP_PHASE_CGATT) {
				co_printf("[CAT1 UDP] CGATT? fail, retry wait\r\n");
				if (s_cgatt_wait_start_ms == 0u) {
					s_cgatt_wait_start_ms = sdk_timer_get_current_ms();
				}
				if (cat1_ms_elapsed(s_cgatt_wait_start_ms) >= CAT1_CGATT_WAIT_TOTAL_MS) {
					co_printf("[CAT1 UDP] CGATT? fail timeout %u ms\r\n",
					          (unsigned)CAT1_CGATT_WAIT_TOTAL_MS);
					s_cgatt_wait_start_ms = 0u;
					s_setup_phase = SETUP_PHASE_FAIL;
					return CAT1_SETUP_FAIL;
				}
				s_cgatt_next_ms = sdk_timer_get_current_ms();
				s_setup_phase = SETUP_PHASE_CGATT_RETRY_WAIT;
				return CAT1_SETUP_BUSY;
			}
			if (s_setup_phase == SETUP_PHASE_CREATE) {
				co_printf("[CAT1 UDP] ECSOCR fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
			}
			if (s_setup_phase == SETUP_PHASE_ECSONMI) {
				co_printf("[CAT1 UDP] ECSONMI fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
			}
		}

		if (mr == CAT1_MODEM_OK) {
			if (s_setup_phase == SETUP_PHASE_CSCON || s_setup_phase == SETUP_PHASE_CEREG
			    || s_setup_phase == SETUP_PHASE_CSQ || s_setup_phase == SETUP_PHASE_ICCID
			    || s_setup_phase == SETUP_PHASE_FREQ || s_setup_phase == SETUP_PHASE_CCLK
			    || s_setup_phase == SETUP_PHASE_CGPADDR || s_setup_phase == SETUP_PHASE_CLOSE) {
				cat1_setup_on_diag_done(s_setup_phase, 1u);
			} else if (s_setup_phase == SETUP_PHASE_CGATT) {
				cat1_setup_log_rsp("AT+CGATT?");
				if (cat1_setup_cgatt_attached() == 0) {
					s_cgatt_wait_start_ms = 0u;
					s_setup_phase = SETUP_PHASE_CGPADDR_PENDING;
				} else {
					if (s_cgatt_wait_start_ms == 0u) {
						s_cgatt_wait_start_ms = sdk_timer_get_current_ms();
					}
					if (cat1_ms_elapsed(s_cgatt_wait_start_ms) >= CAT1_CGATT_WAIT_TOTAL_MS) {
						co_printf("[CAT1 UDP] CGATT not attached (timeout %u ms)\r\n",
						          (unsigned)CAT1_CGATT_WAIT_TOTAL_MS);
						s_cgatt_wait_start_ms = 0u;
						s_setup_phase = SETUP_PHASE_FAIL;
						return CAT1_SETUP_FAIL;
					}
					co_printf("[CAT1 UDP] CGATT=0, retry in %u ms (waited %u/%u)\r\n",
					          (unsigned)CAT1_CGATT_POLL_INTERVAL_MS,
					          (unsigned)cat1_ms_elapsed(s_cgatt_wait_start_ms),
					          (unsigned)CAT1_CGATT_WAIT_TOTAL_MS);
					s_cgatt_next_ms = sdk_timer_get_current_ms();
					s_setup_phase = SETUP_PHASE_CGATT_RETRY_WAIT;
				}
			} else if (s_setup_phase == SETUP_PHASE_CREATE) {
				if (cat1_setup_parse_ecsocr_id() != 0) {
					s_setup_phase = SETUP_PHASE_FAIL;
					return CAT1_SETUP_FAIL;
				}
				co_printf("[CAT1 UDP] ECSOCR sock=%u\r\n", (unsigned)s_sock_id);
				s_setup_phase = SETUP_PHASE_ECSONMI_PENDING;
			} else if (s_setup_phase == SETUP_PHASE_ECSONMI) {
				co_printf("[CAT1 UDP] ready sock=%u -> %s data:%u ota:%u\r\n",
				          (unsigned)s_sock_id, CAT1_UDP_REMOTE_HOST,
				          (unsigned)CAT1_UDP_REMOTE_PORT,
				          (unsigned)CAT1_OTA_REMOTE_PORT);
				cat1_rx_discard_all();
				s_setup_phase = SETUP_PHASE_DONE;
				return CAT1_SETUP_OK;
			}
		}
		return CAT1_SETUP_BUSY;
	}

	if (s_setup_phase == SETUP_PHASE_IDLE) {
		return cat1_setup_issue_cscon();
	}
	if (s_setup_phase == SETUP_PHASE_CEREG) {
		return cat1_setup_issue_cereg();
	}
	if (s_setup_phase == SETUP_PHASE_CSQ) {
		return cat1_setup_issue_csq();
	}
	if (s_setup_phase == SETUP_PHASE_ICCID_PENDING) {
		return cat1_setup_issue_iccid();
	}
	if (s_setup_phase == SETUP_PHASE_FREQ_PENDING) {
		return cat1_setup_issue_freq();
	}
	if (s_setup_phase == SETUP_PHASE_CCLK_PENDING) {
		return cat1_setup_issue_cclk();
	}
	if (s_setup_phase == SETUP_PHASE_CGATT_PENDING) {
		return cat1_setup_issue_cgatt();
	}
	if (s_setup_phase == SETUP_PHASE_CGATT_RETRY_WAIT) {
		if (s_cgatt_wait_start_ms != 0u
		    && cat1_ms_elapsed(s_cgatt_wait_start_ms) >= CAT1_CGATT_WAIT_TOTAL_MS) {
			co_printf("[CAT1 UDP] CGATT not attached (timeout %u ms)\r\n",
			          (unsigned)CAT1_CGATT_WAIT_TOTAL_MS);
			s_cgatt_wait_start_ms = 0u;
			s_setup_phase = SETUP_PHASE_FAIL;
			return CAT1_SETUP_FAIL;
		}
		if (cat1_ms_elapsed(s_cgatt_next_ms) < CAT1_CGATT_POLL_INTERVAL_MS) {
			return CAT1_SETUP_BUSY;
		}
		return cat1_setup_issue_cgatt();
	}
	if (s_setup_phase == SETUP_PHASE_CGPADDR_PENDING) {
		return cat1_setup_issue_cgpaddr();
	}
	if (s_setup_phase == SETUP_PHASE_CLOSE_PENDING) {
		return cat1_setup_issue_close();
	}
	if (s_setup_phase == SETUP_PHASE_CREATE_PENDING) {
		return cat1_setup_issue_create();
	}
	if (s_setup_phase == SETUP_PHASE_ECSONMI_PENDING) {
		return cat1_setup_issue_ecsonmi();
	}

	return CAT1_SETUP_BUSY;
}

cat1_modem_result_t cat1_skt_udp_send_begin(const uint8_t *payload, uint16_t len)
{
	return cat1_skt_udp_send_begin_wait(payload, len, CAT1_UDP_DOWNLINK_WAIT_MS);
}

cat1_modem_result_t cat1_skt_udp_send_begin_wait(const uint8_t *payload, uint16_t len,
                                                 uint32_t wait_after_ok_ms)
{
	return cat1_skt_udp_send_begin_wait_ex(payload, len, wait_after_ok_ms, 1u,
	                                       CAT1_UDP_REMOTE_PORT);
}

cat1_modem_result_t cat1_skt_udp_send_begin_wait_ex(const uint8_t *payload, uint16_t len,
                                                    uint32_t wait_after_ok_ms,
                                                    uint8_t wait_downlink_chunk,
                                                    uint16_t remote_port)
{
	char *line = s_modem_line;
	unsigned prefix_len;
	unsigned hex_cap;

	if (payload == NULL || len == 0u || len > CAT1_UDP_MAX_UDP_PDU) {
		co_printf("[CAT1 UDP] ECSOST bad param len=%u\r\n", (unsigned)len);
		return CAT1_MODEM_FAIL;
	}
	if (s_sock_id == 255u) {
		co_printf("[CAT1 UDP] ECSOST no socket\r\n");
		return CAT1_MODEM_FAIL;
	}
	if (cat1_modem_is_busy() != 0u) {
		return CAT1_MODEM_BUSY;
	}

	CAT1_UDP_LOG("[CAT1 UDP] ECSOST enter len=%u port=%u\r\n",
	          (unsigned)len, (unsigned)remote_port);

	prefix_len = (unsigned)snprintf(line, sizeof(s_modem_line),
	                                "AT+ECSOST=%u,%s,%u,%u,",
	                                (unsigned)s_sock_id,
	                                CAT1_UDP_REMOTE_HOST,
	                                (unsigned)remote_port,
	                                (unsigned)len);
	if (prefix_len >= sizeof(s_modem_line)) {
		co_printf("[CAT1 UDP] ECSOST prefix overflow\r\n");
		return CAT1_MODEM_FAIL;
	}

	hex_cap = (unsigned)(sizeof(s_modem_line) - prefix_len - 1u);
	if ((unsigned)len * 2u + 1u > hex_cap) {
		co_printf("[CAT1 UDP] ECSOST line too long\r\n");
		return CAT1_MODEM_FAIL;
	}

	bin_to_hex_upper(payload, len, line + prefix_len, hex_cap);
	prefix_len += (unsigned)len * 2u;
	line[prefix_len] = '\0';

	/* 业务/OTA 统一：pump 保住 +ECSONMI，非 URC 残字节由 feed_char 丢弃；勿 RxFlush */
	cat1_skt_udp_pump_urc();
	if (usart1_at_send_pump(line, (unsigned)sizeof(s_modem_line),
	                        cat1_skt_udp_pump_urc) != 0) {
		co_printf("[CAT1 UDP] ECSOST TX timeout\r\n");
		return CAT1_MODEM_FAIL;
	}
	cat1_skt_udp_pump_urc();
	s_wait_downlink_chunk = wait_downlink_chunk;
	/* 保留 s_ecsonmi_pending：迟到 Tag11 在 WAIT_DOWNLINK 仍可读 */
	cat1_modem_begin_wait(NULL, 0u, 0u, wait_after_ok_ms);
	return CAT1_MODEM_BUSY;
}

void cat1_skt_udp_setup_reset(void)
{
	/* 不清时间锚点：墙钟与 socket 无关；仅 rc_cat1_init 清零，AT+CCLK 成功才更新 */
	if (cat1_modem_is_busy() == 0u) {
        s_baud_phase = BAUD_SYNC_INIT;
        s_baud_round = 0U;
		s_setup_phase = SETUP_PHASE_IDLE;
		s_sock_id = 255u;
		cat1_ecsonmi_pending_clear();
		s_idle_ecsorf_active = 0u;
		s_cgatt_wait_start_ms = 0u;
		s_cgatt_next_ms = 0u;
		s_socket_gone_event = 0u;
	}
}

void cat1_skt_udp_log_recv_diag(uint16_t req_seq)
{
	char head[17];
	unsigned i;
	unsigned n = s_urc_line_len;
	uint16_t rx_head;
	uint16_t rx_tail;
	uint16_t rx_used;

	if (n > 16u) {
		n = 16u;
	}
	for (i = 0u; i < n; i++) {
		char c = s_ecsonmi_line[i];

		head[i] = (char)((c >= 0x20 && c < 0x7f) ? c : '.');
	}
	head[n] = '\0';

	USART1_CAT1_RxSync();
	rx_head = USART1_CAT1_RX_HEAD;
	rx_tail = USART1_CAT1_RX_TAIL;
	if (rx_head >= rx_tail) {
		rx_used = (uint16_t)(rx_head - rx_tail);
	} else {
		rx_used = (uint16_t)(USART1_CAT1_RX_RING_CAP - rx_tail + rx_head);
	}

	co_printf("[OTA diag] seq=%u urc_len=%u rx_used=%u rx_drop=%lu pending=%u "
	          "ecsonmi=%u ecsonmi_len=%u at_busy=%u at_ok=%u phase=%u head=%s\r\n",
	          (unsigned)req_seq,
	          (unsigned)s_urc_line_len,
	          (unsigned)rx_used,
	          (unsigned long)USART1_CAT1_RxDropCount(),
	          (unsigned)s_recv_pending,
	          (unsigned)s_ecsonmi_pending,
	          (unsigned)s_ecsonmi_len,
	          (unsigned)s_at_active,
	          (unsigned)s_at_got_ok,
	          (unsigned)s_at_phase,
	          head);
}
