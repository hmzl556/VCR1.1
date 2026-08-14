/**
 * @file    pm_standby.c
 * @brief   待机态: BLE 省电参数 + CAT1 控制 + LED 关闭
 */

#include "pm_config.h"
#include "gap.h"
#include "ble_gap.h"
#include "userhard.h"
#include "gmem.h"
#include <string.h>
#include "co_printf.h"
#include "rcc.h"
#include "br1601x.h"        /* USART2, USART_CTRL1_TXEN */
extern void rc_cat1_power_down(void);
extern void rc_cat1_power_up(void);
extern void Audio_Pcm_HardwareShutdown(void);  /* app_audio_pcm.h */

/* 外设时钟门控: STANDBY 期间关不用的外设, 退出时恢复 */
static uint32_t s_apb2_save, s_apb1_save, s_ahb_save;
static uint8_t  s_clk_saved;

static void pm_clk_off(void)
{
    if (s_clk_saved) return;
    s_apb2_save = RCC->APB2PCLKEN;
    s_apb1_save = RCC->APB1PCLKEN;
    s_ahb_save  = RCC->AHBPCLKEN;
    s_clk_saved = 1u;
    /* APB2: 关 SPI1/SPI2/TIM1/USART1 */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_SPI1 | RCC_APB2_PERIPH_SPI2
                          | RCC_APB2_PERIPH_TIM1 | RCC_APB2_PERIPH_USART1, DISABLE);
    /* APB1: 关 WWDG (保留 USART2, TIM3, TIM6) */
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_WWDG, DISABLE);
    /* AHB: 关 CRC */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_CRC, DISABLE);
    /* LPUART 保留, co_printf/BLE栈可能用 */
}

static void pm_clk_on(void)
{
    if (!s_clk_saved) return;
    RCC->APB2PCLKEN = s_apb2_save;
    RCC->APB1PCLKEN = s_apb1_save;
    RCC->AHBPCLKEN  = s_ahb_save;
    RCC_EnableLpuartClk(ENABLE);
    s_clk_saved = 0u;
}

/* br_sleep() 回调: 睡之前灭 LED */
void app_sleep_prepare_proc(void) { CLRLED1; CLRLED2; }
void app_sleep_resume_proc(void)  {}

static void pm_led_all_off(void) { CLRLED1; CLRLED2; }

static void pm_conn_params_set_standby(void)
{
    peer_t peer = ble_gap_get_peer();
    if (peer == NULL) return;
    le_conn_update_t par;
    par.peer = peer;
    // 设置待机态连接参数: 大间隔 + 最大延迟数
    par.intv_min    = PM_STANDBY_CONN_INTV_MIN; 
    // 最大连接间隔
    par.intv_max    = PM_STANDBY_CONN_INTV_MAX;
    // 最大延迟数
    par.latency_max = PM_STANDBY_CONN_LATENCY;
    // 连接超时
    par.timeout     = PM_STANDBY_CONN_TIMEOUT;
    par.ce_len_min  = 0; par.ce_len_max = 0;
    le_conn_update(&par);
}

static void pm_conn_params_set_working(void)
{
    peer_t peer = ble_gap_get_peer();
    if (peer == NULL) return;
    le_conn_update_t par;
    par.peer = peer;
    par.intv_min    = PM_WORK_CONN_INTV_MIN;
    par.intv_max    = PM_WORK_CONN_INTV_MAX;
    par.latency_max = PM_WORK_CONN_LATENCY;
    par.timeout     = PM_WORK_CONN_TIMEOUT;
    par.ce_len_min  = 0; par.ce_len_max = 0;
    le_conn_update(&par);
}

void pm_standby_enter(void)
{
    GPIO_InitType gpio_lp;

    pm_conn_params_set_standby();
    /*
     * CAT1 不再由待机态管理: 平时就是关的(事务完成后自动断电),
     * 仅在有 RC-TRANS 待上报/OTA 时由 rc_cat1_poll 自动上电。
     */
    // rc_cat1_power_down();

    /* 同 SLEEP: 关 CAT1 串口 + 音频硬件 */
    // USART2->CTRL1 &= (uint16_t)(~((uint16_t)USART_CTRL1_TXEN | (uint16_t)USART_CTRL1_RXEN));
    // Audio_Pcm_HardwareShutdown();

    pm_clk_off();
    pm_led_all_off();

    /*
     * 闲置 GPIO → Analog 消浮空漏电。
     * 不设 Analog 的引脚:
     *   PA4/PA5 — SWD 引脚, 复位后已有上下拉, 不浮空
     *   PB2     — LTE_EN, rc_cat1_power_down 已拉 LOW(Output), 不耗电
     *   PB8-10  — KEYSCAN 列线, 需保持 AF_OD 用于按键唤醒
     *   PA0-3/PA6 — KEYSCAN 行线, 同上
     */
    // memset(&gpio_lp, 0, sizeof(gpio_lp));
    // gpio_lp.GPIO_Mode = GPIO_MODE_ANALOG;
    // gpio_lp.GPIO_Pull = GPIO_NO_PULL;
    // /* GPIOA: 无需要 Analog 的引脚 (PA4/PA5 有 SWD 上下拉) */
    // /* GPIOB: PB0(闲置), PB6(ADC5) → Analog */
    // gpio_lp.Pin = GPIO_PIN_0 | GPIO_PIN_6;
    // GPIO_InitPeripheral(GPIOB, &gpio_lp);

    /* 释放睡眠锁让 br_sleep() 可深睡 */
    {
        uint8_t lock_cnt;
        do { lock_cnt = br_sleep_lock_release(); }
        while (lock_cnt != 0u);
    }
}

void pm_standby_exit(void)
{
    /* 加锁: 工作态下 br_sleep() 只进 IDLE 保证低延迟 */
    br_sleep_lock_acquire();
    pm_conn_params_set_working();
    pm_clk_on();
    /* CAT1 上电由 rc_cat1_poll 检测 s_pending_tlv 后触发, 此处不再强制上电 */
}
