/**
 *******************************************************************************
 *
 * Copyright (c) 1999-2023 BRT Techonology Limited
 *
 * All rights reserved.
 *
 * -----------------------------------------------------------------------------
 *
 * @file    app_ble.h
 * @author  Qiu
 *
 *******************************************************************************
 */

#ifndef _APP_BLE_H_
#define _APP_BLE_H_

#include <stdbool.h>
#include "br1601x.h"
#include "ble_gap.h"
#include "prf_dtss.h"
#include "porting.h"
#include "app_ble_cfg.h"
#include "hogp.h"

extern uint8_t g_ble_command;
extern uint8_t g_ble_multi;
extern uint8_t g_ble_mode;
extern uint8_t g_ble_autounlock;


/// Invalid activity index
#define GAP_INVALID_ACTV_IDX                    ((void *)0xFFFFFFFF)
/// Invalid connection index
#define GAP_INVALID_CONIDX                      ((void *)0xFFFFFFFF)
#define GAP_INVALID_CONHDL                      0xFFFF

#define MASTER_ROLE                             0
#define SLAVE_ROLE                              1
#define UNKNOWN_ROLE                            0xFF   //Used in LC to init the links role

#define GATT_MTU_DEFAULT                        23

#define BLE_MODE()				    g_ble_mode
#define BLE_COMMAND_GET()	 		(g_ble_command ?  true : false)
#define BLE_MULTI_GET() 		    (g_ble_multi ? true : false)
#define BLE_COMMAND_SET(mode)	    g_ble_command = (mode)
#define BLE_MULTI_SET(multi)	    g_ble_multi = (multi)
#define BLE_AUTOUNLOCK_GET()        (g_ble_autounlock ? true : false)

enum {
    BLE_ERR_NONE,
    BLE_ERR_FAIL,
};
    
enum BLE_MODE{
     BLE_MODE_S = 0,   /// 从机   
     BLE_MODE_M,        /// 主机
     BLE_MODE_MS,       ///主从一体
};


#if 0
typedef enum app_ble_event {
    BLE_EV_ADV_MODE_CHANGE,
    BLE_EV_ADV_TERMINATED,
    BLE_EV_CONNECTED,
    BLE_EV_DISCONNECTED,
    BLE_EV_NOTIFY_ENABLED,
    BLE_EV_ENCRYPTED,
    BLE_EV_BOND_REQ,
    BLE_EV_BONDED,
    BLE_EV_CONN_PARAM_UP,
} app_ble_event_t;
#endif

//typedef struct{
//    ///6-byte array address value
//    uint8_t  addr[BD_ADDR_SIZE];
//}bd_addr_t;

//typedef struct 
//{
//    /// Connection interval minimum
//    uint16_t intv_min;
//    /// Connection interval maximum
//    uint16_t intv_max;
//    /// Latency
//    uint16_t latency;
//    /// Supervision timeout
//    uint16_t time_out;
//}conn_param_t;

struct bd_addr{
    uint8_t addr[BD_ADDR_LEN];  /*对端设备地址*/
};

typedef struct {
     uint8_t addr_type;
     uint8_t addr[BD_ADDR_LEN];
}ble_addr_t;

/**
 * @brief 发起连接对应参数结构。
 */
typedef ble_addr_t ble_conn_req_t;


typedef struct {
    /// Connection handle
    uint16_t conhdl;
	/// mtu
	uint16_t mtu;
    /// Peer BT address
    struct bd_addr peer_addr;
	/// Peer address type
    uint8_t peer_addr_type;
    /// local role
    uint8_t role;
}app_ble_gap_connection_ind_t;

typedef struct {
    /// Connection handle
    uint16_t conhdl;
    /// Reason of disconnection
    uint8_t reason;
}app_ble_gap_disconnect_ind_t;

typedef struct {
    /// Connection interval minimum
    uint16_t intv_min;
    /// Connection interval maximum
    uint16_t intv_max;
    /// Latency
    uint16_t latency;
    /// Supervision timeout
    uint16_t time_out;
}app_ble_gap_param_update_req_ind_t;

/// Indication of ongoing connection RSSI
typedef struct {
    uint16_t conhdl;
    /// RSSI value
    int8_t rssi;
}app_ble_gap_rssi_ind_t;

