/**
 * @file    pm_sleep.h
 * @brief   Sleep mode management API
 */

#ifndef _PM_SLEEP_H_
#define _PM_SLEEP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wakeup reason codes */
#define PM_WAKE_REASON_NONE     0U  /* Undefined / not a wakeup event    */
#define PM_WAKE_REASON_KEYSCAN  1U  /* Woken by KEYSCAN EXTI (key press) */
#define PM_WAKE_REASON_RTC      2U  /* Woken by RTC wakeup timer         */

/**
 * @brief  Enter Sleep mode (CPU+BLE off, SRAM retained, KEYSCAN+RTC wake)
 * @note   Stops BLE, configures wakeup sources, then calls PWR_EnterSLEEPMode().
 *         On wakeup, returns the wake reason.
 * @return Wakeup reason (PM_WAKE_REASON_*)
 */
uint8_t pm_sleep_enter(void);

#ifdef __cplusplus
}
#endif

#endif /* _PM_SLEEP_H_ */
