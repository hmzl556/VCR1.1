/**
 * @file    pm_sleep.c
 * @brief   Sleep mode management — enter Sleep mode / configure wakeup sources
 * @note    Uses BR1601 Sleep mode (NOT Power Down):
 *          - Wake sources: KEYSCAN EXTI (key press)
 *
 * Dependencies:
 *   - pwr.h    -> PWR_EnterSLEEPMode()
 *   - sw_wdt.h -> sw_wdt_suspend() (suspend watchdog before sleep)
 *   - exti.h   -> EXTI wakeup line status
 *   - gmem.h   -> bleAdvStat (reset to force BLE restart after wake)
 *   - ble_adv.h -> ble_adv_stop()
 */

#include "pm_config.h"
#include "pm_sleep.h"
#include <string.h>
#include "pwr.h"        /* PWR_EnterSLEEPMode, PWR_SLEEPENTRY_WFI */
#include "rcc.h"        /* RCC */
#include "exti.h"       /* EXTI_GetITStatus, EXTI_LINE13 */
#include "gmem.h"       /* bleAdvStat */
#include "ble_adv.h"    /* ble_adv_stop() */
#include "userhard.h"   /* CLRLED1, CLRLED2, SETLED1                 */
#include "iwdg.h"       /* IWDG_ReloadKey()                          */
#include "cat1_skt_udp.h" /* cat1_sleep_prepare, cat1_sleep_wakeup     */
#include "cat1_usart2.h"  /* USART2_CAT1_RxFlush after sleep            */
#include "app_audio_pcm.h" /* Audio_Pcm_HardwareShutdown                */

/** @brief 休眠中标志：进入 WFI 前置 true，KEYSCAN ISR 据此在按键唤醒瞬间亮 LED1 */
volatile bool g_pm_sleeping = false;

/*===========================================================================
 * 1. Wakeup Source Detection
 *===========================================================================*/

/**
 * @brief  Determine what caused the wakeup from Sleep mode
 * @return PM_WAKE_REASON_KEYSCAN or PM_WAKE_REASON_NONE
 */
static uint8_t pm_detect_wake_reason(void)
{
    if (EXTI_GetITStatus(EXTI_LINE13) == SET) {
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

    gpio_lp.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitPeripheral(GPIOA, &gpio_lp);

    gpio_lp.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_6;
    GPIO_InitPeripheral(GPIOB, &gpio_lp);
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
 * 3. Public Entry Point
 *===========================================================================*/

/**
 * @brief  Enter Sleep mode
 * @return Wakeup reason (PM_WAKE_REASON_KEYSCAN or PM_WAKE_REASON_NONE)
 *
 * Flow:
 *   1. Stop BLE advertising
 *   2. Reset bleAdvStat
 *   3. Suspend software watchdog (TIM6 interrupt off)
 *   4. Shutdown non-essential peripherals
 *   5. Enter Sleep mode via WFI
 *   6. CPU sleeps — wake on KEYSCAN EXTI
 *   7. Restore peripherals
 *   8. Return wake reason
 */
uint8_t pm_sleep_enter(void)
{
    uint8_t wake_reason;

    PM_LOG("Sleep enter: preparing...");

    /* 1. Stop BLE advertising if running */
    ble_adv_stop();

    /* 2. Reset bleAdvStat to force BLE restart on main loop after wake */
    bleAdvStat = 0U;

    /*
     * 3. Kick IWDG before sleep — gives 10s grace period.
     *    After IWDG resets the system, main() detects IWDGRSTF
     *    and skips IWDG init → sleep without watchdog.
     */
    IWDG_ReloadKey();

    /* 4. Shutdown non-essential peripherals */
    pm_peripherals_sleep_shutdown();

    PM_LOG("Sleep enter: entering Sleep mode (WFI)...");

    /*
     * 5. Enter Sleep mode via WFI.
     *    Wake on KEYSCAN EXTI (key press).
     */
    g_pm_sleeping = true;
    PWR_EnterSLEEPMode(PWR_SLEEPENTRY_WFI);

    /*
     * CPU resumes HERE after wakeup.
     */

    /* 6. Detect wake source */
    wake_reason = pm_detect_wake_reason();

    /* Clear g_pm_sleeping if ISR didn't already */
    if (g_pm_sleeping) {
        g_pm_sleeping = false;
    }

    /* 7. Restore peripherals */
    pm_peripherals_sleep_restore();

    PM_LOG("Sleep exit: wake reason=%u, returning to main loop", (unsigned)wake_reason);
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

    g_pm_sleeping = true;
    PWR_EnterSLEEPMode(PWR_SLEEPENTRY_WFI);

    wake_reason = pm_detect_wake_reason();
    if (g_pm_sleeping) {
        g_pm_sleeping = false;
    }
    pm_peripherals_sleep_restore();

    PM_LOG("Sleep (no-BLE) exit: reason=%u", (unsigned)wake_reason);
    return wake_reason;
}
