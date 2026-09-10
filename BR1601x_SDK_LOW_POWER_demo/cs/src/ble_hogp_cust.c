/*****************************************************************************
 * Copyright (c) 2024, Barrot Technology Co.,Ltd..
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
 * @file ble_hogp_cust.c
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2024, Barrot Technology Co.,Ltd.. All rights reserved.
 */

 /** @addtogroup
 * @{
 */

/* Includes ------------------------------------------------------------------*/
#include "ble_hogp_cust.h"

/* Public define ------------------------------------------------------------*/

/* Public typedef -----------------------------------------------------------*/

/* Private macro ------------------------------------------------------------*/

/* Private variables --------------------------------------------------------*/

/* Private function prototypes ----------------------------------------------*/

/* Private functions --------------------------------------------------------*/
ble_hogp_info_t ble_hogp_env = {0};

typedef struct rpt_t{
    uint8_t id;
    uint8_t type;
}rpt_t;

void hogp_cust_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            co_printf("hogp svc start rst:%X\n", in->result);
            break;
        }
        default:
            break;
    }
}

int hid_svc_init(void)
{
    uint8_t inf[4]  = {
        [0] = 0x11,
        [1] = 0x01,
        [2] = 0x00,
        [3] = 0x00
    };

    uint8_t map_data[] = {0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01, 0x05, 0x07,
                     0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
                     0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
                     0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0xff, 0x05, 0x07,
                     0x19, 0x00, 0x29, 0xff, 0x81, 0x00, 0xc0, 0x05, 0x0c, 0x09,
                     0x01, 0xa1, 0x01, 0x85, 0x02, 0x75, 0x10, 0x95, 0x02, 0x15,
                     0x00, 0x26, 0xff, 0x03, 0x19, 0x00, 0x2a, 0xff, 0x03, 0x81,
                     0x00, 0xc0, 0x06, 0x00, 0xff, 0x09, 0x00, 0xa1, 0x01, 0x85,
                     0x04, 0x95, 0xff, 0x75, 0x08, 0x15, 0x00, 0x25, 0xff, 0x19,
                     0x00, 0x29, 0xff, 0x81, 0x00, 0xc0};

    uint8_t cntl_data[1] = {0x00};
    uint8_t map_des_data[2] = {0x00,0x00};

    uint8_t rpt_data[3][2] = {
        0x01,0x01,
        0x02,0x01,
        0x04,0x01
    };

    uuid_t svc_uuid = {
      UUID_LEN_16
    , UUID16_VAL_SET(0x1812)
    };

    ble_hogp_env.hid_svc = gatts_svc_create(&svc_uuid, hogp_cust_svc_cbk, NULL);

    ble_hogp_env.hid_chr_inf = gatts_chr_ro_add_uuid16(ble_hogp_env.hid_svc,0x2A4A
                                          ,GATT_CHR_PROP_READ
                                          ,ATT_PERM_READ
                                          ,NULL
                                          ,NULL
                                          ,inf,sizeof(inf));

    ble_hogp_env.hid_chr_map = gatts_chr_ro_add_uuid16(ble_hogp_env.hid_svc,0x2A4B
                                          ,GATT_CHR_PROP_READ
                                          ,ATT_PERM_READ | ATT_PERM_WRITE
                                          ,NULL
                                          ,NULL
                                          ,map_data,sizeof(map_data));

    #if 0 /*RO方式和非RO方式添加都可以*/
    uuid_t hid_chr_cntl_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A4C)
    };

    hid_chr_cntl = gatts_chr_add(ble_hogp_env.hid_svc, &hid_chr_cntl_uuid
                               , GATT_CHR_PROP_WRITE_WITHOUT_RSP
                               , (ATT_PERM_READ | ATT_PERM_WRITE)
                               , NULL, NULL);
    #else
    ble_hogp_env.hid_chr_cntl = gatts_chr_ro_add_uuid16(ble_hogp_env.hid_svc,0x2A4C
                                          ,GATT_CHR_PROP_READ
                                          ,ATT_PERM_READ | ATT_PERM_WRITE
                                          ,NULL
                                          ,NULL
                                          ,cntl_data,sizeof(cntl_data));
    #endif


    gatts_dsc_add(ble_hogp_env.hid_chr_map,0x2907
                , GATT_CHR_PROP_READ
                , ATT_PERM_READ | ATT_PERM_WRITE,sizeof(map_des_data),map_des_data);
                //, ATT_PERM_READ | ATT_PERM_WRITE,0,NULL);

    for(uint8_t i=0;i<3;i++){
        uuid_t hogp_chr_prt_in_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x2A4D)
        };

       ble_hogp_env.hid_chr_rpt[i] = gatts_chr_add(ble_hogp_env.hid_svc, &hogp_chr_prt_in_uuid
                               , GATT_CHR_PROP_READ|GATT_CHR_PROP_WRITE | GATT_CHR_PROP_NOTIFY
                               , (ATT_PERM_READ | ATT_PERM_WRITE)
                               , NULL, NULL);

        gatts_ref_dsc_ccc_add(ble_hogp_env.hid_chr_rpt[i]
                              ,GATT_CHR_PROP_NOTIFY
                              ,ATT_PERM_READ | ATT_PERM_WRITE
                              ,2
                              ,(uint8_t *)rpt_data[i]);

        gatts_dsc_ccc_add(ble_hogp_env.hid_chr_rpt[i]
                          ,GATT_CHR_PROP_READ|GATT_CHR_PROP_WRITE | GATT_CHR_PROP_NOTIFY
                          ,0x031A);
    }

    gatts_svc_start(ble_hogp_env.hid_svc);

    return 0;
}


/**
  * @}
  */
