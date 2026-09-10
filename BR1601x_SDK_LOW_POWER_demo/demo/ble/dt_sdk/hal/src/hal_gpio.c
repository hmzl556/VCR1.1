/**
 * @file hal_gpio.c
 * @author Barrot Firmware Team
 * @brief This file implements the GPIO operation interface, and specific platform related
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "hal_gpio.h"
#include "br1601x.h"
#include "gpio.h"
#include "porting.h"
#include "btlist.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define  GPIOA_PIN_NUM  7
#define  GPIOB_PIN_NUM  14

#define  GPIO_PORT(pin)         (pin >= GPIOA_PIN_NUM) ? (GPIOB) : (GPIOA)
#define  GPIO_PIN(pin)          (1 << ((pin >= GPIOA_PIN_NUM) ? (pin - GPIOA_PIN_NUM) : (pin)))
#define  RCC_CLOCK(port)        (port == GPIOB) ? RCC_APB2_PERIPH_GPIOB : RCC_APB2_PERIPH_GPIOA
#define  HAL_GPIO_PIN(port,pin) (port == GPIOB) ? (pin + GPIOA_PIN_NUM) : (pin)

/// Execute a callback after an interrupt
#define  EXECUTE_CBK(pin)        io_cbk(pin)

/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */

/// io interrupt structure variable
static struct  {
    hal_gpio_interrupt_t interrupt_mode;
    hal_gpio_callback_t *func;
    void * arg;
}gpio_interrupt_cbk[HAL_GPIO_PIN_END]; 

void io_cbk(hal_gpio_pin_t pin)
{
    if(gpio_interrupt_cbk[pin].func != NULL)
        gpio_interrupt_cbk[pin].func(gpio_interrupt_cbk[pin].arg);
}

