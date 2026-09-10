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
#include "gap.h"
#include "gatt.h"
#include "ble_sec.h"
#include "datatype.h"
#include "qflash.h"
#include "co_printf.h"

#define SEC_PARAM_PASSKEY                   123456
#define SEC_PARAM_BOND                      1
#define SEC_PARAM_MITM                      1
#define SEC_PARAM_LESC                      1
#define SEC_PARAM_IOCAP                     IO_CAPABILITY_NOINPUTNOOUTPUT
#define SEC_PARAM_MODE_LEVEL                GAP_SECURITY_MODE1_LEVEL2
/// Binding Data Magic Word
#define BOND_DATA_MAGIC                     "BRDB"
#define BOND_DATA_MAGIC_SIZE                (sizeof(BOND_DATA_MAGIC) - 1)
/// Length of Binding Information for a Single Device
#define BOND_DATA_SINGLE_LEN                sizeof(smp_key_store_t)
/// Base Address for Storing Binding Information
#define BOND_DATA_BASE_ADDR                 0x0103f000

struct local_device_bond_data
{
    uint8_t magic[BOND_DATA_MAGIC_SIZE];
    smp_key_store_t   bond_data;
};

static void ble_sec_bond_db_init(void)
{
    struct local_device_bond_data bond_info;
    Qflash_Read(BOND_DATA_BASE_ADDR, (uint8_t *)&bond_info, sizeof(struct local_device_bond_data));
    if(!memcmp(bond_info.magic, BOND_DATA_MAGIC, BOND_DATA_MAGIC_SIZE)) {
        peer_recover((uint8_t *)&bond_info.bond_data, BOND_DATA_SINGLE_LEN, NULL, NULL);
    }
}

void ble_bond_db_add_device(peer_t peer, uint8_t *bond_data)
{
    struct local_device_bond_data bond_info;
	/// Record the address of the peer device
	hci_address_t  remote_addr;
	peer_addr_get(peer, &remote_addr.atype, remote_addr.bd);
    Qflash_Read(BOND_DATA_BASE_ADDR, (uint8_t *)&bond_info, sizeof(struct local_device_bond_data));
    /// If there is an existing pairing record, delete the pairing record.
    if(!memcmp(bond_info.magic, BOND_DATA_MAGIC, BOND_DATA_MAGIC_SIZE)) {
        if(!smp_addr_compare(remote_addr.atype, remote_addr.bd, &bond_info.bond_data)) {
            peer_bonded_remove_by_addr((addr_type_t)bond_info.bond_data.addr.atype, bond_info.bond_data.addr.bd, NULL, NULL);
        }
    }
    memcpy(bond_info.magic, BOND_DATA_MAGIC, BOND_DATA_MAGIC_SIZE);
    memcpy(&bond_info.bond_data, bond_data, BOND_DATA_SINGLE_LEN);
    Qflash_Erase_Sector(BOND_DATA_BASE_ADDR);
    Qflash_Write(BOND_DATA_BASE_ADDR, (uint8_t *)&bond_info, sizeof(struct local_device_bond_data));
}

static void ble_sec_pairing_cmp_ind(gap_event_t ev, void *ctx, peer_inf_update_t *arg)
{
    co_printf("%s, peer:%x\r\n", __func__, arg->peer);
#if SEC_PARAM_BOND
    ble_bond_db_add_device(arg->peer, arg->inf);
#else
    peer_bonded_remove(arg->peer, NULL, NULL);
#endif
}

static void ble_sec_peer_keymiss_ind(gap_event_t ev, void *ctx, peer_key_miss_t *arg)
{
    co_printf("%s, peer:%x \r\n",__func__, arg->peer);
    le_conn_destory(arg->peer);
    peer_bonded_remove(arg->peer, NULL, NULL);
}

static void ble_sec_local_keymiss_ind(gap_event_t ev, void *ctx, peer_no_bond_t *arg)
{
    co_printf("%s, peer:%x \r\n",__func__, arg->peer);
    le_conn_destory(arg->peer);
    peer_bonded_remove(arg->peer, NULL, NULL);
}

