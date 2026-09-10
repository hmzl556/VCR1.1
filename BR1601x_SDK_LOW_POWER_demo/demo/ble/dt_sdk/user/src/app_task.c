/**
 * @file app_task.c
 * @author Barrot Firmware Team
 * @brief This file contains the implementation of the main business logic
 * @version 0.1
 * @date 2023-09-19
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */


/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "gatt.h"
#include "app_ble.h"
#include "task.h"
#include "rbuf_data.h"
#include "porting.h"
#include "prf_dtss.h"
#include "prf_dtsc.h"
#include "prf_cmd.h"
#include "prf_dfu.h"
#include "cfg.h"
#include "flash_manage.h"
#include "btlist.h"
#include "app_gpio.h"
#include "app_keyscan.h"
#include "app_task.h"
#include "app_whitelist.h"
#include "ble_adv.h"
#include "ble_scan.h"
#include "ble_sec.h"
#include "app_image_service.h"
#include "app_image_at.h"
#include "dfu_execute.h"
#include "app_label_adv_runtime.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define RBUF_TX_SPEED_MODE      		1
#define CONHDL_DEFAULT					GAP_INVALID_CONHDL
#define SYS_STARTUP_TIME        		2500 /// ms
#define BLE_CREATE_CONN_TIMEOUT 		5000
#define SEND_DATA_TIMEOUT				500
#define ENABLE_DEFAULT          		0xFF
#define ADVIO_BLINK_PERIOD      		1000
#define MULTI_CREDIT            		2
#define SINGLE_CREDIT           		6
/// 默认mtu
#define DEFAULT_MTU_MAX         		(512 + 18)

/// event enum
#define IM_READY                		"IM_READY"
#define IM_CONN                 		"IM_CONN"
#define IM_DISCONN              		"IM_DISC"
#define IM_CONN_TIMEOUT         		"IM_CTO"
#define IM_NO_SERVICE           		"IM_NOSVC"
#define IM_AUTH                 		"IM_AUTH"
#define IM_DISPALY_PASSKEY      		"IM_DISPPSK"
#define IM_PAIR                 		"IM_PAIR"
#define IM_PASSKEY_ENTRY        		"IM_PSKENTRY"
#define IM_NUMERIC_COMP                 "IM_NUMERIC"
    
#define USER_UART_EVENT_IND             trans_uart_event_ind
#define USER_UART_DATA_SEND				trans_uart_data_send

enum CMD_EVT_ERR{
    CMD_EVT_ERR_NONE = 0,
    CMD_EVT_ERR_NOT_ALLOW = CMD_UNMATCH + 1,
    CMD_EVT_ERR_PARAM,
};

enum SMP_STATE{
    SMP_NONE,
    SMP_PAIRING,
    SMP_PAIRING_RESPONSE,
    SMP_PASSKEY_RESPONSE,
    SMP_NUMERIC_RESPONSE,
    SMP_ENCRYPT_RSP,
    SMP_PAIR_SUCCESS,
};

enum BLE_SLEEP_LOCK_BIT{
    BLE_SLEEP_LOCK_MCU = 0,
    BLE_SLEEP_LOCK_IO,
    BLE_SLEEP_LOCK_TIM,
    BLE_SLEEP_LOCK_DTM,
    BLE_SLEEP_LOCK_RBUF,
};
/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct {
    conn_record_t *conn;
    uint32_t (*rsp_func)(uint16_t, const uint8_t *, uint16_t);
}atcmd_record_t;

/**
 * GLOBAL FUNCTIONS DECLARATIONS
 ****************************************************************************************
 */
extern uint32_t ModuleIrqRegister(IRQn_Type irqn, void (*IRQ_HANNDLE_FUN)(void));
extern uint32_t ModuleIrqRemoval(IRQn_Type irqn);
extern void xEXTI0_1_IRQHandler(void);
extern void xEXTI2_3_IRQHandler(void);
extern void EXTI4_12_IRQn_callback(void);

/**
 * LOCAL FUNCTIONS DECLARATIONS
 ****************************************************************************************
 */
static void user_rbuf_event_cb(const trans_uart_ev_t* evt);
static void user_send_data_timeout_task(uint8_t ev, void *arg);
static void user_ble_adv_scan_update_task_add(uint32_t ms, uint8_t adv_enable, uint8_t scan_enable);
static void user_conn_param_diag_timeout(uint8_t ev, void *arg);
static uint32_t user_uart_rcv_data(const uint8_t *buf,uint32_t len);
static conn_record_t * user_device_find_by_conhdl(uint16_t conhdl);
static conn_record_t * user_device_find_by_bd(uint8_t addr_type, uint8_t *addr);

/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */
trans_uart_init_t trans_init = {    
    .baudrate = 115200,
    .flow_ctrl = 1,
    .evt_callback = user_rbuf_event_cb,
    .data_rcv_callback = cmd_parse,
    .suffix_type = 1,
    .prefix_type = 1,
};

static bool ble_sleep_done = false;
static bool s_conn_param_update_done = false;
/// Connection timeout handle
static tid_t s_create_tid = NULL;
/// Broadcast enable control
static uint8_t s_adv_enable = true;
/// Scan correlation parameter
static uint8_t s_scan_enable = false;
static cmd_scankw_t s_scan_kw = {0};
#ifdef CFG_BLE_FC 
//Bluetooth fluidic task handle
static task_handler_t * s_credit_task_id = NULL;
#endif
/// Send data timeout handle
static tid_t s_send_timeout_id = NULL;
/// Destination connection address
static cmd_conn_req_t  s_target_addr = {0};
/// Record the current AT command from which device and which channel.
static atcmd_record_t s_atcmd;

#ifdef CFG_ADV_LED
///Adv LED timer id
static tid_t s_advio_id = NULL;
/// Adv LED Flickering period
static advled_t s_advled_par = {
    .enable = true,
    .period = ADVIO_BLINK_PERIOD,
};
#endif
/**
 * GLOBAL VARIABLES DECLARATION
 ****************************************************************************************
 */
uint32_t ble_sleep_lock_mask = 0;
struct ListStru g_conn_list = {NULL, NULL};

uint32_t (*p_user_uart_rcv_data)(const uint8_t *buf, uint32_t len) = user_uart_rcv_data;
uint32_t (*p_user_uart_cmd_parse)(const uint8_t *buf, uint32_t len) = cmd_parse;

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
#ifdef CFG_BLE_FC
static uint32_t user_send_credit(uint16_t conhdl, uint8_t credit)
{
	uint32_t ret = BLE_ERR_NONE;
	conn_record_t *node = user_device_find_by_conhdl(conhdl);
    uint8_t _credit[2] = {1, 0};
    _credit[1] = credit;
    if(node->role == MASTER_ROLE){
        ret = prf_dtsc_send_fc(conhdl, _credit, sizeof(_credit));
    }
    else {   
        ret = prf_dtss_send_fc(conhdl, _credit, sizeof(_credit));
    }
    return ret;
}

static uint8_t user_device_all_credit_get(void)
{
    uint8_t credit_resver = 0;
    conn_record_t *node = g_conn_list.head;

    while(node){
        credit_resver += node->credit;
        node = LNEXT(node);
    }
    return credit_resver;
}

static void user_credit_send_task(uint8_t ev, void *arg)
{
    conn_record_t *node = g_conn_list.head;
    conn_record_t *node_next = node;
	/// Buffer space
    int8_t txbuf_ptk_n = trans_uart_txbuf_unused() / DEFAULT_MTU_MAX - user_device_all_credit_get();
    
    while(node_next) {
        uint8_t send_credit = 0;
        node = node_next;
        node_next = LNEXT(node_next);
        /// The available space is 0
        if(txbuf_ptk_n <= 0)
            break;
        /// No flow control or no delivery completed
        if(node->fc_cmp == false)
            continue;
		/// credit consumption is less than half
        if(node->credit > node->credit_max / 2)
            continue;
        /// Calculate the available credit
        send_credit = txbuf_ptk_n;           
        if(send_credit > node->credit_max - node->credit)
            send_credit = node->credit_max - node->credit;
        node->fc_cmp = false;
        if(user_send_credit(node->conhdl, send_credit) == BLE_ERR_NONE) {
            node->credit += send_credit;
            txbuf_ptk_n -= send_credit;
        }
    }
}

static void user_data_flow_control(uint16_t conhdl)
{
    int8_t txbuf_ptk_n = 0;
    conn_record_t *node = user_device_find_by_conhdl(conhdl);
    do{
        if(node == NULL || node->fc_cmp == false)
            break;
        if(node->credit > node->credit_max / 2)
            break;
        /// the number of distributable credits
        txbuf_ptk_n = trans_uart_txbuf_unused() / DEFAULT_MTU_MAX - user_device_all_credit_get();
        
        if(txbuf_ptk_n <= 0)
            break; 
        
        if(txbuf_ptk_n > node->credit_max - node->credit)
            txbuf_ptk_n = node->credit_max - node->credit;
		node->fc_cmp = false;
        if(user_send_credit(node->conhdl, txbuf_ptk_n) == BLE_ERR_NONE) {
            node->credit += txbuf_ptk_n;
        }
    }while(0);
}

#if 0
static uint32_t user_send_mtu(uint16_t conhdl,uint16_t mtu)
{
    uint32_t ret = BLE_ERR_NONE;
    conn_record_t *node = user_device_find_by_conhdl(conhdl);
    uint8_t mtu_nfy[3] = {2,0,0};
    mtu_nfy[1] = mtu & 0xff;
    mtu_nfy[2] = mtu >> 8;
    if(node->role == MASTER_ROLE){
        ret = prf_dtsc_send_fc(conhdl, mtu_nfy, sizeof(mtu_nfy)); ///fixme
    }
    else {
        ret = prf_dtss_send_fc(conhdl, mtu_nfy, sizeof(mtu_nfy));
    }
    return ret;
}
#endif

#endif

