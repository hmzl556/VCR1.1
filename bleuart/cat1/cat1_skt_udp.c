/**
 * @file cat1_skt_udp.c
 * @brief 模组 UDP AT：协作式状态机，SKTDELETE → CREATE → BIND → CONNECT → SKTSEND。
 */
#include "cat1_skt_udp.h"
#include "cat1_usart2.h"
#include "cat1_udp_config.h"
#include "co_printf.h"
#include "br_timer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CAT1_TOTAL_WAIT_MS  5000u

typedef enum {
	AT_PHASE_IDLE = 0,
	AT_PHASE_WAIT_OK,
	AT_PHASE_WAIT_DOWNLINK,
} at_phase_t;

typedef enum {
	SETUP_PHASE_IDLE = 0,
	SETUP_PHASE_DELETE,
	SETUP_PHASE_CREATE_PENDING,
	SETUP_PHASE_CREATE,
	SETUP_PHASE_BIND_PENDING,
	SETUP_PHASE_BIND,
	SETUP_PHASE_CONNECT_PENDING,
	SETUP_PHASE_CONNECT,
	SETUP_PHASE_DONE,
	SETUP_PHASE_FAIL,
} setup_phase_t;

static uint8_t  s_sock_id = 255u;
static char     s_modem_line[CAT1_AT_LINE_MAX];
static unsigned s_urc_line_len;
static uint8_t  s_modem_rsp_accum;
static uint8_t *s_recv_target;
static uint16_t s_recv_target_cap;
static uint16_t s_recv_pdu_len;
static uint8_t  s_recv_pending;

static uint8_t           s_at_active;
static at_phase_t        s_at_phase;
static uint32_t          s_at_start_ms;
static uint32_t          s_at_ok_at_ms;
static uint8_t           s_at_allow_error;
static uint32_t          s_at_wait_after_ok_ms;
static char             *s_at_rsp;
static unsigned          s_at_rsp_cap;
static unsigned          s_at_rsp_len;
static uint8_t           s_at_got_ok;
static char              s_at_ok_line[8];
static unsigned          s_at_ok_line_len;
static cat1_modem_result_t s_at_last_done;

static setup_phase_t s_setup_phase;

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

static int cat1_skt_udp_try_parse_sktrecv_line(const char *line)
{
	const char *hex;
	const char *open_quote;
	const char *close_quote;
	const char *len_start;
	const char *len_end;
	unsigned data_len;
	unsigned hex_len;

	if (line == NULL || strncmp(line, "+SKTRECV:", 9) != 0) {
		return 0;
	}

	open_quote = strchr(line + 9, '"');
	if (open_quote != NULL) {
		len_end = open_quote - 1u;
		while (len_end > (line + 9) && (*len_end == ' ' || *len_end == '\t')) {
			len_end--;
		}
		if (len_end <= line + 9 || *len_end != ',') {
			return -1;
		}

		len_start = len_end;
		while (len_start > (line + 9) && *(len_start - 1u) != ',') {
			len_start--;
		}
		data_len = (unsigned)atoi(len_start);
		close_quote = strchr(open_quote + 1u, '"');
		if (close_quote == NULL) {
			return -1;
		}

		hex = open_quote + 1u;
		hex_len = (unsigned)(close_quote - hex);
	} else {
		hex = strrchr(line, ',');
		if (hex == NULL || hex[1] == '\0') {
			return -1;
		}

		len_end = hex;
		len_start = len_end;
		while (len_start > (line + 9) && *(len_start - 1u) != ',') {
			len_start--;
		}
		data_len = (unsigned)atoi(len_start);
		hex++;
		hex_len = (unsigned)strlen(hex);
	}

	if (data_len == 0u || data_len > (unsigned)CAT1_UDP_MAX_UDP_PDU
		|| hex_len != data_len * 2u) {
		return -1;
	}
	if (hex_to_bin(hex, hex_len, s_recv_target, s_recv_target_cap, &s_recv_pdu_len) != 0
		|| s_recv_target == NULL || s_recv_pdu_len == 0u
		|| s_recv_pdu_len != data_len) {
		return -1;
	}

	s_recv_pending = 1u;
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
			s_modem_line[s_urc_line_len] = '\0';
			(void)cat1_skt_udp_try_parse_sktrecv_line(s_modem_line);
		}
		s_urc_line_len = 0u;
		return;
	}

	if (s_urc_line_len == 0u && c != '+') {
		return;
	}

	if (s_urc_line_len + 1u < sizeof(s_modem_line)) {
		s_modem_line[s_urc_line_len++] = c;
	} else {
		s_urc_line_len = 0u;
	}
}

