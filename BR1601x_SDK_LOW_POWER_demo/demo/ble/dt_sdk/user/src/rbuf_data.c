/**
 * @file rbuf_data.c
 * @author Barrot Firmware Team
 * @brief This file realizes the implementation of the communication module
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

/**
 * INCLUDE FILES
 *******************************************************************************
 */

#include "rbuf_data.h"
#include "rbuf.h"
#include "hal_uart.h"
#include "task.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/**
 * DEFINE
 *******************************************************************************
 */
/// Maximum length of a formatted string
#define EVENT_LEN_MAX      256

/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct{
    /// Send data buffer
    rbuf_t      tx_rbuf;
    /// Receive data buffer
    rbuf_t      rx_rbuf;
    /// Software flow control
    uint32_t    sw_fc;
}rbuf_data_t;

/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */

/// Array of prefix and suffix strings
const  char *affix[] = {
        AFFIX_TYPE_0,
        AFFIX_TYPE_1,
        AFFIX_TYPE_2,
        AFFIX_TYPE_3,
        AFFIX_TYPE_4,
        AFFIX_TYPE_5,
        AFFIX_TYPE_6,
        AFFIX_TYPE_7,       
};

/// Send ring buffer memory
FUNC_RAM static uint8_t tx_buf[TX_BUFF_SIZE];
/// rec ring buffer memory
FUNC_RAM static uint8_t rx_buf[RX_BUFF_SIZE];
/// Idle event callback
static void *evt_idle_handl;
/// Send a completion event callback
static void *evt_scmp_handl;
/// rbuf data env tag
static struct {
    /// The initialization structure variable of the communication module  
    trans_uart_init_t trans_init;
    /// ringbuf usart of the communication serial port
    rbuf_data_t rbuf_data;
    /// Last send length
    uint32_t last_send_len;
    /// The communication module is sending the flag
    bool trans_sending;
    /// The user is receiving the flag
    bool tran_recving;
    /// The module sends the task handle
    void *rbuf_tx_handl;
    /// The module receives the task handle
    void *rbuf_rx_handl; 
    /// Send data enable
    bool send_enable;
    /// Indicates whether the module is initialized
    bool init_flag;
}rbuf_data_env = {NULL};

trans_cb_t p_event_send = trans_uart_data_send;
trans_cb_t p_data_send = hal_data_uart_send;

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */

/**
 * @brief Event reporting user
 * 
 * @param ev_type Event type
 */
static void trans_user_event_cb(uint8_t ev_type)
{
    trans_uart_ev_t arg;
    arg.ev_type = ev_type;
    //Report events to users
    if(rbuf_data_env.trans_init.evt_callback)
        rbuf_data_env.trans_init.evt_callback(&arg);
}

static void trans_user_event_cb_idle(uint8_t ev, void *arg)
{
    trans_user_event_cb(TRANS_EV_RX_IDLE);
}
static void trans_user_event_cb_scmp(uint8_t ev, void *arg)
{
    trans_user_event_cb(TRANS_EV_SEND_CMP);
}

/**
 * @brief Continue sending to the serial port
 * 
 * @param ev    resever
 * @param arg   resever
 */
static void trans_uart_write_more(uint8_t ev, void *arg)
{
    //Start next transmission
    if(rbuf_data_env.rbuf_data.tx_rbuf.cb.on_write){
        trans_uart_init(&rbuf_data_env.trans_init);
        rbuf_data_env.rbuf_data.tx_rbuf.cb.on_write();
    }
}

/**
 * @brief Continue sending data to the user
 * 
 * @param ev  resever
 * @param arg resever
 */
