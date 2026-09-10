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
 * @file ble_sec.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "gap.h"
#include "gatt.h"
#include "ble_sec.h"
#include "flash_manage.h"
#include "task.h"
#include "app_ble.h"
//#define BR_DEBUG_LEVEL 0xFF
#include "log.h"
/* Private typedef -----------------------------------------------------------*/
typedef struct {
    // Bond status
    bool bonded;
    // Pairing Features 
    br_sec_init_t sec_init;
    // Update the binding information task timer
    tid_t update_tid;
}ble_sec_env_tag ;

/* Private define ------------------------------------------------------------*/
#define BOND_SPACE_VALID_FLAG           0x1234
#define BOND_DATA_SAVE_IMMEDIATE        1
/* Private constants ---------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
struct app_sec_bond_data_env_tag app_sec_bond_data;
struct local_device_bond_data_tag local_sec_bond_data;
/// Application Security Environment Structure
ble_sec_env_tag sec_env;
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static uint8_t ble_bond_db_compare(uint8_t atype, uint8_t *bd, struct app_sec_bond_data_env_tag *bond_info)
{
    extern uint8_t SMP54_ResolveRPA_Remote(struct SMP_KeysStru *key, uint8_t *rpa);
    if (((!memcmp(bond_info->addr.bd, bd, BD_ADDR_LEN)) && (bond_info->addr.atype == atype))
        || ((ADDR_TYPE_RANDOM == atype) && SMP54_ResolveRPA_Remote((struct SMP_KeysStru *)&bond_info->k, bd))
        || ((ADDR_TYPE_RANDOM != atype) && (!memcmp(bd, bond_info->k.Identity_Address.bd, BD_ADDR_LEN)))
       )
        return true;
    else 
        return false;
}

static void ble_bond_db_update(uint8_t ev, void *arg)
{
    BR_LOG_DEBUG("%s\r\n",__func__);
#if (BOND_DATA_SAVE_IMMEDIATE == 0)
    int32_t duration = br_idle_time_get();
    if(duration > 80 || duration < -80)
    {
        flash_bt_pair_erase(0x00);
        flash_bt_pair_write(0x00, (uint8_t *)&local_sec_bond_data, sizeof(struct local_device_bond_data_tag));
    }
    else
    {
        TIMER_CREATE_ONESHOT(&sec_env.update_tid, 10, NULL, ble_bond_db_update);
    }
#else 
    flash_bt_pair_erase(0x00);
    flash_bt_pair_write(0x00, (uint8_t *)&local_sec_bond_data, sizeof(struct local_device_bond_data_tag));
#endif
}


static void ble_bond_db_save_trigger(void)
{
    if(sec_env.update_tid == NULL){
        TIMER_CREATE_ONESHOT(&sec_env.update_tid, 10, NULL, ble_bond_db_update);
    }
}


static void ble_sec_pairing_cmp_ind(gap_event_t ev, void *ctx, peer_inf_update_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x\r\n",__func__,arg->peer);
    
    app_ble_msg_t param = {0};
    param.sec_pair_cmp.conhdl = ble_get_conhdl_by_conidx(arg->peer);

    if(sec_env.sec_init.bond_enable){
	    param.sec_pair_cmp.bond_data = arg->inf;
	    param.sec_pair_cmp.bond_data_len = arg->inf_sz;
    }
    else {
        peer_bonded_remove(arg->peer, NULL, NULL);
        BR_LOG_DEBUG("bond info don't save\r\n");
    } 
	ble_event_callback(BLE_SMP_PAIR_SUCCESS, &param);
}


static void ble_sec_bond_info_reover_cmp_ind(gap_event_t ev, void *ctx, peer_recover_cmpl_t *arg)
{    
    BR_LOG_DEBUG("%s, ev:%x, arg->peer:%x\r\n",__func__, ev, arg->peer);
}

static void ble_sec_bond_info_rm_cmp_ind(gap_event_t ev, void *ctx, peer_rmv_cmpl_t *arg)
{    
    BR_LOG_DEBUG("%s, ev:%x, arg->peer:%x\r\n",__func__, ev, arg->peer);
}

static void ble_sec_peer_keymiss_ind(gap_event_t ev, void *ctx, peer_key_miss_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
    app_ble_msg_t param = {0};
    param.sec_key_miss.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_KEYMISSING, &param);
}

#if 0
static void ble_sec_peer_found_ind(gap_event_t ev, void *ctx, peer_key_found_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
    app_ble_msg_t param = {0};
    param.sec_ltk_found.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    param.sec_ltk_found.public_bd.addr_type = arg->id_type;
    memcpy(param.sec_ltk_found.public_bd.addr, arg->id_bd, BD_ADDR_LEN);
    ble_event_callback(BLE_SMP_KEY_FOUND, &param);
}
#endif

static void ble_sec_local_keymiss_ind(gap_event_t ev, void *ctx, peer_no_bond_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);

    app_ble_msg_t param = {0};
    param.sec_key_miss.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_KEYMISSING, &param);
}

static void ble_sec_encrypt_cmp_ind(gap_event_t ev, void *ctx, encrypt_cmpl_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
    app_ble_msg_t param = {0};
    param.sec_ency_cmp.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_ENCRY_SUCCESS, &param);
}

static void ble_sec_display_cmp_ind(gap_event_t ev, void *ctx, display_cmpl_t *arg)
{    
    /// Pop-up complete, 
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
}

static void ble_sec_numeric_display_ind(gap_event_t ev, void *ctx, numeric_display_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
    BR_LOG_DEBUG("%s, conidx:%x passkey:%d\r\n",__func__, arg->peer ,(((((arg->numeric[5]<<8)+arg->numeric[4]<<8)
                                                                +arg->numeric[3]<<8)+arg->numeric[2]<<8)
                                                                +arg->numeric[1]<<8)+arg->numeric[0]);
    app_ble_msg_t param = {0};
    param.sec_numeric_ind.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    param.sec_numeric_ind.numeric = arg->numeric;
    ble_event_callback(BLE_SMP_NUMERIC_CMP, &param);
}

static void ble_sec_passkey_input_ind(gap_event_t ev, void *ctx, passkey_input_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x \r\n",__func__, arg->peer);
    app_ble_msg_t param = {0};
    param.sec_psk_entry.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_PASSKEY_ENTRY_REQ, &param);
}

static void ble_sec_passkey_display_ind(gap_event_t ev, void *ctx, passkey_display_t *arg)
{     
    app_ble_msg_t param = {0}; 
    param.sec_psk_ind.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    memcpy(param.sec_psk_ind.passkey, arg->passkey, 6);
    ble_event_callback(BLE_SMP_PASSKEY_DISPLAY, &param);
}

static void ble_sec_pairing_req_ind(gap_event_t ev, void *ctx, pairing_req_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x, sc:%d\r\n",__func__,arg->peer,!!(arg->method&SMP_KEY_GENERATION_METHOD_MASK_LE_SECURE_CONNECTIONS));
    app_ble_msg_t param = {0};
    param.sec_req.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_PAIR_REQ, &param);
}

static void ble_sec_repeat_attempt_ind(gap_event_t ev, void *ctx, repeat_attempt_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x\r\n",__func__,arg->peer);
}

static void ble_sec_pairing_failed_ind(gap_event_t ev, void *ctx, pairing_failed_t *arg)
{    
    BR_LOG_DEBUG("%s, conidx:%x\r\n",__func__,arg->peer);
    app_ble_msg_t param = {0};
    param.sec_pair_failed.conhdl = ble_get_conhdl_by_conidx(arg->peer);
    ble_event_callback(BLE_SMP_PAIR_FAILED, &param);
}

static gap_ev_cbk_t ble_sec_ev_cbk[] = {
    //{GAP_EV_PEER_INF_UPDATE,        (gap_cbk_t *)ble_sec_pairing_cmp_ind},  
    {GAP_EV_PEER_BONDED,            (gap_cbk_t *)ble_sec_pairing_cmp_ind}, 
    {GAP_EV_PEER_RECOVER_CMPL,      (gap_cbk_t *)ble_sec_bond_info_reover_cmp_ind}, 
    {GAP_EV_PEER_RMV_CMPL,          (gap_cbk_t *)ble_sec_bond_info_rm_cmp_ind},/** peer_rmv_cmpl_t */
    {GAP_EV_PEER_KEY_MISS,          (gap_cbk_t *)ble_sec_peer_keymiss_ind},  /** peer_key_miss_t */
    {GAP_EV_PEER_NO_BOND,           (gap_cbk_t *)ble_sec_local_keymiss_ind}, /*peer_no_bond_t  */
    {GAP_EV_PASSKEY_INPUT,          (gap_cbk_t *)ble_sec_passkey_input_ind},   /** passkey_input_t */
    {GAP_EV_PASSKEY_DISPLAY,        (gap_cbk_t *)ble_sec_passkey_display_ind},  /** passkey_display_t */
    {GAP_EV_NUMERIC_DISPLAY,        (gap_cbk_t *)ble_sec_numeric_display_ind},  /** numeric_display_t */
    {GAP_EV_DISPLAY_CMPL,           (gap_cbk_t *)ble_sec_display_cmp_ind},   /** display_cmpl_t */ 
    {GAP_EV_ENCRYPT_CMPL,           (gap_cbk_t *)ble_sec_encrypt_cmp_ind},  /** encrypt_cmpl_t */
    {GAP_EV_PAIRING_REQ,			(gap_cbk_t *)ble_sec_pairing_req_ind},	/** pairing_req_t */
    {GAP_EV_REPEAT_ATTEMPT,         (gap_cbk_t *)ble_sec_repeat_attempt_ind},
    {GAP_EV_PAIRING_FAILED,         (gap_cbk_t *)ble_sec_pairing_failed_ind},  
};


