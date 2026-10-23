#include <string.h>
#include "rc_cat1_bridge.h"
#include "cat1_usart1.h"
#include "cat1_skt_udp.h"
#include "cat1_udp_config.h"
#include "remote_reports.h"
#include "task_keyscan.h"
#include "bat_adc.h"
#include "heartbeat.h"      /* heartbeat_on_udp_ready — UDP 就绪补发心跳 */
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
#include "cat1_time_sync.h"
#include "flash_layout.h"
#include "qflash.h"
#include "userhard.h"   /* LTE_EN_ON / LTE_EN_OFF / LTE_EN_IS_ON */

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

static uint8_t s_cloud_save_pending;
static uint32_t s_setup_started_ms;
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
/** BUSY 孤儿监视：无 pending/队列/inflight 时超时强制 ERROR，避免 STB 永久卡死 */
static uint32_t s_busy_orphan_ms;

static uint8_t  s_inflight_ota;
/** 当前 in-flight 0x6868 帧头（与 UDP send OK/fail 成对打印） */
static uint8_t  s_inflight_6868_valid;
static uint8_t  s_inflight_6868_time[6];
static uint16_t s_inflight_6868_mid;
static uint16_t s_inflight_6868_len;
/** 本帧 ECSOST OK 后已打过 send OK / 已提前 DONE */
static uint8_t  s_uplink_ok_logged;
static uint8_t  s_rc_done_on_uplink;
/** 连续业务上行下行空等次数（Tag11 成功解析后清零） */
static uint8_t  s_silent_dl;
/** silent 达阈值但 modem busy 时延后重建 */
static uint8_t  s_silent_rebuild_pending;

/* BUSY 孤儿监视超时时间 */
#ifndef RC_BUSY_ORPHAN_TIMEOUT_MS
#define RC_BUSY_ORPHAN_TIMEOUT_MS  8000u
#endif

static uint32_t rc_cat1_now_ms(void)
{
	return sdk_timer_get_current_ms();
}

static void rc_cat1_inflight_6868_clear(void);
static void rc_cat1_inflight_6868_capture(const uint8_t *frame, uint16_t frame_len);
static void rc_cat1_log_inflight_6868_time(void);

/** Tag0x11 策略 Flash 持久化结构（与 flash_layout.h 扇区对应） */
typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t  key_capture;    /* DataID 0x01: 1=开, 2=关 */
	uint8_t  reserved0;
	uint16_t interval_sec;   /* DataID 0x02 */
	uint16_t item_count;     /* DataID 0x03 */
	uint16_t reserved1;
	uint32_t checksum;
} app_cloud_policy_store_t;

static uint32_t rc_cat1_cloud_policy_checksum(const app_cloud_policy_store_t *p)
{
	uint32_t sum = 0u;

	sum ^= p->magic;
	sum ^= (uint32_t)p->version;
	sum ^= (uint32_t)p->key_capture;
	sum ^= (uint32_t)p->interval_sec;
	sum ^= (uint32_t)p->item_count;
	return sum;
}

static void rc_cat1_policy_to_store(const uint8_t policy[6], uint8_t key_capture,
                                    app_cloud_policy_store_t *out)
{
	out->magic = APP_CLOUD_POLICY_MAGIC;
	out->version = (uint16_t)APP_CLOUD_POLICY_VER;
	out->key_capture = key_capture;
	out->reserved0 = 0u;
	out->interval_sec = (uint16_t)(((uint16_t)policy[2] << 8) | policy[3]);
	out->item_count = (uint16_t)(((uint16_t)policy[4] << 8) | policy[5]);
	out->reserved1 = 0u;
	out->checksum = rc_cat1_cloud_policy_checksum(out);
}

static int rc_cat1_cloud_policy_store_valid(const app_cloud_policy_store_t *p)
{
	if (p->magic != APP_CLOUD_POLICY_MAGIC) {
		return 0;
	}
	if (p->version != (uint16_t)APP_CLOUD_POLICY_VER) {
		return 0;
	}
	if (p->key_capture != 1u && p->key_capture != 2u) {
		return 0;
	}
	if (p->checksum != rc_cat1_cloud_policy_checksum(p)) {
		return 0;
	}
	return 1;
}

/**
 * @brief  从 Flash 恢复 Tag0x11 策略到 RAM（须在 Qflash_Init 之后调用）
 */
