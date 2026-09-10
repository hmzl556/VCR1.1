/**
 * @file hal_gpio.h
 * @author Barrot Firmware Team
 * @brief This file contains the gpio operating interface
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */
#ifndef __GPIO__H__
#define __GPIO__H__

/*
 * INCLUDE FILES
 *******************************************************************************
 */
#include <stdint.h>

/*
 * TYPEDEF
 ****************************************************************************************
 */

/// io direction and pull up and down
typedef enum{
    HAL_GPIO_PP_OUT = 0,     
    HAL_GPIO_OD_OUT,         
    HAL_GPIO_PU_IN,         
    HAL_GPIO_PD_IN,         
    HAL_GPIO_FLOAT,
}hal_gpio_direction_t;

/// Interrupt type
typedef enum{
    HAL_GPIO_INT_DISABLE = 0,      
    HAL_GPIO_INT_LEVEL_LOW,         
    HAL_GPIO_INT_LEVEL_HIGH,        
    HAL_GPIO_INT_RISING_EDGE,        
    HAL_GPIO_INT_FALLING_EDGE,        
    HAL_GPIO_INT_BOTH_EDGES,        
}hal_gpio_interrupt_t;

/// GPIO_pins_define Pin definition
typedef enum{
    HAL_GPIO_PIN_0 = 0, //PA0
    HAL_GPIO_PIN_1,//PA1
    HAL_GPIO_PIN_2,//PA2
    HAL_GPIO_PIN_3,//PA3
    HAL_GPIO_PIN_4,//PA4
    HAL_GPIO_PIN_5,//PA5
    HAL_GPIO_PIN_6,//PA6
    HAL_GPIO_PIN_7,//PB0
    HAL_GPIO_PIN_8,//PB1
    HAL_GPIO_PIN_9,//PB2
    HAL_GPIO_PIN_10,//PB3
    HAL_GPIO_PIN_11,//PB4
    HAL_GPIO_PIN_12,//PB5
    HAL_GPIO_PIN_13,//PB6
    HAL_GPIO_PIN_14,//PB7
    HAL_GPIO_PIN_15,//PB8
    HAL_GPIO_PIN_16,//PB9
    HAL_GPIO_PIN_17,//PB10
    HAL_GPIO_PIN_18,//PB11
    HAL_GPIO_PIN_19,//PB12
    HAL_GPIO_PIN_20,//PB13
    HAL_GPIO_PIN_END,
}hal_gpio_pin_t;

/// GPIO interrupt callback function type
typedef void (hal_gpio_callback_t)(void *arg);

/// gpio initializes the structure
typedef struct{
    hal_gpio_pin_t pin;                           
    hal_gpio_direction_t dir;                    
    hal_gpio_interrupt_t interrupt_mode;       
    void * arg;                                 
    hal_gpio_callback_t *interrupt_callback;    //todo ����ע�᣿
}hal_gpio_init_t;

/*
 * ENUM
 ****************************************************************************************
 */

/// Bit_SET and Bit_RESET enumeration
enum {
    BIT_RESET,
    BIT_SET
};

//Reserve for future use 
enum GPIO_OP_RESULT{
    GPIO_SUCCESS = 0,       
    GPIO_INVALID_HANDLE,   
    GPIO_DIR_ERR,          
    GPIO_UNSUPPOT_FEA,      
};    

/**
 * @brief Initializes the GPIOx peripheral according to the specified
 *         parameters in the gpio_init_struct.
 * 
 * @param gpio_init_struct hal_gpio_init_t pointer to a hal_gpio_init_t structure that
 *         contains the configuration information for the specified GPIO peripheral.
 */
void hal_gpio_init(hal_gpio_init_t *gpio_init_struct);

/**
 * @brief  Deinitializes the GPIOx peripheral registers to their default reset values.
 * @param pin specifies the port bit to be written.
 */
void hal_gpio_deinit(hal_gpio_pin_t pin);

/**
 * @brief  Reads the specified input port pin.
 * @param Pin specifies the port bit to read.
 * @return The input port pin value.
 */
uint8_t  hal_gpio_read_bit(hal_gpio_pin_t pin);
    
/**
 * @brief  Sets or clears the selected data port bit.
 * @param pin specifies the port bit to be written.
 * @param bit_cmd specifies the value to be written to the selected bit.
 *   This parameter can be one of the Bit_OperateType enum values:
 *     @arg BIT_RESET to clear the port pin
 *     @arg BIT_SET to set the port pin
 */
void  hal_gpio_write_bit(hal_gpio_pin_t pin, uint8_t bit_cmd);

/**
  * @brief  Toggles the specified GPIO pins.
  * @param pin Specifies the pins to be toggled.
  * @retval None
  */
void  hal_gpio_toggle_pin(hal_gpio_pin_t pin);

#endif
