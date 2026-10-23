/** @file ir_rc6.h @brief Nonblocking Gehua RC6 transmission on PB4, 36kHz. */
#ifndef IR_RC6_H
#define IR_RC6_H
#include <stdint.h>
#include <stdbool.h>
#include "ir_raw.h"

/* Call after irc_init(). TIM3_CH1 owns carrier during TX; TIM6 owns envelope.
 * Learning reconfigures TIM3 on its next start. Raw replay allocates a bounded buffer in task context; no busy waits. */
bool ir_rc6_send(uint32_t frame);
bool ir_rc6_send_wave(const ir_raw_t *wave, bool repeat);
bool ir_rc6_is_busy(void);
void ir_rc6_stop(void);
uint32_t ir_rc6_error_count(void);
#endif
