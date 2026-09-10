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
#include "gatt.h"
#include "co_printf.h"
#include "ble_scan.h"
//#define BR_DEBUG_LEVEL    0xFF
#include "log.h"

typedef struct{
    ble_scan_params_t  scan_param;
    uint8_t scan_state;
}scan_env_tag;

scan_env_tag scan_env;


void ble_scan_cbk(gap_event_t ev, void *ctx, void *arg)
{
    BR_LOG_DEBUG("%s, ev:%x\r\n",__func__, ev);
    switch(ev)
    {
        case GAP_EV_SCN_STARTED:
            scan_env.scan_state = SCAN_STATE_RUNING;
            break;
        case GAP_EV_SCN_STOPED:
        case GAP_EV_SCN_TIMEOUT:
            scan_env.scan_state = SCAN_STATE_IDLE;
#if defined(BR1601A01) || defined(BR1601A02)
            ble_scan_en_update();
#endif
            break;
        case GAP_EV_ADV_REPORT:
            {
                adv_rpt_t *param = (adv_rpt_t *)arg;    
                app_ble_msg_t msg = {0};
                msg.adv_report.actv_idx = param->adv_sid;
                msg.adv_report.info = param->ev_type;
                msg.adv_report.rssi = param->rssi;
                msg.adv_report.length = param->data_len;
                msg.adv_report.data = param->data;
                memcpy(msg.adv_report.peer_addr.addr, param->addr, BD_ADDR_LEN);
                msg.adv_report.peer_addr_type = param->addr_type;
                ble_event_callback(BLE_GAP_SCAN_REPORT, &msg);
            }
            break;
        default:
            break;
    }
}

bool ble_scan_state_get(void)
{
    if(scan_env.scan_state == SCAN_STATE_IDLE)
        return false;
    else
        return true;
}

uint32_t ble_scan_param_set(ble_scan_params_t* param)
{
    uint32_t ret = BLE_ERR_NONE;
    scan_env.scan_param.prop_active_enable = param->prop_active_enable;
    scan_env.scan_param.scan_intv = param->scan_intv;
    scan_env.scan_param.scan_wd = param->scan_wd;
    return ret;
}

void ble_scan_enable(void)
{
    scn_param_t param = {0};
    scn_phy_param_t phy_par = {
        .scn_type = scan_env.scan_param.prop_active_enable,
        .scn_intv = scan_env.scan_param.scan_intv,
        .scn_win = scan_env.scan_param.scan_wd,
    };
    param.filter_duplicates = (bool)scan_env.scan_param.dup_filt_pol;
    param.duration = scan_env.scan_param.duration;
    param.period = scan_env.scan_param.period;
    param.own_addr_type = ADDR_TYPE_PUBLIC; 
    param.filter_policy = SCN_POLICY_BASIC_UNFILTERED;
    param.phys = SCH_PHY_BITS_1M;
#if defined(BR1601A01) || defined(BR1601A02)
    br_scan_en_set(true);//todo
    br_modem_set(PHY_1MBPS);
#endif
    if(scan_env.scan_param.phy_coded_enable){
        param.phys |= SCH_PHY_BITS_CODED;
#if defined(BR1601A01) || defined(BR1601A02)
        br_modem_set(PHY_CODED);
#endif
    }
    param.phys_par = &phy_par;
    le_scan_enable(&param, ble_scan_cbk, NULL);
}

void ble_scan_disable(void)
{ 
    le_scan_disable();
}

void ble_scan_init(ble_scan_params_t *scan_init)
{
    memcpy(&scan_env.scan_param, scan_init, sizeof(ble_scan_params_t));
    scan_env.scan_state = SCAN_STATE_IDLE;
}

