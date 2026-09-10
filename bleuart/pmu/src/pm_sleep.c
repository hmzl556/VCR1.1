/**
 * @file    pm_sleep.c
 * @brief   休眠态管理 — 断开连接后进入 br_sleep() 深睡, 按键唤醒
 * @note    与待机态同一睡眠机制 (br_sleep, 由 SDK 统一管理 BLE modem):
 *          - 进休眠前: 主动断连(只断连不擦绑定) + 停广播 + 外设关断;
 *            若保持连接, br_sleep 会为连接事件周期性唤醒(平均 20+µA),
 *            不满足休眠态 <20µA 要求
 *          - 睡眠: br_sleep 循环 — modem 周期事件等噪音唤醒继续睡,
 *            只有按键 (key_receive/EXTI13) 才退出
 *          - 唤醒: 复用现有 BLE 栈重启广播 (bleAdvStat 保持 0xa6,
 *            主循环不会重建栈 — 重建=二次 malloc → 堆耗尽 HardFault)
 *
 * Dependencies:
 *   - cs_core.h -> cs_schedule() (每次 br_sleep 返回后服务栈事件)
 *   - gap.h     -> le_conn_destory() / peer_bonded_count()
 *   - ble_gap.h -> ble_gap_get_peer()
 *   - keyscan.h -> KEYSCAN_InfoClear / KEYSCAN_ClearInterrupt
 */

#include "pm_config.h"
#include "pm_sleep.h"
#include <string.h>
#include "pwr.h"        /* PWR_EnterSLEEPMode, PWR_SLEEPENTRY_WFI */
#include "rcc.h"        /* RCC */
#include "exti.h"       /* EXTI_GetITStatus, EXTI_LINE13 */
#include "gmem.h"       /* bleAdvStat */
#include "ble_adv.h"    /* ble_adv_stop() */
#include "cs_core.h"    /* cs_schedule() — br_sleep 返回后服务栈事件 */
#include "gap.h"        /* peer_t, le_conn_destory(), peer_bonded_count() */
#include "ble_gap.h"    /* ble_gap_get_peer() */
#include "userhard.h"   /* CLRLED1, CLRLED2, SETLED1                 */
#include "iwdg.h"       /* IWDG_ReloadKey()                          */
#include "cat1_skt_udp.h" /* cat1_sleep_prepare, cat1_sleep_wakeup     */
#include "cat1_usart2.h"  /* USART2_CAT1_RxFlush after sleep            */
#include "app_audio_pcm.h" /* Audio_Pcm_HardwareShutdown                */
#include "task_keyscan.h"  /* key_receive — 按键唤醒判断                 */
#include "keyscan.h"      /* KEYSCAN_InfoClear / KEYSCAN_ClearInterrupt */
#include "misc.h"         /* NVIC_ClearPendingIRQ                       */
#include "dma.h"          /* DMA_ClrIntPendingBit                       */

/** @brief 休眠中标志：进入 br_sleep 前置 true，KEYSCAN ISR 据此在按键唤醒瞬间亮 LED1 */
volatile bool g_pm_sleeping = false;

extern void sdk_timer_pause(void);   /* main.c: 删 8ms 定时器让 br_sleep 深睡 */

/*===========================================================================
 * 1. Wakeup Source Detection
 *===========================================================================*/

/**
 * @brief  Determine what caused the wakeup from Sleep mode
 * @return PM_WAKE_REASON_KEYSCAN or PM_WAKE_REASON_NONE
 */
static uint8_t pm_detect_wake_reason(void)
{
    /* 按键 ISR 会先读到按键并清掉 EXTI13, 因此优先看 key_receive,
       否则唤醒原因会误报 UNKNOWN */
    if (key_receive != 0u || EXTI_GetITStatus(EXTI_LINE13) == SET) {
        EXTI_ClrITPendBit(EXTI_LINE13);
        PM_LOG("Wake reason: KEYSCAN (key press)");
        return PM_WAKE_REASON_KEYSCAN;
    }

    PM_LOG("Wake reason: UNKNOWN");
    return PM_WAKE_REASON_NONE;
}

/*===========================================================================
 * 2. Peripherals Shutdown / Restore
 *===========================================================================*/

/**
 * @brief  Shutdown non-essential peripherals before entering Sleep mode
 */