static void trans_uart_read_more(uint8_t ev, void *arg)
{
    if(rbuf_data_env.trans_init.data_rcv_callback){
        #define READ_SIZE_MAX   512
        uint8_t read_data[READ_SIZE_MAX];
        uint32_t read_len = 0;
        _ENTER_CRITICAL();
        read_len = rbuf_used(&rbuf_data_env.rbuf_data.rx_rbuf);
        read_len = rbuf_get(&rbuf_data_env.rbuf_data.rx_rbuf, read_data, read_len > READ_SIZE_MAX ? READ_SIZE_MAX : read_len);
        rbuf_data_env.tran_recving = (bool)read_len;
        //If there is no data, the sending ends
        _LEAVE_CRITICAL();
        if(read_len == 0) {
            return ;
        }
        read_len = rbuf_data_env.trans_init.data_rcv_callback(read_data, read_len);
        rbuf_update_read_pointer(&rbuf_data_env.rbuf_data.rx_rbuf, read_len);
		//Read check flow control
        if (rbuf_data_env.trans_init.flow_ctrl) {
            if (rbuf_unused(&rbuf_data_env.rbuf_data.rx_rbuf) >= HAL_USART_RX_DMA_SIZE)
            {
                hal_data_uart_rcv_enable(ENABLE);
            }
        }
    }
}

/**
 * @brief Event processing callback reported by the serial port 
 *  to the communication module
 * 
 * @param ev Serial port event pointer
 */
static void trans_uart_event_cb(const hal_uart_event_t* ev)
{
    BR_LOG_DEBUG("%s, ev:%x\r\n",__FUNCTION__, *ev);
    
    switch(*ev){
        case HAL_DATA_UART_EV_TX_CMP:
            rbuf_data_env.trans_sending = false;
            //Update pointer for tx_rbuf->rd
            rbuf_update_read_pointer(&rbuf_data_env.rbuf_data.tx_rbuf,rbuf_data_env.last_send_len);
            //TRANS_EV_SEND_CMP
            task_set_active(evt_scmp_handl);
            break;
        case HAL_DATA_UART_EV_RX_IDLE:
            //Countdown to sleep
            task_set_active(evt_idle_handl);
            break;
        default:
            break;
    }
}

/**
 * @brief The serial port reports data to the 
 *  communication module callback function
 * 
 * @param data 
 * @param len 
 * @return uint32_t FALSH_MANAGE_ERR
 */
static uint32_t trans_uart_rcv_cb(const uint8_t *data ,uint32_t len)
{
    uint32_t write_len = 0;
    /*Must receive */ 
    do{
        write_len = rbuf_write(&rbuf_data_env.rbuf_data.rx_rbuf, data, len);
        if(write_len != len)
            BR_LOG_ERROR("The receive buffer writes to the last byte\r\n");
    }while(0);

    /*check whether the rxbuf is sufficient */
    if (rbuf_data_env.trans_init.flow_ctrl) {
        if (rbuf_unused(&rbuf_data_env.rbuf_data.rx_rbuf) < HAL_USART_RX_DMA_SIZE)
        {
            BR_LOG_DEBUG("%s,The space is less than %d bytes. Procedurer\n",__FUNCTION__,HAL_USART_RX_DMA_SIZE);
            hal_data_uart_rcv_enable(DISABLE);
        }
    }
    return write_len;
}

/**
 * @brief The callback function after the upper-layer user 
 * writes data to the communication module
 * 
 */
static void trans_uart_write_start(void)
{
    //Test whether the last transmission is complete
    if((rbuf_data_env.trans_sending == true) || (rbuf_data_env.send_enable == false)){
        return ;
    }
#if UDATA_PT_STACK
    //Gets the number of consecutive data
    uint32_t contibuous_len = rbuf_used(&rbuf_data_env.rbuf_data.tx_rbuf);
#else
    uint32_t contibuous_len = rbuf_continuous_readable(&rbuf_data_env.rbuf_data.tx_rbuf);
#endif

    if(contibuous_len > HAL_USART_TX_DMA_SIZE)
        contibuous_len = HAL_USART_TX_DMA_SIZE;

    if(contibuous_len != 0){
        // Update rbuf_data_env.trans_sending status
        rbuf_data_env.trans_sending = true; 
        //Records the length of the send
        rbuf_data_env.last_send_len = contibuous_len;
        //Start next send packet
#if UDATA_PT_STACK
        uint8_t data[HAL_USART_TX_DMA_SIZE] = {0};
        rbuf_get(&rbuf_data_env.rbuf_data.tx_rbuf, data, contibuous_len);
        if(hal_data_uart_send(data, contibuous_len) == true){
			BR_LOG_DEBUG_HEX(__func__, data, contibuous_len > 20 ? 20 : contibuous_len);
        }
        else{             
            //Serious error, log
            //diag_log("trans_uart_write_start","dma send error,len:%d\r\n",contibuous_len);
        }
#else
        if(hal_data_uart_send((uint8_t *)rbuf_data_env.rbuf_data.tx_rbuf.read, contibuous_len) == true){
            BR_LOG_DEBUG("%s, write len:%d\r\n", __func__, contibuous_len);
            //BR_LOG_DEBUG_HEX(__func__, rbuf_data_env.rbuf_data.tx_rbuf.read, ((contibuous_len > 20) ?  20 : contibuous_len));
        }
        else{             
            //Serious error, log
            //diag_log("trans_uart_write_start","dma send error,len:%d\r\n",contibuous_len);
        }
#endif
    }
    else{
    }
}

