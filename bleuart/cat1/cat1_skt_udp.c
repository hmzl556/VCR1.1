/**
 * @file cat1_skt_udp.c
 * @brief 模组 UDP AT（NT26 方案 B）：诊断 AT → CGATT/CGPADDR → ECSOCL → ECSOCR → ECSONMI → ECSOST；
 *        下行 +ECSONMI URC + ECSORF 主动读。
 */
#include "cat1_skt_udp.h"
#include "cat1_usart2.h"
#include "cat1_udp_config.h"
#include "co_printf.h"
#include "br_timer.h"
#include "cat1_time_sync.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CAT1_TOTAL_WAIT_MS  2000u

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
static unsigned s_urc_line_len;
static uint8_t  s_modem_rsp_accum;
static uint8_t  s_ota_uplink_wait;
static uint8_t  s_recv_allow = 1u;
static uint8_t *s_recv_target;
static uint16_t s_recv_target_cap;
static uint16_t s_recv_pdu_len;
static uint8_t  s_recv_pending;

static uint8_t  s_ecsonmi_pending;
static uint16_t s_ecsonmi_len;
static uint32_t s_ecsonmi_at_ms;
static uint8_t  s_idle_ecsorf_active;
static uint8_t  s_ecsorf_empty_retry;
static uint8_t  s_ecsorf_wait_retry;
static uint8_t  s_ecsorf_first_delay_logged;
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
static unsigned          s_at_line_len;
static cat1_modem_result_t s_at_last_done;

static setup_phase_t s_setup_phase;

static void cat1_ecso_issue_ecsorf(void);
static void cat1_ecso_try_start_ecsorf_read(void);

