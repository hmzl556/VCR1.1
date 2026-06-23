#include <string.h>
#include "rc_cat1_bridge.h"
#include "cat1_usart2.h"
#include "cat1_skt_udp.h"
#include "remote_reports.h"
#include "rc_key_input.h"
#include "ble_rc_svc.h"
#include "rc_trans_assembly.h"
#include "app_rtos.h"
#include "co_printf.h"
#include "FreeRTOS.h"
#include "task.h"

#ifndef CAT1_MODEM_BOOT_DELAY_MS
#define CAT1_MODEM_BOOT_DELAY_MS  1000u
#endif

#ifndef CAT1_SETUP_RETRY_MS
#define CAT1_SETUP_RETRY_MS       1000u
#endif

static uint8_t  s_udp_ready;
static uint8_t  s_cat1_booting;
static uint32_t s_cat1_boot_ms;
static volatile uint8_t s_pending_tlv;
static uint16_t s_pending_tlv_len;
static uint32_t s_pending_pack_index;
static uint8_t  s_cat1_tlv[RC_TRANS_ASSEMBLY_MAX];
static uint8_t  s_cloud_policy[6];

static uint8_t  s_tx_waiting;
static uint8_t  s_tx_tlv_notify;
static uint32_t s_tx_tlv_pack;
static uint32_t s_setup_retry_ms;

static uint32_t rc_cat1_now_ms(void)
{
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
	}
	return 0u;
}

void rc_cat1_init(void)
{
	co_printf("[RC   ] CAT1 init start\r\n");

	s_udp_ready = 0u;
	s_cat1_booting = 1u;
	s_cat1_boot_ms = 0u;
	s_pending_tlv = 0u;
	s_tx_waiting = 0u;
	s_tx_tlv_notify = 0u;
	s_setup_retry_ms = 0u;
	memset(s_cloud_policy, 0, sizeof(s_cloud_policy));

	remote_reports_init();
	USART2_CAT1_Init();
	cat1_skt_udp_bind_recv_buf(s_cat1_tlv, (uint16_t)sizeof(s_cat1_tlv));
	co_printf("[RC   ] CAT1 init OK, boot delay %u ms\r\n", (unsigned)CAT1_MODEM_BOOT_DELAY_MS);
}

void rc_cat1_cancel_pending(void)
{
	taskENTER_CRITICAL();
	s_pending_tlv = 0u;
	s_pending_tlv_len = 0u;
	s_pending_pack_index = RC_PACK_INDEX_NONE;
	taskEXIT_CRITICAL();
}

static void rc_cat1_on_setup_ok(void)
{
	s_udp_ready = 1u;
	co_printf("[RC   ] CAT1 UDP ready\r\n");
	if (remote_queue_tag01_status() != 0) {
		co_printf("[RC   ] Tag01 queue fail\r\n");
	}
}

static void rc_cat1_setup_poll(void)
{
	int setup_rc;

	if (s_setup_retry_ms != 0u) {
		if ((rc_cat1_now_ms() - s_setup_retry_ms) < CAT1_SETUP_RETRY_MS) {
			return;
		}
		s_setup_retry_ms = 0u;
	}

	setup_rc = cat1_skt_udp_setup_step();
	if (setup_rc == CAT1_SETUP_BUSY) {
		return;
	}
	if (setup_rc == CAT1_SETUP_OK) {
		rc_cat1_on_setup_ok();
		return;
	}

	co_printf("[RC   ] CAT1 UDP setup failed\r\n");
	s_setup_retry_ms = rc_cat1_now_ms();
	cat1_skt_udp_setup_reset();
}

void rc_cat1_on_tlv_ready(uint16_t tlv_len, uint32_t pack_index)
{
	const uint8_t *tlv;
	uint16_t asm_len;

	if (tlv_len == 0u || tlv_len > RC_TRANS_ASSEMBLY_MAX) {
		return;
	}

	taskENTER_CRITICAL();
	asm_len = tlv_len;
	tlv = rc_trans_get_assembled(&asm_len);
	if (tlv != NULL && asm_len > 0u && asm_len <= RC_TRANS_ASSEMBLY_MAX) {
		memcpy(s_cat1_tlv, tlv, asm_len);
		s_pending_tlv_len = asm_len;
		s_pending_pack_index = pack_index;
		s_pending_tlv = 1u;
	}
	rc_trans_release();
	taskEXIT_CRITICAL();
}

void rc_cat1_on_udp_done(uint8_t result, uint32_t pack_index)
{
	rc_status_t st;
	peer_t peer;

	peer = ble_rc_svc_get_peer();
	memcpy(&st, ble_rc_svc_get_status(), sizeof(st));
	st.pack_index = pack_index;
	st.retry_count = 0u;

	if (result == RC_RESULT_OK) {
		st.state = RC_STATE_DONE;
		st.result = RC_RESULT_OK;
		rc_trans_note_upload_ok(pack_index);
	} else {
		st.state = RC_STATE_ERROR;
		st.result = result;
	}
	ble_rc_svc_status_notify(peer, &st);
}