typedef struct {
    /// Exchanged MTU value
    uint16_t mtu;
//    /// operation sequence number
//    uint16_t seq_num;
	/// conhal 
	uint16_t conhdl;
}app_ble_gatt_mtu_changed_ind_t;

typedef struct {
    uint16_t            conhdl;
    ///Connection interval value
    uint16_t            con_interval;
    ///Connection latency value
    uint16_t            con_latency;
    ///Supervision timeout
    uint16_t            sup_to;
}app_ble_gap_param_updated_ind_t;

typedef struct{
	uint8_t actv_idx;
    /// Bit field providing information about the received report (@see enum gapm_adv_report_info)
    uint8_t info;
    /// trans address (in case of a directed advertising report)
    struct bd_addr peer_addr;
	/// Peer address type
    uint8_t peer_addr_type;
    /// TX power (in dBm)
    int8_t tx_pwr;
    /// RSSI (between -127 and +20 dBm)
    int8_t rssi;
    /// Primary PHY on which advertising report has been received
    uint8_t phy_prim;
    /// Secondary PHY on which advertising report has been received
    uint8_t phy_second;
    /// Advertising SID
    /// Valid only for periodic advertising report
    uint8_t adv_sid;
    /// Periodic advertising interval (in unit of 1.25ms, min is 7.5ms)
    /// Valid only for periodic advertising report
    uint16_t period_adv_intv;
    /// Report length
    uint16_t length;
    /// Report
    uint8_t *data;
} app_ble_gap_ext_adv_report_ind_t;


typedef enum{
    /*==========GAP==========*/
	BLE_READY = 0,
    /*connect*/
    BLE_GAP_CONNECTED,              /*Connection Complete*/
    BLE_GAP_DISCONNEDTED,           /*disconnect*/
    BLE_GAP_CONNECT_TIMEOUT,        /*connection timeout*///Future use
    BLE_GAP_CONNECT_PARAM_UPDATE,   /*Connection parameter update*/
    BLE_GAP_FEATURE_IND,            /*peer feature ind*/
    BLE_GAP_VERSION_IND,            /*peer version ind*/
    BLR_GAP_RSSI_IND,               /*peer rssi complete ind*/
    /*scan*/
    BLE_GAP_SCAN_REPORT,            /*Scan report*/
		
    /*==========GATT==========*/
    BLE_GATT_MTU_EXCHANGE = 0x100,  /*gatt mtu changed*/
    /*======DATA TRANS Server======*/
    BLE_GATTS_TRANS_DATA_CCCD_RPT,  /*Client Write DATA CCCD*/
    BLE_GATTS_TRANS_FC_CCCD_RPT,    /*Client Write FC CCCD*/
    BLE_GATTS_TRANS_DATA_RPT,       /*Client Write Data, Through data channel */
    BLE_GATTS_TRANS_FC_RPT,         /*Client Write FlowCtrl data channel*/
    BLE_GATTS_TRANS_USR_RPT,        /*Client Write Data, Through user channel*/ //Future use
    BLE_GATTS_TRANS_DATA_CMP,       /*Server Write Data complete*/
    BLE_GATTS_TRANS_FC_CMP,         /*Server Write FlowCtrl data complete*/
    /*======Label Image Server======*/
    BLE_GATTS_IMAGE_CCCD_RPT,
    BLE_GATTS_IMAGE_DATA_RPT,
    BLE_GATTS_IMAGE_DATA_CMP,
    /*======OTA Server======*/
    BLE_GATTS_OTA_CCCD_REPORT,      /*Client Write CCCD */ //Future use
    BLE_GATTS_OTA_REPORT,           /*OTA Data*/ //Future use
    BLE_GATTS_OTA_CMP,              /*OTA end*/ //Future use
    BLE_GATTS_OTA_ATCMD_REPORT,     /*The atcmd characteristics under the OTA (Over-The-Air) service.*/
    /*======CMD Server======*/
    BLE_GATTS_CMD_REPORT,           /*Client Write CMD*/ //Future use
    /*======DATA TRANS Client======*/
    BLE_GATTC_NO_TARGET_SVC,        /*The target service search is incomplete*/
    BLE_GATTC_TARGET_READY,         /*Target service lookup completed*/
    BLE_GATTC_TRANS_REPORT,         /*Server Write Data*/
	BLE_GATTC_TRANS_CREDIT,         /*Server Write Credit*/
	BLE_GATTC_TRANS_CMP,            /*Client Write Data or Credit complete*/
	BLE_GATTC_TRANS_CCCD_CMP,       /*Client Write CCCD complete*/
    BLE_GATTC_TRANS_CCCD_FAILED,    /*Client Write CCCD Failed*/
	BLE_GATTC_TRANS_INSUFF,
    /*==========SMP==========*/
    BLE_SMP_PAIR_REQ = 0x200,       /*The peer end initiates a pairing request*/
    BLE_SMP_PASSKEY_DISPLAY,        /*Key display*/
    BLE_SMP_PASSKEY_ENTRY_REQ,      /*Request a key*/
    BLE_SMP_NUMERIC_CMP,            /*Digital comparison complete*/ //Future use
    BLE_SMP_ENCRY_EXCHANGE,         /*Encryption change*/
    BLE_SMP_KEY_UPDATE,             /*linkkey change*/
    BLE_SMP_KEY_REQ,                /*linkkey request*/
    BLE_SMP_KEY_FOUND,              /*linkkey finding*/
    BLE_SMP_KEYMISSING,             /*linkkey missing*/     
    BLE_SMP_ENCRY_SUCCESS,          /*Encryption complete*/
    BLE_SMP_PAIR_SUCCESS,           /*Pairing complete*/
    BLE_SMP_PAIR_FAILED,            /*Pairing failed*/
}app_ble_evt_t;