static uint32_t cat1_ms_elapsed(uint32_t since_ms)
{
	uint32_t now_ms = sdk_timer_get_current_ms();

	if (now_ms >= since_ms) {
		return now_ms - since_ms;
	}
	return (0xFFFFFFFFu - since_ms) + now_ms + 1u;
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
	co_printf("[CAT1 UDP] ECSORF parse fail: %s line_len=%u data_len=%u hex_len=%u cap=%u head=%s\r\n",
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
		if (parse_line[0] == '+' || (parse_line[0] >= '0' && parse_line[0] <= '9')) {
			cat1_ecsorf_log_parse_fail(parse_line, "no_data_quotes", 0u, 0u);
		}
		return 0;
	}

	after_ip = cat1_find_nth_quote(parse_line, 2);
	if (after_ip == NULL) {
		cat1_ecsorf_log_parse_fail(parse_line, "no_ip_quote", 0u, 0u);
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
		return -1;
	}
	len_start++;
	while (*len_start == ' ' || *len_start == '\t') {
		len_start++;
	}
	data_len = (unsigned)atoi(len_start);

	hex = hex_open + 1u;
	hex_len = (unsigned)(hex_close - hex);
	if (hex_len != data_len * 2u) {
		/*
		 * 模组偶发 hex 串比声明长度少 1~2 个 nibble（UART 与 BLE 并发时常见）。
		 * 若 hex 为偶数且不超过声明长度，按 hex 实长解码。
		 */
		if ((hex_len & 1u) == 0u && hex_len >= 2u) {
			unsigned hex_bytes = hex_len / 2u;

			if (hex_bytes <= (unsigned)s_recv_target_cap && hex_bytes <= data_len
			    && hex_bytes + 1u >= data_len) {
				co_printf("[CAT1 UDP] ECSORF hex short %u/%u, use %u bytes\r\n",
				          hex_len, data_len * 2u, hex_bytes);
				data_len = hex_bytes;
			} else {
				cat1_ecsorf_log_parse_fail(parse_line, "len_mismatch", data_len, hex_len);
				s_ecsorf_last_mismatch = 1u;
				return -1;
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
		co_printf("[CAT1 UDP] ECSORF pending, drop new line\r\n");
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
	s_ecsonmi_pending = 0u;
	s_ecsonmi_len = 0u;
	s_ecsonmi_at_ms = 0u;
	s_ecsorf_empty_retry = 0u;
	s_ecsorf_wait_retry = 0u;
	s_ecsorf_first_delay_logged = 0u;
	s_ecsorf_last_mismatch = 0u;
	return 1;
}

static void cat1_on_ecsonmi(uint16_t len)
{
	if (len == 0u) {
		return;
	}
	if (s_ecsonmi_pending != 0u) {
		co_printf("[CAT1 UDP] ECSONMI pending, drop new URC\r\n");
		return;
	}
	s_ecsonmi_pending = 1u;
	s_ecsonmi_len = len;
	s_ecsonmi_at_ms = sdk_timer_get_current_ms();
	s_ecsorf_empty_retry = 0u;
	s_ecsorf_wait_retry = 0u;
	s_ecsorf_first_delay_logged = 0u;
	s_ecsorf_last_mismatch = 0u;
	co_printf("[CAT1 UDP] ECSONMI recv sock=%u len=%u\r\n",
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
		co_printf("[CAT1 UDP] ECSONMI sock=%u expect=%u\r\n",
		          sock, (unsigned)s_sock_id);
		return 0;
	}
	cat1_on_ecsonmi((uint16_t)len);
	return 1;
}

static void cat1_skt_udp_feed_char(char c)
{
	if (s_modem_rsp_accum != 0u) {
		return;
	}

	if (c == '\r') {
		return;
	}
	if (c == '\n') {
		if (s_urc_line_len > 0u) {
			s_ecsonmi_line[s_urc_line_len] = '\0';
			(void)cat1_ecso_try_parse_ecsonmi_line(s_ecsonmi_line);
		}
		s_urc_line_len = 0u;
		return;
	}

	if (s_urc_line_len == 0u && c != '+') {
		return;
	}

	if (s_urc_line_len + 1u < sizeof(s_ecsonmi_line)) {
		s_ecsonmi_line[s_urc_line_len++] = c;
	} else {
		co_printf("[CAT1 UDP] ECSONMI line overflow, drop\r\n");
		s_urc_line_len = 0u;
	}
}

static void cat1_modem_finish(cat1_modem_result_t result)
{
	s_at_active = 0u;
	s_at_phase = AT_PHASE_IDLE;
	s_modem_rsp_accum = 0u;
	s_idle_ecsorf_active = 0u;
	s_ecsorf_wait_retry = 0u;
	s_at_last_done = result;
}

static void cat1_ecsorf_schedule_empty_retry(void)
{
	s_modem_rsp_accum = 0u;
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
	if (s_ecsorf_last_mismatch != 0u) {
		co_printf("[CAT1 UDP] ECSORF mismatch, retry read (ecsonmi=%u)\r\n",
		          (unsigned)s_ecsonmi_len);
		s_ecsorf_last_mismatch = 0u;
	}
	if (s_ecsorf_empty_retry < CAT1_ECSORF_EMPTY_RETRY_MAX) {
		s_ecsorf_empty_retry++;
		co_printf("[CAT1 UDP] ECSORF empty OK, retry %u/%u (ecsonmi=%u)\r\n",
		          (unsigned)s_ecsorf_empty_retry,
		          (unsigned)CAT1_ECSORF_EMPTY_RETRY_MAX,
		          (unsigned)s_ecsonmi_len);
		cat1_ecsorf_schedule_empty_retry();
		return CAT1_MODEM_BUSY;
	}
	co_printf("[CAT1 UDP] ECSORF no data after %u retries, ecsonmi_len=%u\r\n",
	          (unsigned)CAT1_ECSORF_EMPTY_RETRY_MAX, (unsigned)s_ecsonmi_len);
	s_ecsonmi_pending = 0u;
	s_ecsonmi_len = 0u;
	s_ecsonmi_at_ms = 0u;
	s_ecsorf_first_delay_logged = 0u;
	cat1_modem_finish(CAT1_MODEM_FAIL);
	return CAT1_MODEM_FAIL;
}

static uint8_t cat1_ecsorf_delay_elapsed(void)
{
	uint32_t need_ms;
	uint32_t since_ms;

	if (s_ecsorf_empty_retry > 0u) {
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
	if (s_ecsorf_empty_retry == 0u) {
		if (s_ecsorf_first_delay_logged == 0u) {
			co_printf("[CAT1 UDP] ECSORF wait first %u ms (ecsonmi=%u)\r\n",
			          (unsigned)CAT1_ECSORF_FIRST_DELAY_MS, (unsigned)s_ecsonmi_len);
			s_ecsorf_first_delay_logged = 1u;
		}
	} else {
		s_ecsorf_retry_at_ms = sdk_timer_get_current_ms();
	}
	s_ecsorf_wait_retry = 1u;
	s_modem_rsp_accum = 0u;
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
	                                               : CAT1_TOTAL_WAIT_MS;
	s_ota_uplink_wait = (uint8_t)(wait_after_ok_ms == 0u && s_wait_downlink_chunk != 0u
	                              && rsp == NULL);
	s_at_rsp = rsp;
	s_at_rsp_cap = cap;
	s_at_rsp_len = 0u;
	s_at_got_ok = 0u;
	s_at_line_len = 0u;
	s_modem_rsp_accum = (rsp != NULL && cap > 0u) ? 1u : 0u;

	if (rsp != NULL && cap > 0u) {
		rsp[0] = '\0';
	}
}

static void cat1_ecso_issue_ecsorf(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+ECSORF=%u,%u",
	         (unsigned)s_sock_id, (unsigned)CAT1_ECSORF_READ_MAX);
	usart2_at_send(s_modem_line, (unsigned)sizeof(s_modem_line));
	s_at_phase = AT_PHASE_WAIT_ECSORF;
	s_at_got_ok = 0u;
	s_at_line_len = 0u;
	s_at_rsp = s_modem_rsp;
	s_at_rsp_cap = (unsigned)sizeof(s_modem_rsp);
	s_at_rsp_len = 0u;
	s_modem_rsp[0] = '\0';
	s_modem_rsp_accum = 1u;
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
		s_at_total_wait_ms = CAT1_TOTAL_WAIT_MS;
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
	cat1_skt_udp_feed_char((char)b);

	if (s_at_rsp != NULL && s_at_rsp_cap > 0u && s_at_rsp_len + 1u < s_at_rsp_cap) {
		s_at_rsp[s_at_rsp_len++] = (char)b;
		s_at_rsp[s_at_rsp_len] = '\0';
	}

	if (b == '\n') {
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
			co_printf("[CAT1 UDP] AT ERROR\r\n");
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		} else if (s_at_line_len >= 11u
				   && strncmp(s_modem_line, "+CME ERROR:", 11) == 0) {
			if (s_at_allow_error != 0u) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			co_printf("[CAT1 UDP] %s\r\n", s_modem_line);
			cat1_modem_finish(CAT1_MODEM_FAIL);
			return CAT1_MODEM_FAIL;
		}
		s_at_line_len = 0u;
	} else if (b != '\r') {
		if (s_at_line_len + 1u < CAT1_AT_LINE_MAX) {
			s_modem_line[s_at_line_len++] = (char)b;
		} else if (s_at_phase == AT_PHASE_WAIT_ECSORF
		           && s_ecsorf_line_overflow_logged == 0u) {
			s_ecsorf_line_overflow_logged = 1u;
			co_printf("[CAT1 UDP] ECSORF line overflow cap=%u\r\n",
			          (unsigned)CAT1_AT_LINE_MAX);
		}
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
		if (s_at_got_ok == 0u) {
			co_printf("[CAT1 UDP] AT timeout phase=%u\r\n", (unsigned)s_at_phase);
		}
		cat1_modem_finish(s_at_got_ok != 0u ? CAT1_MODEM_OK : CAT1_MODEM_FAIL);
		return s_at_last_done;
	}

	if (s_ecsonmi_pending != 0u
	    && (s_at_phase == AT_PHASE_WAIT_DOWNLINK || s_ota_uplink_wait != 0u)) {
		cat1_ecso_try_start_ecsorf_read();
	}

	if (s_at_got_ok != 0u) {
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

	while (USART2_CAT1_RxPop(&b) != 0U) {
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

	while (USART2_CAT1_RxPop(&b) != 0U) {
		cat1_skt_udp_feed_char((char)b);
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
			s_ecsonmi_pending = 0u;
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
	USART2_CAT1_RxFlush();
	s_urc_line_len = 0u;
}

static void cat1_send_at_line(char *line)
{
	USART2_CAT1_RxFlush();
	usart2_at_send(line, (unsigned)sizeof(s_modem_line));
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
	strncpy(s_modem_line, "AT+CSCON=0", sizeof(s_modem_line) - 1u);
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
		cat1_setup_log_rsp("AT+CSCON=0");
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
		cat1_setup_log_rsp("AT+ECICCID");
		s_setup_phase = SETUP_PHASE_FREQ_PENDING;
		break;
	case SETUP_PHASE_FREQ:
		cat1_setup_log_rsp("AT+ECFREQ?");
		s_setup_phase = SETUP_PHASE_CCLK_PENDING;
		break;
	case SETUP_PHASE_CCLK:
		cat1_setup_log_rsp("AT+CCLK?");
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

	if (cat1_modem_is_busy() != 0u) {
		mr = cat1_modem_step();
		if (mr == CAT1_MODEM_BUSY) {
			return CAT1_SETUP_BUSY;
		}
		if (mr == CAT1_MODEM_FAIL) {
			if (s_setup_phase == SETUP_PHASE_CSCON || s_setup_phase == SETUP_PHASE_CEREG
			    || s_setup_phase == SETUP_PHASE_CSQ || s_setup_phase == SETUP_PHASE_ICCID
			    || s_setup_phase == SETUP_PHASE_FREQ || s_setup_phase == SETUP_PHASE_CCLK
			    || s_setup_phase == SETUP_PHASE_CGPADDR || s_setup_phase == SETUP_PHASE_CLOSE) {
				cat1_setup_on_diag_done(s_setup_phase, 0u);
				return CAT1_SETUP_BUSY;
			}
			if (s_setup_phase == SETUP_PHASE_CGATT) {
				co_printf("[CAT1 UDP] CGATT? fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
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
				if (cat1_setup_cgatt_attached() != 0) {
					co_printf("[CAT1 UDP] CGATT not attached\r\n");
					s_setup_phase = SETUP_PHASE_FAIL;
					return CAT1_SETUP_FAIL;
				}
				s_setup_phase = SETUP_PHASE_CGPADDR_PENDING;
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

	co_printf("[CAT1 UDP] ECSOST enter len=%u port=%u\r\n",
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

	if (wait_after_ok_ms == 0u) {
		cat1_skt_udp_pump_urc();
	} else {
		cat1_rx_discard_all();
	}
	if (usart2_at_send_pump(line, (unsigned)sizeof(s_modem_line),
	                        (wait_after_ok_ms == 0u) ? cat1_skt_udp_pump_urc : NULL) != 0) {
		co_printf("[CAT1 UDP] ECSOST TX timeout\r\n");
		return CAT1_MODEM_FAIL;
	}
	if (wait_after_ok_ms == 0u) {
		cat1_skt_udp_pump_urc();
	}
	s_wait_downlink_chunk = wait_downlink_chunk;
	s_ecsonmi_pending = 0u;
	cat1_modem_begin_wait(NULL, 0u, 0u, wait_after_ok_ms);
	return CAT1_MODEM_BUSY;
}

void cat1_skt_udp_setup_reset(void)
{
	if (cat1_modem_is_busy() == 0u) {
		s_setup_phase = SETUP_PHASE_IDLE;
		s_sock_id = 255u;
		s_ecsonmi_pending = 0u;
		s_idle_ecsorf_active = 0u;
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

	rx_head = USART2_CAT1_RX_HEAD;
	rx_tail = USART2_CAT1_RX_TAIL;
	if (rx_head >= rx_tail) {
		rx_used = (uint16_t)(rx_head - rx_tail);
	} else {
		rx_used = (uint16_t)(USART2_CAT1_RX_RING_CAP - rx_tail + rx_head);
	}

	co_printf("[OTA diag] seq=%u urc_len=%u rx_used=%u rx_drop=%lu pending=%u "
	          "ecsonmi=%u ecsonmi_len=%u at_busy=%u at_ok=%u phase=%u head=%s\r\n",
	          (unsigned)req_seq,
	          (unsigned)s_urc_line_len,
	          (unsigned)rx_used,
	          (unsigned long)USART2_CAT1_RxDropCount(),
	          (unsigned)s_recv_pending,
	          (unsigned)s_ecsonmi_pending,
	          (unsigned)s_ecsonmi_len,
	          (unsigned)s_at_active,
	          (unsigned)s_at_got_ok,
	          (unsigned)s_at_phase,
	          head);
}

/*===========================================================================
 * 休眠态 CAT1 管理 — 进入/退出 QSCLK 睡眠模式
 *
 * 休眠前发送 AT+QSCLK=1 让模组进入 DRX 睡眠（保持寻呼接收能力），
 * 唤醒后发送 AT+QSCLK=0 恢复全功能。
 * 由于 PWRKEY 常接地且无 GPIO 控制，无法物理断电，只能通过 AT 命令降功耗。
 *===========================================================================*/

/**
 * @brief  阻塞发送 AT 命令并等待 "OK" 或 "ERROR" 响应
 * @param  cmd        AT 命令字符串（不含 \r\n）
 * @param  timeout_ms 超时毫秒数
 * @return true = 收到 OK, false = 超时或收到 ERROR
 * @note   调用前确保无进行中的 AT 事务（cat1_modem_is_busy() == 0）
 */
static bool cat1_blocking_at(const char *cmd, uint32_t timeout_ms)
{
    char  line_buf[64];
    int   line_len;
    uint32_t start_ms;
    char  rx_line[32];
    uint8_t rx_idx;

    /*
     * 等待正在进行的 AT 事务结束。
     * 主动调用 cat1_modem_step() 推进状态机超时，
     * 否则 CAT1 断电/无响应时 s_at_active 永远卡在 1。
     */
    start_ms = sdk_timer_get_current_ms();
    while (cat1_modem_is_busy() != 0u) {
        (void)cat1_modem_step();
        if ((sdk_timer_get_current_ms() - start_ms) >= 5000u) {
            co_printf("[CAT1] blocking_at: wait busy timeout\r\n");
            return false;
        }
    }

    /* 组装命令行：cmd + \r\n */
    line_len = snprintf(line_buf, sizeof(line_buf), "%s\r\n", cmd);
    if (line_len < 0 || (unsigned)line_len >= sizeof(line_buf)) {
        return false;
    }

    /* 清空 RX 环，避免旧数据干扰 */
    USART2_CAT1_RxFlush();

    /* 阻塞发送命令行 */
    usart2_at_send(line_buf, (unsigned)line_len);

    /* 轮询 RX 环等待 "OK" 或 "ERROR" */
    start_ms = sdk_timer_get_current_ms();
    rx_idx   = 0u;

    for (;;) {
        uint8_t b;

        while (USART2_CAT1_RxPop(&b) != 0u) {
            if (b == '\r') {
                continue;
            }
            if (b == '\n') {
                /* 一行结束，检查是否为 OK 或 ERROR */
                if (rx_idx > 0u) {
                    rx_line[rx_idx] = '\0';
                    if (strcmp(rx_line, "OK") == 0) {
                        return true;
                    }
                    if (strcmp(rx_line, "ERROR") == 0) {
                        co_printf("[CAT1] blocking_at: %s -> ERROR\r\n", cmd);
                        return false;
                    }
                    rx_idx = 0u;
                }
            } else if (rx_idx < sizeof(rx_line) - 1u) {
                rx_line[rx_idx++] = (char)b;
            }
        }

        if ((sdk_timer_get_current_ms() - start_ms) >= timeout_ms) {
            co_printf("[CAT1] blocking_at: %s timeout %lums\r\n",
                      cmd, (unsigned long)timeout_ms);
            return false;
        }
    }
}

/**
 * @brief  休眠前准备：让 CAT1 进入 QSCLK 睡眠模式
 * @note   模组在 DRX 睡眠期间仍可接收寻呼和 UDP 数据，
 *         收到后自动缓冲于内部 RAM，唤醒 MCU 后可取走。
 */
void cat1_sleep_prepare(void)
{
    co_printf("[CAT1] sleep prepare: AT+QSCLK=1\r\n");
    (void)cat1_blocking_at("AT+QSCLK=1", 3000u);
    co_printf("[CAT1] sleep prepare done\r\n");
}

/**
 * @brief  休眠唤醒后恢复：让 CAT1 退出 QSCLK 睡眠模式
 * @note   模组可能处于睡眠态，UART 先发 AT 字节唤醒它，
 *         再发 AT+QSCLK=0 恢复正常功能。
 */
void cat1_sleep_wakeup(void)
{
    uint32_t start_ms;

    /*
     * 先发一个空 AT 命令唤醒模组 UART（睡眠态下前几个字节可能丢失）。
     * 不用等响应，只需让数据出现在 TX 线上触发模组唤醒。
     */
    usart2_at_send("AT\r\n", 4u);

    /* 等待模组稳定（文档建议 ≥50ms） */
    start_ms = sdk_timer_get_current_ms();
    while ((sdk_timer_get_current_ms() - start_ms) < 100u) {
        /* 空转等待 */
    }

    /* 清空唤醒期间可能产生的杂乱数据 */
    USART2_CAT1_RxFlush();

    co_printf("[CAT1] sleep wakeup: AT+QSCLK=0\r\n");
    (void)cat1_blocking_at("AT+QSCLK=0", 3000u);

    /*
     * AT+QSCLK=0 完成后，模组可能立即吐出缓存的 +ECSONMI URC。
     * cat1_blocking_at 只匹配 OK/ERROR，其他行会被丢弃。
     * 此处主动抽干 RX 环并解析 +ECSONMI，确保 OTA 数据不丢。
     */
    cat1_skt_udp_drain_urc();

    co_printf("[CAT1] sleep wakeup done\r\n");
}
