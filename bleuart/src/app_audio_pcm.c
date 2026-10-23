#include "app_audio_pcm.h"
#include "app_encoder_sbc.h"
#include "blecomm.h"
#include "ble_dtss.h"
#include "adc.h"
#include "dma.h"
#include "rcc.h"
#include "co_printf.h"
#include "gmem.h"

#define PCM_RAW_OUTPUT_ENABLE    (0U)  // 1: 输出DMA原始PCM(hex)，0: 输出SBC数据

uint8_t g_pcm_raw_output_enabled = PCM_RAW_OUTPUT_ENABLE;
volatile uint8_t g_audio_pcm_enabled = 0;
static uint8_t g_audio_platform_ready = 0U;  // RCC+GPIO 已就绪
static uint8_t g_audio_capture_hw_on = 0U;   // DMA 中断已打开
static uint8_t g_audio_dma_adc_once = 0U;    // DMA+ADC 已完整配置过，Stop 不再动外设


extern void STDOUT(uint8_t *_buf, uint32_t _dlen);

__IO int16_t g_adc_dma_buffer[ADC_DMA_BUFFER_SIZE];	
__IO uint32_t g_ping_ready_cnt = 0;
__IO uint32_t g_pong_ready_cnt = 0;
__IO uint32_t g_dma_err_cnt = 0;

#define ADC_OVERSAMPLE_COUNT     (7U)
#define PCM_STRIP_PERIOD_SAMPLES (250U)
#define PCM_STRIP_DROP_SAMPLES   (2U)
#define PCM_PERIODIC_FILTER_ENABLE (1U)  // 1 打开去除“00 00 00 00”的滤波，0 关闭滤波

// 周期字滤波状态与统计
static uint8_t  g_pcm_strip_locked = 0U;
static uint8_t  g_pcm_phase_valid = 0U;
static uint32_t g_pcm_strip_phase = 0U;
static uint16_t g_pcm_strip_remaining = 0U;
static uint32_t g_pcm_no_lock_count = 0U;
static uint32_t g_pcm_lock_detects = 0U;
static uint32_t g_pcm_drop_events = 0U;
static uint32_t g_pcm_total_input_count = 0U;  // 已累计输入样点数
static uint16_t g_pcm_remove_tolerance = 3U;   // 周期匹配允许偏差（样点）
static int16_t  g_pcm_pending_sample = 0;
static uint8_t  g_pcm_pending_valid = 0U;
static uint8_t  g_pcm_startup_discard_halves = 0U;

static int16_t s_filtered_half[ADC_DMA_HALF_SIZE];

#define PCM_STARTUP_DISCARD_HALVES   (8U)

void Audio_Pcm_ResetStartupState(void)
{
	__disable_irq();
	g_ping_ready_cnt = 0U;
	g_pong_ready_cnt = 0U;
	g_pcm_strip_locked = 0U;
	g_pcm_phase_valid = 0U;
	g_pcm_strip_phase = 0U;
	g_pcm_strip_remaining = 0U;
	g_pcm_no_lock_count = 0U;
	g_pcm_lock_detects = 0U;
	g_pcm_drop_events = 0U;
	g_pcm_total_input_count = 0U;
	g_pcm_pending_sample = 0;
	g_pcm_pending_valid = 0U;
	g_pcm_startup_discard_halves = PCM_STARTUP_DISCARD_HALVES;
	__enable_irq();
}

static void RCC_Configuration(void)
{
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
	RCC_Enable_ADC_CLK_SRC_AUDIOPLL(ENABLE);
	RCC_ConfigAdcClk(RCC_ADCCLK_SRC_AUDIOPLL);
}

static void GPIO_Configuration(void)
{
	GPIO_InitType GPIO_InitStructure;
	GPIO_InitStruct(&GPIO_InitStructure);
	GPIO_InitStructure.Pin       = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_ANALOG;
	GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);
}