typedef struct {
	uint16_t conhdl;
	uint16_t len;
	uint8_t const *data;
}app_ble_data_rec_t;


typedef struct{
    uint16_t conhdl;
    uint16_t data;
    uint16_t handle;
} app_ble_rcv_ntf_t;


typedef struct{
    uint16_t conhdl;
    uint8_t  handle;
} app_ble_ntf_cmp_t;

typedef struct{
    uint16_t conhdl;
}app_ble_target_t;


typedef app_ble_target_t app_ble_sec_pairing_req_t;
typedef app_ble_target_t app_ble_sec_req_t;
typedef app_ble_target_t app_ble_sec_passkey_entry_t;
typedef app_ble_target_t app_ble_sec_ency_cmp_t;
typedef app_ble_target_t app_ble_sec_key_miss_t;
typedef app_ble_target_t app_ble_sec_ltk_found_t;
typedef app_ble_target_t app_ble_sec_pair_failed_t;

typedef struct {
    uint16_t conhdl;
    uint8_t *bond_data;
    uint16_t bond_data_len;
}app_ble_sec_pair_cmp_t;

typedef struct {
    uint16_t conhdl;
    uint8_t feature[8];
}app_ble_gap_feature_ind_t;

typedef struct {
    uint16_t conhdl;
    uint8_t   verion;
    uint16_t  manufacture_name;
    uint16_t  subversion;
}app_ble_gap_version_ind_t;

typedef struct {
    uint16_t conhdl;
    uint8_t  status;
}app_ble_gatt_insuff_auth_t,
app_ble_write_ccc_cmp_t,
app_ble_write_ccc_failed_t;

typedef struct{
    uint16_t  conhdl;
    uint8_t  passkey[6];
}app_ble_sec_passkey_ind_t;

typedef struct{
    uint16_t  conhdl;
    uint8_t  *numeric;
}app_ble_sec_numeric_ind_t;

typedef union {
//        struct gapm_cmp_evt const*              p_gapm_cmp;
    app_ble_gap_connection_ind_t 				connection_ind;
    app_ble_gap_disconnect_ind_t 				disconnect_ind;
    app_ble_gap_param_update_req_ind_t 			param_req;
//        struct gapc_cmp_evt const*              p_gapc_cmp;
    app_ble_gap_rssi_ind_t                      rssi_ind; 
    app_ble_gatt_mtu_changed_ind_t 				gattc_mtu;
    app_ble_gap_param_updated_ind_t 			param_updated;
    app_ble_gap_feature_ind_t                   feature_ind;
    app_ble_gap_version_ind_t                   version_ind;
	app_ble_data_rec_t 							data_rcv;
	app_ble_gap_ext_adv_report_ind_t 			adv_report;
    app_ble_rcv_ntf_t                           rcv_ntf;
    app_ble_ntf_cmp_t                           ntf_cmp;
    app_ble_write_ccc_cmp_t                     ccc_cmp;
    app_ble_write_ccc_failed_t                  ccc_failed;
    app_ble_gatt_insuff_auth_t                  insuff_auth;
    app_ble_target_t                            targe;
    app_ble_sec_req_t                           sec_req;
    app_ble_sec_passkey_ind_t                   sec_psk_ind;  
    app_ble_sec_passkey_entry_t                 sec_psk_entry;
    app_ble_sec_numeric_ind_t                   sec_numeric_ind;
    app_ble_sec_ency_cmp_t                      sec_ency_cmp;
    app_ble_sec_ltk_found_t                     sec_ltk_found;
    app_ble_sec_key_miss_t                      sec_key_miss;
    app_ble_sec_pair_cmp_t                      sec_pair_cmp;
    app_ble_sec_pair_failed_t                   sec_pair_failed;

}app_ble_msg_t;


