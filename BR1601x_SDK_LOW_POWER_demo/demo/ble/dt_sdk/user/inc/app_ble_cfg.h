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
 * @file app_user_config.h
 * @author Barrot Firmware Team
 * @version v1.0.1
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#ifndef _APP_BLE_CONFIG_H_
#define _APP_BLE_CONFIG_H_

#define BLE_MAX_CONN                        1
#define APP_CON_IDX_MAX                     BLE_MAX_CONN
#define BLE_SLAVE_CONN                      BLE_MAX_CONN 
#define BLE_MASTER_CONN                     BLE_MAX_CONN 

//gap
#ifdef i2616e_u1
#define BLE_NAME_DEFAULT            "i2616e-u1"
#elif  i2616e
#define BLE_NAME_DEFAULT            "i2616e"
#endif

#define BLE_NAME_DEFAULT_LEN                (sizeof(BLE_NAME_DEFAULT) - 1)

//adv
#define ADV_PARAM_DATA                      "\x02\x01\x06\x03\x16\x00\xFF"
#define ADV_PARAM_DATA_LEN                  (sizeof(ADV_PARAM_DATA) - 1)
#define ADV_PARAM_AUTOUNLOCK_DATA           "\x02\x01\x06\x05\x03\x00\xFF\x12\x18"
#define ADV_PARAM_AUTOUNLOCK_DATA_LEN       (sizeof(ADV_PARAM_AUTOUNLOCK_DATA) - 1)
#define ADV_PARAM_SCAN_RSP_DATA             "\x11\x09" "047F0E011203FFF8"
#define ADV_PARAM_SCAN_RSP_DATA_LEN         (sizeof(ADV_PARAM_SCAN_RSP_DATA) - 1)
#define ADV_PARAM_ATTACH_NAME               0
#define ADV_PARAM_ATTACH_APPEARANCE         0
#define ADV_PARAM_DURATION                  0
#define ADV_PARAM_PHY                       PHY_1MBPS      

//scan
#define SCAN_PARAM_CODED_PHY                0                         
#define SCAN_PARAM_DUP_FILT_POL             0                                           
#define SCAN_PARAM_DURATION                 0                                           
#define SCAN_PARAM_PERIOD                   0                                           

//sec                                            
#define SEC_PARAM_BOND                      1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                      1                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                      0 											/** unused **/
#define SEC_PARAM_BONDED_NUM                10
#define SEC_PARAM_MODE_LEVEL                GAP_SECURITY_MODE1_LEVEL1

//autounlock wlst
#define MAX_WHTIELIST_NUM                   SEC_PARAM_BONDED_NUM  

//prf
#define CFG_PRF_DTSS     1
#define CFG_PRF_IMAGE    1
#define CFG_PRF_HOGPD    1
#define CFG_PRF_GAP      1
#define CFG_PRF_GATT     1
#define CFG_PRF_BAS      0
#define CFG_PRF_DIS      0
#define CFG_PRF_DTSC     1
#define CFG_PRF_CMD      0
#ifdef CFG_OTA
#define CFG_PRF_DFU      1
#else
#define CFG_PRF_DFU      0
#endif
#define CFG_SUPPORT_MASTER  0

#endif /* _APP_USER_CONFIG_H_ */
