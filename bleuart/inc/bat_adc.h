#ifndef _BAT_ADC_H_
#define _BAT_ADC_H_
#include <stdint.h>

/* Both readers return the same cached percent, never access ADC.
 * Initial value is 100 until a complete valid sample set is available. */
uint8_t bat_get_level_percent(void);
uint8_t bat_get_cached_level(void);

/* Main BLE task only; one bounded ADC group per call, no blocking delays.
 * >=60s between valid updates; LTE-off/voice/IR/AT/OTA busy defer sampling. */
void bat_adc_poll(void);
/* Invalidate partial samples/settling window on LTE reset/power transition.
 * Keep the valid level, fractional EMA and update timestamp across sleep. */
void bat_adc_restart_window(void);
#endif