typedef struct{
    uint16_t intv; /*连接参数最大值*/
    uint16_t latency;      /*连接允许从机最多忽略连接事件数*/
    uint16_t time_out;      /*连接丢失超时时间*/
}gap_conn_param_t;


typedef struct {
    /// Connection handle
    uint16_t conhdl;
    /// Connection Index
    void *  conidx;
    /// Last paired peer address type
    uint8_t peer_addr_type;
    /// Last paired peer address
    struct bd_addr peer_addr;
    /// public address type
    uint8_t public_addr_type;
    /// public address
    struct bd_addr public_addr;
    /// Maximum device MTU size
    uint16_t mtu;
    ///DLE, the maximum number of payload octets in TX
    uint16_t tx_pkt_size;    
    /// Role of device in connection (0 = Master / 1 = Slave)
    uint8_t role;        
    gap_conn_param_t conn_par;
}app_con_env_t;

/**
 * @brief Type of the connection event callback function
 *
 * This is the type of the function that will be called when a connection event occurs.
 *
 * @param [in] ev  The connection event
 */
typedef void (*app_ble_event_cbk_t)(app_ble_evt_t ev, app_ble_msg_t *par);

/// Application environment structure
typedef struct {
    /// Connection handle
    uint16_t conhdl;
    /// Connection Index
    void * conidx;
    /// Last paired peer address,reserver
    uint8_t peer_addr_type;
     /// Last paired peer address,,reserver
    struct bd_addr peer_addr;
    /// Maximum device MTU size
    uint16_t max_mtu;
    ///DLE, the maximum number of payload octets in TX,resever
    uint16_t tx_pkt_size;
    //master device 
    bool initing;
    app_con_env_t conn_env[APP_CON_IDX_MAX];
    //ble msg hanlder
    app_ble_event_cbk_t ble_msg_handler;
}app_env_t;




typedef struct {
    uint8_t prf_num;
    void (*prf_init_func[8])(void);
}ble_prf_init_t;

typedef struct {
    void (*ble_msg_handler)(void const *);
} app_ble_init_params_t;

typedef struct{
    uint16_t conhdl;
    uint32_t reason;
}app_ble_disc_cmd_t;



/**
 * @brief Initializes the BLE application
 *
 * This function is used to initialize the BLE application. It sets up the necessary BLE services,
 * characteristics, and advertising data. It should be called before any other BLE operations.
 */
void app_ble_init(app_ble_event_cbk_t func);

/**
 * @brief Initiates the advertising process
 *
 * This function triggers the start of the advertising process based on the specified mode. 
 * It activates the advertising module, which allows the device to broadcast its presence.
 *
 * @param [in] mode  The mode of advertising, which determines the advertising parameters
 */
 
/**
 * @brief Function to stop advertising
 *
 * This function stops the advertising by disabling the advertising module.
 * It should be called when the device no longer needs to advertise its presence.
 */
void app_ble_stop_adv(void);

/**
 * @brief Function to set slave latency
 *
 * This function set local slave latency.
 *
 * @param [in] latency  local slave latency
 */
void app_ble_set_slave_latency(uint16_t latency);

/**
 * @brief Function to recover slave latency
 *
 * This function recover local slave latency to previous value.
 */
void app_ble_recover_slave_latency(void);

/**
 * @brief Function to set whether disconnect if connection is fake
 *
 * This function set whether disconnect if connection is fake.Default is enable.
 *
 * @param [in] enable  ture for disconnect,otherwise not
 */
void app_ble_set_fake_conn_detect_enable(bool enable);

