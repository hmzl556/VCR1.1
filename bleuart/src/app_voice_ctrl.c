#include "app_voice_ctrl.h"
#include "app_audio_pcm.h"
#include "queue.h"

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
            Audio_Pcm_Start();
        } else if (cmd == VOICE_CMD_STOP) {
            Audio_Pcm_Stop();
        }
    }
}
