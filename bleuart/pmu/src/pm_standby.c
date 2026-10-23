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
#include "br1601x.h"        /* USART1, USART_CTRL1_TXEN */
#include "cat1_usart1.h"    /* USART1_CAT1_Init — restore PA4/PA5 AF */
#include "irc.h"
#include "rc_cat1_bridge.h"
#include "cat1_skt_udp.h"   /* cat1_skt_udp_force_fail_active — 终止在途 AT */
extern void Audio_Pcm_HardwareShutdown(void);  /* app_audio_pcm.h */
extern void sdk_timer_pause(void);   /* main.c: 删 8ms 定时器让 br_sleep 深睡 */
extern void sdk_timer_resume(void);  /* main.c: 恢复 8ms 定时器 */

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
    /* APB1: gate WWDG; leave IR timer clocks available for restoration. */
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
    /* V1.2: CAT1 PA4/PA5, IR PB4/PB5. Preserve all KEYSCAN AF5 pins
     * (PA0/1/2/3/6, PB8/9/10), unlike the old board's measurement setup. */
    if(rc_cat1_is_powered())rc_cat1_power_down();
    cat1_skt_udp_force_fail_active();
    USART1->CTRL1 &= (uint16_t)~(USART_CTRL1_TXEN|USART_CTRL1_RXEN);
    Audio_Pcm_HardwareShutdown();
    pm_clk_off();pm_led_all_off();
    GPIO_InitStruct(&gpio_lp);
    gpio_lp.Pin=GPIO_PIN_4;gpio_lp.GPIO_Mode=GPIO_MODE_OUTPUT_PP;
    gpio_lp.GPIO_Pull=GPIO_PULL_DOWN;
    GPIO_InitPeripheral(GPIOA,&gpio_lp);GPIO_ResetBits(GPIOA,GPIO_PIN_4);
    gpio_lp.Pin=GPIO_PIN_5;gpio_lp.GPIO_Mode=GPIO_MODE_INPUT;gpio_lp.GPIO_Pull=GPIO_NO_PULL;
    GPIO_InitPeripheral(GPIOA,&gpio_lp);
    gpio_lp.Pin=GPIO_PIN_4;gpio_lp.GPIO_Mode=GPIO_MODE_OUTPUT_PP;gpio_lp.GPIO_Pull=GPIO_PULL_DOWN;
    GPIO_InitPeripheral(GPIOB,&gpio_lp);GPIO_ResetBits(GPIOB,GPIO_PIN_4);
    gpio_lp.Pin=GPIO_PIN_5;gpio_lp.GPIO_Mode=GPIO_MODE_INPUT;gpio_lp.GPIO_Pull=GPIO_PULL_UP;
    GPIO_InitPeripheral(GPIOB,&gpio_lp);
    gpio_lp.Pin=GPIO_PIN_0|GPIO_PIN_6|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13;
    gpio_lp.GPIO_Mode=GPIO_MODE_ANALOG;gpio_lp.GPIO_Pull=GPIO_NO_PULL;
    GPIO_InitPeripheral(GPIOB,&gpio_lp);

    /*
     * ★ 删掉 8ms SDK 定时器 (关键省电点):
     *   8ms 定时器是 FreeRTOS 系统节拍来源(callBakSdkTimer → xTaskIncrementTick),
     *   常驻 pending。br_sleep() 按最近 pending sdk 定时器到期时间设睡眠时长,
     *   8ms 定时器在 → 每 8ms 被唤醒跑一遍主循环 → 平均 ~500µA
     *   (实测唤醒率 242Hz 的 500µA 态即此; 详见 main.c 的竞态修复注释)。
     *   删掉后 br_sleep 睡到下一个硬件事件: BLE 连接事件(50ms 间隔 ×
     *   latency 100 ≈ 最多 5s 一次)或按键 EXTI。待机态唯一任务阻塞在
     *   br_sleep 里, FreeRTOS tick 冻结安全; 唤醒后 pm_standby_exit()
     *   里 sdk_timer_resume() 重建。
     */
    sdk_timer_pause();

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
    /* 恢复 8ms SDK 定时器 (FreeRTOS tick) */
    sdk_timer_resume();
    pm_conn_params_set_working();
    pm_clk_on();
    /* Restore V1.2 CAT1 PA4/PA5 and IR PB4 AF2 independently. */
    USART1_CAT1_Init();
    irc_init();
    /* CAT1 上电由 rc_cat1_poll 检测 s_pending_tlv 后触发, 此处不再强制上电 */
}