static void DMA_Configuration(void)
{
    DMA_InitType DMA_InitStructure;
	DMA_DeInit(DMA_CH1);
	DMA_InitStructure.PeriphAddr     = (uint32_t)&ADC->DAT;
	DMA_InitStructure.MemAddr        = (uint32_t)&g_adc_dma_buffer[0];
	DMA_InitStructure.Direction      = DMA_DIR_PERIPH_SRC;
	DMA_InitStructure.BufSize        = ADC_DMA_BUFFER_SIZE;
	DMA_InitStructure.PeriphInc      = DMA_PERIPH_INC_DISABLE;
	DMA_InitStructure.DMA_MemoryInc  = DMA_MEM_INC_ENABLE;
	DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_HALFWORD;
	DMA_InitStructure.MemDataSize    = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.CircularMode   = DMA_MODE_CIRCULAR;
	DMA_InitStructure.Priority       = DMA_PRIORITY_HIGH;
	DMA_InitStructure.Mem2Mem        = DMA_M2M_DISABLE;
	DMA_Init(DMA_CH1, &DMA_InitStructure);
	DMA_RequestRemap(DMA_REMAP_ADC, DMA, DMA_CH1, ENABLE);

	DMA_ClearFlag(DMA_FLAG_GL1, DMA);
	DMA_ClrIntPendingBit(DMA_INT_GLB1 | DMA_INT_HTX1 | DMA_INT_TXC1 | DMA_INT_ERR1, DMA);
	DMA_ConfigInt(DMA_CH1, DMA_INT_HTX | DMA_INT_TXC | DMA_INT_ERR, ENABLE);

	NVIC_ClearPendingIRQ(DMA_Channel1_2_3_4_IRQn);
	NVIC_SetPriority(DMA_Channel1_2_3_4_IRQn, 1);
	NVIC_EnableIRQ(DMA_Channel1_2_3_4_IRQn);

	DMA_EnableChannel(DMA_CH1, ENABLE);
}

// 只屏蔽DMA_CH1的外设中断，不关闭多个通道共用的NVIC入口。
static void Audio_Pcm_CaptureIrqDisable(void)
{
	if (g_audio_capture_hw_on == 0U) {
		return;
	}

	__disable_irq();
	DMA_ConfigInt(DMA_CH1, DMA_INT_HTX | DMA_INT_TXC | DMA_INT_ERR, DISABLE);
    DMA_ClrIntPendingBit(DMA_INT_GLB1 | DMA_INT_HTX1 | DMA_INT_TXC1 | DMA_INT_ERR1, DMA);
	__enable_irq();

	g_audio_capture_hw_on = 0U;
}

// DMA/ADC 已就绪时，只重新打开 DMA 中断与 NVIC
static void Audio_Pcm_CaptureIrqEnable(void)
{
	DMA_ClrIntPendingBit(DMA_INT_GLB1 | DMA_INT_HTX1 | DMA_INT_TXC1 | DMA_INT_ERR1, DMA);
	DMA_ClearFlag(DMA_FLAG_GL1, DMA);
	DMA_ConfigInt(DMA_CH1, DMA_INT_HTX | DMA_INT_TXC | DMA_INT_ERR, ENABLE);

	__disable_irq();
	NVIC_ClearPendingIRQ(DMA_Channel1_2_3_4_IRQn);
	NVIC_EnableIRQ(DMA_Channel1_2_3_4_IRQn);
	__enable_irq();
}

