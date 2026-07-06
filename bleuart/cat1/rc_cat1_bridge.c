#include <string.h>
#include "rc_cat1_bridge.h"
#include "cat1_usart2.h"
#include "cat1_skt_udp.h"
#include "cat1_udp_config.h"
#include "remote_reports.h"
#include "rc_key_input.h"
#include "ble_rc_svc.h"
#include "rc_trans_assembly.h"
#include "app_rtos.h"
#include "co_printf.h"
#include "br_timer.h"
#include "pm_state.h"
#include "FreeRTOS.h"
#include "task.h"
#include "udp_ota.h"
#include "ota_frame.h"

#ifndef CAT1_MODEM_BOOT_DELAY_MS
#define CAT1_MODEM_BOOT_DELAY_MS  2000u		//启动CAT1之前的上电等待时间
#endif

#ifndef CAT1_SETUP_RETRY_MS
#define CAT1_SETUP_RETRY_MS       3000u		//设置重试时间
#endif

#if (CAT1_UDP_MAX_UDP_PDU > RC_TRANS_ASSEMBLY_MAX)
#define RC_CAT1_IO_BUF_MAX  CAT1_UDP_MAX_UDP_PDU
#else
#define RC_CAT1_IO_BUF_MAX  RC_TRANS_ASSEMBLY_MAX
#endif

static uint8_t  s_udp_ready;
static uint8_t  s_cat1_booting;
static uint32_t s_cat1_boot_ms;
static volatile uint8_t s_pending_tlv;
static uint16_t s_pending_tlv_len;
static uint32_t s_pending_pack_index;
static uint8_t  s_cat1_tlv[RC_CAT1_IO_BUF_MAX];
static uint8_t  s_cloud_policy[6];

static uint8_t  s_tx_waiting;
static uint8_t  s_inflight_rc_trans;
static uint32_t s_inflight_pack_index;
static uint32_t s_setup_retry_ms;
static uint8_t  s_sktsend_fail_streak;
static uint8_t  s_tag01_queued;

static uint8_t  s_inflight_ota;

static uint32_t rc_cat1_now_ms(void)
{
	return sdk_timer_get_current_ms();
}

void rc_cat1_init(void)
{
	co_printf("[RC   ] CAT1 init start\r\n");

	s_udp_ready = 0u;
	s_cat1_booting = 1u;
	s_cat1_boot_ms = 0u;
	s_pending_tlv = 0u;
	s_tx_waiting = 0u;
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_inflight_ota = 0u;
	s_setup_retry_ms = 0u;
	s_sktsend_fail_streak = 0u;
	s_tag01_queued = 0u;
	memset(s_cloud_policy, 0, sizeof(s_cloud_policy));

	remote_reports_init();
	udp_ota_init();
	USART2_CAT1_Init();
	cat1_skt_udp_bind_recv_buf(s_cat1_tlv, (uint16_t)sizeof(s_cat1_tlv));
	cat1_skt_udp_set_recv_allow(1u);
	co_printf("[RC   ] CAT1 init OK, boot delay %u ms\r\n", (unsigned)CAT1_MODEM_BOOT_DELAY_MS);
}

void rc_cat1_cancel_pending(void)
{
	taskENTER_CRITICAL();
	s_pending_tlv = 0u;
	s_pending_tlv_len = 0u;
	s_pending_pack_index = RC_PACK_INDEX_NONE;
	taskEXIT_CRITICAL();
	rc_trans_release();
}

static void rc_cat1_udp_rebuild(const char *reason)
{
	if (cat1_modem_is_busy() != 0u) {
		return;
	}

	co_printf("[RC   ] CAT1 UDP rebuild: %s\r\n", reason);
	s_udp_ready = 0u;
	s_tx_waiting = 0u;
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_inflight_ota = 0u;
	cat1_skt_udp_setup_reset();
	s_setup_retry_ms = rc_cat1_now_ms();
	s_sktsend_fail_streak = 0u;
}