static void rc_cat1_cloud_policy_load_from_flash(void)
{
	app_cloud_policy_store_t store;

	if (Qflash_Read(FLASH_USER_CFG_START, (uint8_t *)&store,
	                (uint32_t)sizeof(store)) != FlashOperationSuccess) {
		co_printf("[RC   ] CloudPolicy flash read fail\r\n");
		return;
	}
	if (rc_cat1_cloud_policy_store_valid(&store) == 0) {
		co_printf("[RC   ] CloudPolicy flash empty/invalid\r\n");
		return;
	}

	taskENTER_CRITICAL();
	s_cloud_policy[0] = 0u;
	s_cloud_policy[1] = 0u;
	s_cloud_policy[2] = (uint8_t)((store.interval_sec >> 8) & 0xFFu);
	s_cloud_policy[3] = (uint8_t)(store.interval_sec & 0xFFu);
	s_cloud_policy[4] = (uint8_t)((store.item_count >> 8) & 0xFFu);
	s_cloud_policy[5] = (uint8_t)(store.item_count & 0xFFu);
	taskEXIT_CRITICAL();

	remote_set_key_capture_enable(store.key_capture);

	co_printf("[RC   ] CloudPolicy restored: capture=%u interval=%u count=%u\r\n",
	          (unsigned)store.key_capture,
	          (unsigned)store.interval_sec,
	          (unsigned)store.item_count);
}

/**
 * @brief  将当前策略写入 Flash；与已存内容相同则跳过擦写
 */
static void rc_cat1_cloud_policy_save_to_flash(const uint8_t policy[6])
{
	app_cloud_policy_store_t neu;
	app_cloud_policy_store_t old;
	uint8_t key_capture;

	if (policy == NULL) {
		return;
	}

    /* Flash masks IRQs; preserve the live IR frame/capture and save later. */
    if (task_keyscan_ir_busy() || udp_ota_session_busy()) {
        s_cloud_save_pending = 1u;
        return;
    }
    s_cloud_save_pending = 0u;
	key_capture = (remote_is_key_capture_enabled() != 0) ? 1u : 2u;
	rc_cat1_policy_to_store(policy, key_capture, &neu);

	if (Qflash_Read(FLASH_USER_CFG_START, (uint8_t *)&old,
	                (uint32_t)sizeof(old)) == FlashOperationSuccess
	    && rc_cat1_cloud_policy_store_valid(&old) != 0
	    && old.key_capture == neu.key_capture
	    && old.interval_sec == neu.interval_sec
	    && old.item_count == neu.item_count) {
		co_printf("[RC   ] CloudPolicy flash unchanged, skip write\r\n");
		return;
	}

	if (Qflash_Erase_Sector(FLASH_USER_CFG_START) != FlashOperationSuccess) {
		co_printf("[RC   ] CloudPolicy flash erase fail\r\n");
		return;
	}
	if (Qflash_Write(FLASH_USER_CFG_START, (uint8_t *)&neu,
	                 (uint32_t)sizeof(neu)) != FlashOperationSuccess) {
		co_printf("[RC   ] CloudPolicy flash write fail\r\n");
		return;
	}

	co_printf("[RC   ] CloudPolicy flash saved: capture=%u interval=%u count=%u\r\n",
	          (unsigned)neu.key_capture,
	          (unsigned)neu.interval_sec,
	          (unsigned)neu.item_count);
}

/**
 * @brief  通知 STB RC-TRANS 结束（队列满则本任务直接 Notify，避免丢 DONE）
 */
static void rc_cat1_notify_udp_done(uint8_t result, uint32_t pack_index)
{
	if (pack_index == RC_PACK_INDEX_NONE) {
		return;
	}
	if (cat1_post_udp_done(result, pack_index) != pdTRUE) {
		co_printf("[RC   ] udp_done Q full, direct notify result=0x%02X pack=0x%08X\r\n",
		          (unsigned)result, (unsigned)pack_index);
		rc_cat1_on_udp_done(result, pack_index);
	}
}

/**
 * @brief  中止进行中的 RC-TRANS 并回 ERROR/BUSY_REJECT，解锁 STB 后续上报
 */
static void rc_cat1_abort_inflight_rc_trans(uint8_t result, const char *reason)
{
	uint8_t was_rc = s_inflight_rc_trans;
	uint32_t pack = s_inflight_pack_index;

	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	rc_cat1_inflight_6868_clear();
	if (was_rc == 0u) {
		return;
	}
	co_printf("[RC   ] abort RC-TRANS: %s result=0x%02X pack=0x%08X\r\n",
	          (reason != NULL) ? reason : "?", (unsigned)result, (unsigned)pack);
	rc_cat1_notify_udp_done(result, pack);
}

/**
 * @brief  丢弃发送队列中尚未发出的 RC-TRANS，并回 STATUS（避免 OTA/rebuild 期间永久 BUSY）
 */
