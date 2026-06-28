/**
 * @file    pm_sleep.c
 * @brief   Sleep mode management — enter Sleep mode / configure wakeup sources
 * @note    Uses BR1601 Sleep mode (NOT Power Down):
 *          - Typical current: 1.6uA @ 3V (well within 20uA target)
 *          - 48KB SRAM retained, LSI kept running for RTC
 *          - Wake sources: KEYSCAN EXTI (key press) + RTC wakeup timer
 *          - CPU resumes from WFI, no cold boot needed
 *
 * Dependencies:
 *   - pwr.h    -> PWR_EnterSLEEPMode()
 *   - rtc.h    -> RTC wakeup timer configuration
 *   - exti.h   -> EXTI wakeup line status
 *   - gmem.h   -> bleAdvStat (reset to force BLE restart after wake)
 *   - ble_adv.h -> ble_adv_stop()
 */

#include "pm_config.h"
#include "pm_sleep.h"
#include "pwr.h"        /* PWR_EnterSLEEPMode, PWR_SLEEPENTRY_WFI */
#include "rtc.h"        /* RTC_Init, RTC_ConfigWakeUpClock, etc.   */
#include "rcc.h"        /* RCC_EnableLSI, RCC_GetFlagStatus etc.   */
#include "exti.h"       /* EXTI_GetITStatus, EXTI_LINE13, EXTI_LINE9 */
#include "gmem.h"       /* bleAdvStat */
#include "ble_adv.h"    /* ble_adv_stop() */
#include "userhard.h"   /* CLRLED1, CLRLED2                       */

/** @brief 标记 RTC 是否已完成一次性初始化 */
static bool g_rtc_initialized = false;

/*===========================================================================
 * 1. RTC Initialization & Wakeup Timer Configuration
 *
 * Clock: LSI 32KHz / 16 = 2KHz
 * Count: 2000 ticks/sec * PM_SLEEP_RTC_WAKEUP_SEC
 * Example: 2000 * 5 = 10000 ticks for 5-second wakeup interval
 *
 * The RTC must be initialized once before the wakeup timer can be used.
 * This is done lazily on first sleep entry, since earlier code never used RTC.
 *===========================================================================*/

/**
 * @brief  One-time RTC initialization (called on first sleep entry)
 * @note   RTC uses LSI (32KHz internal RC) as clock source.
 *         The LSI is already enabled by the system (cfg_user.h: LSCI_32000HZ),
 *         but we verify it's ready before proceeding.
 */
static void pm_rtc_init_once(void)
{
    ErrorStatus status;

    if (g_rtc_initialized) {
        return;
    }

    PM_LOG("RTC one-time init starting...");

    /* ① 确保 LSI 已使能并稳定 */
    if (RCC_GetFlagStatus(RCC_LSCTRL_FLAG_LSIRD) == RESET) {
        RCC->LSCTRL |= RCC_LSCTRL_LSIEN;
        while (RCC_GetFlagStatus(RCC_LSCTRL_FLAG_LSIRD) == RESET) {
            /* 等待 LSI 起振稳定（通常 ~100μs） */
        }
        PM_LOG("LSI enabled and stabilized");
    }

    /* ② 选择 LSI 为低速时钟源（LSXSEL=0 即 LSI）并清零 LSE 选择位 */
    RCC->LSCTRL &= ~RCC_LSCTRL_LSXSEL;      /* 确保 LSI 被选为 LSC 时钟源 */

    /* ③ 使能 RTC 外设时钟（RCC_LSCTRL.RTCEN） */
    RCC->LSCTRL |= RCC_LSCTRL_RTCEN;

    /* ④ 使能 PWR 接口时钟（RTC 属于备份域，需要 PWR 时钟才能访问） */
    RCC_EnableAPB1PeriphClk(RCC_APB1PCLKEN_PWREN, ENABLE);

    /* ⑤ 配置并初始化 RTC */
    RTC_InitType rtc_cfg;
    RTC_StructInit(&rtc_cfg);
    rtc_cfg.RTC_HourFormat  = RTC_24HOUR_FORMAT;
    rtc_cfg.RTC_AsynchPrediv = 0x7F;       /* 128 - 1 = 127，适配 32KHz LSI */
    rtc_cfg.RTC_SynchPrediv  = 0xFF;       /* 256 - 1 = 255 */
    status = RTC_Init(&rtc_cfg);
    if (status == ERROR) {
        PM_LOG("RTC init FAILED!");
        return;
    }

    g_rtc_initialized = true;
    PM_LOG("RTC init OK (LSI 32KHz, wakeup timer ready)");
}

/**
 * @brief  Configure RTC wakeup timer for periodic wakeup during sleep
 * @note   Must call pm_rtc_init_once() before this function.
 *         Uses LSI / 16 = 2KHz base clock.
 *         Counter = 2000 * PM_SLEEP_RTC_WAKEUP_SEC.
 */
static void pm_rtc_wakeup_config(void)
{
    /* One-time RTC init (lazy, done on first sleep entry) */
    pm_rtc_init_once();
    if (!g_rtc_initialized) {
        /* RTC not available, skip wakeup timer config (can't wake from sleep!) */
        return;
    }

    /* Disable RTC wakeup before reconfiguring */
    RTC_EnableWakeUp(DISABLE);

    /* Wait until the wakeup timer flag is cleared (ensure previous config settled) */
    RTC_ClrFlag(RTC_FLAG_WTF);

    /* Set clock source: LSI / 16 = 2000 Hz */
    RTC_ConfigWakeUpClock(PM_SLEEP_RTC_WKUP_CLOCK);

    /* Set counter: 2000 * wakeup_seconds */
    RTC_SetWakeUpCounter(PM_SLEEP_RTC_WKUP_COUNT);

    /* Enable RTC wakeup timer interrupt (wake from Sleep via EXTI_LINE9) */
    RTC_EnableWakeUp(ENABLE);

    PM_LOG("RTC wakeup configured: %u sec", (unsigned)PM_SLEEP_RTC_WAKEUP_SEC);
}