#ifdef CFG_ADV_LED
static void user_ble_advio_blink_task(void)
{
    TIMER_CREATE_ONESHOT(&s_advio_id, s_advled_par.period/2, NULL, (task_func *)user_ble_advio_blink_task);
    LED_ADV_IND_BLINK();
}
#endif

static void user_ble_advio_blink_task_enable(bool enable)
{   
#ifdef CFG_ADV_LED
    if(s_advled_par.enable) {
        ble_adv_io_init();
        if((!enable) || List_Count(&g_conn_list) != 0)  {
            timer_delete(&s_advio_id);
            if(List_Count(&g_conn_list)) {
                LED_ADV_IND_OFF();
            }
            else {
                LED_ADV_IND_ON();
            }
        }
        else if(enable && (!s_advio_id)){
            TIMER_CREATE_ONESHOT(&s_advio_id, s_advled_par.period/2, NULL, (task_func *)user_ble_advio_blink_task);
        }
    }
    else {
        timer_delete(&s_advio_id);
        ble_adv_io_deinit();
    }
#endif
}

static void user_device_record_add(app_ble_gap_connection_ind_t *conn)
{
    conn_record_t *node = List_NodeNew(sizeof(conn_record_t));
    if(node != NULL){
        List_AddTail(&g_conn_list, node);
        memset(node, 0, sizeof(conn_record_t));
        node->conhdl = conn->conhdl;
        node->role = conn->role;
        node->mtu = conn->mtu;
        node->addr_type = conn->peer_addr_type;
        node->rcv_credit = 0xFF;
        memcpy(node->addr, conn->peer_addr.addr, BD_ADDR_LEN);
    }
    else{
        BR_LOG_WARNING("%s failed\r\n", __func__);
    }
}

static void user_device_record_del(uint16_t conhdl)
{
    conn_record_t *node = g_conn_list.head;
    
    while(node){
        if(node->conhdl == conhdl){
           List_RemoveAt(&g_conn_list, node);
           LFREE(node);
           break;
        }
        node = LNEXT(node);
    }  
}

static conn_record_t * user_device_find_by_conhdl(uint16_t conhdl)
{
    conn_record_t *node = g_conn_list.head;
    
    while(node){
        if(node->conhdl == conhdl){
           break;
        }
        node = LNEXT(node);
    }
    return node;
}

static conn_record_t * user_device_find_by_bd(uint8_t addr_type, uint8_t *addr)
{
    conn_record_t *node = g_conn_list.head;
    
    while(node){
        if((node->addr_type == addr_type && !memcmp(node->addr, addr, BD_ADDR_LEN)) || 
           (node->public_bd.addr_type == addr_type && !memcmp(node->public_bd.addr, addr, BD_ADDR_LEN))){
           break;
        }
        node = LNEXT(node);
    }
    return node;
}

static uint8_t user_get_slave_connection_num(void)
{
    uint8_t num = 0;
    conn_record_t *node = g_conn_list.head;

    while(node){
        num += (node->role == ROLE_SLAVE);
        node = LNEXT(node);
    }
    return num;
}

static uint8_t user_get_master_connection_num(void)
{
    uint8_t num = 0;
    conn_record_t *node = g_conn_list.head;

    while(node){
        num += (node->role == ROLE_MASTER);
        node = LNEXT(node);
    }
    return num;
}

/**
 * @brief 按照pdu格式发送指定缓冲区内的数据
 * 
 * @param handle ble连接句柄
 * @param data 缓冲区指针
 * @param len  需要发送的数据长度，单位为字节
 * @return uint32_t 实际放入待发送缓冲区数据的长度，单位为字节
 */
static uint32_t user_uart_pdu_send(uint16_t handle,const uint8_t *data, uint32_t len)
{   
    /// pdu格式  : \r\r\nDATA:<4bytes>,<3bytes>,<data>\r\r\n
#define PDU_PREFIX		"\r\nDATA:"
#define PDU_SUFFIX		"\r\n"
    /// <4bytes>,<3bytes>,
#define PDU_OTHER_LEN 	9 

    uint32_t total_len = len + strlen(PDU_PREFIX) + strlen(PDU_SUFFIX) + PDU_OTHER_LEN;
    uint16_t index = 0;
    uint8_t  *pdu = NEW(total_len);

    if(!pdu)
        return 0;
    index += sprintf((char *)&pdu[index], "\r\nDATA:%04u,%03u,", handle, len);
    memcpy(&pdu[index], data, len);
    index += len;
    memcpy(&pdu[index], PDU_SUFFIX, strlen(PDU_SUFFIX));
    index += strlen(PDU_SUFFIX);
    
    if(USER_UART_DATA_SEND(pdu, index) == false){
        index = 0;
    }
    FREE(pdu);
    return index;
}

/**
 * @brief 非pdu模式下接收串口数据
 * 
 * @param buf 缓冲区指针
 * @param len 数据长度
 * @return uint32_t 
 */
static uint32_t user_uart_rcv_data(const uint8_t *buf,uint32_t len)
{
    uint32_t  ble_send_len = 0;
    conn_record_t *node = g_conn_list.head;
    
    if(node != NULL && node->ccc_enable != 0 && node->rcv_credit != 0 && ble_is_data_sendable(node->conhdl)){
        ble_send_len = co_min((node->mtu - ATT_HEAD_SIZE), len);
        if(ble_send_data(node->conhdl, (uint8_t *)buf, ble_send_len) == BLE_ERR_NONE) {
            if(node->rcv_credit != 0xFF)  node->rcv_credit--;
            TIMER_CREATE_ONESHOT(&s_send_timeout_id, SEND_DATA_TIMEOUT, NULL, user_send_data_timeout_task);
        }
        else {
            user_send_data_timeout_task(NULL, NULL);
        } 
        
    }
    else {
		TIMER_CREATE_ONESHOT(&s_send_timeout_id, 10, NULL, user_send_data_timeout_task);
    }
    return ble_send_len;
}

/**
 * @brief 超时自动取消连接任务
 * 
 * @param ev 事件类型
 * @param arg 任务参数
 */
static void user_connect_cancel_task(uint8_t ev, void *arg)
{
    if(arg != NULL){
        ble_conn_req_t *conn = (ble_conn_req_t *)arg;
        if(user_device_find_by_bd(conn->addr_type, conn->addr) == NULL){
            ble_gap_connect_cancel();
            USER_UART_EVENT_IND(IM_CONN_TIMEOUT);
        }
        user_ble_adv_scan_update_task_add(10, s_adv_enable, ENABLE_DEFAULT);
    }
}

/**
 * @brief 更新广播和扫描行为
 * 
 * @param adv_enable 广播使能
 * @param scan_enable 扫描使能
 */
static void user_ble_adv_scan_update_task(uint8_t ev, void *arg)
{
    if(arg == NULL) return;
    
    uint8_t adv_enable = *(uint8_t*)arg;  
    uint8_t scan_enable = *((uint8_t*)arg + 1);
    uint8_t slave_num =  user_get_slave_connection_num();
    uint8_t master_num =  user_get_master_connection_num();
    uint8_t conn_num = slave_num + master_num;
    FREE(arg);
    
    /// 广播动作更新
    do{
        if(adv_enable == ENABLE_DEFAULT)
            break;
        
        if(BLE_MODE() == BLE_MODE_M)
            break;

        if(BLE_MODE() == BLE_MODE_S){
            if ((slave_num == BLE_SLAVE_CONN && BLE_MULTI_GET()) ||
                ((!BLE_MULTI_GET()) && slave_num != 0)){
                adv_enable = DISABLE; 
            }
        }
        /// 主从一体
        if(BLE_MODE() == BLE_MODE_MS){
            if (((slave_num != 0) && BLE_MULTI_GET()) 
                || ((!BLE_MULTI_GET()) && (conn_num != 0))){
                adv_enable = DISABLE;
            }
        }
        ble_gap_adv_enable(adv_enable);
        user_ble_advio_blink_task_enable(adv_enable);
    }while(0);
    /// 扫描动作更新
    do{
        if(scan_enable == ENABLE_DEFAULT)
            break;
        
        if(BLE_MODE() == BLE_MODE_S)
            break;

        if(BLE_MODE() == BLE_MODE_M){
            if ((master_num == BLE_MASTER_CONN && BLE_MULTI_GET()) ||
                ((!BLE_MULTI_GET()) && master_num != 0)){
                scan_enable = DISABLE;
            }
        }
        if(BLE_MODE() == BLE_MODE_MS){
            if (((master_num != 0) && BLE_MULTI_GET()) 
                || ((!BLE_MULTI_GET()) && (conn_num != 0))){
                scan_enable = DISABLE;
            }
        }
        
        ble_gap_scan_enable(scan_enable);
    }while(0);
}

static void user_ble_adv_scan_update_task_add(uint32_t ms, uint8_t adv_enable, uint8_t scan_enable)
{

    uint8_t *arg = NULL;

    if((arg = NEW(sizeof(uint8_t) * 2)) != NULL){
        *arg = adv_enable;
        *(arg + 1) = scan_enable;
        if(ms == 0) 
            user_ble_adv_scan_update_task(NULL, arg);
        else
            TIMER_CREATE(ms, arg, (task_func *)user_ble_adv_scan_update_task);
    }
}

static void user_ble_create_conn_task(uint8_t ev, void *arg)
{
    if(arg){
        ble_gap_connect((ble_conn_req_t *)arg);
        TIMER_CREATE_ONESHOT(&s_create_tid, BLE_CREATE_CONN_TIMEOUT, arg, (task_func *)user_connect_cancel_task);
   }
}

static void user_ble_update_baud_task(uint8_t ev, void *arg)
{
    if(arg){
        trans_uart_init_t *uart = (trans_uart_init_t *)arg;
        trans_uart_baud_update(uart->baudrate);
    } 
}