static void ADC_Configuration(void)
{
	// 关闭旁路滤波，避免 DAT 中插入状态字
	ADC_EnableBypassFilter(ADC, DISABLE);                // 关闭ADC旁路，启用滤波器
	// 关闭语音检测类功能
	ADC_EnableZeroCross(ADC, DISABLE);
	ADC_EnableEnergy(ADC, DISABLE);
	
	ADC_ConfigChannel(ADC, ADC_CTRL_CH_0);              // 选择ADC通道0（对应GPIOB11）
	ADC_EnableMICBIAS(ADC, ENABLE);                     // 启用MIC偏置电压输出
	ADC_SetMICOutputVoltage(ADC, MICBIAS_1V6);          // 设置MIC偏置电压为1.6V
	ADC_EnablePGA(ADC, ENABLE);                         // 启用可编程增益放大器（PGA）
	ADC_SetPGAGain(ADC, ADC_PGA_CFG_GAIN_42dB);         // 设置PGA增益为42dB

	ADC_SetOverSampleCounter(ADC, ADC_OVERSAMPLE_COUNT); // 过采样率为 2^ADC_OVERSAMPLE_COUNT
	ADC_ConfigContinuousMode(ADC, ENABLE);
	ADC_EnableDMA(ADC, ENABLE);
	ADC_Enable(ADC, ENABLE);
}

void Audio_Pcm_PlatformInit(void)
{
	if (g_audio_platform_ready != 0U) {
		return;
	}
	RCC_Configuration();
	GPIO_Configuration();
	g_audio_platform_ready = 1U;
}

void Audio_Pcm_CaptureEnable(void)
{
	Audio_Pcm_PlatformInit();

	if (g_audio_dma_adc_once == 0U) {
		DMA_Configuration();
		ADC_Configuration();
		g_audio_dma_adc_once = 1U;
		g_audio_capture_hw_on = 1U;
		return;
	}

	// 第二次及以后：硬件与时钟保持，只恢复 DMA 中断
	if (g_audio_capture_hw_on == 0U) {
		Audio_Pcm_CaptureIrqEnable();
		g_audio_capture_hw_on = 1U;
	}
}

void Audio_Pcm_CaptureDisable(void)
{
	Audio_Pcm_CaptureIrqDisable();
}

void Audio_Pcm_Start(void)
{
	if (g_audio_pcm_enabled != 0U) {  // 已在采集则直接返回
		return;
	}

	Encoder_Sbc_Enable();
    if(!Encoder_Sbc_IsReady()){co_printf("[VOICE] start blocked: encoder unavailable\r\n");return;}
	Audio_Pcm_CaptureEnable();  // 内含 Audio_Pcm_PlatformInit

	bleResetVoiceSbcSeq();  // 语音包序列号/时间戳从 0 开始
	Audio_Pcm_ResetStartupState();  // 丢弃前若干半缓冲，等待 ADC 稳定
	g_audio_pcm_enabled = 1;

	ble_dtss_send_control_response(0x01U);  // 通知对端开始采集
	bleSendVoiceStartFrame();               // 发送 START 帧
}

/**
 * @brief  完全关闭 ADC/MICBIAS/PGA/AUDIOPLL；仅停 DMA_CH1。
 * @note   供 Audio_Pcm_Stop() 和 pm_peripherals_sleep_shutdown() 复用。
 *         下次 Audio_Pcm_Start() 会从 RCC 开始完整重新初始化。
 *         不关闭 AHB DMA 时钟：CAT1 USART1 RX 使用 DMA_CH2。
 */
void Audio_Pcm_HardwareShutdown(void)
{
	/* 1. 确保时钟已开才能安全访问寄存器 */
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);

	/* 2. 关 ADC 及其子模块 */
	ADC_Enable(ADC, DISABLE);
	ADC_EnableMICBIAS(ADC, DISABLE);
	ADC_EnablePGA(ADC, DISABLE);
	ADC_EnableDMA(ADC, DISABLE);

	/* 3. 仅关 ADC 用的 DMA_CH1；勿关 AHB DMA 时钟（USART1 RX 占用 DMA_CH2） */
	DMA_EnableChannel(DMA_CH1, DISABLE);

	/* 4. 关 ADC/AUDIOPLL 时钟省电；DMA 时钟保持给 CAT1 */
	RCC_Enable_ADC_CLK_SRC_AUDIOPLL(DISABLE);
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, DISABLE);

	/* 5. 重置状态标志，下次 Start 走完整初始化 */
	g_audio_dma_adc_once   = 0U;
	g_audio_capture_hw_on  = 0U;
	g_audio_platform_ready = 0U;
}

