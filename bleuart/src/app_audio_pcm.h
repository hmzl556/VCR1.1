#ifndef __APP_AUDIO_PCM_H__
#define __APP_AUDIO_PCM_H__

#include "br1601x.h"
#include <stdint.h>

#define ADC_DMA_BUFFER_SIZE      (256U)
#define ADC_DMA_HALF_SIZE        (ADC_DMA_BUFFER_SIZE / 2U)

extern __IO int16_t g_adc_dma_buffer[ADC_DMA_BUFFER_SIZE];
extern __IO uint32_t g_ping_ready_cnt;
extern __IO uint32_t g_pong_ready_cnt;
extern __IO uint32_t g_dma_err_cnt;

extern uint8_t g_pcm_raw_output_enabled;
extern volatile uint8_t g_audio_pcm_enabled;

void Audio_Pcm_PlatformInit(void);
void Audio_Pcm_CaptureEnable(void);
void Audio_Pcm_CaptureDisable(void);

void Audio_Pcm_ResetStartupState(void);
void Audio_Pcm_Start(void);
void Audio_Pcm_Stop(void);
void Audio_Pcm_CheckAndProcess(void);
void Audio_Pcm_HardwareShutdown(void);   /* 完全关闭 ADC/DMA/MICBIAS/PGA/AUDIOPLL */
void DMA_Channel1_2_3_4_IRQHandler(void);

#endif /* __APP_AUDIO_PCM_H__ */