void ble_send_sec_pairing_rsp(uint16_t conhdl, bool accept)
{
    BR_LOG_DEBUG("%s, accept:%d\r\n",__func__, accept);
    if(ble_get_role_by_conhdl(conhdl) == ROLE_SLAVE){
        smp_pair_accept(ble_get_conidx_by_conhdl(conhdl), accept);
    }
    BR_LOG_DEBUG("%s, app_env.conn_env[conhdl].role:%d\r\n",__func__, ble_get_role_by_conhdl(conhdl));
}

void ble_send_sec_numeric_cfm(uint16_t conhdl, bool same)
{
    smp_user_confirm(ble_get_conidx_by_conhdl(conhdl), same);
}

void ble_send_sec_passkey_rsp(uint16_t conhdl, uint32_t passkey)
{   
    BR_LOG_DEBUG("conidx:%p, passkey:%06d\r\n",ble_get_conidx_by_conhdl(conhdl),passkey);
    smp_passkey_entry(ble_get_conidx_by_conhdl(conhdl), passkey);
}

void ble_gap_iocap_set(uint8_t iocap)
{
    sec_env.sec_init.pairing_feat.iocap = iocap;
    gap_set_io_cap((hci_io_cap_t)sec_env.sec_init.pairing_feat.iocap);
}