void hal_gpio_init(hal_gpio_init_t *gpio_init_struct)
{
    GPIO_InitType GPIO_InitStructure;
    EXTI_InitType EXTI_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    uint32_t pin = GPIO_PIN(gpio_init_struct->pin);
    GPIO_Module* port = GPIO_PORT(gpio_init_struct->pin);
    IRQn_Type  io_irqn = EXTI0_1_IRQn;
    uint32_t  exti_line = EXTI_LINE0;
    /* Enable the GPIO Clock */
    RCC_EnableAPB2PeriphClk(RCC_CLOCK(port), ENABLE);
   
    GPIO_InitStruct(&GPIO_InitStructure);
    /*Configure the GPIO pin as input floating*/
    if (pin <= GPIO_PIN_ALL)
    {
        GPIO_InitStructure.Pin       = pin;
        switch(gpio_init_struct->dir){
            case HAL_GPIO_PP_OUT: 
                GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
                break;
            case HAL_GPIO_OD_OUT:   
                GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_OD;
                break;
            case HAL_GPIO_PU_IN:  
                GPIO_InitStructure.GPIO_Pull = GPIO_PULL_UP;
                break;
            case HAL_GPIO_PD_IN: 
                GPIO_InitStructure.GPIO_Pull = GPIO_PULL_DOWN;
                break;
            case HAL_GPIO_FLOAT: 
                GPIO_InitStructure.GPIO_Mode = GPIO_MODE_ANALOG;
                break;    
            default:
                break;
        }
        GPIO_InitPeripheral(port, &GPIO_InitStructure);
    }
    
    if(gpio_init_struct->interrupt_mode)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_AFIO, ENABLE);
        //recode callback
        gpio_interrupt_cbk[gpio_init_struct->pin].arg = gpio_init_struct->arg;
        gpio_interrupt_cbk[gpio_init_struct->pin].func = gpio_init_struct->interrupt_callback;
        gpio_interrupt_cbk[gpio_init_struct->pin].interrupt_mode = gpio_init_struct->interrupt_mode;
    
        /*Configure key EXTI line*/
        if( gpio_init_struct->pin == HAL_GPIO_PIN_0 || gpio_init_struct->pin == HAL_GPIO_PIN_1 || gpio_init_struct->pin == HAL_GPIO_PIN_7 )
            exti_line = EXTI_LINE0;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_3 || gpio_init_struct->pin == HAL_GPIO_PIN_2 || gpio_init_struct->pin == HAL_GPIO_PIN_8)
            exti_line = EXTI_LINE1;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_9 || gpio_init_struct->pin == HAL_GPIO_PIN_6 )
            exti_line = EXTI_LINE2; 
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_5 || gpio_init_struct->pin == HAL_GPIO_PIN_10)
            exti_line = EXTI_LINE3;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_11 || gpio_init_struct->pin == HAL_GPIO_PIN_16)
            exti_line = EXTI_LINE4;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_12 || gpio_init_struct->pin == HAL_GPIO_PIN_17 || gpio_init_struct->pin == HAL_GPIO_PIN_18)
            exti_line = EXTI_LINE5;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_4 || gpio_init_struct->pin == HAL_GPIO_PIN_13 || gpio_init_struct->pin == HAL_GPIO_PIN_19)
            exti_line = EXTI_LINE6;
        else if(gpio_init_struct->pin == HAL_GPIO_PIN_14 || gpio_init_struct->pin == HAL_GPIO_PIN_15 || gpio_init_struct->pin == HAL_GPIO_PIN_20)
            exti_line = EXTI_LINE7;
        
        EXTI_InitStructure.EXTI_Line    = exti_line; //todo
        EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
        
        switch(gpio_init_struct->interrupt_mode)
        {    
            case HAL_GPIO_INT_LEVEL_HIGH:
            case HAL_GPIO_INT_RISING_EDGE:
                EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; // EXTI_Trigger_Rising;
                break;
            case HAL_GPIO_INT_LEVEL_LOW:
            case HAL_GPIO_INT_FALLING_EDGE:   
                EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
                break;
            case HAL_GPIO_INT_BOTH_EDGES:
                EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
                break;
            default:
                break;
        }
        /// First, disable the corresponding interrupt mask register to prevent accidental triggering. 
        EXTI_InitStructure.EXTI_LineCmd = DISABLE;
        EXTI_InitPeripheral(&EXTI_InitStructure);
        EXTI_ClrITPendBit(exti_line);
        /// Then configure the GPIO interrupt line. If configured first, the internal timing may accidentally trigger an interrupt once.
        if(port == GPIOA)
            GPIO_ConfigEXTILine(GPIOA_PORT_SOURCE, gpio_init_struct->pin);
        else 
            GPIO_ConfigEXTILine(GPIOB_PORT_SOURCE, gpio_init_struct->pin - GPIOA_PIN_NUM);
        /// Finally, re-enable the interrupt mask register.
        EXTI_InitStructure.EXTI_LineCmd = ENABLE;
        EXTI_InitPeripheral(&EXTI_InitStructure);
        EXTI_ClrITPendBit(exti_line);

        if( exti_line == EXTI_LINE0 || exti_line == EXTI_LINE1)
            io_irqn = EXTI0_1_IRQn;
        else if(exti_line == EXTI_LINE2 || exti_line == EXTI_LINE3)
            io_irqn = EXTI2_3_IRQn;
        else
            io_irqn = EXTI4_12_IRQn;
        /*Set key input interrupt priority*/
        NVIC_InitStructure.NVIC_IRQChannel                   = io_irqn;
        NVIC_InitStructure.NVIC_IRQChannelPriority           = 2;  //todo
        NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
}

