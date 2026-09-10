/**
 *******************************************************************************
 *
 * Copyright (c) 1999-2023 BRT Techonology Limited
 *
 * All rights reserved.
 *
 * -----------------------------------------------------------------------------
 *
 * @file    app_ble.c
 * @author  Qiu
 *
 *******************************************************************************
 */
#include <string.h>
#include "gap.h"
#include "gatt.h"
#include "ble_gap.h"
#include "ble_adv.h"
#include "ble_sec.h"
#include "ble_scan.h"
#include "prf_bas.h"
#include "prf_dis.h"
#include "prf_gap.h"
#include "prf_gatt.h"
#include "prf_dtss.h"
#include "prf_image.h"
#include "prf_dfu.h"
#include "prf_hid.h"
#include "prf_dtsc.h"
#include "prf_cmd.h"
#include "app_ble_cfg.h"
#include "app_ble.h"
#include "app_image_service.h"
#include "app_label_adv_runtime.h"
#include "cfg.h"
#include "cs_core.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

app_env_t      app_env;
uint8_t g_ble_command = 0;
uint8_t g_ble_multi = 0;
uint8_t g_ble_mode = 0;
uint8_t g_ble_autounlock = 0;

/* Private function prototypes -----------------------------------------------*/
void *ble_get_conidx_by_conhdl(uint16_t conhdl)
{
    return app_env.conn_env[conhdl].conidx;
}

uint16_t ble_get_conhdl_by_conidx(void * conidx)
{
    uint16_t conhdl = GAP_INVALID_CONHDL;
    for (uint8_t i = 0;i < APP_CON_IDX_MAX;i++){
        if ((app_env.conn_env[i].conidx == conidx) && (app_env.conn_env[i].conhdl != GAP_INVALID_CONHDL)){
            conhdl = i;
            break;
        }
    }
    return conhdl;   
}

uint8_t ble_get_role_by_conhdl(uint16_t conhdl)
{
    return app_env.conn_env[conhdl].role;
}

static void * ble_get_active_connection(void)
{
    return app_env.conidx;
}

uint32_t ble_gap_connect(ble_conn_req_t *addr)
{
    uint32_t ret = BLE_ERR_FAIL;
    if(!app_env.initing){
        le_conn_par_t target;
        target.peer_addr_type = addr->addr_type;
        memcpy(target.peer_addr, addr->addr, BD_ADDR_LEN);
        le_conn_create(&target, NULL);
        app_env.initing = true;
#if defined(BR1601A01) || defined(BR1601A02)
        br_scan_en_set(true);//todo
        br_modem_set(PHY_1MBPS);
#endif
        ret = BLE_ERR_NONE;
    }
    return ret;
}

uint32_t ble_gap_connect_cancel(void)
{
    uint32_t ret = BLE_ERR_FAIL;
    if(app_env.initing){
        le_conn_cancel();
        app_env.initing = false; 
        ret = BLE_ERR_NONE;
    }
	return ret;
}

uint32_t ble_gap_disconn(uint16_t conhdl, uint8_t reason)
{
    uint32_t ret = BLE_ERR_NONE;
    void *cif  = GAP_INVALID_CONIDX;

	if(conhdl == GAP_INVALID_CONHDL)
		cif = ble_get_active_connection();
	else
		cif = ble_get_conidx_by_conhdl(conhdl);

	if(cif == GAP_INVALID_CONIDX){
		return BLE_ERR_FAIL;
	}
    le_conn_destory(cif);
    
    return ret;
}

void ble_mtu_set(uint16_t conhdl, uint16_t mtu)
{
    gatts_exchange_mtu(ble_get_conidx_by_conhdl(conhdl), mtu);
}

void ble_rssi_get(uint16_t conhdl)
{
    le_conn_read_rssi(ble_get_conidx_by_conhdl(conhdl));
}