static void ble_sec_encrypt_cmp_ind(gap_event_t ev, void *ctx, encrypt_cmpl_t *arg)
{
    co_printf("%s, peer:%x \r\n",__func__, arg->peer);
}

static void ble_sec_numeric_display_ind(gap_event_t ev, void *ctx, numeric_display_t *arg)
{
    co_printf("%s, peer:%x, numeric:%c%c%c%c%c%c\r\n",__func__, arg->peer,
                                                    arg->numeric[0], arg->numeric[1], arg->numeric[2],
                                                    arg->numeric[3], arg->numeric[4], arg->numeric[5]);
    smp_user_confirm(arg->peer, true);
}

static void ble_sec_passkey_input_ind(gap_event_t ev, void *ctx, passkey_input_t *arg)
{
    co_printf("%s, peer:%x \r\n",__func__, arg->peer);
    /// Enter the PIN displayed by the other party. for example: SEC_PARAM_PASSKEY
    smp_passkey_entry(arg->peer, SEC_PARAM_PASSKEY);
}

static void ble_sec_passkey_display_ind(gap_event_t ev, void *ctx, passkey_display_t *arg)
{
    co_printf("%s: peer:%x, passkey:%c%c%c%c%c%c", __func__, arg->peer,
                                                    arg->passkey[0], arg->passkey[1], arg->passkey[2],
                                                    arg->passkey[3], arg->passkey[4], arg->passkey[5]);
}

static void ble_sec_pairing_req_ind(gap_event_t ev, void *ctx, pairing_req_t *arg)
{
    co_printf("%s, peer:%x, sc:%d\r\n",__func__,arg->peer,!!(arg->method&SMP_KEY_GENERATION_METHOD_MASK_LE_SECURE_CONNECTIONS));
    smp_pair_accept(arg->peer, true);
}

static void ble_sec_pairing_failed_ind(gap_event_t ev, void *ctx, pairing_failed_t *arg)
{
    co_printf("%s, peer:%x\r\n",__func__,arg->peer);
    /// Disconnect ble
    le_conn_destory(arg->peer);
}

static gap_ev_cbk_t ble_sec_ev_cbk[] = {
    {GAP_EV_PEER_BONDED,            (gap_cbk_t *)ble_sec_pairing_cmp_ind},
    {GAP_EV_PEER_KEY_MISS,          (gap_cbk_t *)ble_sec_peer_keymiss_ind},
    {GAP_EV_PEER_NO_BOND,           (gap_cbk_t *)ble_sec_local_keymiss_ind},
    {GAP_EV_PASSKEY_INPUT,          (gap_cbk_t *)ble_sec_passkey_input_ind},
    {GAP_EV_PASSKEY_DISPLAY,        (gap_cbk_t *)ble_sec_passkey_display_ind},
    {GAP_EV_NUMERIC_DISPLAY,        (gap_cbk_t *)ble_sec_numeric_display_ind},
    {GAP_EV_ENCRYPT_CMPL,           (gap_cbk_t *)ble_sec_encrypt_cmp_ind},
    {GAP_EV_PAIRING_REQ,			(gap_cbk_t *)ble_sec_pairing_req_ind},
    {GAP_EV_PAIRING_FAILED,         (gap_cbk_t *)ble_sec_pairing_failed_ind},
};

int ble_sec_init(void)
{
	/// Priority registration of callback functions.
    gap_ev_subscribe(ble_sec_ev_cbk, sizeof(ble_sec_ev_cbk) / sizeof(gap_ev_cbk_t));
#if SEC_PARAM_BOND
    Qflash_Init();
    /// Restore the linkkey to the host stack
    ble_sec_bond_db_init();
#endif
    /// Set security parameters.
    gap_set_sec_level(SEC_PARAM_MODE_LEVEL);
    gap_set_bondable(SEC_PARAM_BOND);
    gap_set_mitm_protect(SEC_PARAM_MITM);
    gap_set_lesc_enable(SEC_PARAM_LESC);
    gap_set_io_cap(SEC_PARAM_IOCAP);
    smp_set_local_display_passkey(SEC_PARAM_PASSKEY);
    return 0;
}
