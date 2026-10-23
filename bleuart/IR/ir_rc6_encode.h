/** @file ir_rc6_encode.h @brief Gehua RC6-6A-32 encoding, without MCU dependencies. */
#ifndef IR_RC6_ENCODE_H
#define IR_RC6_ENCODE_H
#include <stdint.h>
#include <stdbool.h>

/* Compact segments: bit15=MARK, bits14:0=duration in microseconds.
 * The 36kHz carrier and 444us envelope timebase are independent. */
#define IR_RC6_SEGMENT_MARK 0x8000u
#define IR_RC6_SEGMENT_US   0x7FFFu
#define IR_RC6_MAX_SEGMENTS 80u
#define IR_RC6_HALF_US      444u

uint8_t ir_rc6_encode(uint32_t frame, uint16_t *segments, uint8_t capacity);
uint32_t ir_rc6_gehua_frame(uint8_t command, bool toggle);
bool ir_rc6_gehua_command(uint8_t physical_key, uint8_t *command);
#endif
