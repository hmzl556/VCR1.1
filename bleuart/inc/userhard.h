#ifndef _USERHARD_H_
#define _USERHARD_H_

#include "gpio.h"

#define SETLED1 GPIO_SetBits(GPIOB, GPIO_PIN_3)
#define CLRLED1 GPIO_ResetBits(GPIOB, GPIO_PIN_3)
#define TOGGLELED1 GPIO_TogglePin(GPIOB, GPIO_PIN_3)
#define SETLED2 GPIO_SetBits(GPIOB, GPIO_PIN_7)
#define CLRLED2 GPIO_ResetBits(GPIOB, GPIO_PIN_7)
#define TOGGLELED2 GPIO_TogglePin(GPIOB, GPIO_PIN_7)

// PB2 LTE_EN
#define LTE_EN_ON()  GPIO_SetBits(GPIOB, GPIO_PIN_2)
#define LTE_EN_OFF() GPIO_ResetBits(GPIOB, GPIO_PIN_2)
#define LTE_EN_IS_ON() (GPIO_ReadOutputDataBit(GPIOB, GPIO_PIN_2) != Bit_RESET)

void initialGpio(void);
void rstApplication(void);

#endif /* _USERHARD_H_ */
