#include "rc_key_input.h"
#include "ble_rc_svc.h"
#include "remote_reports.h"
#include "co_printf.h"
#include "FreeRTOS.h"
#include "task.h"

static volatile uint8_t s_pending_tag10;
static uint8_t s_pending_tag10_code;

void rc_key_input_init(void)
{
    s_pending_tag10 = 0u;
    s_pending_tag10_code = 0u;
}

void rc_key_input_poll_tag10(void)
{
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
    if (!remote_is_key_capture_enabled()) {
        return;
    }
    {
        int rc = remote_queue_tag10_key(code);

        if (rc == -2) {
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
    co_printf("[RC   ] Tag10 queue OK, key = %u\r\n", (unsigned)code);
}

void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t key_value)
{
    uint8_t key_code;

    ble_rc_svc_key_notify(NULL, key_type, action, key_value);

    if (action != 0x01u) {
        return;
    }

    if (!remote_is_key_capture_enabled()) {
        return;
    }

    key_code = (uint8_t)(key_value & 0xFFu);
    if (key_code == 0u) {
        return;
    }

    taskENTER_CRITICAL();
    s_pending_tag10_code = key_code;
    s_pending_tag10 = 1u;
    taskEXIT_CRITICAL();
}
