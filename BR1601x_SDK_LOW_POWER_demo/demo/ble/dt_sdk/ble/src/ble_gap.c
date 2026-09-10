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
 * @file ble_gap.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "porting.h"
#include "ble_gap.h"
#include "ble_adv.h"
#include "ble_scan.h"
#include "app_ble.h"
#include "cfg.h"
#include "prf_gap.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

extern void ble_adv_cbk(gap_event_t ev, void *ctx, void *par);

ble_gap_params_t  gap_env;
extern app_env_t      app_env;

static uint16_t ble_get_idle_idx(void)
{
    uint16_t idx = GAP_INVALID_CONHDL;
    for(uint16_t i=0; i<APP_CON_IDX_MAX; i++)
    {
        if(app_env.conn_env[i].conidx == GAP_INVALID_CONIDX || app_env.conn_env[i].conhdl == GAP_INVALID_CONHDL){
            idx = i;
            break; 
        }
    }
    return idx;
}

static void ble_gap_phy_ind(gap_event_t ev, void *ctx, phy_update_cmpl_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x, rx_phy:%d, tx_phy:%d\r\n",__func__, arg->peer, arg->rx_phy, arg->tx_phy);
#if defined(BR1601A01) || defined(BR1601A02)
    br_modem_set(arg->tx_phy);
#endif
}

static void ble_gap_connected_ind(gap_event_t ev, void *ctx, gap_conn_cmpl_t *arg)
{  
    uint16_t idx = ble_get_idle_idx();
    BR_LOG_DEBUG("%s, condix: %x, conhdl:%d\r\n",__func__, arg->peer, idx);

    if(idx < APP_CON_IDX_MAX){
        app_env.conidx = arg->peer;
        app_env.conhdl = idx;
        app_env.conn_env[idx].conidx = arg->peer;
        app_env.conn_env[idx].conhdl = idx;
        app_env.conn_env[idx].mtu = GATT_MTU_DEFAULT;
        peer_addr_get(arg->peer, &app_env.conn_env[idx].peer_addr_type, app_env.conn_env[idx].peer_addr.addr);
        app_env.conn_env[idx].role = le_conn_get_local_role(arg->peer);

        app_ble_msg_t param = {0};
        param.connection_ind.conhdl = app_env.conn_env[idx].conhdl;
		param.connection_ind.mtu = app_env.conn_env[idx].mtu;				
		param.connection_ind.peer_addr_type = app_env.conn_env[idx].peer_addr_type;
        param.connection_ind.role = app_env.conn_env[idx].role;
		memcpy(param.connection_ind.peer_addr.addr, app_env.conn_env[idx].peer_addr.addr, BD_ADDR_LEN);

		ble_event_callback(BLE_GAP_CONNECTED, &param);

        if(app_env.conn_env[idx].role == MASTER_ROLE){
            app_env.initing = false;
#if defined(BR1601A01) || defined(BR1601A02)
            ble_scan_en_update();
#endif
        }
    }
    else
        BR_LOG_DEBUG("%s, err\r\n",__func__);
}

static void ble_gap_disconnected_ind(gap_event_t ev, void *ctx, gap_disc_cmpl_t *arg)
{    
    BR_LOG_INFO("%s, condix: %x ,reason:%x\r\n",__func__, arg->peer, arg->result);
    uint16_t idx = ble_get_conhdl_by_conidx(arg->peer);

    app_ble_msg_t param = {0};
	param.disconnect_ind.conhdl = idx; 		
	param.disconnect_ind.reason = arg->result;	
     
    if(idx != GAP_INVALID_CONHDL){
        ble_event_callback(BLE_GAP_DISCONNEDTED, &param);
        memset(&app_env.conn_env[idx], 0xFF, sizeof(app_con_env_t));
    }
    else if(arg->result == 0x02) {//unknown connection identifier, Usually the connection is cancelled
        __NOP();
    }
}

static void ble_gap_dle_ind(gap_event_t ev, void *ctx, data_len_change_t *arg)
{    
    BR_LOG_DEBUG("%s, condix: %x\r\n",__func__, arg->peer);
    uint16_t idx = ble_get_conhdl_by_conidx(arg->peer);
    
    if(idx != GAP_INVALID_CONHDL){
        app_env.conn_env[idx].tx_pkt_size = arg->maxtxoctets;
    }
}