static void pm_peripherals_sleep_shutdown(void)
{
    GPIO_InitType gpio_lp;

    /* Turn off LEDs */
    CLRLED1;
    CLRLED2;

    /* Disable CAT1 UART completely (TX+RX)；DMA_CH2 保持配置，唤醒后 RxFlush */
    USART2->CTRL1 &= (uint16_t)(~((uint16_t)USART_CTRL1_TXEN | (uint16_t)USART_CTRL1_RXEN));

    /* Shutdown audio (ADC/CH1)；AHB DMA 时钟保留给 USART2 RX */
    Audio_Pcm_HardwareShutdown();

    /* Unused GPIO → analog mode to eliminate floating-input leakage */
    memset(&gpio_lp, 0, sizeof(gpio_lp));
    gpio_lp.GPIO_Mode = GPIO_MODE_ANALOG;
    gpio_lp.GPIO_Pull = GPIO_NO_PULL;

    /* ★ 不动 GPIOA: PA4(SWDCLK)/PA5(SWDIO) 必须保持默认调试功能,
     *   改成模拟后复位前调试器无法连接 */

    gpio_lp.Pin = GPIO_PIN_0 | GPIO_PIN_6;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);

    /*
     * PB2(LTE_EN)/PB4(USART2 TX) 输出低再睡:
     * STANDBY→SLEEP 前 pm_exit_standby 的 USART2_CAT1_Init 会把 PB4 重配为
     * AF 空闲高电平 — 整个休眠期都会经 D1(肖特基) 给未上电 CAT1 充电,
     * 唤醒后再进待机时放电窗口拉高功耗; 拉低后彻底断干净。
     * 唤醒后 rc_cat1_init()→USART2_CAT1_Init() 会恢复 AF。
     */
    gpio_lp.Pin = GPIO_PIN_2 | GPIO_PIN_4;
    gpio_lp.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
    gpio_lp.GPIO_Pull = GPIO_PULL_DOWN;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);
    GPIO_ResetBits(GPIOB, GPIO_PIN_2 | GPIO_PIN_4);
}

/**
 * @brief  Restore peripherals after waking from Sleep mode
 */
static void pm_peripherals_sleep_restore(void)
{
    /* Re-enable CAT1 UART RX+TX，并清空休眠期间可能积压的 DMA 环 */
    USART2->CTRL1 |= (USART_CTRL1_RXEN | USART_CTRL1_TXEN);
    USART2_CAT1_RxFlush();
}

/*===========================================================================
 * 2.5 休眠前清除残留中断
 *
 * WFI 前必须清干净所有 pending 中断, 否则一进去就被唤醒
 * (现象: 待机→休眠瞬间自动唤醒)。
 *   - USART2 RX DMA: 待机态关 USART2 时环形缓冲可能正跨半/全满边界,
 *     HTX/TXC 置位且 NVIC 未清 → WFI 立刻醒;
 *   - KEYSCAN/EXTI13: 待机引脚重配或扫描可能留下 pending;
 *   - USART2 OREF 等残留同样清除。
 *===========================================================================*/
static void pm_clear_pending_wake_irqs(void)
{
    KEYSCAN_InfoClear();
    KEYSCAN_ClearInterrupt();
    EXTI_ClrITPendBit(EXTI_LINE13);
    NVIC_ClearPendingIRQ(KEYSCAN_IRQn);
    NVIC_ClearPendingIRQ(USART2_IRQn);
    DMA_ClrIntPendingBit(DMA_INT_GLB1 | DMA_INT_HTX1 | DMA_INT_TXC1 | DMA_INT_ERR1, DMA);
    NVIC_ClearPendingIRQ(DMA_Channel1_2_3_4_IRQn);
}

/*===========================================================================
 * 3. Public Entry Point
 *===========================================================================*/

/**
 * @brief  进入休眠态: 断连 → 关外设 → br_sleep 循环深睡 → 按键唤醒
 * @return 唤醒原因 (PM_WAKE_REASON_KEYSCAN 或 PM_WAKE_REASON_NONE)
 *
 * 流程:
 *   1. 主动断连 (只断连不擦绑定) + 停止广播
 *   2. 保持 bleAdvStat=0xa6 (唤醒后复用栈, 不重建, 防堆耗尽 HardFault)
 *   3. 喂 IWDG (深睡期间 LSI 停止, IWDG 冻结)
 *   4. 关断非必要外设 (LED/USART2/音频/引脚低功耗配置)
 *   5. 清残留中断 (防止一进睡眠就被唤醒)
 *   6. br_sleep 循环深睡: 噪音唤醒继续睡, 按键才退出
 *   7. 恢复外设
 *   8. 返回唤醒原因
 */