static void cat1_modem_finish(cat1_modem_result_t result)
{
	s_at_active = 0u;
	s_at_phase = AT_PHASE_IDLE;
	s_modem_rsp_accum = 0u;
	s_at_last_done = result;
}

static void cat1_modem_begin_wait(char *rsp, unsigned cap, uint8_t allow_error, uint32_t wait_after_ok_ms)
{
	s_at_active = 1u;
	s_at_phase = AT_PHASE_WAIT_OK;
	s_at_start_ms = sdk_timer_get_current_ms();
	s_at_ok_at_ms = 0u;
	s_at_allow_error = allow_error;
	s_at_wait_after_ok_ms = wait_after_ok_ms;
	s_at_rsp = rsp;
	s_at_rsp_cap = cap;
	s_at_rsp_len = 0u;
	s_at_got_ok = 0u;
	s_at_ok_line_len = 0u;
	s_urc_line_len = 0u;
	s_modem_rsp_accum = (rsp != NULL && cap > 0u) ? 1u : 0u;

	if (rsp != NULL && cap > 0u) {
		rsp[0] = '\0';
	}
}

uint8_t cat1_modem_is_busy(void)
{
	return s_at_active;
}

static cat1_modem_result_t cat1_modem_process_byte(uint8_t b)
{
	cat1_skt_udp_feed_char((char)b);

	if (s_at_rsp != NULL && s_at_rsp_cap > 0u && s_at_rsp_len + 1u < s_at_rsp_cap) {
		s_at_rsp[s_at_rsp_len++] = (char)b;
		s_at_rsp[s_at_rsp_len] = '\0';
	}

	if (b == '\n') {
		s_at_ok_line[s_at_ok_line_len] = '\0';
		if (s_at_ok_line_len == 2u && s_at_ok_line[0] == 'O' && s_at_ok_line[1] == 'K') {
			s_at_got_ok = 1u;
			s_at_ok_at_ms = sdk_timer_get_current_ms();
			if (s_at_wait_after_ok_ms == 0u) {
				cat1_modem_finish(CAT1_MODEM_OK);
				return CAT1_MODEM_OK;
			}
			s_at_phase = AT_PHASE_WAIT_DOWNLINK;
		} else if (s_at_allow_error != 0u && s_at_ok_line_len == 5u
				   && memcmp(s_at_ok_line, "ERROR", 5) == 0) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
		s_at_ok_line_len = 0u;
	} else if (b != '\r' && s_at_ok_line_len + 1u < sizeof(s_at_ok_line)) {
		s_at_ok_line[s_at_ok_line_len++] = (char)b;
	}

	if (s_at_got_ok != 0u && s_recv_pending != 0u) {
		cat1_modem_finish(CAT1_MODEM_OK);
		return CAT1_MODEM_OK;
	}

	return CAT1_MODEM_BUSY;
}