static void user_ble_update_fc_task(uint8_t ev, void *arg)
{
    if(arg){
        trans_uart_init_t *uart = (trans_uart_init_t *)arg;
        trans_uart_fc_update(uart->flow_ctrl);
    } 
}

static void user_ble_update_silent_task(uint8_t ev, void *arg)
{
    if(arg){
        trans_uart_init_t *uart = (trans_uart_init_t *)arg;
        trans_uart_fmt_update(uart->btmode, INVALID_TYPE, INVALID_TYPE);
    } 
}


static void user_send_data_timeout_task(uint8_t ev, void *arg)
{
    trans_uart_pkt_proc_done();
}

static void user_ble_test_freq_task(uint8_t ev, void *arg)
{
    if(arg){
        cmd_test_freq_t *freq = (cmd_test_freq_t *)arg;
        br_freq_fix_start(freq->ch);
        TIMER_CREATE(freq->time, NULL, (task_func *)br_freq_fix_end);
        FREE(arg);
    }
}

void user_system_reset_task(void)
{
    NVIC_SystemReset();
}

uint32_t user_atcmd_rsp_ind(const char* fmt, ...)
{
    uint32_t len = 0;
	uint8_t buf[256];
    //Prefix setting
    buf[len++] = 0x0D;
    buf[len++] = 0x0A;
    va_list args;
	va_start(args, fmt);
	len += vsnprintf((char *)buf + len, 256, fmt, args); // Format output and cat fmt ...
	va_end(args);
    //Suffix setting
    buf[len++] = 0x0D;
    buf[len++] = 0x0A;
    uint8_t *p = buf;
    while(len) {
        //Fill data to master
        uint8_t send_len = co_min(len, s_atcmd.conn->mtu - ATT_HEAD_SIZE);
        s_atcmd.rsp_func(s_atcmd.conn->conhdl, (const uint8_t *)p, send_len);
        p += send_len;
        len -= send_len;
    }
    return len;
}

uint32_t user_dtm_parse(const uint8_t* buf, uint32_t len)
{
enum {
    DTM_STATE_WAIT_HDR,
    DTM_STATE_WAIT_OPCODE,
    DTM_STATE_WAIT_LEN,
    DTM_STATE_WAIT_PAYLOAD,
    DTM_STATE_WAIT_MATCH,  
};
#define HCI_CMD_TYPE            0x01
#define CHECK_READ(_read_len)\
    if(len>=((_read_len)+use_len)){\
        memcpy(dtm_rx_buf+idx, buf + use_len, (_read_len));\
        idx += (_read_len);\
        use_len += (_read_len);\
    }\
    else {\
        break;\
    }

    static uint8_t rx_state = DTM_STATE_WAIT_HDR;
    static uint8_t dtm_rx_buf[64] = {0}; 
    static uint32_t idx = 0;
    uint32_t use_len = 0;
    do{ 
        if(rx_state == DTM_STATE_WAIT_HDR){
            CHECK_READ(1);
            if(dtm_rx_buf[0] != HCI_CMD_TYPE) {
                idx = 0;
                continue;
            }
            else
                rx_state = DTM_STATE_WAIT_OPCODE;
        }
        if(rx_state == DTM_STATE_WAIT_OPCODE){
            CHECK_READ(2);
            rx_state = DTM_STATE_WAIT_LEN;  
        }
        if(rx_state == DTM_STATE_WAIT_LEN){
            CHECK_READ(1);
            rx_state = DTM_STATE_WAIT_PAYLOAD;  
        }
        if(rx_state == DTM_STATE_WAIT_PAYLOAD){
            CHECK_READ(dtm_rx_buf[3]);
            rx_state = DTM_STATE_WAIT_MATCH;  
        }
        if(rx_state == DTM_STATE_WAIT_MATCH){
            void dtm_send_cmd(uint8_t *bufptr, uint32_t size);
            dtm_send_cmd(dtm_rx_buf, idx);
            idx = 0;
            rx_state = DTM_STATE_WAIT_HDR;
			break;
        }
    }while(1);
    trans_uart_pkt_proc_done();
    
    if(use_len >= len)
        return len;
    else 
        return use_len; 
}

static void user_enter_dtm_task(uint8_t ev, void *arg)
{
    trans_init.baudrate = 115200;
    trans_init.flow_ctrl = 0;
    trans_init.btmode = 0;
    trans_init.prefix_type = 0;
    trans_init.suffix_type = 0;
    trans_init.evt_callback = user_rbuf_event_cb;
    trans_init.data_rcv_callback = user_dtm_parse;
    trans_uart_deinit();
    trans_uart_init(&trans_init);
    void dtm_hci_init(uint32_t (*dtm_rcv_func)(const uint8_t *bufptr, uint32_t size));
    uint32_t hci_evt_rsp(const uint8_t *p_data,uint32_t len);
    dtm_hci_init(hci_evt_rsp);
    BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_DTM);     
    void dtm_send_cmd(uint8_t *bufptr, uint32_t size);
    dtm_send_cmd((uint8_t *)"\x01\x03\x0C\x00", 4);
}

static void user_disc_dev_task(uint8_t ev, void *arg)
{
    app_ble_disc_cmd_t *disc = (app_ble_disc_cmd_t *)arg;
    if(disc != NULL){
        if(user_device_find_by_conhdl(disc->conhdl) != NULL)
            ble_gap_disconn(disc->conhdl, disc->reason);
        FREE(disc);  
    }
}

static void user_auto_update_conn_param(uint8_t ev, void *arg)    
{
    bool wait_for_result = false;

    if(arg != NULL){
        conn_record_t *node = (conn_record_t *)arg;
        if(user_device_find_by_conhdl(node->conhdl) != NULL){
            cmd_conn_param_t conn_param = {0};
            cfg_get(CFG_TYPE_CONN_PARAMS, &conn_param, sizeof(cmd_conn_param_t));
            if(BLE_MULTI_GET() == true /*&& BLE_MODE() == BLE_MODE_M*/){
                conn_param.conn_interval_min += (node->conhdl)*12;
                conn_param.conn_interval_max += (node->conhdl)*12;
                ///Check whether the connection parameters are correct
                if((conn_param.conn_interval_max * (conn_param.conn_latency + 1) * 3) > conn_param.sup_timemout * 8){
                    conn_param.sup_timemout = 1000;
                    ///Maximum timeout, does not meet connection parameter rules, connection parameters will not be updated
                    if((conn_param.conn_interval_max * (conn_param.conn_latency + 1) * 3) > conn_param.sup_timemout * 8){
                        BR_LOG_INFO("connect param upate fail\r\n");
                        FREE(arg);
                        return ;
                    }
                }
            }
            USER_UART_EVENT_IND("+CONNPARAM_REQ:%d,%d,%d,%d",
                                conn_param.conn_interval_min,
                                conn_param.conn_interval_max,
                                conn_param.conn_latency,
                                conn_param.sup_timemout);
            s_conn_param_update_done = false;
            ble_update_param(node->conhdl, (ble_conn_param_t *)&conn_param);
            TIMER_CREATE(2000, (void *)(uint32_t)node->conhdl,
                         user_conn_param_diag_timeout);
            wait_for_result = true;
        }
        FREE(arg);
    }
    if(!wait_for_result){
        BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_TIM);
    }
}

static void user_conn_param_diag_timeout(uint8_t ev, void *arg)
{
    uint16_t conhdl = (uint16_t)(uint32_t)arg;

    if(!s_conn_param_update_done && user_device_find_by_conhdl(conhdl) != NULL){
        USER_UART_EVENT_IND("+CONNPARAM_RESULT:TIMEOUT");
    }
    BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_TIM);
}

void user_ble_wakeup_task(void)
{
    if(BLE_SLEEP_LOCK_MASK_CHECK(BLE_SLEEP_LOCK_IO) && ble_sleep_done == true){ 
        app_power_sleep_pin_init(HAL_GPIO_PD_IN, HAL_GPIO_INT_RISING_EDGE);
        trans_uart_init(&trans_init);
    
        if((!BLE_COMMAND_GET()) && List_Count(&g_conn_list)){
            trans_uart_cb_update(p_user_uart_rcv_data);
        }
        else{
            /// pdu模式或者无连接
            trans_uart_cb_update(p_user_uart_cmd_parse);
        }
        ble_sleep_done = false;
    }
}

/**
 * @brief ble睡眠控制
 * 
 */
void user_ble_sleep_task(void)
{
    if(!BLE_SLEEP_LOCK_MASK_GET()){ 
        if(ble_sleep_done == false) {
            app_keyscan_prepare_sleep();
            app_power_sleep_pin_init(HAL_GPIO_PU_IN, HAL_GPIO_INT_FALLING_EDGE);
            ble_sleep_done = true;
        }
        trans_uart_deinit();
    }
}

/**
 * @brief 串口事件回调
 * 
 * @param evt 回调参数
 */
void user_rbuf_event_cb(const trans_uart_ev_t* evt)
{
    BR_LOG_DEBUG("%s, evt->type:%x\r\n",__func__, evt->ev_type);
    
    switch(evt->ev_type){
        case TRANS_EV_READ_NTF:
        case TRANS_EV_RX_IDLE:
            app_power_mcu_sleep_timer_update();
            break;
        case TRANS_EV_SEND_READY:
            app_power_mcu_wakeup();
            BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_RBUF); 
            break;
        case TRANS_EV_SEND_CMP:
            app_power_mcu_sleep_timer_update();
            if(trans_uart_txbuf_used() == 0) {
                BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_RBUF);
            }
#ifdef CFG_BLE_FC
            task_set_active(s_credit_task_id);
#endif
#if (RBUF_TX_SPEED_MODE == 1)
            if(trans_uart_txbuf_used()){
                trans_uart_send_enable(ENABLE);
            }
            else{
                trans_uart_send_enable(DISABLE);
                app_power_mcu_wakeup_end_exec();	
            } 
#else //(RBUF_TX_SPEED_MODE == 0)
            trans_uart_send_enable(DISABLE);
            app_power_mcu_wakeup_end_exec(); 
