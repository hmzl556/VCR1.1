/**
 * @file rbuf_data.h
 * @author Barrot Firmware Team
 * @brief This file contains the interface definition of the communication module
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef _RBUF_DATA_H_
#define _RBUF_DATA_H_

/**
 * INCLUDE FILES
 *******************************************************************************
 */
#include "porting.h"

/**
 * DEFINE
 ****************************************************************************************
 */

/// Buffer size for sending and receiving serial ports
#define TX_BUFF_SIZE    4*1024
#define RX_BUFF_SIZE    2*1024
/// Invalid type of format 
#define  INVALID_TYPE	 0xFF	

/// Presuffix definitions of events
#define  AFFIX_TYPE_0    ""
#define  AFFIX_TYPE_1    "\r\n"
#define  AFFIX_TYPE_2    "\r\r\n"    
#define  AFFIX_TYPE_3    "*"
#define  AFFIX_TYPE_4    "+"
#define  AFFIX_TYPE_5    "-"
#define  AFFIX_TYPE_6    "\r\n["
#define  AFFIX_TYPE_7    "]\r\n"

/**
 * ENUM
 ****************************************************************************************
 */

/// Communication module event type
enum TRANS_EVENT{
    /// The serial port sends FIFO status updates, including empty or full status information, reserve
    TRANS_EV_SND_FIFO_STS_NTF, 
    /// FIFO overflow accepting serial port data, reserve
    TRANS_EV_RCV_FIFO_OVER,
    /// Serial port hardware RTS status update, reserve 
    TRANS_EV_RTS_STS_UD,       
    /// RTS status updates connected to the serial port, reserve
    TRANS_EV_RMT_RTS_UD,        
    /// hardware error, reserve
    TRANS_EV_HW_ERR,    
    /// Receive idle      
    TRANS_EV_RX_IDLE,
    /// The notification can continue writing data, reserve
    TRANS_EV_WRITE_NTF, 
    /// Notify to read data from the USART
    TRANS_EV_READ_NTF,
	/// Ready to send data
    TRANS_EV_SEND_READY,
    /// Send complete
    TRANS_EV_SEND_CMP,
};

/**
 * TYPEDEF
 ****************************************************************************************
 */

/// Type of communication event structure
typedef struct{
    uint8_t ev_type;    /* 事件类型，详细定义@see TRANS_EVENT*/
#if 0
    union{
        int fifo_sts;       /*TRANS_EV_SND_FIFO_STS_NTF事件参数*/
        data_t over_info;   /*TRANS_EV_RCV_FIFO_OVER参数*/
        bool rts_level;     /* TRANS_EV_RTS_STS_UPDATA 与 TRANS_EV_RMT_RTS_UPDATA参数
                                低电平表示可以发送数据，高电平表示不可发送数据 */
        int fw_err_code;    /* 硬件错误，保留暂未定义*/
    }params;
#endif
}trans_uart_ev_t;

/// Communication module event callback function definition
typedef void (*trans_uart_evt_callback_t)(const trans_uart_ev_t* evt);

/// Communication module accepts data callback function
typedef uint32_t (*trans_uart_rcv_callback_t)(const uint8_t* data, uint32_t len);

typedef uint32_t (*trans_cb_t)(const uint8_t* data, uint32_t len);
/// The communication module initializes the structure
typedef struct{
    /// Baud rate
    uint32_t baudrate; 
    /// Specifies wether the hardware flow control mode is enabled or disabled.
    bool flow_ctrl; 
    /// Silent mode 
    bool btmode;                        
    /// Event indicating prefix                                        
    uint8_t prefix_type;                   
    /// Event indicating suffix
    uint8_t suffix_type;           
    /// Event callback
    trans_uart_evt_callback_t evt_callback;         
    /// user data rev callback
    trans_uart_rcv_callback_t data_rcv_callback;
}trans_uart_init_t;

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */


/**
 * @brief Initialize the main communication serial port according to the parameters.
 * 
 * @param trans_t Initializes the structure of the main communication serial port information
 * @return bool  Execution result.
 *          true indicates successful execution, 
 *          false indicates failure.
 */
bool trans_uart_init(trans_uart_init_t *trans_t);

/**
 * @brief Deinitialize the main communication serial port.
 * 
 * @return bool  Execution result.
 *          true indicates successful execution, 
 *          false indicates failure.
 */
bool trans_uart_deinit(void);

/**
 * @brief Update the baud rate of the communication module.
 * 
 * @return bool  Execution result.
 *          true indicates successful execution, 
 *          false indicates failure.
 */
bool trans_uart_baud_update(uint32_t baud);

/**
 * @brief 更新通信模块的流控方式
 * 
 * @param fc 流控状态
 * @return true 更新成功
 * @return false 更新失败
 */
bool trans_uart_fc_update(uint32_t fc);

/**
 * @brief Updated formatted output information content.
 * 
 * @param btmd Updated silent mode enabled status
 * @param prefix Prefix type
 * @param suffix suffix type
 * @return bool  Execution result.
 *          true indicates successful execution, 
 *          false indicates failure.
 */
bool trans_uart_fmt_update(bool btmd, uint8_t prefix, uint8_t suffix);

/**
 * @brief Update the user callback function for serial port receiving data
 * 
 * @param callback The user callback function to set.
 * @return bool  Execution result.
 *          true indicates successful execution, 
 *          false indicates failure.
 */
bool trans_uart_cb_update(trans_uart_rcv_callback_t callback);


/**
 * @brief Use the communication module to output formatting information
 * 
 * @param fmt output format
 * @param arg parameter list 
 * @return uint32_t FALSH_MANAGE_ERRNumber of characters actually output to the serial port
 *          >0 : success
 *          =0 : failed
 */
uint32_t trans_uart_event_ind(const char* fmt, ...);

/**
 * @brief Use the communication module to send data within the specified buffer.
 * 
 * @param p_data Buffer pointer.
 * @param len The length of the data to be sent, in bytes.
 * @return uint32_t FALSH_MANAGE_ERRThe length, in bytes, of the data actually put into the buffer to be sent
 */
uint32_t trans_uart_data_send(const uint8_t *data, uint32_t len);

/**
 * @brief 串口发送使能
 * 
 * @param enable 使能状态
 */
void trans_uart_send_enable(bool enable);


/**
 * @brief Notification module on the packet processing completed
 *  Called by the upper-layer user
 */
void trans_uart_pkt_proc_done(void);

/**
 * @brief 检测通信模块是否正在发送
 */
bool trans_uart_sending_check(void);

/**
 * @brief 获取通信模块发送缓冲区已使用长度
 */
uint16_t trans_uart_txbuf_used(void);

/**
 * @brief 获取通信模块发送缓冲区的剩余可写长度
 */
uint16_t trans_uart_txbuf_unused(void);

/**
 * RESERVER
 ****************************************************************************************
 */

#if 0
/**
 * @brief   控制接受uart的数据回调是否继续，用于流量控制。当APP的fifo快要满的时候，应该通过该接口
 *          关闭接收数据，防止出现数据丢失的情况。
 * 
 * @param[in]   enalbe  false表示APP不再接受串口数据，数据接受的回调函数不应该再调用。true表示
 *                      APP有足够的内存可以继续接收串口数据，这时当有串口数据时，数据接受回调函
 *                      数应该被调用。
 */ 
void trans_uart_rcv_enable(bool enable);

/**
 * @brief Upper-layer users actively read data
 * 
 * @param data target pointer
 * @param len The length user want to read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t trans_uart_data_read(uint8_t *data, uint32_t len);

#endif

#endif  
