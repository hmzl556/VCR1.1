#ifndef IR_WAVE_STORE_H
#define IR_WAVE_STORE_H
#include <stdbool.h>
#include "ir_raw.h"
void ir_wave_store_load(void);
bool ir_wave_store_has(unsigned key);
bool ir_wave_store_frame(unsigned key,uint32_t *frame);
bool ir_wave_store_get(unsigned key,ir_raw_t *wave);
bool ir_wave_store_save(unsigned key,const ir_raw_t *wave);
bool ir_wave_store_clear(void);
#endif