void Audio_Pcm_Stop(void)
{
	if (g_audio_pcm_enabled == 0U) {  // 未在采集则直接返回
		return;
	}

	g_audio_pcm_enabled = 0;  // 主循环不再处理半缓冲
	Audio_Pcm_CaptureDisable();  // 关 DMA 中断
	Encoder_Sbc_Disable();  // 释放 SBC，下次 Start 再 Enable
	Audio_Pcm_ResetStartupState();  // 清 ping/pong 与滤波状态
	Audio_Pcm_HardwareShutdown();  // ★ 关 ADC/DMA/MICBIAS/PGA/AUDIOPLL

	ble_dtss_send_control_response(0x02U);  // 通知对端停止采集
	bleSendVoiceEndFrame();                 // 发送 AUDIOEND 帧
}

static uint32_t PcmAbsDiffU32(uint32_t a, uint32_t b)
{
	return (a >= b) ? (a - b) : (b - a);
}

static uint16_t PcmStripPeriodicWord(const __IO int16_t *in_data, uint16_t in_count, int16_t *out_data)
{
	uint16_t in_idx = 0U;
	uint16_t out_count = 0U;
	uint32_t base_idx = g_pcm_total_input_count;

	while (in_idx < in_count) {
		int16_t current_sample = in_data[in_idx];
		uint32_t current_pos = base_idx + (uint32_t)in_idx;

		if (g_pcm_pending_valid == 0U) {
			g_pcm_pending_sample = current_sample;
			g_pcm_pending_valid = 1U;
			in_idx++;
			continue;
		}

		if ((g_pcm_strip_locked != 0U) && (g_pcm_strip_remaining > 0U)) {
			g_pcm_strip_remaining--;
		}

		if ((g_pcm_pending_sample == 0) && (current_sample == 0)) {
			uint32_t pair_pos = current_pos - 1U;
			uint8_t drop_pair = 0U;

			if (g_pcm_phase_valid == 0U) {
				g_pcm_phase_valid = 1U;
				g_pcm_strip_phase = pair_pos;
				g_pcm_strip_remaining = (uint16_t)(PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance);
				g_pcm_no_lock_count++;
			} else {
				uint32_t phase_diff = PcmAbsDiffU32(pair_pos, g_pcm_strip_phase);
				uint32_t period_diff = PcmAbsDiffU32(phase_diff, (uint32_t)PCM_STRIP_PERIOD_SAMPLES);

				if (period_diff <= (uint32_t)g_pcm_remove_tolerance) {
					if (g_pcm_strip_locked == 0U) {
						g_pcm_strip_locked = 1U;
						g_pcm_lock_detects++;
					}
					g_pcm_strip_phase = pair_pos;
					g_pcm_strip_remaining = (uint16_t)(PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance);
					g_pcm_drop_events++;
					drop_pair = 1U;
				} else {
					if ((g_pcm_strip_locked != 0U) &&
					    (pair_pos > (g_pcm_strip_phase + PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance))) {
						g_pcm_strip_locked = 0U;
						g_pcm_phase_valid = 1U;
						g_pcm_strip_phase = pair_pos;
						g_pcm_strip_remaining = (uint16_t)(PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance);
						g_pcm_no_lock_count++;
					} else if ((g_pcm_strip_locked == 0U) &&
					           (pair_pos > (g_pcm_strip_phase + PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance))) {
						g_pcm_strip_phase = pair_pos;
						g_pcm_strip_remaining = (uint16_t)(PCM_STRIP_PERIOD_SAMPLES + g_pcm_remove_tolerance);
						g_pcm_no_lock_count++;
					}
				}
			}

			if (drop_pair != 0U) {
				g_pcm_pending_valid = 0U;
				in_idx++;
				continue;
			}
		}

		if ((g_pcm_strip_locked != 0U) && (g_pcm_strip_remaining == 0U) && (g_pcm_phase_valid != 0U)) {
			g_pcm_strip_locked = 0U;
			g_pcm_phase_valid = 0U;
			g_pcm_no_lock_count++;
		}

		out_data[out_count++] = g_pcm_pending_sample;
		g_pcm_pending_sample = current_sample;
		g_pcm_pending_valid = 1U;
		in_idx++;
	}

	g_pcm_total_input_count += in_count;
	return out_count;
}

