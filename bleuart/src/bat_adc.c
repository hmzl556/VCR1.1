#include "bat_adc.h"
#include "userhard.h"
#include "app_audio_pcm.h"
#include "br1601x.h"
#include "rcc.h"
#include "gpio.h"
#include "adc.h"
#include "co_printf.h"
#include "gmem.h"
#include "cat1_skt_udp.h"
#include "task_keyscan.h"
#include "udp_ota.h"

/* 分压 R16=20k / R28=4.7k → V_adc = V_bat * 4.7/24.7 */
#define BAT_DIV_NUM           2470U
#define BAT_DIV_DEN           470U

/* 线性映射：3.3V=0%，4.2V=100% */
#define BAT_MV_EMPTY          3300U
#define BAT_MV_FULL           4200U
#define BAT_MV_SPAN           (BAT_MV_FULL - BAT_MV_EMPTY)

#define BAT_SAMPLE_COUNT      8U          /* 连续采样次数，用于平均抑噪 */
#define BAT_ADC_DONE_TIMEOUT  100000U     /* 单次转换完成等待上限 */

/* Q8 percent retains fractional changes; public values remain 0..100. */
#define BAT_Q                 256U
#define BAT_HYST_THRESHOLD    2U
#define BAT_UPDATE_MS         60000U
#define BAT_POWER_SETTLE_MS   200U
#define BAT_QUIET_MS          100U
#define BAT_GROUP_GAP_MS      20U
#define BAT_RETRY_MS          1000U
#define BAT_GROUP_COUNT       3U

static uint8_t s_bat_level_display = 100U;
static uint16_t s_bat_ema_q8;
static uint16_t s_group_mv[BAT_GROUP_COUNT];
static uint32_t s_power_ms, s_quiet_ms, s_group_ms, s_last_update_ms;
static uint8_t s_lte_seen, s_quiet_valid, s_group_count, s_retry_pending;
static uint8_t s_bat_valid;
static uint8_t s_pb6_analog_ready;

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

/* Preserve the existing 3.3..4.2V linear range, with Q8 precision. */
static uint16_t bat_mv_to_q8(uint32_t mv)
{
    if (mv <= BAT_MV_EMPTY) return 0U;
    if (mv >= BAT_MV_FULL) return (uint16_t)(100U * BAT_Q);
    return (uint16_t)(((mv - BAT_MV_EMPTY) * 100U * BAT_Q) / BAT_MV_SPAN);
}

uint8_t bat_get_cached_level(void)
{
    return s_bat_level_display;
}

/* Compatibility entry point: reading/reporting must never trigger ADC work. */
uint8_t bat_get_level_percent(void)
{
    return bat_get_cached_level();
}

/* Called on modem reset/power transitions, including deep-sleep wake.
 * Do not discard the valid battery cache or restart the 60s rate limit. */
void bat_adc_restart_window(void)
{
    s_lte_seen = 0U;
    s_quiet_valid = 0U;
    s_group_count = 0U;
    s_retry_pending = 0U;
}

/* One bounded group per poll: discard the first conversion after switching
 * channel, then trim one minimum and maximum from eight valid samples.
 * No partial/failed group may initialize or change the published battery. */
static int bat_sample_group(uint16_t *mv_out)
{
    uint16_t raw;
    uint32_t sum = 0U, lo = 0xFFFFFFFFU, hi = 0U, mv;
    uint8_t i;
    bat_adc_hw_prepare();
    if (bat_adc_read_once_raw(&raw) != 0) {
        bat_adc_hw_release();
        return -1;
    }
    for (i = 0U; i < BAT_SAMPLE_COUNT; ++i) {
        if (bat_adc_read_once_raw(&raw) != 0) {
            bat_adc_hw_release();
            return -1;
        }
        mv = ADC_ConverValueToVoltage(raw, ADC_CTRL_CH_5);
        sum += mv;
        if (mv < lo) lo = mv;
        if (mv > hi) hi = mv;
    }
    bat_adc_hw_release();
    mv = (sum - lo - hi) / (BAT_SAMPLE_COUNT - 2U);
    *mv_out = (uint16_t)((mv * BAT_DIV_NUM) / BAT_DIV_DEN);
    return 0;
}

