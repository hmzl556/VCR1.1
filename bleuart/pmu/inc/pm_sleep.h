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

/**
 * @brief  Enter Sleep mode (CPU+BLE off, SRAM retained, KEYSCAN wake)
 * @note   Stops BLE, enters WFI. On KEYSCAN EXTI wakeup, returns the wake reason.
 * @return Wakeup reason (PM_WAKE_REASON_*)
 */
uint8_t pm_sleep_enter(void);

#ifdef __cplusplus
}
#endif

#endif /* _PM_SLEEP_H_ */