/**
 * @brief The callback function after the lower serial port 
 * writes data to the communication module
 * 
 */
static void trans_uart_read_start(void)
{
    //Restart the read task
    if(rbuf_data_env.tran_recving == 0){
        rbuf_data_env.tran_recving = 1;
        task_set_active(rbuf_data_env.rbuf_rx_handl);
    }
    task_set_active(evt_idle_handl);///fixme
    //trans_user_event_cb(TRANS_EV_READ_NTF);
}


/**
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
bool trans_uart_init(trans_uart_init_t *trans_t)
{
    bool ret = false;
    
    if(rbuf_data_env.init_flag == true){
        return true;
    }

    do {
        hal_data_uart_init_t data_init;
        
        data_init.baudrate = trans_t->baudrate;
        data_init.flow_ctrl = trans_t->flow_ctrl;
        data_init.data_rcv_callback = trans_uart_rcv_cb;
        data_init.evt_callback = trans_uart_event_cb;

        if(!hal_data_uart_init(&data_init))
            break;

        rbuf_data_env.trans_sending = false;
        rbuf_data_env.tran_recving = false;
        rbuf_data_env.init_flag = true;
		memcpy(&rbuf_data_env.trans_init, trans_t, sizeof(trans_uart_init_t));

		//Example Create an automatic serial port sending task but do not start it
        if(rbuf_data_env.rbuf_tx_handl == NULL && rbuf_data_env.rbuf_rx_handl == NULL){
			rbuf_init(&rbuf_data_env.rbuf_data.tx_rbuf,tx_buf,TX_BUFF_SIZE);
            rbuf_init(&rbuf_data_env.rbuf_data.rx_rbuf,rx_buf,RX_BUFF_SIZE);
            rbuf_wr_notify_reg(&rbuf_data_env.rbuf_data.tx_rbuf, trans_uart_write_start);
            rbuf_wr_notify_reg(&rbuf_data_env.rbuf_data.rx_rbuf, trans_uart_read_start);
            rbuf_data_env.rbuf_tx_handl = (void *)task_create(&trans_uart_write_more, NULL, 0, false, true);
            rbuf_data_env.rbuf_rx_handl = (void *)task_create(&trans_uart_read_more, NULL, 0, false, true);
            evt_idle_handl = (void *)task_create(&trans_user_event_cb_idle, NULL, 0, false, true);
            evt_scmp_handl = (void *)task_create(&trans_user_event_cb_scmp, NULL, 0, false, true);
        }

        ret = true;
    }while(0);

    return ret;
}

bool trans_uart_deinit(void)
{
    uint8_t ret = false;

    if(rbuf_data_env.init_flag == false){
        return true;
    }

    do{
        if(!hal_data_uart_deinit())
            break;
        
        rbuf_data_env.tran_recving = true;
        rbuf_data_env.trans_sending = true;
        rbuf_data_env.init_flag = false;

        ret = true;
    }while(0);
    
    return ret;
}

bool trans_uart_baud_update(uint32_t baud)
{
    rbuf_data_env.trans_init.baudrate = baud; 
    hal_data_uart_baud_update(baud);
    return true;
}

bool trans_uart_fc_update(uint32_t fc)
{
	rbuf_data_env.trans_init.flow_ctrl = fc;
	hal_data_uart_fc_updata(fc);
    return true;
}

bool trans_uart_fmt_update(bool btmd, uint8_t prefix, uint8_t suffix)
{
	if(btmd != INVALID_TYPE)
    	rbuf_data_env.trans_init.btmode = btmd;
	if(prefix != INVALID_TYPE)
    	rbuf_data_env.trans_init.prefix_type = prefix;
	if(suffix != INVALID_TYPE)
    	rbuf_data_env.trans_init.suffix_type = suffix;
    return true;
}

bool trans_uart_cb_update(trans_uart_rcv_callback_t callback)
{
    rbuf_data_env.trans_init.data_rcv_callback = callback;
    return true;
}


uint32_t  trans_uart_data_send(const uint8_t *data, uint32_t len) 
{
    //Gets the writable length for tx rbuf
    if (len <= rbuf_unused(&rbuf_data_env.rbuf_data.tx_rbuf)) {
        //Prepare before adding the ring buffer
        trans_user_event_cb(TRANS_EV_SEND_READY);
        //Write data to tx rbuf and try to send data
        if(len != rbuf_write(&rbuf_data_env.rbuf_data.tx_rbuf, data, len)){
            BR_LOG_ERROR("rbuf_write wrror\r\n");        
        }
    } else {
        BR_LOG_WARNING("tx ring buffer not enought\r\n");
        /// [TODO] WARNING!!! The Ring Buffer should not full!
        //send message with fifo not enough
        len = 0;
    }

	return len;
}

uint32_t trans_uart_event_ind(const char* fmt, ...)
{
    //check mode
    if(rbuf_data_env.trans_init.btmode == 1)
        return 0;

    uint16_t len = 0;
	uint8_t buf[EVENT_LEN_MAX];
	memset(buf, 0, EVENT_LEN_MAX);
    //Prefix setting
	memcpy(buf, affix[rbuf_data_env.trans_init.prefix_type], strlen(affix[rbuf_data_env.trans_init.prefix_type])); 
    len += strlen(affix[rbuf_data_env.trans_init.prefix_type]);
    va_list args;
	va_start(args, fmt);
	len += vsnprintf((char *)buf + len, EVENT_LEN_MAX, fmt, args); // Format output and cat fmt ...
	va_end(args);
    //Suffix setting
    memcpy(buf + len,affix[rbuf_data_env.trans_init.suffix_type],strlen(affix[rbuf_data_env.trans_init.suffix_type])); 
    len += strlen(affix[rbuf_data_env.trans_init.suffix_type]);
    //Fill data to tx rbuf

    if(trans_uart_data_send(buf, len) == false){
        return 0;
    }
    return len;
}

void trans_uart_send_enable(bool enable)
{
	rbuf_data_env.send_enable = enable;
	
	if(rbuf_data_env.send_enable == ENABLE)
		task_set_active(rbuf_data_env.rbuf_tx_handl); //Applies to asynchronous events only
}

void trans_uart_pkt_proc_done(void)
{
	task_set_active(rbuf_data_env.rbuf_rx_handl);
}

bool trans_uart_sending_check(void)
{
    return rbuf_data_env.trans_sending;
}

uint16_t trans_uart_txbuf_used(void)
{
    return rbuf_used(&rbuf_data_env.rbuf_data.tx_rbuf);
}

uint16_t trans_uart_txbuf_unused(void)
{
    return rbuf_unused(&rbuf_data_env.rbuf_data.tx_rbuf);
}
/**
 * RESERVER
 ****************************************************************************************
 */

#if 0
void trans_uart_rcv_enable(bool enable)
{
	s_rcv_enable = enable;

	if(s_rcv_enable == ENABLE)
		task_set_active(rbuf_data_env.rbuf_rx_handl); //Applies to asynchronous events only
}

uint32_t trans_uart_data_read(uint8_t *data, uint32_t len)
{
    //Read the data of rx rbuf
    uint32_t read_len = rbuf_read(&rbuf_data_env.rbuf_data.rx_rbuf, data, len);
    //Check flow control
    if (rbuf_data_env.trans_init.flow_ctrl) {
        if (rbuf_unused(&rbuf_data_env.rbuf_data.rx_rbuf) > HAL_USART_RX_DMA_SIZE)
        {
            hal_data_uart_rcv_enable(ENABLE);
        }
    }
    return read_len;
}

uint32_t trans_uart_get_data_len(void)
{
    //Check how much data rx buf has to read
    return rbuf_continuous_readable(&rbuf_data_env.rbuf_data.rx_rbuf);
}

#endif