static void bat_publish(uint16_t mv, uint32_t now)
{
    uint16_t target = bat_mv_to_q8(mv);
    uint32_t steps, i;
    uint8_t rounded;
    int16_t diff;
    if (!s_bat_valid) {
        s_bat_ema_q8 = target;
        s_bat_level_display = (uint8_t)((target + BAT_Q / 2U) / BAT_Q);
        s_bat_valid = 1U;
    } else {
        /* Account for missed minutes while LTE was off, without waking it.
         * Limit catch-up to 64 EMA steps; no dependence on read count. */
        steps = (now - s_last_update_ms) / BAT_UPDATE_MS;
        if (steps < 1U) steps = 1U;
        if (steps > 64U) steps = 64U;
        for (i = 0U; i < steps; ++i)
            s_bat_ema_q8 = (uint16_t)(((uint32_t)s_bat_ema_q8 * 15U + target + 8U) / 16U);
        rounded = (uint8_t)((s_bat_ema_q8 + BAT_Q / 2U) / BAT_Q);
        diff = (int16_t)rounded - s_bat_level_display;
        if (diff >= (int16_t)BAT_HYST_THRESHOLD || diff <= -(int16_t)BAT_HYST_THRESHOLD ||
            rounded == 0U || rounded == 100U)
            s_bat_level_display = rounded;
    }
    s_last_update_ms = now;
    co_printf("[BAT  ] vbat=%umV raw=%u%% ema=%u.%u%% disp=%u%%\r\n",
              (unsigned)mv, (unsigned)((target + BAT_Q / 2U) / BAT_Q),
              (unsigned)(s_bat_ema_q8 / BAT_Q),
              (unsigned)(((uint32_t)(s_bat_ema_q8 % BAT_Q) * 10U) / BAT_Q),
              (unsigned)s_bat_level_display);
}

void bat_adc_poll(void)
{
    uint32_t now = app_now_ms();
    uint16_t mv, a, b, c, middle;
    if (!LTE_EN_IS_ON()) {
        bat_adc_restart_window();
        return;
    }
    if (!s_lte_seen) {
        s_lte_seen = 1U;
        s_power_ms = now;
        s_quiet_valid = 0U;
        s_group_count = 0U;
    }
    if (g_audio_pcm_enabled || cat1_modem_is_busy() || task_keyscan_ir_busy() ||
        udp_ota_session_busy()) {
        s_quiet_valid = 0U;
        s_group_count = 0U;
        return;
    }
    if (!s_quiet_valid) {
        s_quiet_valid = 1U;
        s_quiet_ms = now;
    }
    if (now - s_power_ms < BAT_POWER_SETTLE_MS || now - s_quiet_ms < BAT_QUIET_MS)
        return;
    if (s_bat_valid && now - s_last_update_ms < BAT_UPDATE_MS) return;
    if (s_retry_pending && now - s_group_ms < BAT_RETRY_MS) return;
    if (s_group_count && now - s_group_ms < BAT_GROUP_GAP_MS) return;
    s_group_ms = now;
    if (bat_sample_group(&mv) != 0) {
        s_group_count = 0U;
        s_retry_pending = 1U;
        co_printf("[BAT  ] sample failed, keep %u%%\r\n", (unsigned)s_bat_level_display);
        return;
    }
    s_retry_pending = 0U;
    s_group_mv[s_group_count++] = mv;
    if (s_group_count != BAT_GROUP_COUNT) return;
    a = s_group_mv[0]; b = s_group_mv[1]; c = s_group_mv[2];
    middle = a > b ? (b > c ? b : (a > c ? c : a)) : (a > c ? a : (b > c ? c : b));
    s_group_count = 0U;
    bat_publish(middle, now);
}