#endif
            app_image_at_retry_pending();
            break;
        default:
            break;
    }
    
}


/**
 * @brief 功耗控制模块的回调
 * 
 * @param evt 事件类型
 * @param arg 回调参数指针
 */
void user_power_event_cb(app_power_evt_t evt, void* arg)
{
    BR_LOG_DEBUG("%s, evt:%d\r\n",__func__,evt);
    
    switch(evt){
        case POWER_MCU_WAKE_END_IND:
			//Check is there's data to send
            if(trans_uart_txbuf_used()){
                app_power_mcu_wakeup();
            }
            break;
        case POWER_MCU_WAKE_IND:
            BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_MCU); 
            break;
        case POWER_MCU_SLEEP_IND:
            BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_MCU); 
            break;
        case POWER_MCU_WAKE_DELAY_IND:
            trans_uart_send_enable(ENABLE);
            break;
        case POWER_BLE_WAKE_IND:
            BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_IO); 
            break;
        case POWER_BLE_SLEEP_IND:
            BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_IO); 
            break;
        default:
            break;
    }
}


/**
 * @brief at指令模块回调
 * 
 * @param evt 事件类型，参考cmd_change_evt_t
 * @param arg 回调参数
 * @return int 事件处理结果
 */
int user_atcmd_evt_ck(cmd_change_evt_t evt, void* arg)
{
    int  ret = CMD_EVT_ERR_NONE;
    switch(evt)
    {
        case CMD_UART_BAUD: 
            {
                cmd_uart_baud_t *p_baud = (cmd_uart_baud_t *)arg;
                trans_init.baudrate = p_baud->baud;
                TIMER_CREATE(50, &trans_init, user_ble_update_baud_task);
            }
            break;
        case CMD_UART_FLOWCTRL:  
            {
                cmd_uart_flowctrl_t *p_fc = (cmd_uart_flowctrl_t *)arg;
                trans_init.flow_ctrl = p_fc->flowctrl;
                TIMER_CREATE(50, &trans_init, user_ble_update_fc_task);
            }
            break;  
        case CMD_SILENT: 
            {
                trans_init.btmode = *(uint8_t *)arg;
                TIMER_CREATE(50, &trans_init, user_ble_update_silent_task);
            }
            break;
        case CMD_LABEL_INFO:
            app_label_adv_refresh();
            break;

        case CMD_MULTI_CONN:  
            { 
                BLE_MULTI_SET(*(uint8_t *)arg); 
                user_ble_adv_scan_update_task_add(0, s_adv_enable, ENABLE_DEFAULT);
            }
            break;         
        case CMD_COMMAND_MODE:  
            {
                BLE_COMMAND_SET(*(uint8_t *)arg);
            }
            break;          
        case CMD_AUTOUNLOCK:  
            {
            }
            break; 
        case CMD_BT_MODE:
            {
                uint8_t *p_mode = (uint8_t *)arg;
                if(*p_mode == 6) { /// dtm
                    user_ble_adv_scan_update_task_add(0, DISABLE, DISABLE); ///fixme
                    TIMER_CREATE(100, NULL, (task_func *)user_enter_dtm_task);
                }
            }
            break;
        case CMD_TX_DELAY:       
        case CMD_WAKEACT:
            app_power_param_update();
            break;  
        case CMD_WHITE_LIST_GET:
            {
                cmd_whitelist_t  *p_wl = (cmd_whitelist_t *)arg;
                ble_addr_t wl_data;
                p_wl->dev_n = user_white_list_get_size();
                for(uint8_t i=0; i<p_wl->dev_n; i++){
                    user_white_list_get_dev(i, &wl_data);
                    memcpy(p_wl->dev[i].addr, wl_data.addr, BD_ADDR_LEN);
                }                 
            }
            break;
        case CMD_WHITE_LIST_ADD:  
            {
                cmd_white_list_add_t *p_wl = (cmd_white_list_add_t *)arg;
                ble_addr_t wl_data = {0};
                conn_record_t *node = NULL;
                if(p_wl->type == INDEX_TYPE_IDX){
                    node = user_device_find_by_conhdl(p_wl->info.conhdl);
                 }
                 else {
                    node = user_device_find_by_bd(ADDR_TYPE_PUBLIC, p_wl->info.addr);
                 }
                 if(node != NULL && memcmp(node->public_bd.addr, "\x00\x00\x00\x00\x00\x00", BD_ADDR_LEN)){
                     memcpy(wl_data.addr, node->public_bd.addr, BD_ADDR_LEN);
                     wl_data.addr_type = node->public_bd.addr_type;
                     ///Add to the whitelist and save the pairing information immediately.
                     user_white_list_add_dev(&wl_data);
                     if(node->bond_info != NULL) {
                         ble_bond_db_add_device((void *)node->bond_info);
                         FREE(node->bond_info);
                         node->bond_info = NULL;
                     }
                 }
                 else {
                     ret = CMD_EVT_ERR_PARAM;
                 }
            }
            break;          
        case CMD_WHITE_LIST_DEL:  
            {
                cmd_white_list_del_t *p_wl = (cmd_white_list_del_t *)arg;
                ble_addr_t wl_data = {0};
                ///get addr
                if(p_wl->type == INDEX_TYPE_IDX && p_wl->info.index < user_white_list_get_size()){
                    user_white_list_get_dev(p_wl->info.index, &wl_data);
                }
                else if(p_wl->type == INDEX_TYPE_ADDR){
                    wl_data.addr_type = 0;//ADDR_PUBLIC;
                    memcpy(wl_data.addr, p_wl->info.addr, BD_ADDR_LEN);
                }
                else
                    ret = CMD_EVT_ERR_PARAM;
                /// del wl
                if(ret == CMD_EVT_ERR_NONE){
                    user_white_list_del_dev(&wl_data);
                    ble_bond_db_erase_device(wl_data.addr_type, wl_data.addr);
                }
                //立即执行或者创建事件执行
            }
            break;          
        case CMD_FACTORYRESET:
            {
                cfg_factory();
                /// clear bonding info
                ble_bond_db_erase_all();
                /// clear white list
                user_white_list_erase_all(); 
                /// delay reset
                TIMER_CREATE(100, NULL, (task_func*)user_system_reset_task);
            }
            break;
        case CMD_RESET:
            {
                /// store cfg
                cfg_store();
                /// store wlst
                user_white_list_store();
                /// store linkkey db
                ble_bond_db_store();
                /// delay reset
                TIMER_CREATE(100, NULL, (task_func*)user_system_reset_task);
            }
            break;
        case CMD_CLEARLINKKEY:
            {
                ble_bond_db_erase_all();
            }
            break;
        /*-------------*/
        /*蓝牙信息修改*/
        /*----GAP----*/
        case CMD_BT_NAME:   
            {
                uint8_t *p_name = (uint8_t *)arg;
                ble_gap_name_set((const char *)p_name, strlen((const char*)p_name));
            }
            break;
        case CMD_BT_ADDR:  
            break;
        case CMD_BT_ADV_ENABLE:  
            {
                if(BLE_MODE() != BLE_MODE_M){
                    s_adv_enable = (*(uint32_t *)arg) & 0xFF;
                    if (s_adv_enable != DISABLE)
                    {
                        app_label_adv_refresh();
                    }
                    user_ble_adv_scan_update_task_add(0, s_adv_enable, ENABLE_DEFAULT);
                }
                else
                    ret = CMD_EVT_ERR_NOT_ALLOW;
            }
            break;  
        case CMD_BT_ADV_PARAMS:  
            {
                cmd_adv_param_t *p_adv_par = (cmd_adv_param_t *)arg;
                ble_adv_intv_set(p_adv_par->adv_intreval_min);
                if(ble_adv_state_get() == ENABLE) {
                    app_label_adv_refresh();
                    user_ble_adv_scan_update_task_add(0, DISABLE, ENABLE_DEFAULT);
                    user_ble_adv_scan_update_task_add(10, s_adv_enable, ENABLE_DEFAULT);
                }
            }
            break;      
        case CMD_BT_ADV_DATA:  
            {
                ret = CMD_EVT_ERR_NOT_ALLOW;
            }
            break;      
        case CMD_BT_ADV_SRDATA:  
            {
                cmd_sr_data_t *p_sr_data = (cmd_sr_data_t *)arg;
                uint8_t name[32];
                cfg_get(CFG_TYPE_NAME, name, 32);
                if(p_sr_data->sr_data_len + strlen((const char *)name) + 2 <= ADV_DATA_MAX_SIZE){                    
                    ble_scan_rsp_data_set((ble_adv_data_t *)p_sr_data);
                }
                else
                    ret = CMD_EVT_ERR_PARAM;
            }
            break; 
        case CMD_BT_SCAN_KW:
            {
                cmd_scankw_t *p_scankw = (cmd_scankw_t *)arg;
                if(BLE_MODE() != BLE_MODE_S) {
                    memcpy(&s_scan_kw, p_scankw, sizeof(cmd_scankw_t));
                }
                else {
                    ret = CMD_EVT_ERR_NOT_ALLOW;
                }
            }
            break;
        case CMD_BT_SCAN_ENABLE: 
            {
                uint8_t *p_scan = (uint8_t *)arg;
                if(BLE_MODE() != BLE_MODE_S){
                    s_scan_enable = *p_scan;
                    user_ble_adv_scan_update_task_add(0, ENABLE_DEFAULT, s_scan_enable);
                }
                else
                    ret = CMD_EVT_ERR_NOT_ALLOW;
            }
            break;     
        case CMD_BT_SCAN_PARAMS:  
            {
                cmd_scan_param_t *p_scan_par = (cmd_scan_param_t *)arg;
                ble_scan_params_t scan_param;
                scan_param.prop_active_enable = p_scan_par->scan_type;
                scan_param.scan_intv = p_scan_par->scan_interval;
                scan_param.scan_wd = p_scan_par->scan_window;
                ble_scan_param_set(&scan_param);
            }     
            break;   
        case CMD_BT_CONNECT_DEV:  
            {
                cmd_conn_req_t *p_conn = (cmd_conn_req_t *)arg;
                uint8_t slave_num = user_get_slave_connection_num();
                uint8_t master_num = user_get_master_connection_num();
                ret = CMD_EVT_ERR_NOT_ALLOW;
                
                do {
                    if(BLE_MODE() == BLE_MODE_S) {
                        break;
                    }        
                    if((BLE_MODE() == BLE_MODE_M) &&
                        ((master_num >= BLE_MASTER_CONN) || ((!BLE_MULTI_GET()) && (master_num != 0)))){
                        break;
                    }
                    if((BLE_MODE() == BLE_MODE_MS) &&
                        ((master_num != 0) || ((!BLE_MULTI_GET()) && (slave_num  != 0)))){
                        break;
                    }
                    if(s_create_tid != 0){
                        break;
                    }
                    if(user_device_find_by_bd(p_conn->addr_type, p_conn->addr) != NULL){
                        break;
                    }
                    user_ble_adv_scan_update_task_add(0, DISABLE, DISABLE);//todo
                    memcpy(&s_target_addr, p_conn, sizeof(cmd_conn_req_t));
                    TIMER_CREATE(50, (void * )&s_target_addr, (task_func *)user_ble_create_conn_task);
                    ret = CMD_EVT_ERR_NONE;
                }while(0);
            }
            break;
        case CMD_BT_CONN_PARAMS:  
            {
                cmd_conn_param_t *p_conn_param = (cmd_conn_param_t *)arg;
                //下一次连接生效
                ble_gap_conn_param_get((ble_conn_param_t*)p_conn_param);
            }
            break;     
        case CMD_BT_DISCONN:  
            {
                cmd_disconn_t *p_diss = (cmd_disconn_t *)arg;
                conn_record_t *node = user_device_find_by_conhdl(p_diss->conn_id);
                if(node != NULL)
                    ble_gap_disconn(node->conhdl, 0x16/*CO_ERROR_REMOTE_USER_TERM_CON*/);
                else
                    ret = CMD_EVT_ERR_PARAM;
            }
            break;     
        case CMD_BT_CONNMTU: 
            {
                uint8_t i=0;
                cmd_conn_mtu_t *conn_mtu = (cmd_conn_mtu_t *)arg;
                conn_record_t *node = g_conn_list.head;
                while(node){
                    conn_mtu->mtu[i].mtu = node->mtu;
                    conn_mtu->mtu[i].conhdl = node->conhdl;
                    i++;
                    node = LNEXT(node);
                } 
                conn_mtu->conn_n = i;
            }
            break;
        case CMD_BT_CONNDEV: 
            {
                cmd_conn_dev_t *conn_dev = (cmd_conn_dev_t *)arg;
                conn_record_t *node = g_conn_list.head;
                while(node){
                    if(memcmp(node->public_bd.addr, "\x00\x00\x00\x00\x00\x00", BD_ADDR_LEN)){
                        memcpy(conn_dev->dev[conn_dev->conn_n].addr, node->public_bd.addr, BD_ADDR_LEN);
                    }
                    else {
                        memcpy(conn_dev->dev[conn_dev->conn_n].addr, node->addr, BD_ADDR_LEN);
                    }
                    conn_dev->dev[conn_dev->conn_n].conhdl = node->conhdl;
                    conn_dev->conn_n++;
                    node = LNEXT(node);
                } 
            }
            break;
        case CMD_BT_PHYPREF:
            {
                cmd_phy_pref_t *phy_pref = (cmd_phy_pref_t *)arg;
                ble_set_def_phy(phy_pref->tx_phys, phy_pref->rx_phys);
            }
            break;
        case CMD_BT_ADV_GET:
            {
                uint32_t *p_adv = (uint32_t *)arg;
                *p_adv  =  ble_adv_state_get();
            }
            break;
        case CMD_BT_ADVDATA_GET:
            {
                cmd_adv_data_t *p_advdata = (cmd_adv_data_t *)arg;
                ble_adv_data_get(p_advdata->adv_data, &p_advdata->adv_data_len);
            }
            break;
        case CMD_BT_SRDATA_GET:
            {
                cmd_sr_data_t *p_srdata = (cmd_sr_data_t *)arg;
                ble_scan_rsp_data_get(p_srdata->sr_data, &p_srdata->sr_data_len);
            }
            break;  
        case CMD_BT_SCAN_GET:
            {
                uint8_t *p_scan = (uint8_t *)arg;
                *p_scan = (uint8_t)ble_scan_state_get();
            }
            break;
        case CMD_BT_RSSI_GET:
            {
                uint32_t rssi_cid = *(uint32_t *)arg;
                if(user_device_find_by_conhdl(rssi_cid) != NULL)
                    ble_rssi_get(rssi_cid);
                else
                    ret = CMD_EVT_ERR_PARAM;
            }
            break;
        /*----SMP----*/
        /*settings*/
        case CMD_PAIR_REQ_CFM_FEA: 
            break;
        case CMD_PAIR_IOCAP:  
            ble_gap_iocap_set(*(uint8_t*)arg);
            break;         /*设备的IOCAP设置*/
        case CMD_PAIR_LESC:
            ble_gap_lesc_set((bool)(*(uint8_t*)arg));
            break;
        case CMD_PARI_PSK_SET:  
            ble_gap_passkey_set(*(uint32_t*)arg);
            break;       /*设置固定的PASSKEY*/
        /*reply*/
        case  CMD_PAIR_REQ_REPLY:  
            {
                cmd_pair_req_reply_t *p_reply = (cmd_pair_req_reply_t *)arg;
                conn_record_t * node = user_device_find_by_conhdl(p_reply->conhdl);
                if(node != NULL && node->smp_state == SMP_PAIRING_RESPONSE){
                    node->smp_state = SMP_PAIRING;
                    ble_send_sec_pairing_rsp(p_reply->conhdl, p_reply->accept);     
                }                    
                else
                    ret = CMD_EVT_ERR_PARAM;
            }
            break;     /*响应配对确认请求*/
        case CMD_PAIR_PSK_ENTRY:  
            {
                cmd_pair_passkey_entry_t *p_passkey = (cmd_pair_passkey_entry_t *)arg;
                conn_record_t * node = user_device_find_by_conhdl(p_passkey->conhdl);
                if(node != NULL && node->smp_state == SMP_PASSKEY_RESPONSE){
                    node->smp_state = SMP_PAIRING;
                    ble_send_sec_passkey_rsp(p_passkey->conhdl, p_passkey->passkey);
                }
                else
                    ret = CMD_EVT_ERR_PARAM;
            }   
            break;     /*输入PASSKEY响应*/  
        case CMD_PAIR_NUMCF_RPL:  
            {
                cmd_pair_numcf_reply_t *p_numcf = (cmd_pair_numcf_reply_t *)arg;
                conn_record_t * node = user_device_find_by_conhdl(p_numcf->conhdl);
                if(node != NULL && node->smp_state == SMP_NUMERIC_RESPONSE){
                    node->smp_state = SMP_PAIRING;
                    ble_send_sec_numeric_cfm(p_numcf->conhdl, p_numcf->result);
                }
                else
                    ret = CMD_EVT_ERR_PARAM;
            }
            break;     /*数字比较结果*/

        /*----GATT----*/
        case CMD_GATT_MTU:  
            /*Restart to take effect*/
            break;            
        /*----HOGP----*/
        case CMD_HOGP_KB_RPT:
        case CMD_HOGP_FUNC_RPT:
            {
                cmd_hogp_rpt_t *kb_func_rpt = (cmd_hogp_rpt_t*)arg;
                if(user_device_find_by_conhdl(kb_func_rpt->conhdl) != NULL && BLE_AUTOUNLOCK_GET()){
                    ble_send_hid_report(kb_func_rpt->in_rpt,ble_get_conidx_by_conhdl(kb_func_rpt->conhdl));
                    ret = CMD_EVT_ERR_NONE;
                }
                else{
                    ret = CMD_EVT_ERR_PARAM;
                }
            }
            break;
        /*====Server====*/
        case CMD_GATTS_SVC:  
            break;          /*GATT Server注册服务UUID*/
        case CMD_GATTS_WRTCHAT:  
            break;       /*用于接受Clinet端数据的特征值UUID*/
        case CMD_GATTS_NTFCHAT:  
            break;       /*用于发送数据给Client端特征值UUID*/
        /*====Client====*/
        case CMD_GATTC_SVC:       /*GATT Client通信目标服务*/
            /*
            */
            break;     
        case CMD_GATTC_WRTCHAT:   /*发送数据给Server端使用的特征值UUID*/
            break;     
        case CMD_GATTC_NTFCHAT:  /*接受Server端数据特征UUID*/
            break;      
        /*Format*/
        case CMD_IND_PREFIX:  
            {
                uint8_t *p_prefix = (uint8_t *)arg;
                trans_uart_fmt_update(INVALID_TYPE, *p_prefix, INVALID_TYPE);
            }
            break;        
        case CMD_IND_SUFFIX:  
            {
                uint8_t *p_suffix = (uint8_t *)arg;
                trans_uart_fmt_update(INVALID_TYPE, INVALID_TYPE ,*p_suffix);
            }
            break;       
        case CMD_CMD_PREFIX:    /*指令前缀*/   
        case CMD_CMD_SUFFIX:    /*指令后缀*/
            break;       
        case CMD_DATA_SEND:  
            {
                cmd_data_send_t *p_data = (cmd_data_send_t *)arg;
                conn_record_t *node = user_device_find_by_conhdl(p_data->cid);

                if(node == NULL || node->ccc_enable == 0 || node->rcv_credit == 0){
                    ret = CMD_EVT_ERR_NOT_ALLOW;
                    break;
                }
                if(node->mtu - ATT_HEAD_SIZE < p_data->length) {
                    ret = CMD_EVT_ERR_PARAM;
                    break;
                }
                if(!ble_is_data_sendable(node->conhdl)) {
                    ret = CMD_EVT_ERR_NOT_ALLOW;
                    break;
                }
                
				ret = ble_send_data(p_data->cid, p_data->data, p_data->length);
                if(ret == BLE_ERR_NONE){
                    if(node->rcv_credit != 0xFF)  node->rcv_credit--;
                    TIMER_CREATE_ONESHOT(&s_send_timeout_id, SEND_DATA_TIMEOUT, NULL, user_send_data_timeout_task);
                }
                BR_LOG_DEBUG("send_data,ret:%x,node:%x,node->ccc_enable:%x\r\n",ret,node,node->ccc_enable);
            }
            break; 
        case CMD_TEST_TXPOWER: 
            {
                uint8_t level = *(uint8_t *)arg;
                if(br_tx_power_set(level) == false)
                    ret = CMD_EVT_ERR_PARAM;      
                else
                    app_label_adv_refresh();
            }
            break;
        case CMD_TEST_FREQ:
            {
                cmd_test_freq_t *p_freq = (cmd_test_freq_t *)arg;
                if(List_Count(&g_conn_list) == 0){
                    user_ble_adv_scan_update_task_add(0, DISABLE, DISABLE);
                    cmd_test_freq_t *param = NEW(sizeof(cmd_test_freq_t));
                    if(param){
                        memcpy(param, p_freq, sizeof(cmd_test_freq_t));
                        TIMER_CREATE(10, param, user_ble_test_freq_task);
                    }
                }
                else
                    ret = CMD_EVT_ERR_NOT_ALLOW;
            }
            break;
#ifdef CFG_ADV_LED
        case CMD_ADVLED_GET:
            {
                advled_t *advled_par = (advled_t *)arg;
                advled_par->period = s_advled_par.period;
                advled_par->enable = s_advled_par.enable;
            }
            break;
        case CMD_ADVLED_SET:
            {
                uint8_t slave_num =  user_get_slave_connection_num();
                uint8_t master_num =  user_get_master_connection_num();
                uint8_t conn_num = slave_num + master_num;
                
                advled_t *advled_par = (advled_t *)arg;
                s_advled_par.period = advled_par->period;
                s_advled_par.enable = advled_par->enable;
                uint8_t adv_enable = s_adv_enable;
                
                if(BLE_MODE() == BLE_MODE_S){
                    if ((slave_num == BLE_SLAVE_CONN && BLE_MULTI_GET()) ||
                        ((!BLE_MULTI_GET()) && slave_num != 0)){
                        adv_enable = DISABLE; 
                    }
                }
                /// 主从一体
                if(BLE_MODE() == BLE_MODE_MS){
                    if (((slave_num != 0) && BLE_MULTI_GET()) 
                        || ((!BLE_MULTI_GET()) && (conn_num != 0))){
                        adv_enable = DISABLE;
                    }
                }
                user_ble_advio_blink_task_enable(adv_enable);
            }
            break;
#endif
        default:
            break;
    }

    BR_LOG_DEBUG("%s, evt:%x ret:%x\r\n", __func__, evt, ret);
    return ret;
}