static void rc_cat1_on_sktsend_fail(void)
{
	if (s_inflight_rc_trans == 0u) {
		return;
	}

	if (s_sktsend_fail_streak < 255u) {
		s_sktsend_fail_streak++;
	}
	co_printf("[RC   ] SKTSEND fail streak=%u/%u\r\n",
	          (unsigned)s_sktsend_fail_streak,
	          (unsigned)CAT1_UDP_REBUILD_FAIL_THRESH);
	if (s_sktsend_fail_streak >= CAT1_UDP_REBUILD_FAIL_THRESH) {
		rc_cat1_udp_rebuild("SKTSEND fail streak");
	}
}

static void rc_cat1_on_setup_ok(void)
{
	s_udp_ready = 1u;
	s_sktsend_fail_streak = 0u;
	co_printf("[RC   ] CAT1 UDP ready\r\n");
	udp_ota_on_udp_ready();
	if (s_tag01_queued == 0u) {
		if (remote_queue_tag01_status() != 0) {
			co_printf("[RC   ] Tag01 queue fail\r\n");
		} else {
			s_tag01_queued = 1u;
		}
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

int rc_cat1_on_tlv_ready(uint16_t tlv_len, uint32_t pack_index)
{
	/* OTA 会话期间拒绝 RC-TRANS 组装完成后的 8888 上报 */
	if (udp_ota_session_busy() != 0u) {
		co_printf("[RC   ] TLV ready reject: OTA session\r\n");
		rc_trans_release();
		return -1;
	}

	if (tlv_len == 0u || tlv_len > RC_TRANS_ASSEMBLY_MAX) {
		co_printf("[RC   ] TLV ready reject: bad len=%u\r\n", (unsigned)tlv_len);
		rc_trans_release();
		return -1;
	}

	taskENTER_CRITICAL();
	s_pending_tlv_len = tlv_len;
	s_pending_pack_index = pack_index;
	s_pending_tlv = 1u;
	taskEXIT_CRITICAL();
	return 0;
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

static void rc_cat1_tx_complete(cat1_modem_result_t mr);

static void rc_cat1_poll_downlink(void)
{
	uint16_t pdu_len;
	int recv_rc;
	uint8_t policy[6];

	for (;;) {
		if (udp_ota_is_active() != 0u) {
			cat1_skt_udp_pump_urc();
		}

		recv_rc = cat1_skt_udp_poll_recv(s_cat1_tlv, (uint16_t)sizeof(s_cat1_tlv), &pdu_len);
		if (recv_rc <= 0) {
			break;
		}

		co_printf("[RC   ] UDP recv OK, len=%u\r\n", (unsigned)pdu_len);

		if (pdu_len > 0u && s_cat1_tlv[0] == OTA_FRAME_START) {
			int ota_rc = udp_ota_on_rx(s_cat1_tlv, pdu_len);

			if (ota_rc == 1 && s_tx_waiting != 0u && s_inflight_ota != 0u) {
				cat1_skt_udp_finish_active_ok();
				rc_cat1_tx_complete(CAT1_MODEM_OK);
			}
			continue;
		}

		taskENTER_CRITICAL();
		memcpy(policy, s_cloud_policy, 6u);
		taskEXIT_CRITICAL();
		if (remote_handle_downlink_frame(s_cat1_tlv, pdu_len, policy) != 0) {
			co_printf("[RC   ] UDP downlink parse fail\r\n");
			continue;
		}

		pm_activity_notify(PM_ACTIVITY_CAT1);

		taskENTER_CRITICAL();
		memcpy(s_cloud_policy, policy, 6u);
		taskEXIT_CRITICAL();
		co_printf("[RC   ] CloudPolicy updated: interval=%u count=%u\r\n",
				  (unsigned)(((uint32_t)policy[0] << 24) | ((uint32_t)policy[1] << 16)
						   | ((uint32_t)policy[2] << 8) | policy[3]),
				  (unsigned)(((uint16_t)policy[4] << 8) | policy[5]));
	}
}

static void rc_cat1_handle_pending_tlv(void)
{
	const uint8_t *tlv;
	uint16_t asm_len;
	uint16_t pending_tlv_len = 0u;
	uint32_t done_pack = RC_PACK_INDEX_NONE;
	int queue_rc;

	/* OTA 会话期间丢弃 pending TLV，Cat.1 仅服务 OTA 拉片 */
	if (udp_ota_session_busy() != 0u) {
		if (s_pending_tlv != 0u) {
			taskENTER_CRITICAL();
			s_pending_tlv = 0u;
			s_pending_tlv_len = 0u;
			s_pending_pack_index = RC_PACK_INDEX_NONE;
			taskEXIT_CRITICAL();
			rc_trans_release();
			co_printf("[RC   ] pending TLV dropped: OTA session\r\n");
		}
		return;
	}

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

	if (pending_tlv_len == 0u) {
		return;
	}

	asm_len = pending_tlv_len;
	tlv = rc_trans_get_assembled(&asm_len);
	if (tlv == NULL || asm_len == 0u) {
		co_printf("[RC   ] UDP TLV queue fail: no assembly pack=0x%08X\r\n",
		          (unsigned)done_pack);
		rc_trans_release();
		rc_cat1_on_udp_done(RC_RESULT_NET_ERROR, done_pack);
		return;
	}

	cat1_skt_udp_set_recv_allow(0u);
	queue_rc = remote_queue_tlv_blob(tlv, asm_len, done_pack);
	cat1_skt_udp_set_recv_allow(1u);
	rc_trans_release();

	if (queue_rc != 0) {
		co_printf("[RC   ] UDP TLV queue fail pack=0x%08X\r\n", (unsigned)done_pack);
		rc_cat1_on_udp_done(RC_RESULT_NET_ERROR, done_pack);
	}
}

static void rc_cat1_tx_complete(cat1_modem_result_t mr)
{
	uint8_t ota_inflight = s_inflight_ota;

	s_tx_waiting = 0u;

	/* OTA：modem 结束前先处理 pending 的 +SKTRECV，避免先 sent 再收片被 phase 拒绝 */
	if (ota_inflight != 0u) {
		s_inflight_ota = 0u;
		rc_cat1_poll_downlink();
	}

	if (mr == CAT1_MODEM_OK) {
		co_printf("[RC   ] UDP send OK!\r\n");
		s_sktsend_fail_streak = 0u;
		if (ota_inflight != 0u) {
			udp_ota_on_uplink_sent();
		} else {
			rc_cat1_poll_downlink();
		}
		if (s_inflight_rc_trans != 0u) {
			(void)cat1_post_udp_done(RC_RESULT_OK, s_inflight_pack_index);
		}
	} else {
		if (ota_inflight != 0u) {
			udp_ota_on_uplink_timeout();
			rc_cat1_poll_downlink();
		} else {
			co_printf("[RC   ] UDP send fail!\r\n");
			rc_cat1_poll_downlink();
			rc_cat1_on_sktsend_fail();
		}
		if (s_inflight_rc_trans != 0u) {
			(void)cat1_post_udp_done(RC_RESULT_NET_ERROR, s_inflight_pack_index);
		}
	}
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
}

static void rc_cat1_on_ota_paused_release(void)
{
	if (s_tx_waiting != 0u || cat1_modem_is_busy() != 0u) {
		cat1_skt_udp_force_fail_active();
	}
	if (s_tx_waiting != 0u) {
		co_printf("[RC   ] OTA PAUSE, release tx wait\r\n");
		s_tx_waiting = 0u;
		s_inflight_ota = 0u;
		s_inflight_rc_trans = 0u;
		s_inflight_pack_index = RC_PACK_INDEX_NONE;
	}
}

static void rc_cat1_tx_step(void)
{
	const uint8_t *frame;
	uint16_t frame_len;
	uint8_t rc_trans_notify;
	uint32_t pack_index;
	int poll_rc;
	cat1_modem_result_t begin_rc;

	if (udp_ota_is_active() != 0u) {
		cat1_skt_udp_drain_urc();
		rc_cat1_poll_downlink();
	}

	if (s_tx_waiting != 0u || cat1_modem_is_busy() != 0u) {
		return;
	}

	if (udp_ota_poll_tx_get(&frame, &frame_len) == 1) {
		begin_rc = cat1_skt_udp_send_begin_wait_ex(
			frame, frame_len, CAT1_OTA_UPLINK_WAIT_MS,
			udp_ota_uplink_waits_chunk(), CAT1_OTA_REMOTE_PORT);
		if (begin_rc == CAT1_MODEM_BUSY) {
			s_tx_waiting = 1u;
			s_inflight_ota = 1u;
			udp_ota_on_uplink_issued();
			return;
		}
		if (begin_rc == CAT1_MODEM_OK) {
			rc_cat1_poll_downlink();
			return;
		}
		co_printf("[RC   ] OTA UDP send fail!\r\n");
		rc_cat1_poll_downlink();
		return;
	}

	/* OTA 会话期间不 poll 8888 业务上报（含 PAUSE 退避窗口） */
	if (udp_ota_session_busy() != 0u) {
		return;
	}

	rc_trans_notify = 0u;
	pack_index = RC_PACK_INDEX_NONE;
	cat1_skt_udp_set_recv_allow(0u);
	poll_rc = remote_report_poll(s_cat1_tlv, sizeof(s_cat1_tlv), &frame_len,
	                             &rc_trans_notify, &pack_index);
	if (poll_rc != 0) {
		cat1_skt_udp_set_recv_allow(1u);
		return;
	}

	s_inflight_rc_trans = rc_trans_notify;
	s_inflight_pack_index = pack_index;

	begin_rc = cat1_skt_udp_send_begin(s_cat1_tlv, frame_len);
	cat1_skt_udp_set_recv_allow(1u);
	if (begin_rc == CAT1_MODEM_BUSY) {
		s_tx_waiting = 1u;
		return;
	}

	co_printf("[RC   ] UDP send begin fail len=%u\r\n", (unsigned)frame_len);
	if (s_inflight_rc_trans != 0u) {
		(void)cat1_post_udp_done(RC_RESULT_NET_ERROR, s_inflight_pack_index);
		rc_cat1_on_sktsend_fail();
	}
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
}

static void rc_cat1_poll_core(uint8_t ota_exclusive)
{
	uint32_t now_ms;
	cat1_modem_result_t mr;

	if (ota_exclusive == 0u) {
		rc_trans_poll();
	}

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

	if (s_sktsend_fail_streak >= CAT1_UDP_REBUILD_FAIL_THRESH
	    && cat1_modem_is_busy() == 0u) {
		rc_cat1_udp_rebuild("SKTSEND fail streak pending");
	}

	(void)udp_ota_poll_stall_pause();
	if (udp_ota_take_pause_event() != 0u) {
		rc_cat1_on_ota_paused_release();
	}
	(void)udp_ota_poll_auto_resume();

	if (udp_ota_is_active() != 0u) {
		udp_ota_poll_work();
		cat1_skt_udp_drain_urc();
	}

	mr = cat1_modem_step();
	if (s_tx_waiting != 0u && (mr == CAT1_MODEM_OK || mr == CAT1_MODEM_FAIL)) {
		rc_cat1_tx_complete(mr);
	} else if (s_tx_waiting != 0u && cat1_modem_is_busy() == 0u && s_inflight_ota != 0u) {
		co_printf("[RC   ] OTA tx zombie, force retry\r\n");
		rc_cat1_tx_complete(CAT1_MODEM_FAIL);
	}

	if (udp_ota_poll_watchdog() != 0u) {
		if (cat1_modem_is_busy() != 0u) {
			cat1_skt_udp_force_fail_active();
		}
		if (s_tx_waiting != 0u && s_inflight_ota != 0u) {
			rc_cat1_tx_complete(CAT1_MODEM_FAIL);
		}
	}

	if (cat1_modem_is_busy() == 0u) {
		if (ota_exclusive == 0u) {
			rc_cat1_handle_pending_tlv();
		} else if (s_pending_tlv != 0u) {
			rc_cat1_handle_pending_tlv();
		}
	}

	if (cat1_modem_is_busy() == 0u || udp_ota_session_busy() != 0u) {
		rc_cat1_poll_downlink();
	}

	if (udp_ota_is_active() != 0u) {
		udp_ota_poll_work();
		cat1_skt_udp_drain_urc();
	}

	rc_cat1_tx_step();

	if (udp_ota_is_active() != 0u) {
		rc_cat1_poll_downlink();
		udp_ota_poll_work();
		cat1_skt_udp_drain_urc();
	}

	if (ota_exclusive == 0u) {
		rc_key_input_poll_tag10();
	}
}

void rc_cat1_poll(void)
{
	rc_cat1_poll_core(0u);
}

void rc_cat1_poll_ota_exclusive(void)
{
	rc_cat1_poll_core(1u);
}
