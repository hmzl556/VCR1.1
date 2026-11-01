#include "rc_key_input.h"
#include "ble_rc_svc.h"
#include "remote_reports.h"
#include "cat1_udp_config.h"
#include "udp_ota.h"
#include "co_printf.h"
#include "gmem.h"               /* app_now_ms */
#include "rc_cat1_bridge.h"     /* rc_cat1_get_cloud_policy */
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* 单帧 0x6868 数据域最多容纳 (RC_UDP_AES_PLAIN_DATA_MAX-2)/12 条按键 TLV */
#define TAG10_BATCH_MAX  46u

static uint8_t s_tag10_keys[TAG10_BATCH_MAX];
static uint8_t s_tag10_times[TAG10_BATCH_MAX][6];
static uint8_t s_tag10_count;
static uint32_t s_tag10_first_ms;
static uint8_t s_tag10_qfull_logged;		//处理warning时不要删

/**
 * @brief  硬件 key_id → Tag0x10 DataID1 协议键值（《4.3 遥控器按键采集》1~18）
 * @param  key_id  app_keyscan 按键编号（与 ble_hid_kbd 注释一致）
 * @param  action  0x01=按下，0x00=抬起（仅语音键抬起映射为 14）
 * @return 协议键值；0 表示不上报 Tag10
 */
static uint8_t rc_key_id_to_tag10(uint16_t key_id, uint8_t action)
{
	if (key_id == 15u) {
		return (action == 0x01u) ? 13u : 14u;
	}
	if (action != 0x01u) {
		return 0u;
	}
	switch (key_id) {
	case 16u: return 1u;   /* 上 */
	case  7u: return 2u;   /* 下 */
	case  5u: return 3u;   /* 左 */
	case  6u: return 4u;   /* 右 */
	case  1u: return 5u;   /* 开机 */
	case  3u: return 6u;   /* 有线电视 */
	case  2u: return 7u;   /* 静音 */
	case  9u: return 8u;   /* 返回 */
	case  8u: return 9u;   /* 主页 */
	case 10u: return 10u;  /* 菜单 */
	case 12u: return 11u;  /* 音量+ */
	case 14u: return 12u;  /* 音量- */
	case 17u: return 15u;  /* 回看 */
	case 11u: return 16u;  /* 频道+ */
	case 13u: return 17u;  /* 频道- */
	case  4u: return 18u;  /* 确认 */
	default:  return 0u;
	}
}

void rc_key_input_init(void)
{
    s_tag10_count = 0u;
    s_tag10_first_ms = 0u;
    s_tag10_qfull_logged = 0u;
}

/**
 * @brief  读取服务器 Tag11 下发的上报阈值：UploadIntervalSec(4B BE) + UploadItemCount(2B BE)
 */
static void rc_key_input_get_upload_cfg(uint32_t *interval_sec, uint16_t *item_count)
{
    uint8_t policy[6];

    rc_cat1_get_cloud_policy(policy);
    if (interval_sec != NULL) {
        *interval_sec = ((uint32_t)policy[0] << 24) | ((uint32_t)policy[1] << 16)
                      | ((uint32_t)policy[2] << 8) | (uint32_t)policy[3];
    }
    if (item_count != NULL) {
        *item_count = (uint16_t)(((uint16_t)policy[4] << 8) | policy[5]);
    }
}

/**
 * @brief  将聚合缓冲中的按键批量打包入队；成功后清空缓冲
 */
static void rc_key_input_tag10_flush(void)
{
#if RC_TAG10_SELF_REPORT_ENABLE
    uint8_t keys[TAG10_BATCH_MAX];
    uint8_t times[TAG10_BATCH_MAX][6];
    uint8_t n;
    int rc;

    taskENTER_CRITICAL();
    n = s_tag10_count;
    if (n == 0u) {
        taskEXIT_CRITICAL();
        return;
    }
    memcpy(keys, s_tag10_keys, n);
    memcpy(times, s_tag10_times, (size_t)n * 6u);
    taskEXIT_CRITICAL();

    rc = remote_queue_tag10_batch(keys, times, n);
    if (rc == REMOTE_QUEUE_OK) {
        taskENTER_CRITICAL();
        s_tag10_count = 0u;
        s_tag10_first_ms = 0u;
        taskEXIT_CRITICAL();
        s_tag10_qfull_logged = 0u;
        co_printf("[RC   ] Tag10 batch queue OK, n=%u\r\n", (unsigned)n);
    } else if (rc == REMOTE_QUEUE_FULL) {
        /* 队列满：保留缓冲，待队列腾出后再试 */
        if (s_tag10_qfull_logged == 0u) {
            co_printf("[RC   ] Tag10 batch queue full, keep n=%u\r\n", (unsigned)n);
            s_tag10_qfull_logged = 1u;
        }
    } else {
        co_printf("[RC   ] Tag10 batch build fail, n=%u\r\n", (unsigned)n);
    }
#endif /* RC_TAG10_SELF_REPORT_ENABLE */
}