/**
 * @brief ble事件回调函数
 * 
 * @param evt 事件类型，参考app_ble_evt_t
 * @param param 回调参数指针，参考app_ble_msg_t
 */
void user_ble_event_cb(app_ble_evt_t evt, app_ble_msg_t *param)
{   
    BR_LOG_DEBUG("%s,evt:%x\r\n", __func__, evt);
    
    switch(evt){
        case BLE_READY:
            /* Keep the module awake while it is advertising and waiting for
             * the phone, so UART commands and discovery remain available. */
            BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_IO);
            ModuleIrqRemoval(EXTI0_1_IRQn);
            ModuleIrqRegister(EXTI0_1_IRQn, xEXTI0_1_IRQHandler);
            ModuleIrqRemoval(EXTI2_3_IRQn);
            ModuleIrqRegister(EXTI2_3_IRQn, xEXTI2_3_IRQHandler);
            extern void (*user_EXTI4_12_IRQHandler) (void);
            user_EXTI4_12_IRQHandler = EXTI4_12_IRQn_callback;
            app_keyscan_init();
            USER_UART_EVENT_IND(IM_READY);
            app_label_adv_refresh();
            if(BLE_MODE() != BLE_MODE_M) {
                user_ble_adv_scan_update_task_add(0, s_adv_enable, ENABLE_DEFAULT);
            }
#ifdef CFG_BLE_FC
            s_credit_task_id = task_create(user_credit_send_task, NULL, NULL, false, true);
#endif
            break;
        /*connect*/
        case BLE_GAP_CONNECTED :
            {
                BR_LOG_INFO("[IMG] link up conhdl:%04x role:%d mtu:%d peer:%02x%02x%02x%02x%02x%02x\r\n",
                            param->connection_ind.conhdl,
                            param->connection_ind.role,
                            param->connection_ind.mtu,
                            param->connection_ind.peer_addr.addr[5],
                            param->connection_ind.peer_addr.addr[4],
                            param->connection_ind.peer_addr.addr[3],
                            param->connection_ind.peer_addr.addr[2],
                            param->connection_ind.peer_addr.addr[1],
                            param->connection_ind.peer_addr.addr[0]);
                LED_CONN_IND_ON();
                user_device_record_add(&param->connection_ind);
                
                if(param->connection_ind.role == MASTER_ROLE){
					///The connection may be false. Try to obtain information about the peer end first
					ble_get_peer_feature(param->connection_ind.conhdl);//fixme
                }
                else {   /// slave
                    USER_UART_EVENT_IND("%s:%04x", IM_CONN, param->connection_ind.conhdl);
                    ble_dle_set(param->connection_ind.conhdl, 251, 2120);//std 2128
                    uint16_t cfg_mtu = 0;
                    cfg_get(CFG_TYPE_GATT_MTU, (void *)&cfg_mtu, sizeof(cfg_mtu)); 
                    ble_mtu_set(param->connection_ind.conhdl, cfg_mtu);

                    /* Give the phone time to finish the initial link, then
                     * request the low-power connection parameters. */
                    conn_record_t *node = user_device_find_by_conhdl(param->connection_ind.conhdl);
                    conn_record_t *arg = NEW(sizeof(conn_record_t));
                    if(node != NULL && arg != NULL){
                        memcpy(arg, node, sizeof(conn_record_t));
                        BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_TIM);
                        TIMER_CREATE(3000, arg, user_auto_update_conn_param);
                    }
                    else if(arg != NULL){
                        FREE(arg);
                    }
                }

                if(BLE_COMMAND_GET()) //PDU
                    trans_uart_cb_update(p_user_uart_cmd_parse);
                else    
                    trans_uart_cb_update(p_user_uart_rcv_data);

                /* The BLE link is now active. Allow the normal sleep path to
                 * turn off the UART and enter low power between BLE events. */
                BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_IO);

                user_ble_adv_scan_update_task_add(100, s_adv_enable, ENABLE_DEFAULT);
            }
            break;
        case BLE_GAP_DISCONNEDTED:
            {
                /* Keep UART and advertising alive after a disconnect. */
                BLE_SLEEP_LOCK_MASK_SET(BLE_SLEEP_LOCK_IO);
                BR_LOG_WARNING("[IMG] link down conhdl:%04x reason:%02x\r\n",
                               param->disconnect_ind.conhdl,
                               param->disconnect_ind.reason);
                conn_record_t *node = user_device_find_by_conhdl(param->disconnect_ind.conhdl);
                app_image_service_disconnect(param->disconnect_ind.conhdl);
#if CFG_PRF_DFU
                prf_dfu_disconnect();
                dfu_action_disconnect();
#endif
                if(node->role == MASTER_ROLE) {
                    if(param->disconnect_ind.reason == 0x06) {/// CO_ERROR_PIN_MISSING
                        ble_bond_db_erase_device(node->addr_type, node->addr);
                    }
                    else if(param->disconnect_ind.reason == 0x3e) {///CO_ERROR_CONN_FAILED_TO_BE_EST
                        /// If no timeout occurs, reconnect
                        if(s_create_tid != NULL) {
                            ble_conn_req_t remote = {0};
                            remote.addr_type = node->addr_type;
                            memcpy(remote.addr, node->addr, BD_ADDR_LEN);
                            ble_gap_connect(&remote);
                        }
                        else {
                            USER_UART_EVENT_IND(IM_CONN_TIMEOUT);
                        }
                    }
                    if(node->ccc_enable == true) {
                        USER_UART_EVENT_IND("%s:%04x", IM_DISCONN, param->disconnect_ind.conhdl);    
                    }
                    else if(node->svc_disc_cmp == true) {
                        USER_UART_EVENT_IND(IM_CONN_TIMEOUT);
                    }
                }
                else {
                    USER_UART_EVENT_IND("%s:%04x", IM_DISCONN, param->disconnect_ind.conhdl); 
                }
                if(BLE_AUTOUNLOCK_GET() == true) {
                    ble_addr_t wl_data;
                    /// The public address is invalid when the pairing process breaks abnormally
                    if(memcmp(node->public_bd.addr, "\x00\x00\x00\x00\x00\x00", BD_ADDR_LEN)){
                        memcpy(wl_data.addr, node->public_bd.addr, BD_ADDR_LEN);
                        wl_data.addr_type = node->public_bd.addr_type;
                        if(user_while_list_find(&wl_data) == false){
                            ble_bond_db_erase_device(wl_data.addr_type, wl_data.addr);
                        }
                    }
                    if(node->bond_info != NULL) {
                        FREE(node->bond_info);
                        node->bond_info = NULL;
                    }
                }
                user_device_record_del(param->disconnect_ind.conhdl);
                if(List_Count(&g_conn_list) == 0)
                    LED_CONN_IND_OFF();
                trans_uart_cb_update(p_user_uart_cmd_parse); 
                app_label_adv_refresh();
                user_ble_adv_scan_update_task_add(100, s_adv_enable, ENABLE_DEFAULT);
            }
            break;
        case BLE_GAP_CONNECT_TIMEOUT:
            {
            
            }
            break;
        case BLE_GAP_CONNECT_PARAM_UPDATE:
            {
                s_conn_param_update_done = true;
                USER_UART_EVENT_IND("+CONNPARAM_RESULT:%d,%d,%d",
                                    param->param_updated.con_interval,
                                    param->param_updated.con_latency,
                                    param->param_updated.sup_to);
                if(param->param_updated.con_latency == 49){
                    ble_set_slave_latency(param->param_updated.conhdl, 49);
                }
                BLE_SLEEP_LOCK_MASK_CLEAR(BLE_SLEEP_LOCK_TIM);
                BR_LOG_INFO("conhdl:%04x, conn_intv:%d, conn_latency:%d, time_out:%d\r\n",
                                param->param_updated.conhdl,
                                param->param_updated.con_interval,
                                param->param_updated.con_latency,
                                param->param_updated.sup_to);
            }
            break;
        case BLE_GAP_FEATURE_IND:
            {
                 conn_record_t *node = user_device_find_by_conhdl(param->feature_ind.conhdl);
                 if(node != NULL && node->role == MASTER_ROLE){
                     prf_dtsc_enable_prf(node->conhdl);
                     timer_delete(&s_create_tid);
                 }
            }
            break;
        case BLR_GAP_RSSI_IND:
            {
                USER_UART_EVENT_IND("+GRSSI:%04d,%d",param->rssi_ind.conhdl, param->rssi_ind.rssi);
            }
            break;
        case BLE_GAP_SCAN_REPORT:
            {      
                bool target_found = false;

                if(s_scan_kw.enable && s_scan_kw.num) {
                    for(uint8_t i=0; i<s_scan_kw.num; i++){
                        if(ble_scan_data_find(s_scan_kw.adv_data[i].data, s_scan_kw.adv_data[i].len, (uint8_t *)param->adv_report.data, param->adv_report.length)) {
                            target_found = true;
                            break;
                        }
                    }
                }
                else {
                    target_found = true;
                }

                if(target_found == true && s_scan_enable == true){
                    uint8_t adv_report_str[64] = {0};
                    hlp_numayyr_to_hexstr(adv_report_str, param->adv_report.data, co_min(param->adv_report.length, ADV_DATA_MAX_SIZE));
                    USER_UART_EVENT_IND("+ADVINFO:%02x%02x%02x%02x%02x%02x,%d,%d%s%s",
                                      param->adv_report.peer_addr.addr[5], param->adv_report.peer_addr.addr[4], param->adv_report.peer_addr.addr[3],
                                      param->adv_report.peer_addr.addr[2], param->adv_report.peer_addr.addr[1], param->adv_report.peer_addr.addr[0],
                                      param->adv_report.peer_addr_type, param->adv_report.rssi,
                                      strlen((const char *)adv_report_str) ? "," : "", adv_report_str);
                }
            }
            break;
        /*==========GATT==========*/
        case BLE_GATT_MTU_EXCHANGE:
            {
                BR_LOG_INFO("[IMG] mtu conhdl:%04x value:%d\r\n",
                            param->gattc_mtu.conhdl, param->gattc_mtu.mtu);
                 conn_record_t *node = user_device_find_by_conhdl(param->gattc_mtu.conhdl);
                 if(node != NULL && node->mtu != param->gattc_mtu.mtu) {
                    node->mtu = param->gattc_mtu.mtu;
                 }
            }
            break;
        case BLE_GATTS_IMAGE_CCCD_RPT:
            app_image_service_set_notifications(param->rcv_ntf.conhdl,
                param->rcv_ntf.data == GATT_SEND_VAL_NOTIFICATION);
            break;
        case BLE_GATTS_IMAGE_DATA_RPT:
            app_image_service_receive(param->data_rcv.conhdl,
                param->data_rcv.data, param->data_rcv.len);
            break;
        case BLE_GATTS_IMAGE_DATA_CMP:
            app_image_service_notification_complete(param->ntf_cmp.conhdl);
            break;
        case BLE_GATTS_TRANS_DATA_CCCD_RPT:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->rcv_ntf.conhdl);
                if(node != NULL) {
                    node->ccc_enable = param->rcv_ntf.data;
                    /// update conn param
                    conn_record_t *arg = NEW(sizeof(conn_record_t));
                    if(arg != NULL){
                        memcpy(arg, node, sizeof(conn_record_t));
                        TIMER_CREATE(5000, arg, user_auto_update_conn_param);
                    }
                }
            }
            break;
