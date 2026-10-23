/* PB5 active-low raw carrier learning, hardware capture / task validation. */
#ifndef IR_LEARN_H
#define IR_LEARN_H
#include <stdint.h>
#include <stdbool.h>
#include "ir_raw.h"
typedef enum {IR_LEARN_EVT_DONE=0,IR_LEARN_EVT_FAILED,IR_LEARN_EVT_TIMEOUT,IR_LEARN_EVT_CANCELED} ir_learn_evt_t;
/* DONE exposes ir_learn_wave(); legacy user/cmd callback arguments are zero.
 * Buffer survives capture stop through deferred save. Discard after consuming.
 * No callback from IRQ; failed captures retry within the same 15s window. */
typedef void (*ir_learn_cb_t)(ir_learn_evt_t evt,uint8_t user,uint8_t cmd);
void ir_learn_init(ir_learn_cb_t cb);
bool ir_learn_start(void);
void ir_learn_stop(void);
bool ir_learn_is_active(void);
void ir_learn_process(void);
void ir_learn_set_target(uint8_t key);
uint8_t ir_learn_get_target(void);
const ir_raw_t *ir_learn_wave(void);
void ir_learn_discard(void);
#endif