/*===========================================================================
 * 2. Wakeup Source Detection
 *===========================================================================*/

/**
 * @brief  Determine what caused the wakeup from Sleep mode
 * @return PM_WAKE_REASON_KEYSCAN, PM_WAKE_REASON_RTC, or PM_WAKE_REASON_NONE
 * @note   Must be called immediately after WFI returns, before any ISR clears flags.
 */
static uint8_t pm_detect_wake_reason(void)
{
    /* Check KEYSCAN EXTI line (LINE13) — key press */
    if (EXTI_GetITStatus(EXTI_LINE13) == SET) {
        EXTI_ClrITPendBit(EXTI_LINE13);
        PM_LOG("Wake reason: KEYSCAN (key press)");
        return PM_WAKE_REASON_KEYSCAN;
    }

    /* Check RTC Wakeup Timer flag */
    if (RTC_GetFlagStatus(RTC_FLAG_WTF) == SET) {
        RTC_ClrFlag(RTC_FLAG_WTF);
        EXTI_ClrITPendBit(EXTI_LINE9);
        PM_LOG("Wake reason: RTC timer");
        return PM_WAKE_REASON_RTC;
    }

    /* Unknown wake source (should not happen in normal operation) */
    PM_LOG("Wake reason: UNKNOWN");
    return PM_WAKE_REASON_NONE;
}

/*===========================================================================
 * 3. Peripherals Shutdown / Restore
 *===========================================================================*/

/**
 * @brief  Shutdown non-essential peripherals before entering Sleep mode
 * @note   LEDs, CAT1 UART (full), ADC, MICBIAS, IRC etc.
 *         KEYSCAN and RTC are kept for wakeup.
 */
static void pm_peripherals_sleep_shutdown(void)
{
    /* Turn off LEDs */
    CLRLED1;
    CLRLED2;

    /* Disable CAT1 UART completely (TX+RX) — we're going to sleep,
       no data reception needed during deep sleep. On wakeup, CAT1
       will be re-initialized by the BLE restart path (myBleStartAdv
       calls rc_cat1_init which re-inits everything). */
    USART2->CTRL1 &= (uint16_t)(~((uint16_t)USART_CTRL1_TXEN | (uint16_t)USART_CTRL1_RXEN));

    /* TODO (optional): also shutdown ADC, MICBIAS, IRC if they were enabled.
       For now, the main power savings come from BLE off + CPU sleep + UART off. */
}

/**
 * @brief  Restore peripherals after waking from Sleep mode
 * @note   KEYSCAN was kept active for wakeup, no need to reinit.
 *         CAT1 UART RX is re-enabled here; full CAT1 reinit happens
 *         when the main loop re-runs myBleStartAdv().
 */
static void pm_peripherals_sleep_restore(void)
{
    /* Re-enable CAT1 UART RX (TX remains off until standby exit or BLE restart) */
    USART2->CTRL1 |= USART_CTRL1_RXEN;
}

/*===========================================================================
 * 4. Public Entry Point
 *===========================================================================*/

/**
 * @brief  Enter Sleep mode
 * @return Wakeup reason (PM_WAKE_REASON_KEYSCAN or PM_WAKE_REASON_RTC)
 * @note   This function does NOT return until the CPU is woken by EXTI.
 *         After wakeup, it restores peripherals and returns the wake reason.
 *
 * Flow:
 *   1. Stop BLE advertising (saves power, prevents stale state)
 *   2. Reset bleAdvStat so main loop will restart BLE after wake
 *   3. Shutdown non-essential peripherals
 *   4. Configure RTC wakeup timer
 *   5. Enter Sleep mode via WFI
 *   6. ===== CPU sleeps here =====
 *   7. EXTI wakes CPU -> continue from WFI
 *   8. Detect wakeup source
 *   9. Disable RTC wakeup (no longer needed while awake)
 *  10. Restore peripherals
 *  11. Return wake reason to caller
 */
uint8_t pm_sleep_enter(void)
{
    uint8_t wake_reason;

    PM_LOG("Sleep enter: preparing...");

    /* 1. Stop BLE advertising if running */
    ble_adv_stop();

    /* 2. Reset bleAdvStat to force BLE restart on main loop after wake */
    bleAdvStat = 0U;

    /* 3. Shutdown non-essential peripherals */
    pm_peripherals_sleep_shutdown();

    /* 4. Configure RTC wakeup timer */
    pm_rtc_wakeup_config();

    PM_LOG("Sleep enter: entering Sleep mode (WFI)...");

    /*
     * 5. Enter Sleep mode via WFI.
     *    CPU stops here. Wakeup occurs when:
     *    - KEYSCAN detects a key press (EXTI_LINE13)
     *    - RTC wakeup timer expires (EXTI_LINE9)
     */
    PWR_EnterSLEEPMode(PWR_SLEEPENTRY_WFI);

    /*
     * ===== CPU resumes HERE after wakeup =====
     */

    PM_LOG("Sleep exit: CPU woken, detecting wake source...");

    /* 6. Detect what woke us */
    wake_reason = pm_detect_wake_reason();

    /* 7. Disable RTC wakeup timer (active only during sleep) */
    RTC_EnableWakeUp(DISABLE);

    /* 8. Restore peripherals needed for normal operation */
    pm_peripherals_sleep_restore();

    PM_LOG("Sleep exit: wake reason=%u, returning to main loop", (unsigned)wake_reason);
    return wake_reason;
}