static void PrintPcmHexLine(const __IO int16_t *data, uint16_t count)
{
	STDOUT((uint8_t *)data, (uint32_t)count * 2U);
}

void Audio_Pcm_CheckAndProcess(void)
{
	uint16_t filtered_count;
	uint8_t discard_enabled;

	if (g_audio_pcm_enabled == 0U) {
		return;
	}

	discard_enabled = (g_pcm_startup_discard_halves != 0U) ? 1U : 0U;

    if (g_ping_ready_cnt != 0U)
    {
        __disable_irq();
        if (g_ping_ready_cnt != 0U)
        {
            g_ping_ready_cnt--;
        }
        __enable_irq();
		if (discard_enabled != 0U) {
			g_pcm_startup_discard_halves--;
			return;
		}
	#if PCM_PERIODIC_FILTER_ENABLE
		filtered_count = PcmStripPeriodicWord(&g_adc_dma_buffer[0], ADC_DMA_HALF_SIZE, s_filtered_half);
		if (g_pcm_raw_output_enabled != 0U) {
			PrintPcmHexLine(s_filtered_half, filtered_count);
		} else {
			Encoder_Sbc_ProcessPcmBlock(s_filtered_half, filtered_count);
		}
	#else
		if (g_pcm_raw_output_enabled != 0U) {
			PrintPcmHexLine(&g_adc_dma_buffer[0], ADC_DMA_HALF_SIZE);
		} else {
			Encoder_Sbc_ProcessPcmBlock(&g_adc_dma_buffer[0], ADC_DMA_HALF_SIZE);
		}
	#endif
    }

    if (g_pong_ready_cnt != 0U)
    {
        __disable_irq();
        if (g_pong_ready_cnt != 0U)
        {
            g_pong_ready_cnt--;
        }
        __enable_irq();
		if (discard_enabled != 0U) {
			g_pcm_startup_discard_halves--;
			return;
		}
	#if PCM_PERIODIC_FILTER_ENABLE
		filtered_count = PcmStripPeriodicWord(&g_adc_dma_buffer[ADC_DMA_HALF_SIZE], ADC_DMA_HALF_SIZE, s_filtered_half);
		if (g_pcm_raw_output_enabled != 0U) {
			PrintPcmHexLine(s_filtered_half, filtered_count);
		} else {
			Encoder_Sbc_ProcessPcmBlock(s_filtered_half, filtered_count);
		}
	#else
		if (g_pcm_raw_output_enabled != 0U) {
			PrintPcmHexLine(&g_adc_dma_buffer[ADC_DMA_HALF_SIZE], ADC_DMA_HALF_SIZE);
		} else {
			Encoder_Sbc_ProcessPcmBlock(&g_adc_dma_buffer[ADC_DMA_HALF_SIZE], ADC_DMA_HALF_SIZE);
		}
	#endif
    }
}

void DMA_Channel1_2_3_4_IRQHandler(void)
{
    if (DMA_GetIntStatus(DMA_INT_HTX1, DMA) == SET)
    {
        DMA_ClrIntPendingBit(DMA_INT_HTX1, DMA);
        g_ping_ready_cnt++;
    }

    if (DMA_GetIntStatus(DMA_INT_TXC1, DMA) == SET)
    {
        DMA_ClrIntPendingBit(DMA_INT_TXC1, DMA);
        g_pong_ready_cnt++;
    }

    if (DMA_GetIntStatus(DMA_INT_ERR1, DMA) == SET)
    {
        DMA_ClrIntPendingBit(DMA_INT_ERR1, DMA);
        g_dma_err_cnt++;
    }
}