#ifdef CFG_BLE_FC
        case BLE_GATTS_TRANS_FC_CCCD_RPT:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->rcv_ntf.conhdl);
                /// flowctrl char
                if(node != NULL){
                    if(param->rcv_ntf.data == GATT_SEND_VAL_NOTIFICATION){
                        node->credit = (BLE_MULTI_GET()) ? MULTI_CREDIT : SINGLE_CREDIT;
                        node->credit_max = (BLE_MULTI_GET()) ? MULTI_CREDIT : SINGLE_CREDIT;
                        /// send credit
                        user_send_credit(param->rcv_ntf.conhdl, node->credit_max);
						/// send mtu
                        //user_send_mtu(param->rcv_ntf.conhdl,node->mtu);
                    }
                }   
            }
            break;
#endif
        case BLE_GATTS_TRANS_DATA_RPT:
        case BLE_GATTC_TRANS_REPORT:
            {          
                conn_record_t *node = user_device_find_by_conhdl(param->data_rcv.conhdl);
				if(node != NULL){
	                if(BLE_COMMAND_GET()){
	                    user_uart_pdu_send(param->data_rcv.conhdl, param->data_rcv.data, param->data_rcv.len);
                    }
	                else{
	                    USER_UART_DATA_SEND(param->data_rcv.data, param->data_rcv.len);    
                    }
#ifdef CFG_BLE_FC
					/// check credit
                	if(node->credit != 0){
    					node->credit--;
                    	user_data_flow_control(node->conhdl);
					}
					else
						BR_LOG_DEBUG("Flow control error\r\n");
#endif                    
				}
            }    
            break;