bool ble_scan_data_find_by_type(uint8_t types, const uint8_t *p_filter_data, uint8_t *p_data, uint8_t len )
{
    uint8_t pack_len;
    uint8_t pack_type;
    while(len)
    {
        // check if pack incomplete
        if(len < (*p_data)+1 || len < 3)
        {
            break;
        }
        pack_len = (*p_data)-1; // get pack len
        p_data++;
        pack_type = *p_data; // get pack type
        p_data++;
        if(pack_type == types) //
        {
            if(memcmp(p_data,p_filter_data,pack_len) == 0)
            {
                return true;
            }
        }
        p_data += pack_len; //next pack
        len    -= (pack_len+2);
    }

    return false;
}

bool ble_scan_data_find(const uint8_t *p_filter_data, uint8_t filter_len, uint8_t *p_data, uint8_t len)
{
    if(!p_filter_data || !filter_len || !p_data || !len) return false;

    while(len)
    {
        if(len < filter_len) {
            return false;
        }
        if(memcmp(p_data, p_filter_data, filter_len) == 0) {
            return true;
        }
        p_data++; //next pack
        len--;
    }
    return false;
}

///Parse broadcast data to obtain the payload data of a specific field.
bool ble_scan_data_get(uint8_t types, uint8_t *p_macth_data, uint8_t *p_data, uint8_t len)
{
    uint8_t pack_len;
    uint8_t pack_type;
    while(len)
    {
        // check if pack incomplete
        if(len < (*p_data)+1 || len < 3)
        {
            break;
        }
        pack_len = (*p_data)-1; // get pack len
        p_data++;
        pack_type = *p_data; // get pack type
        p_data++;
        if(pack_type == types)
		{
            memcpy(p_macth_data,p_data,pack_len);
            return true;
        }
        p_data += pack_len; //next pack
        len    -= (pack_len+2);
    }

    return false;
}

bool ble_update_param(uint16_t conhdl, ble_conn_param_t *p_conn_param)
{
    BR_LOG_DEBUG("%s\r\n",__func__);
    le_conn_update_t par = {0};

    par.peer = ble_get_conidx_by_conhdl(conhdl);
    par.intv_min = p_conn_param->intv_min;
    par.intv_max = p_conn_param->intv_max;
    par.latency_max = p_conn_param->latency;
    par.timeout = p_conn_param->time_out;
    par.ce_len_min = 0; 
    par.ce_len_max = 0x140; ///Synchronization changes, solving the problem of multiple connections as the master.
    
    le_peer_conn_update(&par);

    return true;
}

void ble_set_slave_latency(uint16_t conhdl, uint16_t latency)
{
    if(ble_get_role_by_conhdl(conhdl) == SLAVE_ROLE){
        le_conn_set_slave_latency(ble_get_conidx_by_conhdl(conhdl), latency);
    }
}

void ble_dle_set(uint16_t conhdl, uint16_t tx_octets, uint16_t tx_time)
{  
    le_conn_set_data_length(ble_get_conidx_by_conhdl(conhdl), tx_octets, tx_time);
}

uint32_t ble_get_slave_connection_num(void)
{
	uint8_t  cnt = 0;
    for (uint8_t i =0;i<APP_CON_IDX_MAX;i++)
    {
        if ((app_env.conn_env[i].conidx != GAP_INVALID_CONIDX) && (app_env.conn_env[i].role == ROLE_SLAVE))
            cnt++;
    }
    return cnt;
}

uint32_t ble_get_master_connection_num(void)
{
	uint8_t  cnt = 0;
    for (uint8_t i =0;i<APP_CON_IDX_MAX;i++)
    {
        if ((app_env.conn_env[i].conidx != GAP_INVALID_CONIDX) && (app_env.conn_env[i].role == ROLE_MASTER))
            cnt++;
    }
    return cnt;
}

uint16_t ble_get_mtu_by_handle(uint16_t handle)
{
	uint16_t  mtu = GATT_MTU_DEFAULT;
    for (uint8_t i = 0;i < APP_CON_IDX_MAX;i++)
    {
        if ((app_env.conn_env[i].conidx != GAP_INVALID_CONIDX) && (app_env.conn_env[i].conhdl == handle))
        {
            mtu = app_env.conn_env[i].mtu;
            break;
        }
    }
	return mtu;
}