static void ble_gap_mtu_ind(gap_event_t ev, void *ctx, att_mtu_ntf_t *arg)
{  
    BR_LOG_INFO("%s, condix: %x, mtu: %d\r\n",__func__, arg->peer, arg->mtu);    
    uint16_t idx = ble_get_conhdl_by_conidx(arg->peer);

    app_ble_msg_t param = {0};
    param.gattc_mtu.conhdl = idx;		
	param.gattc_mtu.mtu = arg->mtu;

    ble_event_callback(BLE_GATT_MTU_EXCHANGE, &param);
    
    if(idx != GAP_INVALID_CONHDL){
        app_env.conn_env[idx].mtu = arg->mtu;
    }
}

static void ble_gap_conn_update_ind(gap_event_t ev, void *ctx, le_peer_conn_update_cmpl_t *arg)
{
    uint16_t conhdl = ble_get_conhdl_by_conidx(arg->peer);
    BR_LOG_DEBUG("le conn update, %08X, idx:%d", (uint32_t)arg->peer, conhdl);

    app_ble_msg_t param = {0};
    param.param_updated.conhdl = conhdl;
    memcpy(&param.param_updated.con_interval, &arg->conn_intv, sizeof(app_ble_gap_param_updated_ind_t));
	ble_event_callback(BLE_GAP_CONNECT_PARAM_UPDATE, &param);
    
    if(conhdl != GAP_INVALID_CONHDL){
        app_env.conn_env[conhdl].conn_par.intv = arg->conn_intv;
        app_env.conn_env[conhdl].conn_par.latency = arg->lantency_max;
        app_env.conn_env[conhdl].conn_par.time_out = arg->timeout;
    }
    else
        BR_LOG_DEBUG("%s, err\r\n",__func__);
}

static void ble_gap_peer_conn_par_req_ind(gap_event_t ev, void *ctx, le_peer_conn_par_req_t *arg)
{
    BR_LOG_DEBUG("%s, %08X",__func__, (uint32_t)arg->peer);
    le_conn_update_t par = {
        .peer = arg->peer,
        .intv_min = arg->intv_min,
        .intv_max = arg->intv_max,
        .latency_max = arg->lantency_max,
        .timeout = arg->timeout,
        .ce_len_min = 0,
        .ce_len_max = 0x140,
    };
    le_peer_conn_par_reply(&par, NULL, NULL);
}

static void ble_gap_read_rmt_feature_ind(gap_event_t ev, void *ctx, le_peer_read_feature_cmpl_t *arg)
{ 
    if(arg->result == 0x00) {
	    app_ble_msg_t param = {0};
	    param.feature_ind.conhdl = ble_get_conhdl_by_conidx(arg->peer);
	    memcpy(param.feature_ind.feature, arg->le_features, 8);
		ble_event_callback(BLE_GAP_FEATURE_IND, &param);  
	}  
}

static void ble_gap_read_rmt_version_ind(gap_event_t ev, void *ctx, le_peer_read_version_cmpl_t *arg)
{ 
    BR_LOG_DEBUG("%s, conidx:%p\r\n", __func__, (uint32_t)arg->peer);
    if(arg->result == 0x00) {
        app_ble_msg_t param = {0};
        param.version_ind.conhdl = ble_get_conhdl_by_conidx(arg->peer);
        param.version_ind.verion = arg->verion;
        param.version_ind.manufacture_name = arg->manufacture_name;
        param.version_ind.subversion = arg->subversion;
        ble_event_callback(BLE_GAP_VERSION_IND, &param);
    }
}

static void ble_gap_bond_info_ind(gap_event_t ev, void *ctx, peer_inf_update_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%p\r\n",__func__, (uint32_t)arg->peer);
    uint16_t idx = ble_get_conhdl_by_conidx(arg->peer);
    if(idx != APP_CON_IDX_MAX){
        smp_get_ident_addr(arg->peer, &app_env.conn_env[idx].public_addr_type, app_env.conn_env[idx].public_addr.addr);
        BR_LOG_DEBUG("type:%02x,public_addr:%02x%02x%02x%02x%02x%02x\r\n",app_env.conn_env[idx].public_addr_type,
            app_env.conn_env[idx].public_addr.addr[5],
            app_env.conn_env[idx].public_addr.addr[4],
            app_env.conn_env[idx].public_addr.addr[3],
            app_env.conn_env[idx].public_addr.addr[2],
            app_env.conn_env[idx].public_addr.addr[1],
            app_env.conn_env[idx].public_addr.addr[0]);
    }
    else {
        BR_LOG_DEBUG("%s, err\r\n",__func__);
    } 
}