static void rc_cat1_abort_queued_rc_trans(uint8_t result, const char *reason)
{
	uint32_t packs[2];
	uint8_t n;
	uint8_t i;

	n = remote_report_abort_rc_trans(packs, (uint8_t)(sizeof(packs) / sizeof(packs[0])));
	for (i = 0u; i < n; i++) {
		co_printf("[RC   ] abort queued RC-TRANS: %s result=0x%02X pack=0x%08X\r\n",
		          (reason != NULL) ? reason : "?", (unsigned)result, (unsigned)packs[i]);
		rc_cat1_on_udp_done(result, packs[i]);
	}
}

/**
 * @brief  BUSY 无在途资源时超时强制 ERROR（防丢 DONE 后永久拒收）
 */
static void rc_cat1_busy_orphan_watch(void)
{
	const rc_status_t *st;
	uint32_t now_ms;

	st = ble_rc_svc_get_status();
	if (st == NULL || st->state != RC_STATE_BUSY) {
		s_busy_orphan_ms = 0u;
		return;
	}
	if (s_pending_tlv != 0u || s_tx_waiting != 0u || s_inflight_rc_trans != 0u
	    || remote_report_queue_count() != 0u) {
		s_busy_orphan_ms = 0u;
		return;
	}

	now_ms = rc_cat1_now_ms();
	if (s_busy_orphan_ms == 0u) {
		s_busy_orphan_ms = now_ms;
		return;
	}
	if ((now_ms - s_busy_orphan_ms) < RC_BUSY_ORPHAN_TIMEOUT_MS) {
		return;
	}

	co_printf("[RC   ] BUSY orphan timeout %u ms, force ERROR pack=0x%08X\r\n",
	          (unsigned)RC_BUSY_ORPHAN_TIMEOUT_MS, (unsigned)st->pack_index);
	s_busy_orphan_ms = 0u;
	rc_cat1_on_udp_done(RC_RESULT_NET_ERROR, st->pack_index);
}

void rc_cat1_init(void)
{
    bat_adc_restart_window();
	co_printf("[RC   ] CAT1 init start\r\n");

	/*
	 * ★ 不重置时间同步: 锚点是纯 MCU 侧状态(基于 sdk_timer 单调计数),
	 *   与模组通断电无关。此函数在每次心跳上电/休眠唤醒时执行,
	 *   若重置会把 Tag11/CCLK 建立的有效锚点抹掉, 导致后续上报
	 *   时间全部 fallback (实测: send11 锚定后下一个心跳又变 000101000000)。
	 *   CCLK 同步成功时会自然覆盖为更优锚点, 无需先清。
	 */
	// cat1_time_sync_reset();
	cat1_skt_udp_setup_reset();

	s_udp_ready = 0u;
	s_cat1_booting = 1u;
    s_setup_started_ms = rc_cat1_now_ms();
	s_cat1_boot_ms = 0u;
	s_pending_tlv = 0u;
	s_tx_waiting = 0u;
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_inflight_ota = 0u;
	s_setup_retry_ms = 0u;
	s_sktsend_fail_streak = 0u;
	s_tag01_queued = 0u;
	s_busy_orphan_ms = 0u;
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	s_silent_dl = 0u;
	s_silent_rebuild_pending = 0u;
	rc_cat1_inflight_6868_clear();
	memset(s_cloud_policy, 0, sizeof(s_cloud_policy));

	remote_reports_init();
	remote_tag10_enqueue_set_paused(0u);
	udp_ota_init();
	/* Qflash 已就绪：从独立扇区恢复 Tag0x11 策略（覆盖默认值） */
	rc_cat1_cloud_policy_load_from_flash();
	USART1_CAT1_Init();
	cat1_skt_udp_bind_recv_buf(s_cat1_tlv, (uint16_t)sizeof(s_cat1_tlv));
	cat1_skt_udp_set_recv_allow(1u);
	co_printf("[RC   ] CAT1 init OK, boot delay %u ms\r\n", (unsigned)CAT1_MODEM_BOOT_DELAY_MS);
}

void rc_cat1_cancel_pending(void)
{
	uint32_t done_pack = RC_PACK_INDEX_NONE;
	uint8_t had_pending = 0u;

	taskENTER_CRITICAL();
	if (s_pending_tlv != 0u) {
		had_pending = 1u;
		done_pack = s_pending_pack_index;
		s_pending_tlv = 0u;
		s_pending_tlv_len = 0u;
		s_pending_pack_index = RC_PACK_INDEX_NONE;
	}
	taskEXIT_CRITICAL();
	rc_trans_release();
	if (had_pending != 0u) {
		co_printf("[RC   ] pending TLV canceled pack=0x%08X\r\n", (unsigned)done_pack);
		rc_cat1_on_udp_done(RC_RESULT_NET_ERROR, done_pack);
	}
}

