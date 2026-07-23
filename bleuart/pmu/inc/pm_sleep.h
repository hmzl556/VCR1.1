/**
 * @file    pm_sleep.h
 * @brief   Sleep mode management API
 */

#ifndef _PM_SLEEP_H_
#define _PM_SLEEP_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wakeup reason codes */
#define PM_WAKE_REASON_NONE     0U  /* Undefined / not a wakeup event    */
#define PM_WAKE_REASON_KEYSCAN  1U  /* Woken by KEYSCAN EXTI (key press) */
#define PM_WAKE_REASON_RTC      2U  /* Woken by RTC wakeup timer         */


/**
 * @brief 由 pm_sleep.c 在进入 WFI 前设为 true，唤醒后 ISR 或主循环清除。
 *        KEYSCAN ISR 据此决定是否在按键瞬间亮 LED1。
 */
extern volatile bool g_pm_sleeping;

/**
 * @brief  Enter Sleep mode (CPU+BLE off, SRAM retained, KEYSCAN+RTC wake)
 * @note   Stops BLE, configures wakeup sources, then calls PWR_EnterSLEEPMode().
 *         On wakeup, returns the wake reason.
 * @return Wakeup reason (PM_WAKE_REASON_*)
 */
uint8_t pm_sleep_enter(void);

/**
 * @brief  Enter Sleep mode without touching BLE (for IWDG wake → direct sleep)
 * @note   Skips ble_adv_stop() / BLE restart logic. Only shuts down peripherals
 *         and enters sleep. Used when BLE stack is not initialized.
 */
uint8_t pm_sleep_enter_no_ble(void);

#ifdef __cplusplus
}
#endif

#endif /* _PM_SLEEP_H_ */