void ble_gap_lesc_set(bool enable)
{
    sec_env.sec_init.pairing_feat.auth &= 0xF7;
    sec_env.sec_init.pairing_feat.auth |= enable << 3;
    gap_set_lesc_enable(enable);
}

void ble_gap_passkey_set(uint32_t passkey)
{
    sec_env.sec_init.pin_code = passkey;
    smp_set_local_display_passkey(passkey); //fixme
}

void ble_get_identity_addr(uint16_t conhdl, uint8_t *addr_type, uint8_t *addr)
{
    smp_get_ident_addr(ble_get_conidx_by_conhdl(conhdl), addr_type, addr);
}

void ble_sec_encrypt_req(uint16_t conhdl)
{
    gap_pair_req(ble_get_conidx_by_conhdl(conhdl));
}

static uint8_t ble_bond_db_get_size(void)
{
    uint8_t bond_num = 0;
    struct local_device_bond_data_tag* p_strored = (void *)&local_sec_bond_data;
    bond_num = p_strored->num;
    return bond_num;
}

static void ble_bond_db_load(uint8_t index, struct app_sec_bond_data_env_tag * single_bond_data)
{
    uint32_t length = sizeof(struct app_sec_bond_data_env_tag);
    struct local_device_bond_data_tag* p_strored = &local_sec_bond_data;
    memcpy(single_bond_data, &(p_strored->single_peer_bond_data[index]), length);
}