static void rc_cat1_udp_rebuild(const char *reason)
{
	uint8_t dropped;

	if (cat1_modem_is_busy() != 0u) {
		return;
	}

	co_printf("[RC   ] CAT1 UDP rebuild: %s (silent=%u)\r\n",
	          (reason != NULL) ? reason : "?", (unsigned)s_silent_dl);

	/* 重建期间暂停 Tag10，并清掉排队中的按键，避免重建后突发灌包 */
	remote_tag10_enqueue_set_paused(1u);
	dropped = remote_report_drop_all_tag10();
	if (dropped > 0u) {
		co_printf("[RC   ] rebuild drop Tag10=%u\r\n", (unsigned)dropped);
	}
	s_silent_dl = 0u;
	s_silent_rebuild_pending = 0u;
	/* 重建成功后重新发 Tag01 探活 NAT/下行 */
	s_tag01_queued = 0u;

	s_udp_ready = 0u;
    s_setup_started_ms = rc_cat1_now_ms();
	if (s_tx_waiting != 0u || s_inflight_rc_trans != 0u) {
		rc_cat1_abort_inflight_rc_trans(RC_RESULT_NET_ERROR, reason);
	}
	rc_cat1_abort_queued_rc_trans(RC_RESULT_NET_ERROR, reason);
	s_tx_waiting = 0u;
	s_inflight_ota = 0u;
	cat1_skt_udp_setup_reset();
	s_setup_retry_ms = rc_cat1_now_ms();
	s_sktsend_fail_streak = 0u;
}

/**
 * @brief  连续下行空等达阈值 → CAT1 断电（下次上电自然重建 socket/网络）
 * @note   替代原软重建: 断电后再次上电走完整 setup(ECSOCL→ECSOCR),
 *         等效网络重建, 且符合 CAT1 平时断开的省电策略。
 */
static void rc_cat1_try_silent_rebuild(void)
{
	if (s_silent_dl < CAT1_UDP_SILENT_REBUILD_THRESH) {
		return;
	}
	if (udp_ota_session_busy() != 0u) {
		return;
	}
	if (cat1_modem_is_busy() != 0u) {
		if (s_silent_rebuild_pending == 0u) {
			s_silent_rebuild_pending = 1u;
			remote_tag10_enqueue_set_paused(1u);
			(void)remote_report_drop_all_tag10();
			co_printf("[RC   ] silent power-off pending (silent=%u thresh=%u)\r\n",
			          (unsigned)s_silent_dl,
			          (unsigned)CAT1_UDP_SILENT_REBUILD_THRESH);
		}
		return;
	}
	co_printf("[RC   ] silent=%u >= thresh=%u, CAT1 power OFF\r\n",
	          (unsigned)s_silent_dl, (unsigned)CAT1_UDP_SILENT_REBUILD_THRESH);
	rc_cat1_power_down();
}

/** 业务上行 ECSOST 失败累计（Tag10/Tag01/RC-TRANS）；满阈值软重建。OTA 不走此路径。 */
static void rc_cat1_on_sktsend_fail(void)
{
	if (s_sktsend_fail_streak < 255u) {
		s_sktsend_fail_streak++;
	}
	co_printf("[RC   ] UDP send fail streak=%u/%u\r\n",
	          (unsigned)s_sktsend_fail_streak,
	          (unsigned)CAT1_UDP_REBUILD_FAIL_THRESH);
	if (s_sktsend_fail_streak >= CAT1_UDP_REBUILD_FAIL_THRESH) {
		/* 替代软重建: 断电, 下次上电自然重建网络 */
		rc_cat1_power_down();
	}
}

static void rc_cat1_on_setup_ok(void)
{
	s_udp_ready = 1u;
	s_sktsend_fail_streak = 0u;
	s_silent_dl = 0u;
	s_silent_rebuild_pending = 0u;
	remote_tag10_enqueue_set_paused(0u);
	co_printf("[RC   ] CAT1 UDP ready\r\n");
	udp_ota_on_udp_ready();
	if (s_tag01_queued == 0u) {
		if (remote_queue_tag01_status() != 0) {
			co_printf("[RC   ] Tag01 queue fail\r\n");
		} else {
			s_tag01_queued = 1u;
			co_printf("[RC   ] Tag01 queued (probe after setup)\r\n");
		}
	}
	/* 心跳挂起补发: 在 UDP 就绪瞬间入队 (提前入队会被建链期误发丢失) */
	heartbeat_on_udp_ready();
}

/**
 * @brief  UDP socket 是否已就绪 (setup 完成)
 */