void ble_init(app_ble_event_cbk_t func)
{
    memset(&app_env, 0xFF, sizeof(app_env_t));
    extern void host_init(void);
    host_init();
    app_env.initing = false;
    app_env.ble_msg_handler = func;
}

uint32_t ble_gap_adv_enable(bool enable)
{
    uint32_t ret = BLE_ERR_NONE;

    if(enable == ble_adv_state_get())
        return ret;
    
    if(enable)
        ble_adv_start();
    else
        ble_adv_stop();

    return ret;
}

void ble_get_peer_feature(uint16_t conhdl)
{
    le_conn_read_feature(ble_get_conidx_by_conhdl(conhdl));
}

void ble_get_peer_version(uint16_t conhdl)
{
    le_conn_read_version(ble_get_conidx_by_conhdl(conhdl));
}

uint32_t ble_gap_scan_enable(bool enable)
{
    uint32_t ret = BLE_ERR_NONE;

    if(enable == ble_scan_state_get())
        return ret;
    if(enable)
        ble_scan_enable(); 
    else
        ble_scan_disable(); 
    return ret;
}

void ble_send_hid_report(hid_rpt_t in_rpt,peer_t      peer)
{
    in_rpt.peer = peer;
    in_rpt.rpt_type = HID_RPT_TYPE_INPUT;
    hid_rpt_send(&in_rpt);
}

void ble_event_callback(app_ble_evt_t evt, app_ble_msg_t *param)
{
	if(app_env.ble_msg_handler != NULL)
		app_env.ble_msg_handler(evt, param);
}