void rc_cat1_get_cloud_policy(uint8_t policy[6])
{
	if (policy != NULL) {
		taskENTER_CRITICAL();
		memcpy(policy, s_cloud_policy, 6u);
		taskEXIT_CRITICAL();
	}
}

static void rc_cat1_poll_downlink(void)
{
	uint16_t pdu_len;
	int recv_rc;
	uint8_t policy[6];

	recv_rc = cat1_skt_udp_poll_recv(s_cat1_tlv, (uint16_t)sizeof(s_cat1_tlv), &pdu_len);
	if (recv_rc <= 0) {
		return;
	}

	co_printf("[RC   ] UDP recv OK, len=%u\r\n", (unsigned)pdu_len);
	taskENTER_CRITICAL();
	memcpy(policy, s_cloud_policy, 6u);
	taskEXIT_CRITICAL();
	if (remote_handle_downlink_frame(s_cat1_tlv, pdu_len, policy) != 0) {
		co_printf("[RC   ] UDP downlink parse fail\r\n");
		return;
	}

	taskENTER_CRITICAL();
	memcpy(s_cloud_policy, policy, 6u);
	taskEXIT_CRITICAL();
	co_printf("[RC   ] CloudPolicy updated: interval=%u count=%u\r\n",
			  (unsigned)(((uint32_t)policy[0] << 24) | ((uint32_t)policy[1] << 16)
					   | ((uint32_t)policy[2] << 8) | policy[3]),
			  (unsigned)(((uint16_t)policy[4] << 8) | policy[5]));
}

static void rc_cat1_handle_pending_tlv(void)
{
	uint16_t pending_tlv_len = 0u;
	uint32_t done_pack = RC_PACK_INDEX_NONE;

	if (s_pending_tlv == 0u) {
		return;
	}

	taskENTER_CRITICAL();
	if (s_pending_tlv != 0u) {
		pending_tlv_len = s_pending_tlv_len;
		done_pack = s_pending_pack_index;
		s_pending_tlv = 0u;
	}
	taskEXIT_CRITICAL();

	if (remote_queue_tlv_blob(s_cat1_tlv, pending_tlv_len) != 0) {
		co_printf("[RC   ] UDP TLV queue fail\r\n");
		(void)cat1_post_udp_done(RC_RESULT_PARAM_ERROR, done_pack);
	} else {
		s_tx_tlv_notify = 1u;
		s_tx_tlv_pack = done_pack;
	}
}

static void rc_cat1_tx_complete(cat1_modem_result_t mr)
{
	s_tx_waiting = 0u;

	if (mr == CAT1_MODEM_OK) {
		co_printf("[RC   ] UDP send OK!\r\n");
		rc_cat1_poll_downlink();
		if (s_tx_tlv_notify != 0u) {
			(void)cat1_post_udp_done(RC_RESULT_OK, s_tx_tlv_pack);
			s_tx_tlv_notify = 0u;
		}
	} else {
		co_printf("[RC   ] UDP send fail!\r\n");
		if (s_tx_tlv_notify != 0u) {
			(void)cat1_post_udp_done(RC_RESULT_NET_ERROR, s_tx_tlv_pack);
			s_tx_tlv_notify = 0u;
		}
	}
}

static void rc_cat1_tx_step(void)
{
	uint16_t frame_len;
	int poll_rc;
	cat1_modem_result_t begin_rc;

	if (s_tx_waiting != 0u || cat1_modem_is_busy() != 0u) {
		return;
	}

	poll_rc = remote_report_poll(s_cat1_tlv, sizeof(s_cat1_tlv), &frame_len);
	if (poll_rc != 0) {
		return;
	}

	begin_rc = cat1_skt_udp_send_begin(s_cat1_tlv, frame_len);
	if (begin_rc == CAT1_MODEM_BUSY) {
		s_tx_waiting = 1u;
		return;
	}

	co_printf("[RC   ] UDP send fail!\r\n");
	if (s_tx_tlv_notify != 0u) {
		(void)cat1_post_udp_done(RC_RESULT_NET_ERROR, s_tx_tlv_pack);
		s_tx_tlv_notify = 0u;
	}
}

void rc_cat1_poll(void)
{
	uint32_t now_ms;
	cat1_modem_result_t mr;

	if (s_cat1_booting != 0u) {
		now_ms = rc_cat1_now_ms();
		if (s_cat1_boot_ms == 0u) {
			s_cat1_boot_ms = now_ms;
		}
		if ((now_ms - s_cat1_boot_ms) < CAT1_MODEM_BOOT_DELAY_MS) {
			return;
		}
		s_cat1_booting = 0u;
		co_printf("[RC   ] CAT1 boot done, setup UDP...\r\n");
	}

	if (s_udp_ready == 0u) {
		rc_cat1_setup_poll();
		return;
	}

	mr = cat1_modem_step();
	if (s_tx_waiting != 0u && (mr == CAT1_MODEM_OK || mr == CAT1_MODEM_FAIL)) {
		rc_cat1_tx_complete(mr);
	}

	if (cat1_modem_is_busy() == 0u) {
		rc_cat1_handle_pending_tlv();
		rc_cat1_poll_downlink();
	}

	rc_cat1_tx_step();
	rc_key_input_poll_tag10();
}
