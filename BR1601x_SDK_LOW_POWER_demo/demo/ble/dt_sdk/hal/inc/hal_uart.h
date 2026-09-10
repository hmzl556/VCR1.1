/**
 * @file hal_uart.h
 * @author Barrot Firmware Team
 * @brief This file defines the operating interface of DATA USART AND LOG USART
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */
#ifndef __UART__H__
#define __UART__H__

/**
 * INCLUDE FILES
 *******************************************************************************
 */
 
#include "porting.h"

/**
 * DEFINE
 ****************************************************************************************
 */

/// The maximum amount of data in the sending channel is reserved
#define HAL_USART_TX_DMA_SIZE        128

/// Maximum data amount of the receiving channel
#define HAL_USART_RX_DMA_SIZE        1024

/// user data point to stack(use for hal_data_uart_send)
#define UDATA_PT_STACK               0

/**
 * TYPEDEF
 ****************************************************************************************
 */

/// Event type of the serial port
typedef enum {
    HAL_DATA_UART_EV_TX_CMP,            /*�������*/
    HAL_DATA_UART_EV_RX_IDLE,           /*���ڿ���*/
}hal_uart_event_t;

/// The type of the event callback function for the communication port
typedef void (*data_uart_evt_callback_t)(const hal_uart_event_t *ev);

/// Data receiving function type of the communication interface
typedef uint32_t (*data_uart_rcv_callback_t)(const uint8_t* data, uint32_t len);

/// Type of the initial structure of the communication serial port 
typedef struct{
    ///  Baud rate
    uint32_t baudrate; 
    /// < Specifies wether the hardware flow control mode is enabled or disabled.
    bool flow_ctrl; 
    /// Event callback  
    data_uart_evt_callback_t evt_callback;      
    /// Data receive callback 
    data_uart_rcv_callback_t data_rcv_callback; 
    
}hal_data_uart_init_t;

/// Baud rate variable type
typedef  enum{
     HAL_UART_BAUDRATE_115200 = 115200,
     HAL_UART_BAUDRATE_128000 = 128000,
     HAL_UART_BAUDRATE_230400 = 230400,
     HAL_UART_BAUDRATE_256000 = 256000,
     HAL_UART_BAUDRATE_460800 = 460800,
     HAL_UART_BAUDRATE_500000 = 500000,
     HAL_UART_BAUDRATE_512000 = 512000,
     HAL_UART_BAUDRATE_600000 = 600000,
     HAL_UART_BAUDRATE_750000 = 750000,
     HAL_UART_BAUDRATE_921600 = 921600,
     HAL_UART_BAUDRATE_1000000 = 1000000,
     HAL_UART_BAUDRATE_2000000 = 2000000,
}hal_baudrate_t;

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief The log usart is initialized based on the baud rate
 * 
 * @param baudrate hal_baudrate_t
 */
void hal_log_uart_init(const uint32_t baudrate);

/**
 * @brief nitialize the main communication serial port according to the parameters.
 * 
 * @param[in] init  Initializes the structure of the main communication serial port information
 * 
 * @return  bool  Execution result.
 *                  true indicates successful execution, 
 *                  false indicates failure.
 */
bool hal_data_uart_init(const hal_data_uart_init_t* init);

/** 
 * @brief  The communication USART is initialized
 * 
 * @return  bool  Execution result.
 *                  true indicates successful execution, 
 *                  false indicates failure.
 */
bool hal_data_uart_deinit(void);

/**
 * @brief   The receiving function of the passable communication USART was enabled
 * 
 * @param[in]   enalbe  ENABLE：Pull down flow control  
 *                      DISABLE：Pull up flow control
 */ 
void hal_data_uart_rcv_enable(bool enable);

/**
 * @brief An active communication USART sends data, possibly using DMA
 * 
 * @param[in] data  Buffer pointer.
 * @param[in] len   The length of the data to be sent, in bytes.
 * 
 * @return  The length, in bytes, of the data actually put into the buffer to be sent
 * @note    Calling the interface simply puts the data into the outgoing buffer. 
 *      The function return cannot be represented as data and the send is complete.
 *      This interface can be used only after @trans_uart_init is successfully executed.
 */
uint32_t  hal_data_uart_send(const uint8_t *p_data,uint32_t len); 

/**
 * @brief Update the baud rate of the communication USART
 * 
 * @param[in] baud  Baud rate to set.
 * @return  bool  Execution result.
 *              rue indicates successful execution, 
 *              false indicates failure.
 */
bool hal_data_uart_baud_update(uint32_t baud);

/**
 * @brief 更新串口流控
 * 
 * @param fc 流控使能
 * @return true 更新成功
 * @return false 更新失败
 */
bool hal_data_uart_fc_updata(uint32_t fc);

#endif