void ble_bond_db_add_device(void * bond_data_param)
{
    // Load bond data from flash
    struct app_sec_bond_data_env_tag  *bond_data = (struct app_sec_bond_data_env_tag *)bond_data_param;
    struct local_device_bond_data_tag *p_strored = &local_sec_bond_data;
    if(!sec_env.sec_init.bond_enable) {
        return;
    }

    for(uint8_t i=0;i<p_strored->num;i++){//todo
        if(ble_bond_db_compare(bond_data->addr.atype, bond_data->addr.bd, &p_strored->single_peer_bond_data[i])) {
            if(memcmp(&bond_data->k, &p_strored->single_peer_bond_data[i].k, sizeof(struct SMP_KeysStru))){
                memcpy(&p_strored->single_peer_bond_data[i], bond_data, sizeof(struct app_sec_bond_data_env_tag));
                ble_bond_db_save_trigger();
            }
            return ;
        }
    }
    
    // Update bond data
    if(p_strored->num >= sec_env.sec_init.bond_max_peer) {
        peer_bonded_remove_by_addr((addr_type_t)p_strored->single_peer_bond_data[0].addr.atype, p_strored->single_peer_bond_data[0].addr.bd, NULL, NULL);//wait for verification
        uint32_t load_size = sizeof(struct app_sec_bond_data_env_tag) * (p_strored->num-1);
        memmove(&p_strored->single_peer_bond_data[0],&p_strored->single_peer_bond_data[1], load_size);
    }
    else {
        p_strored->num ++;
    }

    memcpy(&p_strored->single_peer_bond_data[p_strored->num-1], bond_data,
            sizeof(struct app_sec_bond_data_env_tag));
    
    sec_env.bonded = true;
	/// update local_sec_bond_data
    ble_bond_db_save_trigger();
}

void ble_bond_db_erase_device(uint8_t addr_type, uint8_t *addr)
{ 
    // remove stack linkkey
    peer_bonded_remove_by_addr((addr_type_t)addr_type, addr, NULL, NULL);   
    if (sec_env.bonded) {      
    	// remove flash linkkey
        uint8_t peer_num = 0;
        struct app_sec_bond_data_env_tag  bond_data = {0};
        peer_num = ble_bond_db_get_size();
        struct local_device_bond_data_tag flash_bond_data;

		memset(&flash_bond_data, 0xFF, sizeof(struct local_device_bond_data_tag));
        flash_bond_data.valid_flag = BOND_SPACE_VALID_FLAG;
		flash_bond_data.num = 0;
        for(uint8_t i = 0; i < peer_num; i++){
            ble_bond_db_load( i, &bond_data);
            if(!ble_bond_db_compare(addr_type, addr, &bond_data)) {
                memcpy(&flash_bond_data.single_peer_bond_data[flash_bond_data.num], &bond_data, sizeof(struct app_sec_bond_data_env_tag));
                flash_bond_data.num++;
            }
        }

        if(flash_bond_data.num != peer_num){
            memcpy(&local_sec_bond_data, &flash_bond_data, sizeof(struct local_device_bond_data_tag));
            if(flash_bond_data.num == 0)    sec_env.bonded = false;
            ///Trigger write
            ble_bond_db_save_trigger();    
		    BR_LOG_DEBUG("Pairing information deleted\r\n");
        }
    }

}