/**
 * @brief Retrieves the current advertising mode
 *
 * This function is used to obtain the current mode of advertising that the device is operating in.
 * It can be utilized whenever there's a need to check the advertising status of the device.
 *
 * @return The mode of advertising currently active
 */

/**
 * @brief: Retrieves the connection status of the device
 * @return {*} true if the device is connected, false otherwise
 */
bool app_ble_is_connected(void);

/**
 * @brief Retrieves the bond status of the device
 *
 * This function is used to check if the device is currently bonded with another device.
 * Bonding is a process in BLE where devices exchange and store pairing information to establish a trusted relationship.
 *
 * @return true if the device is bonded, false otherwise
 */
bool app_ble_is_bonded(void);

/**
 * @brief disconnect with peer device
 *
 */
void app_ble_disconnect(void);

/**
 * @brief Unbinds a peer device
 *
 * This function is used to unbind a peer device. Unbinding a device removes the bonding information,
 * and the device will no longer be recognized as a trusted device.
 *
 * @param [in] adv_able true:start pair ADV ; false: no processing
 */
void app_ble_unbind(bool adv_able);

/**
 * @brief Initializes the hid application
 *
 * This function is used to initialize the hid application.
 */
void app_hid_init(void);

/**
 * @brief Check whether the device is ready to send report.
 *
 * This function is used to check if the device is currently ready to send report.
 *
 * @return true if the device is ready, false otherwise
 */
bool app_hid_is_ready(void);

/**
 * @brief Send hid audio data
 *
 * This function send audio data report to remote host.
 *
 * @param [in] data  data
 * @param [in] len   length
 *
 * @return true if sent, false otherwise
 */
bool app_hid_send_audio(uint8_t *data, uint16_t len);

/**
 * @brief Notify battery level
 *
 * This function notify battery level to remote host.
 * battery level will get through ext_get_bat_level, 
 * ext_get_bat_level function should be implemented before calling app_hid_send_bat_level
 *
 */
void app_hid_ntf_bat_level(void);
void *ble_get_conidx_by_conhdl(uint16_t conhdl);
uint16_t ble_get_conhdl_by_conidx(void * conidx);
uint8_t ble_get_role_by_conhdl(uint16_t conhdl);
uint32_t ble_send_data(uint16_t conhdl,const uint8_t *data, uint16_t len);
uint32_t ble_send_credit(uint16_t conhdl, uint8_t credit);
uint32_t ble_gap_connect_cancel(void);
uint32_t ble_get_slave_connection_num(void);
uint32_t ble_get_master_connection_num(void);
uint32_t ble_gap_scan_enable(bool enable);
uint32_t ble_gap_adv_enable(bool enable);
uint32_t ble_gap_connect(ble_conn_req_t *addr);
bool ble_update_param(uint16_t conhdl, ble_conn_param_t *p_conn_param);
void ble_set_slave_latency(uint16_t conhdl, uint16_t latency);
uint32_t ble_gap_name_set(const char* name, uint8_t len);
//uint32_t ble_adv_data_set(ble_adv_data_t *adv_data);
bool ble_scan_data_find_by_type(uint8_t types, const uint8_t *p_filter_data, uint8_t *p_data, uint8_t len);
bool ble_scan_data_find(const uint8_t *p_filter_data, uint8_t filter_len, uint8_t *p_data, uint8_t len);
void ble_dle_set(uint16_t conhdl, uint16_t tx_octets, uint16_t tx_time);
void ble_mtu_set(uint16_t conhdl, uint16_t mtu);
void ble_rssi_get(uint16_t conhdl);
void ble_get_peer_feature(uint16_t conhdl);
void ble_get_peer_version(uint16_t conhdl);
uint32_t ble_gap_disconn(uint16_t conhdl, uint8_t reason);
void ble_event_callback(app_ble_evt_t evt, app_ble_msg_t *param);
void ble_get_identity_addr(uint16_t conhdl, uint8_t *addr_type, uint8_t *addr);
bool ble_is_data_sendable(uint16_t conhdl);
void ble_set_def_phy(uint8_t tx_phys, uint8_t rx_phys);
void ble_send_hid_report(hid_rpt_t in_rpt,peer_t peer);
#if defined(BR1601A01) || defined(BR1601A02)
void ble_scan_en_update(void);

#endif
#endif

