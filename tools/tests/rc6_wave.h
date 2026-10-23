#ifndef RC6_WAVE_H
#define RC6_WAVE_H
#include <stdint.h>
/* 38kHz: 17 cycles = about 447us, close to RC6 nominal 444us. */
#define RC6_HALF_CYCLES 17u
unsigned rc6_build(uint32_t frame, uint32_t *words, unsigned capacity);
#endif
