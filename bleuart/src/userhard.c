#include "cfg_user.h"
#include "br1601x.h"
#include "gpio.h"

typedef void (*pFunction)(void);
static pFunction jump_To_Application;
static uint32_t JumpAddress;

// 初始化 GPIO，主要是 LED
void initialGpio(void)
{
	GPIO_InitType gpioSet;

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_AFIO, ENABLE);
	GPIO_DeInit(GPIOA);
	GPIO_DeInit(GPIOB);

	GPIO_InitStruct(&gpioSet);
	gpioSet.Pin = GPIO_PIN_3 + GPIO_PIN_7;
	gpioSet.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
	gpioSet.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitPeripheral(GPIOB, &gpioSet);
	GPIO_ResetBits(GPIOB, GPIO_PIN_3);
	GPIO_ResetBits(GPIOB, GPIO_PIN_7);
}

// 重置应用程序，跳转到指定地址执行
void rstApplication(void)
{
	JumpAddress = 0x0100017cUL;
	jump_To_Application = (pFunction)JumpAddress;
	jump_To_Application();
}
