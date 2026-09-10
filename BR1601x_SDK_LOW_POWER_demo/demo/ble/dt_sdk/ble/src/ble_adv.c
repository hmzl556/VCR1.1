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
 * @file ble_adv.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include "gap.h"
#include "ble_adv.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"

/*
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct {    
    ble_adv_params_t adv_param;
    uint8_t adv_state;
}ble_adv_env_tag;

/**
 * LOCAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
ble_adv_env_tag  adv_env;
extern ble_gap_params_t  gap_env;

void ble_adv_cbk(gap_event_t ev, void *ctx, void *par)
{
    BR_LOG_DEBUG("%s,%d\r\n",__func__,ev);
    if(ev >=GAP_EV_ADV_SET_EN_CMPL && ev <= GAP_EV_ADV_SET_TERMINATED){
        adv_env.adv_state = ev;
    }
}


uint32_t ble_adv_data_set(ble_adv_data_t *adv_data)
{
    BR_LOG_DEBUG("%s\r\n",__func__);

    uint32_t ret = BLE_ERR_NONE;
    adv_env.adv_param.adv_data_len = adv_data->adv_data_len;
    memcpy(adv_env.adv_param.adv_data, adv_data->adv_data, adv_env.adv_param.adv_data_len);

    adv_data_t adv_fra;
    adv_set_adv_data_param_t ad_par;
    ad_par.adv_hdl = 0x01,
    ad_par.adv_data_frag_num = 1;
    ad_par.adv_data_frag_pref = ADV_FRAG_PREF_CNTL_MIN_FRAG;
    ad_par.adv_data_frag = &adv_fra;
    adv_fra.len = adv_env.adv_param.adv_data_len;
    adv_fra.data = adv_env.adv_param.adv_data;
    void le_adv_set_adv_data(const adv_set_adv_data_param_t *par, gap_cbk_t *cbk, void *ctx);
    le_adv_set_adv_data(&ad_par, ble_adv_cbk, NULL);

    return ret;
}

void ble_adv_intv_set(uint16_t intv)
{
    adv_env.adv_param.adv_time.adv_intv = intv;
}

void ble_adv_data_get(uint8_t *p_data, uint8_t *len)
{
    memcpy((void *)p_data, (const char *)adv_env.adv_param.adv_data, adv_env.adv_param.adv_data_len);
    *len = adv_env.adv_param.adv_data_len;
}


uint32_t ble_scan_rsp_data_set(ble_adv_data_t *rsp_data)
{
    BR_LOG_DEBUG("%s\r\n",__func__);

    uint32_t ret = BLE_ERR_NONE;
    adv_env.adv_param.scan_rsp_data_len = rsp_data->adv_data_len;
    memcpy(adv_env.adv_param.scan_rsp_data, rsp_data->adv_data, adv_env.adv_param.scan_rsp_data_len);
    ble_scan_rsp_data_update();
    return ret;
}


void ble_scan_rsp_data_get(uint8_t *p_data, uint8_t *len)
{
    *len = adv_env.adv_param.scan_rsp_data_len;
    memcpy(p_data, adv_env.adv_param.scan_rsp_data, adv_env.adv_param.scan_rsp_data_len);
}

void ble_scan_rsp_data_update(void)
{
    BR_LOG_DEBUG("%s\r\n",__func__);
    adv_data_t sr_fra;
    adv_set_scn_data_param_t sr_par;
    uint8_t scan_rsp_data[ADV_DATA_MAX_SIZE] = {0};
    uint8_t rem_len = ADV_DATA_MAX_SIZE;
    sr_par.adv_hdl = 0x01,
    sr_par.scn_data_frag_num = 1;
    sr_par.scn_data_frag_pref = ADV_FRAG_PREF_CNTL_MIN_FRAG;
    sr_par.scn_data_frag = &sr_fra;
    
    sr_fra.len = adv_env.adv_param.scan_rsp_data_len;
    sr_fra.data = scan_rsp_data;

    memcpy(sr_fra.data, adv_env.adv_param.scan_rsp_data, adv_env.adv_param.scan_rsp_data_len);
    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    rem_len -= sr_fra.len;
    uint8_t *p_buf = &sr_fra.data[sr_fra.len];
    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 3 && adv_env.adv_param.attach_appearance)
    {
        // appearance length
        *p_buf = 3 ;
        // appearance flag
        *(p_buf + 1) = 0x19;//ADV_TYPE_APPEARANCE
        // Copy device name
        *(p_buf + 3) = (gap_env.appearance>>8)&0xff; //hight 8bit
        *(p_buf + 2) = gap_env.appearance&0xff;      //low 8bit
        // Update advertising data length
        sr_fra.len += 4;
        p_buf += 4;
        //Update Remaining Length
        rem_len -= 4;
    }
    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 2 && adv_env.adv_param.attach_name)
    {
        uint8_t dev_name_length = gap_env.dev_name_len > (rem_len - 2) ?  (rem_len - 2) : gap_env.dev_name_len;

        // Device name length
        *p_buf = dev_name_length + 1;
        // Device name flag (check if device name is complete or not)
        *(p_buf + 1) = (dev_name_length == gap_env.dev_name_len) ? '\x09' : '\x08';
        // Copy device name
        memcpy(p_buf + 2, gap_env.dev_name, dev_name_length);
        // Update advertising data length
        sr_fra.len += (dev_name_length + 2);
    }
    //if guangbo 
    void le_adv_set_scn_data(const adv_set_scn_data_param_t *par, gap_cbk_t *cbk, void *ctx);
    le_adv_set_scn_data(&sr_par, ble_adv_cbk, NULL);
}

void ble_adv_init(ble_adv_params_t *adv_init)
{
    memcpy(&adv_env.adv_param, adv_init, sizeof(ble_adv_params_t));
    adv_env.adv_state = GAP_EV_ADV_SET_CLR_CMPL; ///The broadcast parameters have not been set
}

void ble_adv_deinit(void)
{
    le_adv_remove(0x01, ble_adv_cbk, NULL);
}

int ble_adv_start(void)
{
    uint8_t scan_rsp_data[ADV_DATA_MAX_SIZE];
    
    adv_data_t adv_dat_fra = {
        .data = adv_env.adv_param.adv_data,
        .len = adv_env.adv_param.adv_data_len,
    };
    adv_data_t scan_rsp_fra = {
        .data = scan_rsp_data,
        .len = 0,
    };

    uint8_t rem_len = ADV_DATA_MAX_SIZE;
    scan_rsp_fra.len = adv_env.adv_param.scan_rsp_data_len;
    memcpy(scan_rsp_fra.data, adv_env.adv_param.scan_rsp_data, adv_env.adv_param.scan_rsp_data_len);
    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    rem_len -= scan_rsp_fra.len;
    uint8_t *p_buf = &scan_rsp_fra.data[scan_rsp_fra.len];
    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 3 && adv_env.adv_param.attach_appearance)
    {
        // appearance length
        *p_buf = 3 ;
        // appearance flag
        *(p_buf + 1) = 0x19;//ADV_TYPE_APPEARANCE
        // Copy device name
        *(p_buf + 3) = (gap_env.appearance>>8)&0xff; //hight 8bit
        *(p_buf + 2) = gap_env.appearance&0xff;      //low 8bit
        // Update advertising data length
        scan_rsp_fra.len += 4;
        p_buf += 4;
        //Update Remaining Length
        rem_len -= 4;
    }
    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 2 && adv_env.adv_param.attach_name)
    {
        uint8_t dev_name_length = gap_env.dev_name_len > (rem_len - 2) ?  (rem_len - 2) : gap_env.dev_name_len;

        // Device name length
        *p_buf = dev_name_length + 1;
        // Device name flag (check if device name is complete or not)
        *(p_buf + 1) = (dev_name_length == gap_env.dev_name_len) ? '\x09' : '\x08';
        // Copy device name
        memcpy(p_buf + 2, gap_env.dev_name, dev_name_length);
        // Update advertising data length
        scan_rsp_fra.len += (dev_name_length + 2);
    }
    
    adv_param_t adv_par = {
        .adv_hdl = 0x01,
        .adv_ev_prop = (ADV_EV_PROP_BITS_LEGACY | ADV_EV_PROP_BITS_SCANNABLE | ADV_EV_PROP_BITS_CONNECTABLE),
        .prim_adv_intv_min = adv_env.adv_param.adv_time.adv_intv,
        .prim_adv_intv_max = adv_env.adv_param.adv_time.adv_intv,
        .prim_adv_ch_map = (ADV_CH_MAP_37 | ADV_CH_MAP_38 | ADV_CH_MAP_39),
        .own_addr_type = ADDR_TYPE_PUBLIC,
        .peer_addr_type = ADDR_TYPE_PUBLIC,
        .peer_addr = {0},
        .adv_filter_policy = ADV_POLICY_PROC_SCAN_CONN_ALL,
        .adv_tx_pwr = 0x7F,
        .prim_adv_phy = ADV_PHY_1M,
        .scnd_adv_max_skip = 0x00,
        .scnd_adv_phy = ADV_PHY_1M,
        .adv_sid = 0,
        .scan_req_notif_en = false,
        .adv_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,
        .adv_data_frag_num = 1,
        .adv_data_frag = &adv_dat_fra,
        .scn_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,
        .scn_data_frag_num = 1,
        .scn_data_frag = &scan_rsp_fra,
    };
    le_adv_add(&adv_par, ble_adv_cbk, NULL);

    adv_en_param_t en_par = {
        .hdl = 0x01,
        .duration = adv_env.adv_param.adv_time.duration,
        .max_events = 0x00,
    };
    le_adv_enable(1, &en_par, ble_adv_cbk, NULL);
#if defined(BR1601A01) || defined(BR1601A02)
    br_modem_set(adv_env.adv_param.adv_phy);
#endif
    return 0;
}

int ble_adv_stop(void)
{
    uint8_t adv_hadl = 0x01;
    le_adv_disable(1, &adv_hadl, ble_adv_cbk, NULL);
    
    return 0;
}


bool ble_adv_state_get(void)
{
    if(adv_env.adv_state == GAP_EV_ADV_SET_EN_CMPL)
        return true;
    else
        return false; 
}

