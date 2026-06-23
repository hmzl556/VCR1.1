#ifndef _USERHARD_H_
#define _USERHARD_H_

#include "gpio.h"

#define SETLED1 GPIO_SetBits(GPIOB, GPIO_PIN_3)
#define CLRLED1 GPIO_ResetBits(GPIOB, GPIO_PIN_3)
#define TOGGLELED1 GPIO_TogglePin(GPIOB, GPIO_PIN_3)
#define SETLED2 GPIO_SetBits(GPIOB, GPIO_PIN_7)
#define CLRLED2 GPIO_ResetBits(GPIOB, GPIO_PIN_7)
#define TOGGLELED2 GPIO_TogglePin(GPIOB, GPIO_PIN_7)

void initialGpio(void);
void rstApplication(void);

#endif /* _USERHARD_H_ */