uint8_t pm_sleep_enter(void)
{
    uint8_t wake_reason;

    PM_LOG("Sleep enter: preparing...");

    /*
     * 0. 主动断开 BLE 连接:
     *    休眠态功耗要求 <20µA。若保持连接, br_sleep 会为连接事件每 50ms
     *    醒来一次(瞬时 <10µA 但平均 20+µA, 实测不达标);
     *    而裸 WFI 又会被 modem 周期校准(1-2s)唤醒后崩溃。
     *    因此: 先断开 → br_sleep 无连接深睡(与待机态同机制, ~3µA) →
     *    唤醒后 pm_wakeup_entry 走回连广播重建连接。
     *    断连事件由 pm_notify_disconnected 在 SLEEP 态下忽略。
     */
    {
        peer_t peer = ble_gap_get_peer();
        if (peer != NULL) {
            /* 只断连, 不擦绑定 (擦绑定是 peer_bonded_remove/clear,
             * 仅在组合键解配路径使用) */
            le_conn_destory(peer);
        }
    }

    /* 1. Stop BLE advertising if running */
    ble_adv_stop();

    /*
     * 2. ★ 保持 bleAdvStat=0xa6, 唤醒后主循环不重建 BLE 栈:
     *    重建=在已有栈上二次初始化 → 二次 malloc → 堆耗尽(堆仅 10KB)
     *    → HardFault (实测: 每次按键唤醒都硬复位一次; 冷启动时的
     *    myBleStartAdv 初始化正常, 因为堆是全新的 — 问题只在唤醒路径)。
     *    唤醒后由 pm_enter_sleep 复用现有栈重启广播。
     */

    /* 3. 喂狗: br_sleep 深睡期间 LSI 停止, IWDG 冻结, 不会误复位 */
    IWDG_ReloadKey();

    /* 4. Shutdown non-essential peripherals */
    pm_peripherals_sleep_shutdown();

    co_printf("[SLEEP] >>> entering br_sleep() (no connection, modem-aware)\r\n");

    pm_clear_pending_wake_irqs();

    /*
     * ★ 统一暂停 8ms sdk 定时器: 回连/配对态直接进休眠时定时器还在跑,
     *   br_sleep 会被它每 8ms 叫醒(噪音 125Hz), 十几秒后必重启(实测
     *   ~2000 次噪音唤醒后崩)。待机态入口已暂停过, 此处幂等;
     *   唤醒后由 pm_enter_sleep 里 sdk_timer_resume() 恢复。
     */
    sdk_timer_pause();

    /*
     * ★ 用 br_sleep() 而不是裸 WFI (PWR_EnterSLEEPMode):
     *   BLE modem 有周期性校准事件(~1-2s, SDK 管理), 裸 WFI 会被它唤醒,
     *   醒来后栈/modem 状态不一致 → 乱码打印 + 复位(实测: 睡后 1-2s 必重启)。
     *   已在进入前断开连接, br_sleep 无需维持连接 → 无 50ms 连接事件唤醒,
     *   深睡电流与待机相当(~3µA, 满足 <20µA)。时钟由 SDK 恢复, 打印不乱码。
     */
    {
        uint8_t lock_cnt;
        do { lock_cnt = br_sleep_lock_release(); }
        while (lock_cnt != 0u);
    }

    /*
     * 6. br_sleep 循环深睡:
     *    br_sleep 会因 modem 周期事件(校准等)自行返回 — 这是噪音不是
     *    用户唤醒 → 清残留继续睡; 只有按键才真正退出:
     *    key_receive 由 KEYSCAN ISR 置位, EXTI13 作为后备判断。
     *    每次 br_sleep 返回后先 cs_schedule(): 与厂商 demo 及待机分支
     *    一致, 服务 modem 校准/断连完成等栈事件, 否则事件堆积会崩溃。
     */
    g_pm_sleeping = true;
    for (;;) {
        br_sleep();
        cs_schedule();
        if (key_receive != 0u || EXTI_GetITStatus(EXTI_LINE13) == SET) {
            break;
        }
        pm_clear_pending_wake_irqs();
    }
    g_pm_sleeping = false;

    wake_reason = pm_detect_wake_reason();

    /* Clear g_pm_sleeping if ISR didn't already */
    if (g_pm_sleeping) {
        g_pm_sleeping = false;
    }

    /* 7. Restore peripherals */
    pm_peripherals_sleep_restore();

    return wake_reason;
}

/**
 * @brief  Enter Sleep without touching BLE (IWDG wake → direct sleep path).
 *         Skips ble_adv_stop() / BLE restart — BLE is not initialized.
 */
uint8_t pm_sleep_enter_no_ble(void)
{
    uint8_t wake_reason;

    PM_LOG("Sleep (no-BLE): preparing...");

    /* Reset bleAdvStat so next normal boot restarts BLE */
    bleAdvStat = 0U;

    /* Kick IWDG — safe even if not running, just writes 0xAAAA to KEY reg */
    IWDG_ReloadKey();

    /* Shutdown non-essential peripherals */
    pm_peripherals_sleep_shutdown();

    PM_LOG("Sleep (no-BLE): entering WFI...");

    pm_clear_pending_wake_irqs();

    g_pm_sleeping = true;
    PWR_EnterSLEEPMode(PWR_SLEEPENTRY_WFI);

    /* 醒来后 HSI 刚重启, 稍等时钟稳定再打印, 否则唤醒原因日志乱码 */
    delay_n_10us(200u);

    wake_reason = pm_detect_wake_reason();
    if (g_pm_sleeping) {
        g_pm_sleeping = false;
    }
    pm_peripherals_sleep_restore();

    PM_LOG("Sleep (no-BLE) exit: reason=%u", (unsigned)wake_reason);
    return wake_reason;
}