uint8_t rc_cat1_udp_is_ready(void)
{
	return s_udp_ready;
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

    /* Bound a missing SIM/network: no endless powered setup retry or BUSY. */
    if (rc_cat1_now_ms() - s_setup_started_ms >= 120000u) {
        co_printf("[RC   ] setup deadline, abort and power off\r\n");
        rc_cat1_power_down();
        return;
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
	const rc_status_t *cur;

	cur = ble_rc_svc_get_status();
	/* 仅 BUSY→DONE/ERROR；忽略过期完成，防止 abort 后晚到的 DONE 污染状态 */
	if (cur == NULL || cur->state != RC_STATE_BUSY) {
		co_printf("[RC   ] late udp_done ignored state=0x%02X result=0x%02X pack=0x%08X\r\n",
		          (unsigned)((cur != NULL) ? cur->state : 0xFFu),
		          (unsigned)result, (unsigned)pack_index);
		return;
	}
	if (pack_index != RC_PACK_INDEX_NONE && pack_index != cur->pack_index) {
		co_printf("[RC   ] udp_done pack mismatch cur=0x%08X evt=0x%08X result=0x%02X\r\n",
		          (unsigned)cur->pack_index, (unsigned)pack_index, (unsigned)result);
		return;
	}

	peer = ble_rc_svc_get_peer();
	memcpy(&st, cur, sizeof(st));
	st.pack_index = (pack_index != RC_PACK_INDEX_NONE) ? pack_index : cur->pack_index;
	st.retry_count = 0u;

	if (result == RC_RESULT_OK) {
		st.state = RC_STATE_DONE;
		st.result = RC_RESULT_OK;
		rc_trans_note_upload_ok(st.pack_index);
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

		CAT1_UDP_LOG("[RC   ] UDP recv OK, len=%u\r\n", (unsigned)pdu_len);

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
			uint16_t i;
			co_printf("[RC   ] UDP downlink parse fail, len=%u raw:", (unsigned)pdu_len);
			for (i = 0u; i < pdu_len; i++) {
				co_printf(" %02X", (unsigned)s_cat1_tlv[i]);
				if (((i + 1u) % 16u) == 0u && (i + 1u) < pdu_len) {
					co_printf("\r\n[RC   ]  ");
				}
			}
			co_printf("\r\n");
			continue;
		}

		s_silent_dl = 0u;

		pm_activity_notify(PM_ACTIVITY_CAT1);

		taskENTER_CRITICAL();
		memcpy(s_cloud_policy, policy, 6u);
		taskEXIT_CRITICAL();
		co_printf("[RC   ] CloudPolicy updated: interval=%u count=%u heartbeat=%us\r\n",
				  (unsigned)(((uint32_t)policy[0] << 24) | ((uint32_t)policy[1] << 16)
						   | ((uint32_t)policy[2] << 8) | policy[3]),
				  (unsigned)(((uint16_t)policy[4] << 8) | policy[5]),
                  (unsigned)heartbeat_get_period_s());
		rc_cat1_cloud_policy_save_to_flash(policy);
	}
}

