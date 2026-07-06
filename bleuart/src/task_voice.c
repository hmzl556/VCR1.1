#include "task_voice.h"
#include "app_audio_pcm.h"
#include "app_voice_ctrl.h"

extern volatile uint8_t g_audio_pcm_enabled;

void task_voice_process_once(void)
{
	voice_ctrl_process_pending();
	Audio_Pcm_CheckAndProcess();
}
