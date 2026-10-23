#include "bat_adc.h"
#include "userhard.h"
#include "app_audio_pcm.h"
#include "br1601x.h"
#include "rcc.h"
#include "gpio.h"
#include "adc.h"
#include "co_printf.h"

/* 分压 R16=20k / R28=4.7k → V_adc = V_bat * 4.7/24.7 */
#define BAT_DIV_NUM           2470U
#define BAT_DIV_DEN           470U

/* 线性映射：3.3V=0%，4.2V=100% */
#define BAT_MV_EMPTY          3300U
#define BAT_MV_FULL           4200U
#define BAT_MV_SPAN           (BAT_MV_FULL - BAT_MV_EMPTY)

#define BAT_SAMPLE_COUNT      8U          /* 连续采样次数，用于平均抑噪 */
#define BAT_ADC_DONE_TIMEOUT  100000U     /* 单次转换完成等待上限 */

/* EMA 平滑 + 迟滞滤波参数（放电极慢场景） */
#define BAT_EMA_ALPHA_NUM     1U          /* α = 1/16 ≈ 6.25%，新值权重 */
#define BAT_EMA_ALPHA_DEN     16U
#define BAT_HYST_THRESHOLD    2U          /* EMA 偏离 ≥x% 才更新 */

static uint8_t s_bat_level_cached  = 100U;/* 最近一次有效电量（默认 100%） */
static uint8_t s_bat_level_ema     = 100U;/* EMA 平滑值，每轮采样都更新 */
static uint8_t s_bat_level_display = 100U;/* 对外显示值，迟滞控制 */
static uint8_t s_bat_first_sample  = 1U;  /* 首次有效采样标志，1=尚未采样 */
static uint8_t s_pb6_analog_ready  = 0U;  /* PB6 模拟输入是否已配置 */

/**
 * @brief  将 PB6 配置为模拟输入（ADC CH5），仅初始化一次
 */
static void bat_adc_pb6_analog_init(void)
{
	GPIO_InitType gpio;

	if (s_pb6_analog_ready != 0U) {
		return;
	}

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
	GPIO_InitStruct(&gpio);
	gpio.Pin = GPIO_PIN_6;
	gpio.GPIO_Mode = GPIO_MODE_ANALOG;
	GPIO_InitPeripheral(GPIOB, &gpio);
	s_pb6_analog_ready = 1U;
}

/**
 * @brief  准备单次电压采样：开时钟、配 CH5、关 DMA/连续模式
 */
static void bat_adc_hw_prepare(void)
{
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
	RCC_ConfigAdcClk(RCC_ADCCLK_SRC_HSE_DIV8);

	bat_adc_pb6_analog_init();

	ADC_EnableDMA(ADC, DISABLE);
	ADC_ConfigContinuousMode(ADC, DISABLE);
	ADC_EnableBypassFilter(ADC, ENABLE);
	ADC_ConfigChannel(ADC, ADC_CTRL_CH_5);
}

/**
 * @brief  采样结束后关闭 ADC，避免与语音 DMA 路径冲突
 */
static void bat_adc_hw_release(void)
{
	ADC_Enable(ADC, DISABLE);
	ADC_EnableDMA(ADC, DISABLE);
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, DISABLE);
}

/**
 * @brief  触发一次 ADC 单次转换并读取原始值
 * @param  out_raw  输出 10-bit 原始码
 * @return 0 成功，-1 超时失败
 */
static int bat_adc_read_once_raw(uint16_t *out_raw)
{
	uint32_t t;

	ADC_ClearFlag(ADC, ADC_FLAG_DONE);
	ADC_Enable(ADC, ENABLE);

	t = 0U;
	while (ADC_GetFlagStatus(ADC, ADC_FLAG_DONE) == RESET) {
		if (++t >= BAT_ADC_DONE_TIMEOUT) {
			ADC_Enable(ADC, DISABLE);
			return -1;
		}
	}

	ADC_ClearFlag(ADC, ADC_FLAG_DONE);
	*out_raw = ADC_GetDat(ADC);
	ADC_Enable(ADC, DISABLE);
	return 0;
}

/**
 * @brief  将电池端电压（mV）线性换算为 0–100%
 * @note   ≤3.3V → 0%，≥4.2V → 100%
 */
static uint8_t bat_mv_to_percent(uint32_t vbat_mv)
{
	uint32_t pct;

	if (vbat_mv <= BAT_MV_EMPTY) {
		return 0U;
	}
	if (vbat_mv >= BAT_MV_FULL) {
		return 100U;
	}

	pct = ((vbat_mv - BAT_MV_EMPTY) * 100U) / BAT_MV_SPAN;
	if (pct > 100U) {
		pct = 100U;
	}
	return (uint8_t)pct;
}