static void rc_cat1_handle_pending_tlv(void)
{
	uint8_t *tlv;
	uint16_t asm_len;
	uint16_t pending_tlv_len = 0u;
	uint32_t done_pack = RC_PACK_INDEX_NONE;
	int queue_rc;

	/* OTA 会话期间丢弃 pending TLV，Cat.1 仅服务 OTA 拉片；必须回 ERROR 解锁 BUSY */
	if (udp_ota_session_busy() != 0u) {
		if (s_pending_tlv != 0u) {
			taskENTER_CRITICAL();
			done_pack = s_pending_pack_index;
			s_pending_tlv = 0u;
			s_pending_tlv_len = 0u;
			s_pending_pack_index = RC_PACK_INDEX_NONE;
			taskEXIT_CRITICAL();
			rc_trans_release();
			co_printf("[RC   ] pending TLV dropped: OTA session pack=0x%08X\r\n",
			          (unsigned)done_pack);
			rc_cat1_on_udp_done(RC_RESULT_BUSY_REJECT, done_pack);
		}
		/* 已入队但尚未 ECSOST 的 RC-TRANS：OTA 期间发不出去，立刻 ERROR 解锁 */
		rc_cat1_abort_queued_rc_trans(RC_RESULT_BUSY_REJECT, "OTA session");
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

    if (remote_normalize_tlv_blob(tlv, &asm_len) != REMOTE_QUEUE_OK) {
        co_printf("[RC   ] invalid STB TLV pack=0x%08X\r\n", (unsigned)done_pack);
        rc_trans_release();
        rc_cat1_on_udp_done(RC_RESULT_PARAM_ERROR, done_pack);
        return;
    }
	cat1_skt_udp_set_recv_allow(0u);
	queue_rc = remote_queue_tlv_blob(tlv, asm_len, done_pack);
	cat1_skt_udp_set_recv_allow(1u);
	rc_trans_release();

	if (queue_rc == REMOTE_QUEUE_FULL) {
		co_printf("[RC   ] UDP TLV queue full pack=0x%08X\r\n", (unsigned)done_pack);
		rc_cat1_on_udp_done(RC_RESULT_BUSY_REJECT, done_pack);
	} else if (queue_rc != REMOTE_QUEUE_OK) {
		co_printf("[RC   ] UDP TLV queue fail pack=0x%08X rc=%d\r\n",
		          (unsigned)done_pack, queue_rc);
		rc_cat1_on_udp_done(queue_rc == REMOTE_QUEUE_PARAM ? RC_RESULT_PARAM_ERROR : RC_RESULT_NET_ERROR, done_pack);
	}
}

static void rc_cat1_inflight_6868_clear(void)
{
	s_inflight_6868_valid = 0u;
	s_inflight_6868_mid = 0u;
	s_inflight_6868_len = 0u;
}

static void rc_cat1_inflight_6868_capture(const uint8_t *frame, uint16_t frame_len)
{
	if (frame == NULL || frame_len < 11u || frame[0] != 0x68u || frame[1] != 0x68u) {
		rc_cat1_inflight_6868_clear();
		return;
	}
	memcpy(s_inflight_6868_time, frame + 3u, 6u);
	s_inflight_6868_mid = (uint16_t)(((uint16_t)frame[9] << 8) | frame[10]);
	s_inflight_6868_len = frame_len;
	s_inflight_6868_valid = 1u;
}

static void rc_cat1_log_inflight_6868_time(void)
{
	if (s_inflight_6868_valid == 0u) {
		return;
	}
	CAT1_UDP_LOG("*****************************************************\r\n");
	CAT1_UDP_LOG("[RC   ] UDP6868 time=%02X%02X%02X%02X%02X%02X mid=%u len=%u\r\n",
	          s_inflight_6868_time[0], s_inflight_6868_time[1], s_inflight_6868_time[2],
	          s_inflight_6868_time[3], s_inflight_6868_time[4], s_inflight_6868_time[5],
	          (unsigned)s_inflight_6868_mid, (unsigned)s_inflight_6868_len);
	CAT1_UDP_LOG("*****************************************************\r\n");
}

static void rc_cat1_on_uplink_ok_early(void)
{
	if (s_inflight_ota != 0u || s_uplink_ok_logged != 0u) {
		return;
	}
	rc_cat1_log_inflight_6868_time();
	CAT1_UDP_LOG("[RC   ] UDP send OK!\r\n");
	s_uplink_ok_logged = 1u;
	s_sktsend_fail_streak = 0u;
	if (s_inflight_rc_trans != 0u && s_rc_done_on_uplink == 0u) {
		rc_cat1_notify_udp_done(RC_RESULT_OK, s_inflight_pack_index);
		s_rc_done_on_uplink = 1u;
	}
}

static void rc_cat1_log_recv_fail_if_any(void)
{
	if (cat1_skt_udp_take_downlink_fail_event() != 0u) {
		co_printf("[RC   ] UDP recv fail!\r\n");
	}
}

static void rc_cat1_log_recv_idle_if_any(void)
{
	if (cat1_skt_udp_take_downlink_idle_event() == 0u) {
		return;
	}
	if (s_silent_dl < 255u) {
		s_silent_dl++;
	}
	CAT1_UDP_LOG("[RC   ] UDP recv idle timeout silent=%u\r\n", (unsigned)s_silent_dl);
	rc_cat1_try_silent_rebuild();
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
		if (ota_inflight != 0u) {
			CAT1_UDP_LOG("[RC   ] UDP send OK!\r\n");
			s_sktsend_fail_streak = 0u;
			udp_ota_on_uplink_sent();
		} else {
			if (s_uplink_ok_logged == 0u) {
				rc_cat1_on_uplink_ok_early();
			}
			rc_cat1_log_recv_fail_if_any();
			rc_cat1_log_recv_idle_if_any();
			rc_cat1_poll_downlink();
			/* 空闲补读：窗口外迟到的 +ECSONMI */
			if (cat1_modem_is_busy() == 0u) {
				cat1_skt_udp_pump_urc();
				rc_cat1_poll_downlink();
			}
			if (s_inflight_rc_trans != 0u && s_rc_done_on_uplink == 0u) {
				rc_cat1_notify_udp_done(RC_RESULT_OK, s_inflight_pack_index);
				s_rc_done_on_uplink = 1u;
			}
		}
	} else {
		if (ota_inflight != 0u) {
			udp_ota_on_uplink_timeout();
			rc_cat1_poll_downlink();
		} else if (s_uplink_ok_logged != 0u) {
			/* 上行已成功；终态 FAIL 仅作收尾（理论上 ECSORF 放弃已改为 OK） */
			rc_cat1_log_recv_fail_if_any();
			rc_cat1_poll_downlink();
		} else {
			rc_cat1_log_inflight_6868_time();
			co_printf("[RC   ] UDP send fail!\r\n");
			rc_cat1_poll_downlink();
			rc_cat1_on_sktsend_fail();
			if (s_inflight_rc_trans != 0u && s_rc_done_on_uplink == 0u) {
				rc_cat1_notify_udp_done(RC_RESULT_NET_ERROR, s_inflight_pack_index);
				s_rc_done_on_uplink = 1u;
			}
		}
		if (ota_inflight != 0u && s_inflight_rc_trans != 0u) {
			rc_cat1_notify_udp_done(RC_RESULT_NET_ERROR, s_inflight_pack_index);
		}
	}
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	rc_cat1_inflight_6868_clear();
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
		rc_cat1_abort_inflight_rc_trans(RC_RESULT_BUSY_REJECT, "OTA PAUSE");
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
		rc_cat1_inflight_6868_clear();
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
		rc_cat1_abort_queued_rc_trans(RC_RESULT_BUSY_REJECT, "OTA block TX");
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
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	rc_cat1_inflight_6868_capture(s_cat1_tlv, frame_len);

	begin_rc = cat1_skt_udp_send_begin(s_cat1_tlv, frame_len);
	cat1_skt_udp_set_recv_allow(1u);
	if (begin_rc == CAT1_MODEM_BUSY) {
		s_tx_waiting = 1u;
		return;
	}

	rc_cat1_log_inflight_6868_time();
	co_printf("[RC   ] UDP send fail!\r\n");
	if (s_inflight_rc_trans != 0u) {
		rc_cat1_notify_udp_done(RC_RESULT_NET_ERROR, s_inflight_pack_index);
	}
	rc_cat1_on_sktsend_fail();
	s_inflight_rc_trans = 0u;
	s_inflight_pack_index = RC_PACK_INDEX_NONE;
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	rc_cat1_inflight_6868_clear();
}

