#include "rc_key_input.h"
#include "ble_rc_svc.h"
#include "remote_reports.h"
#include "cat1_udp_config.h"
#include "udp_ota.h"
#include "co_printf.h"
#include "FreeRTOS.h"
#include "task.h"

static volatile uint8_t s_pending_tag10;
static uint8_t s_pending_tag10_code;		//处理warning时不要删
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
    s_pending_tag10 = 0u;
    s_pending_tag10_code = 0u;
    s_tag10_qfull_logged = 0u;
}

void rc_key_input_poll_tag10(void)
{
#if RC_TAG10_SELF_REPORT_ENABLE
    uint8_t code;
    uint8_t pending;

    taskENTER_CRITICAL();
    pending = s_pending_tag10;
    code = s_pending_tag10_code;
    s_pending_tag10 = 0u;
    taskEXIT_CRITICAL();

    if (pending == 0u) {
        return;
    }
    /* OTA 会话期间不上报 Tag10 按键采集 */
    if (udp_ota_session_busy() != 0u) {
        return;
    }
    if (remote_tag10_enqueue_is_paused() != 0u) {
        return;
    }
    if (!remote_is_key_capture_enabled()) {
        return;
    }
    {
        int rc = remote_queue_tag10_key(code);

        if (rc == REMOTE_QUEUE_FULL) {
            if (s_tag10_qfull_logged == 0u) {
                co_printf("[RC   ] Tag10 push fail, key=%u q=%u\r\n",
                          (unsigned)code, (unsigned)remote_report_queue_count());
                s_tag10_qfull_logged = 1u;
            }
            taskENTER_CRITICAL();
            s_pending_tag10_code = code;
            s_pending_tag10 = 1u;
            taskEXIT_CRITICAL();
            return;
        }
        if (rc != 0) {
            return;
        }
    }
    s_tag10_qfull_logged = 0u;
    co_printf("[RC   ] Tag10 queue OK, key = %u\r\n", (unsigned)code);
#endif /* RC_TAG10_SELF_REPORT_ENABLE */
}

void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t notify_value,
                        uint16_t key_id)
{
    uint8_t key_code;

    /* BLE RC-KEY-NOTIFY 仍使用 HID usage，与 Tag10 协议键值分离 */
    ble_rc_svc_key_notify(NULL, key_type, action, notify_value);

#if RC_TAG10_SELF_REPORT_ENABLE
    /* RC-KEY notify 仍发送；Tag10 数据采集在 OTA 会话期间关闭 */
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

    taskENTER_CRITICAL();
    s_pending_tag10_code = key_code;
    s_pending_tag10 = 1u;
    taskEXIT_CRITICAL();
    s_tag10_qfull_logged = 0u;
#endif /* RC_TAG10_SELF_REPORT_ENABLE */
}
