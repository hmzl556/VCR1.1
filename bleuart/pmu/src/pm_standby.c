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
#include "cat1_usart2.h"    /* USART2_CAT1_Init — 退出待机恢复 PB4/PB5 AF */
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

    /*
     * ★ 关 CAT1 串口 + PB4 输出低 (防灌电):
     *   原理图: PB4(USART2 TX, 网络名 B4_IRTX) → D1(B5817WS 肖特基) → LTE_RXD(CAT1 的 MAIN_RXD)。
     *   D1 方向: 阳极=LTE_RXD(模组侧), 阴极=PB4(MCU 侧) — 这是 3.3V MCU 驱动 1.8V
     *   模组的电平隔离: MCU 拉低时 D1 正向导通把线拉低; MCU 高时 D1 反偏,
     *   线由模组内部 1.8V 上拉保持, MCU 的 3.3V 不会灌进 1.8V 域。
     *   PB4 空闲高电平(3.3V)时 D1 反偏, 肖特基反向漏电流会经 LTE_RXD 流入
     *   未上电 CAT1 的输入钳位二极管, 给其 VBAT 轨偷充电(µA~几十 µA 量级,
     *   且随温度上升), 长期会让模组半上电。PB4 输出低后 D1 零偏置, 漏电≈0。
     *   所以先关 USART2, 再把 PB4 输出低。
     *   (注: 早期观察到的 500µA 主因是 8ms sdk 定时器竞态, 见 main.c 注释,
     *   并非此路; 此处是防御性省电配置。)
     */
    USART2->CTRL1 &= (uint16_t)(~((uint16_t)USART_CTRL1_TXEN | (uint16_t)USART_CTRL1_RXEN));
    /* 语音硬件兜底关断 (正常流程 Audio_Pcm_Stop 已关, 此处作保险) */
    Audio_Pcm_HardwareShutdown();

    pm_clk_off();
    pm_led_all_off();

    /*
     * ★ 待机态引脚配置 (按原理图 Vioce_Cat1_Remote_V1.1 逐脚核对):
     *   PB4 (USART2 TX → D1(肖特基) → LTE_RXD): 输出低 — D1 反偏漏电
     *       会给未上电 CAT1 偷充电(防御性); 低电平=零偏置=零漏电
     *   PB6 (VLTE→R16 20K→PB6→R28 4.7K→GND 分压): 模拟输入不上拉
     *       (上拉经 R28 对地白耗 ~74µA; CAT1 断电时 VLTE=0 分压本身无电流)
     *   PB11/12/13 (麦克风偏置网络): 模拟输入, 麦断电
     *       (上拉会经 R23/R24/R25 偏置网络给麦供电, 几十 µA)
     *   PB2 (LTE_EN): 输出低, CAT1 断电; PB3/PB7 (LED): 输出低熄灭
     *   其余 PA0-6/PB0/1/5/8-10: 上拉输入 — 外部无 DC 通路, 上拉无害
     *   注意: 此配置下 KEYSCAN 不能唤醒, 测量阶段仅测功耗。
     */
    memset(&gpio_lp, 0, sizeof(gpio_lp));
    gpio_lp.GPIO_Mode = GPIO_MODE_INPUT;
    gpio_lp.GPIO_Pull = GPIO_PULL_UP;

    /* GPIOA: PA0/1/2/3/6 上拉输入 (键阵, 无 DC 通路)。
     * ★ PA4(SWDCLK)/PA5(SWDIO) 不重配: 否则调试口永久失效,
     *   复位前 Keil 连不上芯片 (实测"有时无法烧录") */
    gpio_lp.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                | GPIO_PIN_6;
    GPIO_InitPeripheral(GPIOA, &gpio_lp);

    /* GPIOB: PB0/5/8/9/10 上拉输入。
     * PB1 = LPUART TX (co_printf 日志) 保留 AF 不重配 — 诊断日志需要;
     * AF 推挽空闲高电平对空载测试点无 DC 通路, 不耗电。 */
    gpio_lp.Pin = GPIO_PIN_0 | GPIO_PIN_5
                | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);

    /* PB6: 模拟输入不上拉 (分压 4.7K 对地, 上拉白耗 74µA) */
    gpio_lp.Pin = GPIO_PIN_6;
    gpio_lp.GPIO_Mode = GPIO_MODE_ANALOG;
    gpio_lp.GPIO_Pull = GPIO_NO_PULL;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);

    /* PB11/12/13: 模拟输入, 麦克风断电 */
    gpio_lp.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);

    /* PB2/3/4/7: 输出低 — CAT1 断电 + LED 灭 + PB4 防灌 CAT1
     * (PB4 漏电原理见上面"关 CAT1 串口"的注释: D1 肖特基反偏漏电) */
    gpio_lp.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_7;
    gpio_lp.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
    gpio_lp.GPIO_Pull = GPIO_PULL_DOWN;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);
    GPIO_ResetBits(GPIOB, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_7);

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
    /* 恢复 USART2 (PB4/PB5 AF 复用 + RX DMA): 待机态把 PB4 改成了输出低 */
    USART2_CAT1_Init();
    /* CAT1 上电由 rc_cat1_poll 检测 s_pending_tlv 后触发, 此处不再强制上电 */
}
