#ifndef _BLE_HOGP_H
#define _BLE_HOGP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>


#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "hogp.h"


#define CFG_PEER_INFO_MAX_LEN       100
#define CFG_MAX_NAME_LEN            40

typedef enum app_adv_mode
{
  	APP_ADV_NONE,
	APP_ADV_PAIR,
	APP_ADV_RECONN,
	APP_ADV_WAKEUPA,
	APP_ADV_WAKEUPB,
	APP_ADV_WAKEUPC,

	APP_ADV_PARMS_INDEX_MAX
} app_adv_mode_t;

typedef enum app_hid_report_id
{
    APP_HID_EMPTY_REPORT_ID, // = 0x00
    APP_HID_KEYBOARD_REPORT_ID,
    APP_HID_CONSUMER_REPORT_ID,
    APP_HID_MOUSE_REPORT_ID,
    APP_HID_AUDIO_REPORT_ID,
    APP_HID_AUDIO_CTRL_REPORT_ID,
} app_hid_report_id_t;

typedef enum app_ble_status_bits {
    BLE_STATUS_BIT_NONE          = 0x00, /** No status */
    BLE_STATUS_BIT_CONNECTED     = (0x01 << 0), /** Connection is established */
    BLE_STATUS_BIT_NOTIFY_ENABLED= (0x01 << 1), /** Notify is enabled */
    BLE_STATUS_BIT_ENCRYPTED     = (0x01 << 2), /** Connection is encrypted */
} app_ble_status_bits_t;


typedef struct app_ble_peer_inf {
    /*
    * need to save to flash
    */
    peer_t peer;    /** Peer device instance. */
    uint8_t addr_type;  /** Peer device address type. */
    bd_addr_t addr;    /** Peer device address. */
    uint8_t  inf[CFG_PEER_INFO_MAX_LEN];     /** Peer information */
    uint32_t  inf_sz;  /** Peer information size, in bytes */

    /*
    * do not need to save to flash

    */
    app_ble_status_bits_t status;    /** Peer device status. */
    uint8_t nb_report;  /** Number of report can be send. */
	/* connection parameter */
	uint16_t  conn_intv;
    uint16_t  lantency_max;
    uint16_t  timeout;
	/* data length change */
	uint16_t maxtxoctets;
	uint16_t maxtxtime;
	uint16_t maxrxoctets;
	uint16_t maxrxtime;
	/* phy update */
	uint8_t tx_phy;
	uint8_t rx_phy;
    /* att mtu */
    uint16_t mtu;
} app_ble_peer_inf_t;

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
} app_ble_evt_t;

typedef void (*app_ble_event_cbk_t)(app_ble_evt_t ev, void *par);

typedef struct dev_inf_stru {
          uint16_t  pver;      /** Product Version */
          uint16_t  pid;       /** Product ID */
          uint16_t  vid;       /** Vendoro ID */
          uint8_t   vid_src;   /** Vendoro ID Source */
          uint32_t  sys_id;    /** System ID */
    const char     *manu_name; /** Manufacturer Name String, NULL-Terminated String */
    const char     *model_nm;  /** Model Number String, NULL-Terminated string */
    const char     *serial_nm; /** Serial Number String, NULL-Terminated string */
    const char     *hd_rev;    /** Hardware Revision String, NULL-Terminated string */
    const char     *fw_rev;    /** Firmware Revision String, NULL-Terminated string */
    const uint8_t  *rcdl;      /** IEEE 11073-20601 Regulatory Certification Data
                                   List */
          size_t    rcdl_len;  /** IEEE 11073-20601 Regulatory Certification Data
                                   List length */
    const uint8_t  *udi;       /** UDI for Medical Devices */
          size_t    udi_len;   /** UDI for Medical Devices length */
} dev_inf_t;

// device information structure
typedef struct
{
    uint8_t  mac_addr[6];
    uint8_t  bt_name[CFG_MAX_NAME_LEN];
    uint8_t  bt_name_len;
    bool adv_en;
    app_adv_mode_t cur_adv_mode;
    app_ble_peer_inf_t peer_inf;
    app_ble_event_cbk_t *event_cbk;
    /* hogp part*/
    hid_inf_t hid_inf;
    dev_inf_t dev_inf;
} local_info_t;

typedef struct peer_store_info {
    peer_t peer;    /** Peer device instance. */
    uint8_t addr_type;  /** Peer device address type. */
    bd_addr_t addr;    /** Peer device address. */
    uint8_t  inf[CFG_PEER_INFO_MAX_LEN];     /** Peer information */
    uint32_t  inf_sz;  /** Peer information size, in bytes */
} peer_store_info_t;

void app_hid_init(void);

#ifdef __cplusplus
}
#endif

#endif
