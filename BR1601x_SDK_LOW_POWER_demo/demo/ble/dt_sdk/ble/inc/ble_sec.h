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
 * @file ble_adv.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _BLE_SEC_H_
#define _BLE_SEC_H_

#include "stdint.h"
#include "stdbool.h"
#include "porting.h"
#include "app_ble.h"
#include "app_ble_cfg.h"

#define  app_sec_bond_data_env_tag KeyStoreStru

typedef struct gapc_pairing {
    /// IO capabilities (@see gap_io_cap)
    uint8_t iocap;
    /// OOB information (@see gap_oob)
    uint8_t oob;
    /// Authentication (@see gap_auth)
    /// Note in BT 4.1 the Auth Field is extended to include 'Key Notification' and
    /// and 'Secure Connections'.
    uint8_t auth;
    /// Encryption key size (7 to 16)
    uint8_t key_size;
    ///Initiator key distribution (@see gap_kdist)
    uint8_t ikey_dist;
    ///Responder key distribution (@see gap_kdist)
    uint8_t rkey_dist;
    /// Device security requirements (minimum security level). (@see gap_sec_req)
    uint8_t sec_req;
}gapc_pairing_t;

struct local_device_bond_data_tag
{
    uint16_t valid_flag;
    uint8_t num;
    struct app_sec_bond_data_env_tag single_peer_bond_data[SEC_PARAM_BONDED_NUM];
};

typedef struct {
    //Pairing Features 
    struct gapc_pairing pairing_feat;
    
    //pin code for pairing
    uint32_t pin_code;
    bool     rand_pin_enable;
    
    //config bond
    uint8_t  bond_enable;
    uint8_t  bond_max_peer;  
    //sec msg hanlder
//    void (*br_sec_msg_handler)(struct sec_msg_t const*);    
}br_sec_init_t;

void ble_sec_init(br_sec_init_t *sec_init);
void ble_send_sec_passkey_rsp(uint16_t conhdl, uint32_t passkey);
void ble_send_sec_pairing_rsp(uint16_t conhdl, bool accept);
void ble_sec_encrypt_req(uint16_t conhdl);
void ble_gap_iocap_set(uint8_t iocap);
void ble_gap_lesc_set(bool enable);
void ble_gap_passkey_set(uint32_t passkey);
void ble_send_sec_numeric_cfm(uint16_t conhdl, bool same);
void ble_bond_db_add_device(void * bond_data_param);
void ble_bond_db_store(void);
void ble_bond_db_erase_device(uint8_t addr_type, uint8_t *addr);
void ble_bond_db_erase_all(void);

#endif