static void rc_cat1_poll_core(uint8_t ota_exclusive)
{
	uint32_t now_ms;
	cat1_modem_result_t mr;

    /* Continue in-flight UART/OTA, but do not start idle Flash/AT work while
     * IR owns precise timing. Deferred cloud policy is saved before a reboot
     * of the modem could reload older policy from Flash. */
    if (task_keyscan_ir_busy() && !cat1_modem_is_busy() && !udp_ota_session_busy()) return;
    if (s_cloud_save_pending && !cat1_modem_is_busy() && !udp_ota_session_busy())
        rc_cat1_cloud_policy_save_to_flash(s_cloud_policy);

    /* Assembly timeout belongs to BLE, including while LTE is powered off. */
    if (ota_exclusive == 0u) rc_trans_poll();

	/*
	 * CAT1 断电时：
	 *   - 有 RC-TRANS 待上传(s_pending_tlv) → 上电并继续初始化流程
	 *   - 无数据 → 跳过所有轮询
	 */
	if (!LTE_EN_IS_ON()) {
		if (s_pending_tlv != 0u || udp_ota_session_busy() != 0u) {
			rc_cat1_power_up();
		} else {
			return;
		}
	}

	/* AT 进行中优先抽干 RX，缩短 ring 占用时间 */
	if (cat1_modem_is_busy() != 0u) {
		cat1_skt_udp_drain_urc();
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
		/* setup 未完成时仍需看护孤儿 BUSY（队列已空但 STATUS 卡住） */
		rc_cat1_busy_orphan_watch();
		return;
	}

	/* CME 406 等：模组侧套接字已不存在，立即软重建 */
	if (cat1_skt_udp_take_socket_gone_event() != 0u) {
		if (cat1_modem_is_busy() != 0u) {
			cat1_skt_udp_force_fail_active();
		}
		rc_cat1_udp_rebuild("socket gone (CME 406)");
		rc_cat1_busy_orphan_watch();
		return;
	}

	if (s_sktsend_fail_streak >= CAT1_UDP_REBUILD_FAIL_THRESH
	    && cat1_modem_is_busy() == 0u) {
		rc_cat1_udp_rebuild("SKTSEND fail streak pending");
	}
	if (s_silent_rebuild_pending != 0u && cat1_modem_is_busy() == 0u) {
		rc_cat1_try_silent_rebuild();
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
	if (s_tx_waiting != 0u && s_inflight_ota == 0u
	    && cat1_skt_udp_take_uplink_ok_event() != 0u) {
		rc_cat1_on_uplink_ok_early();
	}
	if (s_tx_waiting != 0u && (mr == CAT1_MODEM_OK || mr == CAT1_MODEM_FAIL)) {
		rc_cat1_tx_complete(mr);
	} else if (s_tx_waiting != 0u && cat1_modem_is_busy() == 0u
	           && (s_inflight_ota != 0u || s_inflight_rc_trans != 0u)) {
		co_printf("[RC   ] tx zombie ota=%u rc=%u, force fail\r\n",
		          (unsigned)s_inflight_ota, (unsigned)s_inflight_rc_trans);
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

	rc_cat1_busy_orphan_watch();

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

	/*
	 * 事务完成断电检测: 无待发数据 + 无在途 + 无 OTA + modem 空闲
	 * → CAT1 断电。Tag11 已收到且无 OTA 时自然走到这里断电;
	 * 下次 s_pending_tlv 置位时 poll 入口处自动重新上电。
	 * 注意: s_udp_ready==1 才检查, 排除 boot 延时和 setup 阶段
	 * (此时 modem 空闲但初始化未完成, 不应断电)。
	 */
	if (s_udp_ready != 0u
	    && udp_ota_session_busy() == 0u
	    && s_pending_tlv == 0u
	    && s_tx_waiting == 0u
	    && remote_report_queue_count() == 0u
	    && cat1_modem_is_busy() == 0u) {
		rc_cat1_power_down();
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

/**
 * @brief  CAT1 模组断电：关 LTE_EN，重置所有状态，放弃未完成的业务
 * @note   供 pm_standby_enter() 调用。不等模组响应，直接断电。
 *         在途 RC-TRANS 会回 ERROR 给 STB，Tag10 会丢弃。
 */
void rc_cat1_power_down(void)
{
	co_printf("[RC   ] CAT1 power OFF\r\n");

	/* 1. 中止进行中和排队中的 RC-TRANS，避免 STB 永久卡 BUSY */
	rc_cat1_abort_inflight_rc_trans(RC_RESULT_NET_ERROR, "CAT1 power off");
	rc_cat1_abort_queued_rc_trans(RC_RESULT_NET_ERROR, "CAT1 power off");
    if (s_pending_tlv) rc_cat1_cancel_pending();
    remote_report_clear_queue();

	/* 2. 如果 OTA 进行中，也中止 */
	if (udp_ota_session_busy() != 0u || udp_ota_is_active() != 0u) {
		co_printf("[RC   ] CAT1 power off: OTA session interrupted\r\n");
	}

	/* 3. 重置所有 CAT1/UDP 软件状态 */
	cat1_skt_udp_setup_reset();
	s_udp_ready      = 0u;
	s_cat1_booting   = 0u;
	s_cat1_boot_ms   = 0u;
	s_pending_tlv    = 0u;
	s_tx_waiting     = 0u;
	s_inflight_rc_trans = 0u;
	s_inflight_ota   = 0u;
	s_setup_retry_ms = 0u;
	s_tag01_queued   = 0u;
	s_busy_orphan_ms = 0u;
	s_uplink_ok_logged = 0u;
	s_rc_done_on_uplink = 0u;
	s_silent_dl      = 0u;
	s_silent_rebuild_pending = 0u;
	rc_cat1_inflight_6868_clear();

	/* 4. 硬件断电 */
	LTE_EN_OFF();
    bat_adc_restart_window();
}

/**
 * @brief  CAT1 模组上电：开 LTE_EN，完整走 rc_cat1_init() 初始化流程
 * @note   供 pm_standby_exit() 调用。包含 boot 延时 + UDP setup。
 *         保护断电期间 STB 通过 BLE 发来的 RC-TRANS（s_pending_tlv），
 *         避免被 rc_cat1_init() 内部的清零误清除。
 */
void rc_cat1_power_up(void)
{
	uint8_t  saved_pending_tlv      = s_pending_tlv;
	uint16_t saved_pending_tlv_len  = s_pending_tlv_len;
	uint32_t saved_pending_pack_idx = s_pending_pack_index;

	co_printf("[RC   ] CAT1 power ON\r\n");
	LTE_EN_ON();
	rc_cat1_init();

	/* rc_cat1_init() 内部把 s_pending_tlv 清零了，恢复之 */
	s_pending_tlv       = saved_pending_tlv;
	s_pending_tlv_len   = saved_pending_tlv_len;
	s_pending_pack_index = saved_pending_pack_idx;
	if (saved_pending_tlv != 0u) {
		co_printf("[RC   ] CAT1 power ON: restored pending TLV pack=0x%08X\r\n",
		          (unsigned)saved_pending_pack_idx);
	}
}

/**
 * @brief  查询 CAT1 模组当前是否已上电
 * @return 1=上电，0=断电
 */
uint8_t rc_cat1_is_powered(void)
{
	return (LTE_EN_IS_ON()) ? 1u : 0u;
}

uint8_t rc_cat1_has_work(void)
{
    return (uint8_t)(s_cloud_save_pending || s_pending_tlv || s_tx_waiting ||
        cat1_modem_is_busy() || (LTE_EN_IS_ON() && (!s_udp_ready || s_cat1_booting)) ||
        remote_report_queue_count());
}