void rc_key_input_poll_tag10(void)
{
#if RC_TAG10_SELF_REPORT_ENABLE
    uint8_t count;
    uint32_t first_ms;
    uint32_t now_ms;
    uint32_t interval_sec;
    uint16_t item_count;
    uint8_t need_flush;

    taskENTER_CRITICAL();
    count = s_tag10_count;
    first_ms = s_tag10_first_ms;
    taskEXIT_CRITICAL();

    if (count == 0u) {
        return;
    }

    rc_key_input_get_upload_cfg(&interval_sec, &item_count);
    if (item_count == 0u) {
        item_count = 1u;   /* 未下发条数阈值时退化为单键即发 */
    }

    need_flush = 0u;
    if (count >= item_count) {
        need_flush = 1u;   /* 条数阈值 */
    } else if (interval_sec != 0u) {
        now_ms = app_now_ms();
        if (((now_ms - first_ms) / 1000u) >= interval_sec) {
            need_flush = 1u;   /* 时间阈值 */
        }
    }

    if (need_flush != 0u) {
        rc_key_input_tag10_flush();
    }
#endif /* RC_TAG10_SELF_REPORT_ENABLE */
}

/**
 * @brief  将硬件键 → Tag0x10 协议键值并入聚合缓冲（供红外路由/纯红外键使用）
 * @param  key_id  硬件按键编号
 * @param  action  0x01=按下，0x00=抬起（仅语音键抬起映射为 14）
 */
static void rc_key_input_enqueue_tag10(uint16_t key_id, uint8_t action)
{
#if RC_TAG10_SELF_REPORT_ENABLE
    uint8_t key_code;
    uint8_t time_bcd[6];

    /* Tag10 数据采集在 OTA 会话期间关闭 */
    if (udp_ota_session_busy() != 0u) {
        return;
    }
    if (remote_tag10_enqueue_is_paused() != 0u) {
        return;
    }
    if (!remote_is_key_capture_enabled()) {
        return;
    }

    key_code = rc_key_id_to_tag10(key_id, action);
    if (key_code == 0u) {
        return;
    }

    remote_get_report_time_bcd6(time_bcd);

    taskENTER_CRITICAL();
    if (s_tag10_count >= TAG10_BATCH_MAX) {
        taskEXIT_CRITICAL();
        rc_key_input_tag10_flush();   /* 缓冲满，强制腾位 */
        taskENTER_CRITICAL();
        if (s_tag10_count >= TAG10_BATCH_MAX) {
            taskEXIT_CRITICAL();
            co_printf("[RC   ] Tag10 batch overflow, drop key=%u\r\n", (unsigned)key_code);
            return;
        }
    }
    if (s_tag10_count == 0u) {
        s_tag10_first_ms = app_now_ms();
    }
    s_tag10_keys[s_tag10_count] = key_code;
    memcpy(s_tag10_times[s_tag10_count], time_bcd, 6u);
    s_tag10_count++;
    taskEXIT_CRITICAL();
    s_tag10_qfull_logged = 0u;
#endif /* RC_TAG10_SELF_REPORT_ENABLE */
}

void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t notify_value,
                        uint16_t key_id, uint8_t ir_routed)
{
    /* BLE RC-KEY-NOTIFY 仍使用 HID usage，与 Tag10 协议键值分离 */
    ble_rc_svc_key_notify(NULL, key_type, action, notify_value);

    /* 仅 BLE 断开（红外模式）时主动上报 Tag0x10，避免与机顶盒上报重复 */
    if (ir_routed != 0u) {
        rc_key_input_enqueue_tag10(key_id, action);
    }
}

void rc_key_input_report_tag10(uint16_t key_id, uint8_t action)
{
    rc_key_input_enqueue_tag10(key_id, action);
}

uint8_t rc_key_input_has_pending_tag10(void)
{
#if RC_TAG10_SELF_REPORT_ENABLE
    uint8_t count;

    taskENTER_CRITICAL();
    count = s_tag10_count;
    taskEXIT_CRITICAL();
    return (count != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}