void app_ble_init(app_ble_event_cbk_t init_params)
{
    ble_gap_params_t gap_param = {0};
    ble_adv_params_t adv_param = {0};
    br_sec_init_t sec_param = {0}; 
    ble_scan_params_t scan_param = {0};
    cfg_general_t   item;

    cfg_get(CFG_TYPE_MULTI_CONN, &g_ble_multi, sizeof(g_ble_multi));
	cfg_get(CFG_TYPE_COMMAND_MODE, &g_ble_command, sizeof(g_ble_command));
	cfg_get(CFG_TYPE_BTMODE, &g_ble_mode,	sizeof(g_ble_mode));
	cfg_get(CFG_TYPE_AUTOUNLOCK, &g_ble_autounlock,	sizeof(g_ble_autounlock));
    ///gap param
    gap_param.conn_num_max = BLE_MAX_CONN;
    gap_param.mac_addr_type = ADDR_TYPE_PUBLIC;
    cfg_get(CFG_TYPE_ADDR, gap_param.mac_addr.addr, BD_ADDR_LEN);
    cfg_get(CFG_TYPE_GATT_MTU, &gap_param.mtu, sizeof(gap_param.mtu));
    cfg_get(CFG_TYPE_NAME, gap_param.dev_name, APP_DEVICE_NAME_MAX_LEN);
    gap_param.dev_name_len = strlen((const void *)gap_param.dev_name);
    cfg_get(CFG_TYPE_CONN_PARAMS, &gap_param.dev_conn_param, sizeof(cmd_conn_param_t));
    if(BLE_AUTOUNLOCK_GET()){        
        gap_param.appearance = 962;
    }
    ///sec param
    sec_param.bond_enable = SEC_PARAM_BOND;                
    sec_param.bond_max_peer = SEC_PARAM_BONDED_NUM, 
    cfg_get(CFG_TYPE_IOCAP, &sec_param.pairing_feat.iocap, sizeof(uint8_t));
    cfg_get(CFG_TYPE_PSK, &sec_param.pin_code, sizeof(uint32_t));   
    uint8_t lesc = 0;
    cfg_reserver_get(LESC_OFF, &lesc, LESC_LEN);
    sec_param.pairing_feat.auth |= lesc << 3;//GAP_AUTH_SEC_CON
    ///adv param
    adv_param.adv_data_len = app_label_adv_data_get(adv_param.adv_data,
                                                    sizeof(adv_param.adv_data));
    adv_param.attach_appearance = ADV_PARAM_ATTACH_APPEARANCE;
    memcpy(adv_param.scan_rsp_data, ADV_PARAM_SCAN_RSP_DATA, ADV_PARAM_SCAN_RSP_DATA_LEN);
    adv_param.scan_rsp_data_len = ADV_PARAM_SCAN_RSP_DATA_LEN;
    adv_param.attach_name = ADV_PARAM_ATTACH_NAME;
    adv_param.adv_phy = ADV_PARAM_PHY;
    cfg_get(CFG_TYPE_ADV_PARAMS, &item.adv_params, sizeof(item.adv_params));
    adv_param.adv_time.adv_intv = item.adv_params.adv_intreval_min;
    adv_param.adv_time.enable = true;
    adv_param.adv_time.duration = ADV_PARAM_DURATION;
    ///scan param
    scan_param.phy_coded_enable = SCAN_PARAM_CODED_PHY;
    scan_param.dup_filt_pol = SCAN_PARAM_DUP_FILT_POL;
    scan_param.duration = SCAN_PARAM_DURATION;
    scan_param.period = SCAN_PARAM_PERIOD;
    cfg_get(CFG_TYPE_SCAN_PARAMS, &item.scan_params, sizeof(item.scan_params));
    scan_param.prop_active_enable = item.scan_params.scan_type;
    scan_param.scan_intv = item.scan_params.scan_interval;
    scan_param.scan_wd = item.scan_params.scan_window;

    cs_init(gap_param.mac_addr.addr);///fixed
    ble_init(init_params);
    ble_gap_init(&gap_param);
    ble_sec_init(&sec_param);
    ble_adv_init(&adv_param);
    ble_scan_init(&scan_param);
    app_image_service_init();
    ///prf init
#if (CFG_PRF_GAP == 1)
    prf_gap_init();
#endif
#if (CFG_PRF_GATT == 1)
    prf_gatt_init();
#endif
#if (CFG_PRF_DIS == 1)
    prf_dis_init();
#endif
#if (CFG_PRF_BAS == 1)
    prf_bas_init();
#endif
#if (CFG_PRF_HOGPD == 1)    
    if(BLE_AUTOUNLOCK_GET() == TRUE)
        prf_hid_init();
#endif
#if (CFG_PRF_CMD == 1)
    prf_cmd_init();
#endif
#if (CFG_PRF_DTSS == 1)
    prf_dtss_init();
#endif
#if (CFG_PRF_IMAGE == 1)
    prf_image_init();
#endif
#if (CFG_PRF_DTSC == 1)
    prf_dtsc_init();
#endif
#if (CFG_PRF_DFU == 1)
    prf_dfu_init();
#endif
}

bool ble_is_data_sendable(uint16_t conhdl)
{
#define SINGLE_LINK_BUF_SIZE     1024
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
    uint32_t size = le_conn_get_sending_num(conidx) * gap_get_local_pkt_len(GAP_HCI_LINFO_PACKET_LE);
    if(size > SINGLE_LINK_BUF_SIZE)
        return false;
    else
        return true;
}

#if defined(BR1601A01) || defined(BR1601A02)
void ble_scan_en_update(void)
{
    br_scan_en_set(ble_scan_state_get() /*| app_env.initing*/);
}
#endif

uint32_t ble_send_data(uint16_t conhdl,const uint8_t *data, uint16_t len)
{
	uint32_t ret = BLE_ERR_NONE;
    uint8_t role = ble_get_role_by_conhdl(conhdl);
    if(role == MASTER_ROLE){
    	ret = prf_dtsc_send_data(conhdl, (uint8_t *)data, len);
    }
    else {
        ret = prf_dtss_send_data(conhdl, (uint8_t *)data, len);
    }
	return ret;
}

void ble_set_def_phy(uint8_t tx_phys, uint8_t rx_phys)
{
    gap_le_set_def_phy(tx_phys & 0x07, rx_phys & 0x07);
}