void ble_bond_db_erase_all(void)
{   
    peer_bonded_clear();
    if(sec_env.sec_init.bond_enable) {
        if(sec_env.update_tid != NULL) {
            timer_delete(&sec_env.update_tid);
        }
        memset(&local_sec_bond_data, 0xFF, sizeof(struct local_device_bond_data_tag));
        local_sec_bond_data.valid_flag = BOND_SPACE_VALID_FLAG;
        local_sec_bond_data.num = 0;
        sec_env.bonded = false;
        flash_bt_pair_erase(0);
    }
}

void ble_bond_db_store(void)
{
    if(sec_env.update_tid != NULL) {
        timer_delete(&sec_env.update_tid);
        flash_bt_pair_erase(0x00);
        flash_bt_pair_write(0x00, (uint8_t *)&local_sec_bond_data, sizeof(struct local_device_bond_data_tag));          
    }
}

static void ble_bond_db_recover(void)
{
    struct local_device_bond_data_tag* p_strored = (void *)&local_sec_bond_data;
    uint8_t bond_num = p_strored->num;
    for(uint8_t i=0;i<bond_num;i++){
        peer_recover((uint8_t *)&p_strored->single_peer_bond_data[i], sizeof(struct app_sec_bond_data_env_tag), NULL, NULL); 
    }
}

void ble_bond_db_init(void)
{
    uint8_t peer_num = 0;
    ///sec data loading from flash
    flash_bt_pair_read(0x00, (uint8_t *)&local_sec_bond_data, sizeof(struct local_device_bond_data_tag));
    ///Check flag bits and quantity 
    if((local_sec_bond_data.valid_flag != BOND_SPACE_VALID_FLAG) || (local_sec_bond_data.num > sec_env.sec_init.bond_max_peer)){
        memset(&local_sec_bond_data, 0xFF, sizeof(struct local_device_bond_data_tag));
        local_sec_bond_data.valid_flag = BOND_SPACE_VALID_FLAG;
        local_sec_bond_data.num = 0;
    }  
    peer_num = ble_bond_db_get_size();
    if(peer_num != 0)
    {
        sec_env.bonded = true;
    }
    else
    {
        sec_env.bonded = false;
    }
}

void ble_sec_init(br_sec_init_t *sec_init)
{
	/// Priority registration of callback functions.
    gap_ev_subscribe(ble_sec_ev_cbk, sizeof(ble_sec_ev_cbk) / sizeof(gap_ev_cbk_t));
    memcpy(&sec_env.sec_init, sec_init, sizeof(ble_sec_env_tag));
    /// Bond Init
    if(sec_env.sec_init.bond_enable)
    {
        sec_env.update_tid = NULL;
        ble_bond_db_init();
        /// Restore the linkkey to the host stack
        ble_bond_db_recover();
    }
    /// Set security parameters.
    gap_set_sec_level(SEC_PARAM_MODE_LEVEL);
    gap_set_bondable(sec_env.sec_init.bond_enable);
    gap_set_mitm_protect(SEC_PARAM_MITM);
    gap_set_lesc_enable((bool)(sec_env.sec_init.pairing_feat.auth & 0x08));//GAP_AUTH_SEC_CON
    gap_set_io_cap((hci_io_cap_t)sec_env.sec_init.pairing_feat.iocap);
    smp_set_local_display_passkey(sec_env.sec_init.pin_code); //fixme
}