static cat1_modem_result_t cat1_modem_check_timeout(void)
{
	if (cat1_ms_elapsed(s_at_start_ms) >= CAT1_TOTAL_WAIT_MS) {
		if (s_at_allow_error != 0u
			&& (s_at_rsp == NULL || s_at_rsp[0] == '\0'
				|| strstr(s_at_rsp, "OK") != NULL || strstr(s_at_rsp, "ERROR") != NULL)) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
		}
		cat1_modem_finish(s_at_got_ok != 0u ? CAT1_MODEM_OK : CAT1_MODEM_FAIL);
		return s_at_last_done;
	}

	if (s_at_got_ok != 0u) {
		if (s_recv_pending != 0u) {
			cat1_modem_finish(CAT1_MODEM_OK);
			return CAT1_MODEM_OK;
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

int cat1_skt_udp_poll_recv(uint8_t *pdu, uint16_t pdu_cap, uint16_t *pdu_len)
{
	uint8_t b;

	while (USART2_CAT1_RxPop(&b) != 0U) {
		cat1_skt_udp_feed_char((char)b);
	}

	if (s_recv_pending == 0u) {
		return 0;
	}
	if (pdu == NULL || pdu_len == NULL || s_recv_target == NULL) {
		return -1;
	}
	if (s_recv_pdu_len > pdu_cap) {
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

static void cat1_flush_rx_to_urc(void)
{
	uint8_t b;

	while (USART2_CAT1_RxPop(&b) != 0U) {
		cat1_skt_udp_feed_char((char)b);
	}
}

static void cat1_send_at_line(char *line)
{
	USART2_CAT1_RxFlush();
	usart2_at_send(line, (unsigned)sizeof(s_modem_line));
}

static int cat1_setup_parse_create_id(void)
{
	char *p = strstr(s_modem_line, "+SKTCREATE:");
	int id = -1;

	if (p != NULL) {
		p += strlen("+SKTCREATE:");
		while ((*p == ' ') || (*p == '\t')) {
			p++;
		}
		id = atoi(p);
	}

	if (id < 0 || id > 254) {
		co_printf("[CAT1 UDP] bad +SKTCREATE id\r\n");
		return -1;
	}
	s_sock_id = (uint8_t)((unsigned int)id);
	return 0;
}

static int cat1_setup_issue_delete(void)
{
	memset(s_modem_line, 0, sizeof(s_modem_line));
	strncpy(s_modem_line, "AT+SKTDELETE=0", sizeof(s_modem_line) - 1u);
	s_modem_line[sizeof(s_modem_line) - 1u] = '\0';
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_line, (unsigned)sizeof(s_modem_line), 1u, 0u);
	s_setup_phase = SETUP_PHASE_DELETE;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_create(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+SKTCREATE=%u,%u,%u",
			 (unsigned)CAT1_SKT_CREATE_P1, (unsigned)CAT1_SKT_CREATE_P2,
			 (unsigned)CAT1_SKT_CREATE_P3);
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_line, (unsigned)sizeof(s_modem_line), 0u, 0u);
	s_setup_phase = SETUP_PHASE_CREATE;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_bind(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+SKTBIND=%u,%s,%u",
			 (unsigned)s_sock_id, CAT1_UDP_BIND_ADDR, (unsigned)CAT1_UDP_LOCAL_PORT);
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_line, (unsigned)sizeof(s_modem_line), 0u, 0u);
	s_setup_phase = SETUP_PHASE_BIND;
	return CAT1_SETUP_BUSY;
}

static int cat1_setup_issue_connect(void)
{
	snprintf(s_modem_line, sizeof(s_modem_line), "AT+SKTCONNECT=%u,%s,%u",
			 (unsigned)s_sock_id, CAT1_UDP_REMOTE_HOST, (unsigned)CAT1_UDP_REMOTE_PORT);
	cat1_send_at_line(s_modem_line);
	cat1_modem_begin_wait(s_modem_line, (unsigned)sizeof(s_modem_line), 0u, 0u);
	s_setup_phase = SETUP_PHASE_CONNECT;
	return CAT1_SETUP_BUSY;
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
			if (s_setup_phase == SETUP_PHASE_DELETE) {
				s_setup_phase = SETUP_PHASE_CREATE_PENDING;
				return CAT1_SETUP_BUSY;
			}
			if (s_setup_phase == SETUP_PHASE_CREATE) {
				co_printf("[CAT1 UDP] CREATE fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
			}
			if (s_setup_phase == SETUP_PHASE_BIND) {
				co_printf("[CAT1 UDP] BIND fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
			}
			if (s_setup_phase == SETUP_PHASE_CONNECT) {
				co_printf("[CAT1 UDP] CONNECT fail\r\n");
				s_setup_phase = SETUP_PHASE_FAIL;
				return CAT1_SETUP_FAIL;
			}
		}

		if (mr == CAT1_MODEM_OK) {
			if (s_setup_phase == SETUP_PHASE_DELETE) {
				s_setup_phase = SETUP_PHASE_CREATE_PENDING;
			} else if (s_setup_phase == SETUP_PHASE_CREATE) {
				if (cat1_setup_parse_create_id() != 0) {
					s_setup_phase = SETUP_PHASE_FAIL;
					return CAT1_SETUP_FAIL;
				}
				s_setup_phase = SETUP_PHASE_BIND_PENDING;
			} else if (s_setup_phase == SETUP_PHASE_BIND) {
				s_setup_phase = SETUP_PHASE_CONNECT_PENDING;
			} else if (s_setup_phase == SETUP_PHASE_CONNECT) {
				co_printf("[CAT1 UDP] ready sock=%u -> %s:%u\r\n",
						  (unsigned)s_sock_id, CAT1_UDP_REMOTE_HOST,
						  (unsigned)CAT1_UDP_REMOTE_PORT);
				s_setup_phase = SETUP_PHASE_DONE;
				return CAT1_SETUP_OK;
			}
		}
		return CAT1_SETUP_BUSY;
	}

	if (s_setup_phase == SETUP_PHASE_IDLE) {
		return cat1_setup_issue_delete();
	}
	if (s_setup_phase == SETUP_PHASE_CREATE_PENDING) {
		return cat1_setup_issue_create();
	}
	if (s_setup_phase == SETUP_PHASE_BIND_PENDING) {
		return cat1_setup_issue_bind();
	}
	if (s_setup_phase == SETUP_PHASE_CONNECT_PENDING) {
		return cat1_setup_issue_connect();
	}

	return CAT1_SETUP_BUSY;
}

cat1_modem_result_t cat1_skt_udp_send_begin(const uint8_t *payload, uint16_t len)
{
	char *line = s_modem_line;
	unsigned prefix_len;
	unsigned hex_cap;
	const unsigned suffix_len = 4u;

	if (payload == NULL || len == 0u || len > CAT1_UDP_MAX_UDP_PDU) {
		return CAT1_MODEM_FAIL;
	}
	if (s_sock_id == 255u) {
		return CAT1_MODEM_FAIL;
	}
	if (cat1_modem_is_busy() != 0u) {
		return CAT1_MODEM_BUSY;
	}

	prefix_len = (unsigned)snprintf(line, sizeof(s_modem_line),
									"AT+SKTSEND=%u,%u,",
									(unsigned)s_sock_id, (unsigned)len);
	if (prefix_len >= sizeof(s_modem_line)) {
		return CAT1_MODEM_FAIL;
	}

	hex_cap = (unsigned)(sizeof(s_modem_line) - prefix_len - suffix_len);
	if ((unsigned)len * 2u + 1u > hex_cap) {
		co_printf("[CAT1 UDP] SKTSEND line too long\r\n");
		return CAT1_MODEM_FAIL;
	}

	bin_to_hex_upper(payload, len, line + prefix_len, hex_cap);
	prefix_len += (unsigned)len * 2u;
	memcpy(line + prefix_len, ",0,1", suffix_len);
	line[prefix_len + suffix_len] = '\0';

	cat1_flush_rx_to_urc();
	usart2_at_send(line, (unsigned)sizeof(s_modem_line));
	cat1_modem_begin_wait(NULL, 0u, 0u, CAT1_UDP_DOWNLINK_WAIT_MS);
	return CAT1_MODEM_BUSY;
}

void cat1_skt_udp_setup_reset(void)
{
	if (cat1_modem_is_busy() == 0u) {
		s_setup_phase = SETUP_PHASE_IDLE;
	}
}