#ifdef CFG_BLE_FC
        case BLE_GATTS_TRANS_FC_RPT:
        case BLE_GATTC_TRANS_CREDIT:
            {   
                conn_record_t *node = user_device_find_by_conhdl(param->data_rcv.conhdl);
                const uint8_t *credit = param->data_rcv.data;
                if(node && (param->data_rcv.len == 0x02) && (credit[0] == 0x01)) {   
                    if(node->rcv_credit == 0xFF)
                        node->rcv_credit = 0;  
                    node->rcv_credit += credit[1];
				}
            }
            break;
#endif  
        case BLE_GATTS_TRANS_USR_RPT:
            {    
            }
            break;
        case BLE_GATTS_OTA_CCCD_REPORT:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->rcv_ntf.conhdl);
                if(node != NULL){
                    /* Keep the customer connection profile during OTA. */
                    ble_conn_param_t conn_param = {40,40,49,1000};
                    ble_update_param(param->rcv_ntf.conhdl, &conn_param);
                }
            }
            break;
        case BLE_GATTS_OTA_REPORT:
            {
            }
            break;
        case BLE_GATTS_OTA_CMP:
#if CFG_PRF_DFU
            dfu_action_notification_complete();
#endif
            break;
        case BLE_GATTS_OTA_ATCMD_REPORT:
        case BLE_GATTS_CMD_REPORT:
            {
                s_atcmd.conn = user_device_find_by_conhdl(param->data_rcv.conhdl);
#if CFG_PRF_DFU
                s_atcmd.rsp_func = (evt == BLE_GATTS_OTA_ATCMD_REPORT) ? prf_dfu_atcmd_rsp_send : prf_cmd_rsp_send;
#else
                s_atcmd.rsp_func = prf_cmd_rsp_send;
#endif 
                cmd_rsp_out_set((cmd_rsp_fmt_out_t)user_atcmd_rsp_ind);
                cmd_parse_ble(param->data_rcv.data, param->data_rcv.len);
                cmd_rsp_out_set((cmd_rsp_fmt_out_t)trans_uart_event_ind);
            }
            break;
        case BLE_GATTS_TRANS_DATA_CMP:
            {
                timer_delete(&s_send_timeout_id);
	            trans_uart_pkt_proc_done();
            }
            break;
        case BLE_GATTS_TRANS_FC_CMP:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->ntf_cmp.conhdl);
                if(node != NULL)
                    node->fc_cmp = true;
            }
            break;
        case BLE_GATTC_TRANS_CMP:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->ntf_cmp.conhdl);
                if(node != NULL){
                    node->fc_cmp = true;
                    timer_delete(&s_send_timeout_id);
                }
                trans_uart_pkt_proc_done();
            }
            break;
        case BLE_GATTC_TRANS_CCCD_CMP:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->ccc_cmp.conhdl);
                if(node != NULL && param->ccc_cmp.status == 0x00 && node->ccc_enable == false){
                    /// update mtu 
                    uint16_t cfg_mtu = 0;
                    cfg_get(CFG_TYPE_GATT_MTU, (void *)&cfg_mtu, sizeof(cfg_mtu)); 
                    ble_mtu_set(param->ccc_cmp.conhdl, cfg_mtu);          
                    /// update dle defaule param 251 Bytes, 2.12ms
                    ble_dle_set(param->ccc_cmp.conhdl, 251, 2120);
                    node->ccc_enable = true;
                    /// update credit
#ifdef CFG_BLE_FC
                    node->credit = (BLE_MULTI_GET()) ? MULTI_CREDIT : SINGLE_CREDIT;
                    node->credit_max = (BLE_MULTI_GET()) ? MULTI_CREDIT : SINGLE_CREDIT;
                    user_send_credit(node->conhdl, node->credit_max); 
#endif
                    /// update conn param
                    conn_record_t *arg = NEW(sizeof(conn_record_t));
                    if(arg != NULL){
                        memcpy(arg, node, sizeof(conn_record_t));
                        TIMER_CREATE(3000, arg, user_auto_update_conn_param);
                    }  
                    USER_UART_EVENT_IND("%s:%04x", IM_CONN, node->conhdl);
                }
            }
            break;
        case BLE_GATTC_TRANS_CCCD_FAILED:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->ccc_failed.conhdl);
                if(node != NULL) {
                    __NOP();
                }
            }
            break;
        case BLE_GATTC_TRANS_INSUFF:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->insuff_auth.conhdl);
                if(node != NULL && param->insuff_auth.status != 0x00 && node->smp_state == SMP_NONE){
                    node->smp_state = SMP_PAIRING;
                    ble_sec_encrypt_req(node->conhdl);//fixme
                }
            }
            break;
        /*======GATT Client======*/
        case BLE_GATTC_NO_TARGET_SVC:
            ble_gap_disconn(param->targe.conhdl, 0x13/*CO_ERROR_REMOTE_USER_TERM_CON*/);
            USER_UART_EVENT_IND(IM_NO_SERVICE);
            break;
        case BLE_GATTC_TARGET_READY:
            {
				conn_record_t *node = user_device_find_by_conhdl(param->targe.conhdl);
                if(node != NULL) {
                    node->svc_disc_cmp = true;
                }
            }
            break;
        /*==========SMP==========*/
        case BLE_SMP_PAIR_REQ :
            {
                uint8_t req_cfm;
                cfg_get(CFG_TYPE_PAIR_REQ_CFM, &req_cfm, sizeof(req_cfm));
                conn_record_t * node = user_device_find_by_conhdl(param->sec_req.conhdl);
                if(req_cfm == true && BLE_COMMAND_GET() == true){
                    node->smp_state = SMP_PAIRING_RESPONSE;
                    USER_UART_EVENT_IND("%s:%04d", IM_PAIR, param->sec_req.conhdl);
                }
                else {
                    node->smp_state = SMP_PAIRING;
                    ble_send_sec_pairing_rsp(param->sec_req.conhdl, true);
                }
            }
            break;
        case BLE_SMP_PASSKEY_DISPLAY:
            {
                uint8_t req_cfm;
                cfg_get(CFG_TYPE_PAIR_REQ_CFM, &req_cfm, sizeof(req_cfm));
                if(req_cfm == true && BLE_COMMAND_GET() == true)
                    USER_UART_EVENT_IND("%s:%04d,%c%c%c%c%c%c", IM_DISPALY_PASSKEY, param->sec_psk_ind.conhdl, 
                                                                param->sec_psk_ind.passkey[0],param->sec_psk_ind.passkey[1],param->sec_psk_ind.passkey[2],
                                                                param->sec_psk_ind.passkey[3],param->sec_psk_ind.passkey[4],param->sec_psk_ind.passkey[5]);    
            }
            break;
        case BLE_SMP_PASSKEY_ENTRY_REQ:
            {
                uint8_t req_cfm;
                cfg_get(CFG_TYPE_PAIR_REQ_CFM, &req_cfm, sizeof(req_cfm));
                if(req_cfm == true && BLE_COMMAND_GET() == true){
                    conn_record_t * node = user_device_find_by_conhdl(param->sec_psk_entry.conhdl);
                    if(node != NULL){
                        USER_UART_EVENT_IND("%s:%04d", IM_PASSKEY_ENTRY, param->sec_psk_entry.conhdl);
                        node->smp_state = SMP_PASSKEY_RESPONSE;
                    }
                }
                else{
                    uint32_t passkey;
                    cfg_get(CFG_TYPE_PSK, &passkey, sizeof(passkey));
                    ble_send_sec_passkey_rsp(param->sec_psk_entry.conhdl, passkey);
                }
            }   
            break;
        case BLE_SMP_NUMERIC_CMP:
            {
                uint8_t req_cfm;
                cfg_get(CFG_TYPE_PAIR_REQ_CFM, &req_cfm, sizeof(req_cfm));
                if(req_cfm == true && BLE_COMMAND_GET() == true){
                    conn_record_t * node = user_device_find_by_conhdl(param->sec_numeric_ind.conhdl);
                    if(node != NULL){
                        USER_UART_EVENT_IND("%s:%04d,%c%c%c%c%c%c", IM_NUMERIC_COMP, param->sec_numeric_ind.conhdl,
                            param->sec_numeric_ind.numeric[0],param->sec_numeric_ind.numeric[1],
                            param->sec_numeric_ind.numeric[2],param->sec_numeric_ind.numeric[3],
                            param->sec_numeric_ind.numeric[4],param->sec_numeric_ind.numeric[5]);
                        node->smp_state = SMP_NUMERIC_RESPONSE;
                    }
                }
                else{
                    ble_send_sec_numeric_cfm(param->sec_numeric_ind.conhdl, true);
                }
            }
            break;
        case BLE_SMP_KEYMISSING:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->sec_key_miss.conhdl);
                if(node != NULL) {
                    app_ble_disc_cmd_t *arg = NEW(sizeof(app_ble_disc_cmd_t));
                    arg->conhdl = node->conhdl;
                    arg->reason = 0x06; //CO_ERROR_PIN_MISSING
                    TIMER_CREATE(500, (void *)arg, user_disc_dev_task);
                    ble_bond_db_erase_device(node->addr_type, node->addr);
                }   
            }
            break;
        case BLE_SMP_ENCRY_SUCCESS:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->sec_ency_cmp.conhdl);
                          
                if(node) {
                    if(node->role == 0) {
                        prf_dtsc_dsc_ccc_enable(node->conhdl);
                    }
                    if(node->smp_state == SMP_NONE) {
                        ble_get_identity_addr(node->conhdl, &node->public_bd.addr_type, node->public_bd.addr); 
                        if(!memcmp(node->public_bd.addr, "\x00\x00\x00\x00\x00\x00", BD_ADDR_LEN)){
                            memcpy(node->public_bd.addr, node->addr, BD_ADDR_LEN);
                            node->public_bd.addr_type = node->addr_type;
                        }
                        if(BLE_AUTOUNLOCK_GET() == true) {
                            USER_UART_EVENT_IND("%s:%04x,%02x%02x%02x%02x%02x%02x",IM_AUTH,
                                                                            node->conhdl,
                                                                            node->public_bd.addr[5],
                                                                            node->public_bd.addr[4],
                                                                            node->public_bd.addr[3],
                                                                            node->public_bd.addr[2],
                                                                            node->public_bd.addr[1],
                                                                            node->public_bd.addr[0]);
                            conn_record_t *arg = NEW(sizeof(conn_record_t));
                            if(arg != NULL) {
                                memcpy(arg, node, sizeof(conn_record_t));
                                TIMER_CREATE(5000, arg, user_auto_update_conn_param);
                            }  
                        }   
                    }
                    node->smp_state = SMP_PAIR_SUCCESS;
                }      
            }
            break;
       case BLE_SMP_PAIR_SUCCESS:
            {
                conn_record_t *node = user_device_find_by_conhdl(param->sec_pair_cmp.conhdl);
                if(node) {
                    node->smp_state = SMP_PAIR_SUCCESS;  
                    ble_get_identity_addr(node->conhdl, &node->public_bd.addr_type, node->public_bd.addr); 
                    if(!memcmp(node->public_bd.addr, "\x00\x00\x00\x00\x00\x00", BD_ADDR_LEN)){ ///fixme,This logic is usually skipped.
                        memcpy(node->public_bd.addr, node->addr, BD_ADDR_LEN);
                        node->public_bd.addr_type = node->addr_type;
                    }
                    if(BLE_AUTOUNLOCK_GET() == true) {
                        if(user_while_list_find(&node->public_bd) && param->sec_pair_cmp.bond_data_len) {
                            ble_bond_db_add_device(param->sec_pair_cmp.bond_data);
                        }
                        else {
                            node->bond_info = param->sec_pair_cmp.bond_data_len ? NEW(param->sec_pair_cmp.bond_data_len) : NULL;
                            if(node->bond_info != NULL) {
                                memcpy(node->bond_info, param->sec_pair_cmp.bond_data, param->sec_pair_cmp.bond_data_len);
                            }
                            else if(param->sec_pair_cmp.bond_data_len) {/// Save immediately.
                                ble_bond_db_add_device(param->sec_pair_cmp.bond_data);
                            }   
                        }
                        USER_UART_EVENT_IND("%s:%04x,%02x%02x%02x%02x%02x%02x",IM_AUTH,
                                                                            node->conhdl,
                                                                            node->public_bd.addr[5],
                                                                            node->public_bd.addr[4],
                                                                            node->public_bd.addr[3],
                                                                            node->public_bd.addr[2],
                                                                            node->public_bd.addr[1],
                                                                            node->public_bd.addr[0]);
                        conn_record_t *arg = NEW(sizeof(conn_record_t));
                        if(arg != NULL){
                            memcpy(arg, node, sizeof(conn_record_t));
                            TIMER_CREATE(5000, arg, user_auto_update_conn_param);
                        }  
                    }
                    else if(param->sec_pair_cmp.bond_data_len){
                        ble_bond_db_add_device(param->sec_pair_cmp.bond_data);
                    }
                }
            }
            break;   
       case BLE_SMP_PAIR_FAILED: 
            {
                conn_record_t *node = user_device_find_by_conhdl(param->sec_pair_failed.conhdl);
                if(node) {
                    ble_gap_disconn(node->conhdl, 0x13/*CO_ERROR_REMOTE_USER_TERM_CON*/);
                }    
            }
            break;
        default :
            break;
    }
}

/**
 * @}
 */
