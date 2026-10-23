#include "app_voice_ctrl.h"
#include "app_audio_pcm.h"
#include "ble_dtss.h"
#include "co_printf.h"
#include "queue.h"
#include "udp_ota.h"
#include "ble_gap.h"
#include "task_keyscan.h"
#include "pm_state.h"       /* 电源管理 — 语音活动通知 */

static QueueHandle_t s_voice_cmd_queue = NULL;
static StaticQueue_t s_voice_cmd_queue_struct;
static uint8_t s_voice_cmd_queue_storage[4U * sizeof(uint8_t)];

void voice_ctrl_init(void)
{
    s_voice_cmd_queue = xQueueCreateStatic(4U, sizeof(uint8_t),
                                           s_voice_cmd_queue_storage,
                                           &s_voice_cmd_queue_struct);
}

BaseType_t voice_ctrl_post_start(void)
{
    uint8_t cmd = VOICE_CMD_START;

    if (s_voice_cmd_queue == NULL) {
        return pdFAIL;
    }
    return xQueueSend(s_voice_cmd_queue, &cmd, 0);
}

BaseType_t voice_ctrl_post_stop(void)
{
    uint8_t cmd = VOICE_CMD_STOP;

    if (s_voice_cmd_queue == NULL) {
        return pdFAIL;
    }
    /* STOP must survive a burst of host START requests. */
    (void)xQueueReset(s_voice_cmd_queue);
    return xQueueSend(s_voice_cmd_queue, &cmd, 0);
}

void voice_ctrl_process_pending(void)
{
    uint8_t cmd;

    if (s_voice_cmd_queue == NULL) {
        return;
    }

    while (xQueueReceive(s_voice_cmd_queue, &cmd, 0) == pdTRUE) {
        if (cmd == VOICE_CMD_START) {
            if (udp_ota_session_busy() || task_keyscan_ir_busy() || ble_gap_get_peer() == NULL)
                continue;
            {
                uint8_t ready = ble_dtss_ready_channels();
                if (ready != (BLE_DTSS_DATA_READY | BLE_DTSS_CTRL_READY)) {
                    co_printf("[VOICE] not ready: data=%u ctrl=%u\r\n",
                              (unsigned)((ready & BLE_DTSS_DATA_READY) != 0U),
                              (unsigned)((ready & BLE_DTSS_CTRL_READY) != 0U));
                    continue;
                }
            }
            /* 通知PM：语音开始传输，重置空闲计时器/退出待机态 */
            pm_activity_notify(PM_ACTIVITY_VOICE);
            Audio_Pcm_Start();
        } else if (cmd == VOICE_CMD_STOP) {
            Audio_Pcm_Stop();
        }
    }
}
