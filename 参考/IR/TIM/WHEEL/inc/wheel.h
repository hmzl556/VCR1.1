#ifndef _WHEEL_H_
#define _WHEEL_H_

#include "stdarg.h"
#include "stdint.h"
//#include "core_cm0.h"
#include "br1601x.h"
#include "gpio.h"
#include "misc.h"
#include "tim.h"

typedef void (*IRQ_HANNDLE_FUN) (void);

//extern uint32_t ModuleIrqRegister(IRQn_Type irqn,IRQ_HANNDLE_FUN fun);
//extern uint32_t ModuleIrqRemoval(IRQn_Type irqn);




int my_printf(const char *fmt, ...);
void app_wheel_Start(void);
void timer_100ms_to_get_speed(void);
















#endif