/**
 * @brief  返回最近一次有效显示电量（未采样过则为 100）
 */
uint8_t bat_get_cached_level(void)
{
	return s_bat_level_display;
}

/**
 * @brief  读取电池电量百分比（0–100）
 * @note   LTE_EN 低：不上采样，返回 100%；
 *         LTE_EN 高：PB6/CH5 采 8 次平均，分压反推后线性换算；
 *         语音 ADC 忙：跳过采样，返回缓存。
 */
uint8_t bat_get_level_percent(void)
{
	uint32_t sum_raw = 0U;
	uint32_t sum_mv = 0U;
	uint32_t vadc_mv;
	uint32_t vbat_mv;
	uint16_t raw;
	uint8_t i;
	uint8_t ok_cnt = 0U;
	uint8_t level;

	/* LTE 未供电：分压器无电，不上采样，固定报 100% */
	if (!LTE_EN_IS_ON()) {
		s_bat_level_cached = 100U;
		return 100U;
	}

	/* 语音占用 ADC+DMA 时跳过，保留上次电量 */
	if (g_audio_pcm_enabled != 0U) {
		return s_bat_level_cached;
	}

	/* 开启ADC，准备采样 */
	bat_adc_hw_prepare();

	/* 连续 8 次采样取平均，抑制噪声 */
	for (i = 0U; i < BAT_SAMPLE_COUNT; i++) {
		if (bat_adc_read_once_raw(&raw) == 0) {
			sum_raw += raw;
			sum_mv += ADC_ConverValueToVoltage(raw, ADC_CTRL_CH_5);
			ok_cnt++;
		}
	}

	/* 关闭ADC，释放资源 */
	bat_adc_hw_release();

	/* 采样失败，保留上次电量 */
	if (ok_cnt == 0U) {
		co_printf("[BAT  ] ADC sample failed, keep %u%%\r\n", (unsigned)s_bat_level_cached);
		return s_bat_level_cached;
	}

	vadc_mv = sum_mv / ok_cnt;
	/* V_bat = V_adc * (20k+4.7k)/4.7k */
	vbat_mv = (vadc_mv * BAT_DIV_NUM) / BAT_DIV_DEN;
	level = bat_mv_to_percent(vbat_mv);

	/* === 首次有效采样：直接初始化，避免从 100% 缓慢收敛 === */
	if (s_bat_first_sample != 0U) {
		s_bat_level_ema     = level;
		s_bat_level_display = level;
		s_bat_first_sample  = 0U;
	} else {
		/* === EMA 平滑滤波：每轮都更新，独立状态追踪趋势 === */
		{
			uint16_t smoothed;
			smoothed = (uint16_t)s_bat_level_ema * (BAT_EMA_ALPHA_DEN - BAT_EMA_ALPHA_NUM)
			         + (uint16_t)level * BAT_EMA_ALPHA_NUM;
			smoothed = (smoothed + (BAT_EMA_ALPHA_DEN / 2U)) / BAT_EMA_ALPHA_DEN;
			s_bat_level_ema = (uint8_t)smoothed;
		}

		/* === 迟滞保护：EMA 偏离显示值 ≥3% 时才更新 === */
		{
			int16_t diff = (int16_t)s_bat_level_ema - (int16_t)s_bat_level_display;
			uint16_t abs_diff = (uint16_t)(diff < 0 ? -diff : diff);
			if (abs_diff >= BAT_HYST_THRESHOLD) {
				s_bat_level_display = s_bat_level_ema;
			}
		}
	}

	s_bat_level_cached = s_bat_level_display;

	/*
	 * raw_avg：ADC 原始码平均值
	 * vadc：分压器输出电压（mV）
	 * vbat：电池端电压（mV）
	 * raw：当前采样原始码
	 * ema：EMA 平滑值（%）
	 * disp：对外显示值（%）
	 * n：有效采样次数
	 */
	co_printf("[BAT  ] raw_avg=%u vadc=%umV vbat=%umV raw=%u%% ema=%u%% disp=%u%% (n=%u)\r\n",
	          (unsigned)(sum_raw / ok_cnt),
	          (unsigned)vadc_mv,
	          (unsigned)vbat_mv,
	          (unsigned)level,
	          (unsigned)s_bat_level_ema,
	          (unsigned)s_bat_level_display,
	          (unsigned)ok_cnt);

	return s_bat_level_display;
}