void hal_gpio_deinit(hal_gpio_pin_t pin)
{   
    uint32_t  exti_line = EXTI_LINE0;
    /*Configure key EXTI line*/
    if( pin == HAL_GPIO_PIN_0 || pin == HAL_GPIO_PIN_1 || pin == HAL_GPIO_PIN_7 )
        exti_line = EXTI_LINE0;
    else if(pin == HAL_GPIO_PIN_3 || pin == HAL_GPIO_PIN_2 || pin == HAL_GPIO_PIN_8)
        exti_line = EXTI_LINE1;
    else if(pin == HAL_GPIO_PIN_9 || pin == HAL_GPIO_PIN_6 )
        exti_line = EXTI_LINE2; 
    else if(pin == HAL_GPIO_PIN_5 || pin == HAL_GPIO_PIN_10)
        exti_line = EXTI_LINE3;
    else if(pin == HAL_GPIO_PIN_11 || pin == HAL_GPIO_PIN_16)
        exti_line = EXTI_LINE4;
    else if(pin == HAL_GPIO_PIN_12 || pin == HAL_GPIO_PIN_17 || pin == HAL_GPIO_PIN_18)
        exti_line = EXTI_LINE5;
    else if(pin == HAL_GPIO_PIN_4 || pin == HAL_GPIO_PIN_13 || pin == HAL_GPIO_PIN_19)
        exti_line = EXTI_LINE6;
    else if(pin == HAL_GPIO_PIN_14 || pin == HAL_GPIO_PIN_15 || pin == HAL_GPIO_PIN_20)
        exti_line = EXTI_LINE7;

    if(gpio_interrupt_cbk[pin].interrupt_mode != HAL_GPIO_INT_DISABLE) {
        EXTI_InitType EXTI_InitStructure;
        EXTI_InitStructure.EXTI_Line    = exti_line; //todo
        EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
        EXTI_InitStructure.EXTI_LineCmd = DISABLE;
        EXTI_InitPeripheral(&EXTI_InitStructure);
        EXTI_ClrITPendBit(exti_line);
    }
    gpio_interrupt_cbk[pin].arg = NULL;
    gpio_interrupt_cbk[pin].func = NULL;
    gpio_interrupt_cbk[pin].interrupt_mode = HAL_GPIO_INT_DISABLE;

    GPIO_DeInitPin(GPIO_PORT(pin),GPIO_PIN(pin));
}

uint8_t hal_gpio_read_bit(hal_gpio_pin_t pin)
{
    return GPIO_ReadInputDataBit(GPIO_PORT(pin), GPIO_PIN(pin));
}

void hal_gpio_write_bit(hal_gpio_pin_t pin, uint8_t bit_cmd)
{
    GPIO_WriteBit(GPIO_PORT(pin), GPIO_PIN(pin), (Bit_OperateType)bit_cmd);
}

void hal_gpio_toggle_pin(hal_gpio_pin_t pin)
{
    GPIO_TogglePin(GPIO_PORT(pin), GPIO_PIN(pin));
}

void xEXTI0_1_IRQHandler(void)
{   
#ifdef i2616e_u1
    if (RESET != EXTI_GetITStatus(EXTI_LINE1)){
        EXECUTE_CBK(HAL_GPIO_PIN_3);
        EXTI_ClrITPendBit(EXTI_LINE1);
    }
#endif
}

void xEXTI2_3_IRQHandler(void)
{
#ifdef i2616e
    if (RESET != EXTI_GetITStatus(EXTI_LINE2)){
        EXECUTE_CBK(HAL_GPIO_PIN_9);
        EXTI_ClrITPendBit(EXTI_LINE2);
    }
    if (RESET != EXTI_GetITStatus(EXTI_LINE3)){
        EXECUTE_CBK(HAL_GPIO_PIN_10);
        EXTI_ClrITPendBit(EXTI_LINE3);
    }
#endif
}

void EXTI4_12_IRQn_callback(void)
{
#ifdef i2616e_u1
    if (RESET != EXTI_GetITStatus(EXTI_LINE5)){
        EXECUTE_CBK(HAL_GPIO_PIN_12);
        EXTI_ClrITPendBit(EXTI_LINE5);
    }    
#endif
}