static void ble_gap_rssi_ind(gap_event_t ev, void *ctx, rssi_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%p, rssi:%d\r\n",__func__, arg->peer, arg->rssi);
    if(arg->status == 0x00){
        app_ble_msg_t param = {0};
        param.rssi_ind.conhdl = ble_get_conhdl_by_conidx(arg->peer);
        param.rssi_ind.rssi = arg->rssi;
        ble_event_callback(BLR_GAP_RSSI_IND, &param);
    }
    else {
        BR_LOG_DEBUG("%s, err:%02x\r\n",__func__, arg->status);
    } 
}

static gap_ev_cbk_t ble_gap_ev_cbk[] = {
    {GAP_EV_SCN_TIMEOUT,                    (gap_cbk_t *)ble_scan_cbk},
    {GAP_EV_ADV_SET_TERMINATED,             (gap_cbk_t *)ble_adv_cbk},
    {GAP_EV_CONN_CMPL,                      (gap_cbk_t *)ble_gap_connected_ind},
    {GAP_EV_DISC_CMPL,                      (gap_cbk_t *)ble_gap_disconnected_ind},
    {GAP_EV_PHY_UPDATE_CMPL,		        (gap_cbk_t *)ble_gap_phy_ind},	/** phy_update_cmpl_t */
    {GAP_EV_DATA_LEN_CHANGE,		        (gap_cbk_t *)ble_gap_dle_ind},	/** data_len_change_t */
    {GAP_EV_LE_CONN_UPDATE_CMPL,            (gap_cbk_t *)ble_gap_conn_update_ind},
    {GAP_EV_LE_PEER_CONN_PAR_REQ,           (gap_cbk_t *)ble_gap_peer_conn_par_req_ind},
    {GAP_EV_LE_READ_REMOTE_FEATURE_CMPL,    (gap_cbk_t *)ble_gap_read_rmt_feature_ind},
    {GAP_EV_READ_REMOTE_VERSION_CMPL,       (gap_cbk_t *)ble_gap_read_rmt_version_ind},
    {GAP_EV_ATT_MTU_NOTIFY,                 (gap_cbk_t *)ble_gap_mtu_ind}, /** att_mtu_ntf_t */
    {GAP_EV_PEER_BONDED,                    (gap_cbk_t *)ble_gap_bond_info_ind},
    {GAP_EV_READ_RSSI_CMPL,                 (gap_cbk_t *)ble_gap_rssi_ind},
};

uint32_t ble_gap_conn_param_set(const ble_conn_param_t* param)
{
    uint32_t ret = BLE_ERR_NONE;
    memcpy(&gap_env.dev_conn_param, param, sizeof(ble_conn_param_t));
    return ret;  
}

uint32_t ble_gap_name_set(const char* name, uint8_t len)
{
    BR_LOG_DEBUG("%s\r\n",__func__);
	uint32_t ret = BLE_ERR_NONE;
	
    if(len > APP_DEVICE_NAME_MAX_LEN)
		gap_env.dev_name_len = APP_DEVICE_NAME_MAX_LEN;
	else 
		gap_env.dev_name_len = len;
    memcpy(gap_env.dev_name, name, gap_env.dev_name_len); 
    ///update scan rsp  
    ble_scan_rsp_data_update();
    ///update name character value
    prf_gap_name_set(gap_env.dev_name, gap_env.dev_name_len);
	return ret; 
}

void ble_gap_name_get(uint8_t dev_name[APP_DEVICE_NAME_MAX_LEN], uint8_t *dev_name_len)
{
    memcpy((uint8_t *)dev_name, gap_env.dev_name, gap_env.dev_name_len);
    *dev_name_len = gap_env.dev_name_len;
}
void ble_gap_appreance_get(uint16_t *appreance)
{
    *appreance = gap_env.appearance;
}

void ble_gap_conn_param_get(ble_conn_param_t *param)
{
    memcpy(param, &gap_env.dev_conn_param, sizeof(ble_conn_param_t));  
}

uint32_t ble_gap_init(ble_gap_params_t *gap_params)
{
    uint32_t ret = BLE_ERR_NONE;
    memcpy(&gap_env, gap_params, sizeof(ble_gap_params_t));   
    gatt_set_default_mtu(gap_env.mtu);
    gap_set_def_le_sup_to(50);//500ms
    gap_init();
    gap_ev_subscribe(ble_gap_ev_cbk, sizeof(ble_gap_ev_cbk) / sizeof(gap_ev_cbk_t));
    return ret;
}
