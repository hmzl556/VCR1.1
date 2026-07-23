#include "ble_bas.h"
#include "bat_adc.h"
#include "ble_gap.h"
#include "gatt.h"
#include "gap.h"
#include "gmem.h"
#include "co_printf.h"

/** @brief Notify 周期：60 秒 */
#define BAS_NOTIFY_INTERVAL_MS  60000U

static gatt_svc_t s_bas_svc;              /* Battery Service 句柄 */
static gatt_chr_t s_bas_level_chr;        /* Battery Level 特征句柄 */
static peer_t     s_bas_peer;             /* 已订阅 Notify 的对端 */
static uint8_t    s_bas_notify_enabled;   /* CCCD 是否已使能 Notify（断连不清除） */
static uint8_t    s_bas_level;            /* 最近一次电量百分比缓存 */
static uint32_t   s_bas_last_notify_ms;   /* 上次 Notify 的时间戳 */
static uint8_t    s_bas_last_notify_valid;/* 是否已完成过至少一次周期 Notify */

/**
 * @brief  Battery Service 服务级回调
 */
static void bas_svc_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;

	switch (event) {
	case GATTS_EV_SERVICE_START: {
		gatt_svc_start_t *in = (gatt_svc_start_t *)param;
		co_printf("[BAS  ] svc start = %X\r\n", in->result);
		break;
	}
	default:
		break;
	}
}

/**
 * @brief  Battery Level 特征回调（读请求 / CCCD 订阅）
 */
static void bas_level_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;

	switch (event) {
	case GATTS_EV_CHR_VALUE_READ_REQ: {
		gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
		uint8_t val;

		s_bas_level = bat_get_level_percent(); /* 即时采样或按规则返回 */
		val = s_bas_level;
		gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, &val, 1U);
		co_printf("[BAS  ] level read = %u%%\r\n", (unsigned)val);
		break;
	}
	case GATTS_EV_CHR_VALUE_SEND_START: {
		/* 机顶盒写入 CCCD 使能/关闭 Notify 时触发 */
		gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;

		co_printf("[BAS  ] Battery Lecel send start event!\r\n");
		/* 按 type 置位/清除本地订阅标志；断连时不主动清除该标志 */
		s_bas_notify_enabled = (in->type == GATT_SEND_VAL_NOTIFICATION) ? 1U : 0U;
		s_bas_peer = s_bas_notify_enabled ? in->peer : NULL;
		if (s_bas_notify_enabled != 0U) {
			s_bas_level_chr = in->chr;
			/* 订阅后尽快发一次，随后按 60s 周期 */
			s_bas_last_notify_valid = 0U;
		}
		co_printf("[BAS  ] Battery Level CCCD=%s\r\n",
		          s_bas_notify_enabled ? "EN" : "DIS");
		break;
	}
	default:
		break;
	}
}

/**
 * @brief  立即采样并通过 Notify 上报当前电量
 */
static void bas_notify_now(void)
{
	peer_t peer;
	uint8_t val;

	if ((s_bas_notify_enabled == 0U) || (s_bas_level_chr == NULL)) {
		return;
	}

	/* 断连后 peer 可能为空，回连后用当前 GAP peer */
	peer = s_bas_peer;
	if (peer == NULL) {
		peer = ble_gap_get_peer();
	}
	if (peer == NULL) {
		return;
	}

	s_bas_level = bat_get_level_percent();
	val = s_bas_level;
	(void)gatts_chr_val_notif(peer, s_bas_level_chr, &val, 1U);
	co_printf("[BAS  ] notify level = %u%%\r\n", (unsigned)val);
}

/**
 * @brief  注册 BAS 1.0：Service 0x180F + Battery Level 0x2A19（READ|NOTIFY）
 */
int ble_bas_init(void)
{
	uuid_t svc_uuid = {
		UUID_LEN_16,
		UUID16_VAL_SET(0x180F) /* Battery Service */
	};
	uuid_t chr_uuid = {
		UUID_LEN_16,
		UUID16_VAL_SET(0x2A19) /* Battery Level */
	};

	s_bas_svc = NULL;
	s_bas_level_chr = NULL;
	s_bas_peer = NULL;
	s_bas_notify_enabled = 0U;
	s_bas_level = bat_get_cached_level();
	s_bas_last_notify_valid = 0U;

	s_bas_svc = gatts_svc_create(&svc_uuid, bas_svc_cbk, NULL);
	s_bas_level_chr = gatts_chr_add(s_bas_svc, &chr_uuid,
	                                GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY,
	                                ATT_PERM_READ,
	                                bas_level_cbk, NULL);
	gatts_dsc_ccc_add(s_bas_level_chr,
	                  GATT_CHR_PROP_NOTIFY,
	                  ATT_PERM_READ | ATT_PERM_WRITE);
	gatts_svc_start(s_bas_svc);

	co_printf("[BAS  ] init ok (Battery Service 0x180F)\r\n");
	return 0;
}

/**
 * @brief  BLE 断开时仅清 peer 句柄，保留 Notify 订阅标志
 */
void ble_bas_on_disconnect(void)
{
	s_bas_peer = NULL;
	/* 不清除 s_bas_notify_enabled：回连后仍可按原订阅状态周期 Notify */
}

/**
 * @brief  主循环周期调用：CCC 已订阅时每 60s Notify 一次电量
 */
void ble_bas_poll(void)
{
	uint32_t now;

	if (s_bas_notify_enabled == 0U) {
		return;
	}

	/* 未连接时不采样、不上报 */
	if ((s_bas_peer == NULL) && (ble_gap_get_peer() == NULL)) {
		return;
	}

	now = app_now_ms();
	if (s_bas_last_notify_valid == 0U) {
		/* 刚订阅：立即上报一次 */
		bas_notify_now();
		s_bas_last_notify_ms = now;
		s_bas_last_notify_valid = 1U;
		return;
	}

	if ((now - s_bas_last_notify_ms) >= BAS_NOTIFY_INTERVAL_MS) {
		bas_notify_now();
		s_bas_last_notify_ms = now;
	}
}
