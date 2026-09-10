/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Barrot's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY BARROT'S "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL BARROT'S BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file app_keyscan.c
 * @brief KEYSCAN application configuration.
 */
#include "app_keyscan.h"

#ifdef CFG_KEYSCAN
#include "br1601x.h"
#include "exti.h"
#include "gpio.h"
#include "keyscan.h"
#include "misc.h"
#include "rcc.h"

#define APP_KEYSCAN_DEFAULT_MASK        KEY_44
#define APP_KEYSCAN_DEFAULT_MODE        MODE_PRESS_TRIG
#define APP_KEYSCAN_DEFAULT_DTS         DTS_10MS
#define APP_KEYSCAN_DEFAULT_WTS         WTS_0MS
#define APP_KEYSCAN_IRQ_PRIORITY        3

#if defined(__CC_ARM) || defined(__ICCARM__)
#define APP_KEYSCAN_WEAK                __weak
#elif defined(__GNUC__)
#define APP_KEYSCAN_WEAK                __attribute__((weak))
#else
#define APP_KEYSCAN_WEAK
#endif

extern uint32_t ModuleIrqRegister(IRQn_Type irqn, void (*IRQ_HANNDLE_FUN)(void));
extern uint32_t ModuleIrqRemoval(IRQn_Type irqn);

static void app_keyscan_irq_handler(void);

static void app_keyscan_gpio_init(uint8_t key_mask)
{
    GPIO_InitType GPIO_InitStructure;

    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF5_KEYSCAN;

    if (key_mask == KEY_44)
    {
        GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6;
    }
    else
    {
        GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                               | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    }
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = 0;
    if ((key_mask == KEY_44) || (key_mask == KEY_65))
    {
        GPIO_InitStructure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    }
    else
    {
        GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2
                               | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    }
    GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);
}

static void app_keyscan_exti_init(void)
{
    EXTI_InitType EXTI_InitStructure;

    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Line    = EXTI_LINE13;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitPeripheral(&EXTI_InitStructure);
    EXTI_ClrITPendBit(EXTI_LINE13);

    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);
    EXTI_ClrITPendBit(EXTI_LINE13);
}

static void app_keyscan_nvic_init(void)
{
    NVIC_InitType NVIC_InitStructure;

    NVIC_DisableIRQ(KEYSCAN_IRQn);
    ModuleIrqRemoval(KEYSCAN_IRQn);
    ModuleIrqRegister(KEYSCAN_IRQn, app_keyscan_irq_handler);

    NVIC_InitStructure.NVIC_IRQChannel         = KEYSCAN_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = APP_KEYSCAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_EnableIRQ(KEYSCAN_IRQn);
}

void KEYSCAN_Configuration(uint8_t key_mask, uint8_t mode, uint8_t dts, uint8_t wts, bool int_en)
{
    KEYSCAN_InitType KEYSCAN_InitStructure;

    RCC_EnableLsi(ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC->LSCTRL |= RCC_LSCTRL_KEYSCANEN;

    app_keyscan_gpio_init(key_mask);

    KEYSCAN_InitStructure.Mask   = key_mask;
    KEYSCAN_InitStructure.Mode   = mode;
    KEYSCAN_InitStructure.Wts    = wts;
    KEYSCAN_InitStructure.Dts    = dts;
    KEYSCAN_InitStructure.Int_en = int_en ? INT_EN : INT_DIS;
    KEYSCAN_Init(&KEYSCAN_InitStructure);

    KEYSCAN_InfoClear();
    KEYSCAN_ClearInterrupt();
    EXTI_ClrITPendBit(EXTI_LINE13);

    if (int_en)
    {
        app_keyscan_exti_init();
        app_keyscan_nvic_init();
    }

    KEYSCAN_Enable(ENABLE);
}

void app_keyscan_init(void)
{
    KEYSCAN_Configuration(APP_KEYSCAN_DEFAULT_MASK,
                          APP_KEYSCAN_DEFAULT_MODE,
                          APP_KEYSCAN_DEFAULT_DTS,
                          APP_KEYSCAN_DEFAULT_WTS,
                          true);
}

void app_keyscan_prepare_sleep(void)
{
    KEYSCAN_InfoClear();
    KEYSCAN_ClearInterrupt();
    EXTI_ClrITPendBit(EXTI_LINE13);
}

APP_KEYSCAN_WEAK void app_keyscan_event(const uint32_t key_data[APP_KEYSCAN_DATA_WORDS])
{
    (void)key_data;
}

static void app_keyscan_irq_handler(void)
{
    uint32_t key_data[APP_KEYSCAN_DATA_WORDS];

    if (KEYSCAN_GetInterruptState() == SET)
    {
        KEYSCAN_ReadKeyData(key_data);
        KEYSCAN_ClearInterrupt();
        EXTI_ClrITPendBit(EXTI_LINE13);
        app_keyscan_event(key_data);
        KEYSCAN_InfoClear();
    }
}

void KEYSCAN_IRQHandler(void)
{
    app_keyscan_irq_handler();
}
#endif /* CFG_KEYSCAN */
