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
 * @file ble_diss.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include "ble_diss.h"

gatt_svc_t device_info_svc = NULL;
gatt_chr_t device_info_chr0 = NULL;
gatt_chr_t device_info_chr1 = NULL;
gatt_chr_t device_info_chr2 = NULL;
gatt_chr_t device_info_chr3 = NULL;
gatt_chr_t device_info_chr4 = NULL;
gatt_chr_t device_info_chr5 = NULL;
gatt_chr_t device_info_chr6 = NULL;
gatt_chr_t device_info_chr7 = NULL;
gatt_chr_t device_info_chr8 = NULL;


void device_info_svc_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_SERVICE_START: {
            gatt_svc_start_t *in = (gatt_svc_start_t *)param;
            co_printf("[DISS ] svc start = %X\r\n", in->result);
            break;
        }
    }
}

void device_info_chr0_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] System ID value read successfully\n");
            break;
        }
    }
}

void device_info_chr1_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {'B','R','-','B','L','E','-','1','.','0'};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Model Number String value read successfully\n");
            break;
        }
    }
}

void device_info_chr2_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[DISS ] device_info_chr2_cbk event %d\n", event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {'1','.','0','.','0','.','0','-','L','E'};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Serial Number String value read successfully\n");
            break;
        }
    }
}

void device_info_chr3_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[DISS ] device_info_chr3_cbk event %d\n", event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {'1','.','0','.','0'};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Firmware Revision String value read successfully\n");
            break;
        }
    }
}

void device_info_chr4_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[DISS ] device_info_chr4_cbk event %d\n", event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {'1','.','0','.','0'};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Hardware Revision String value read successfully\n");
            break;
        }
    }
}

void device_info_chr5_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {'1','.','0','.','0'};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Software Revision String value read successfully\n");
            break;
        }
    }
}

void device_info_chr6_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[DISS ] device_info_chr6_cbk event %d\n", event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0x28,0xE8};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] Manufacture Name String value read successfully\n");
            break;
        }
    }
}

void device_info_chr7_cbk(gatt_event_t event, void *context, void *param)
{
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint8_t val[] = {0xFF,0xEE,0xDD,0xCC,0xBB,0xAA};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] IEEE value read successfully\n");
            break;
        }
    }
}

void device_info_chr8_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[DISS ] device_info_chr8_cbk event %d\n", event);
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            // PnP ID: VendorSrc=Bluetooth SIG Company: Iton Technology(0x01), VID=0xC753, PID=0x527D, Ver=0x0003
            uint8_t val[] = {0x01, 0x53, 0xC7, 0x7D, 0x52, 0x00, 0x03};
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, val, sizeof(val));
            co_printf("[DISS ] PnP ID value read successfully\n");
            break;
        }
    }
}

int ble_device_info_init(void)
{
    uuid_t svc_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x180A)
    };
    device_info_svc = gatts_svc_create(&svc_uuid, device_info_svc_cbk, NULL);

    /*
     * 全部改用 gatts_chr_ro_add（走 GATTS_ChrAdd_Value 路径）替代
     * gatts_chr_add（走 GATTS_ChrAdd 路径），以规避 SDK 库中
     * GATTS_ChrAdd 导致特征声明 UUID 偏移一个位置的 bug。
     */

    // chr0: System ID (0x2A23)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A23) };
        uint8_t val[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
        device_info_chr0 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            (ATT_PERM_READ | ATT_PERM_WRITE),
                            device_info_chr0_cbk, NULL,
                            val, sizeof(val));
    }

    // chr1: Model Number String (0x2A24)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A24) };
        uint8_t val[] = {'B','R','-','B','L','E','-','1','.','0'};
        device_info_chr1 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr1_cbk, NULL,
                            val, sizeof(val));
    }

    // chr4: Hardware Revision String (0x2A27)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A27) };
        uint8_t val[] = {'1','.','0','.','0'};
        device_info_chr4 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr4_cbk, NULL,
                            val, sizeof(val));
    }

    // chr2: Serial Number String (0x2A25)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A25) };
        uint8_t val[] = {'1','.','0','.','0','.','0','-','L','E'};
        device_info_chr2 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr2_cbk, NULL,
                            val, sizeof(val));
    }

    // chr3: Firmware Revision String (0x2A26)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A26) };
        uint8_t val[] = {'1','.','0','.','0'};
        device_info_chr3 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr3_cbk, NULL,
                            val, sizeof(val));
    }

    // chr5: Software Revision String (0x2A28)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A28) };
        uint8_t val[] = {'1','.','0','.','0'};
        device_info_chr5 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr5_cbk, NULL,
                            val, sizeof(val));
    }



    // chr6: Manufacture Name String (0x2A29)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A29) };
        uint8_t val[] = {0x28, 0xE8};
        device_info_chr6 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr6_cbk, NULL,
                            val, sizeof(val));
    }

    // chr7: IEEE (0x2A2A)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A2A) };
        uint8_t val[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};
        device_info_chr7 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr7_cbk, NULL,
                            val, sizeof(val));
    }
    
    // chr8: PnP ID (0x2A50)
    {
        uuid_t uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A50) };
        //uint8_t val[] = {0x01, 0x53, 0xC7, 0x7D, 0x52, 0x00, 0x03}; // PnP ID: VendorSrc=Bluetooth SIG Company: Iton Technology(0x01), VID=0xC753, PID=0x527D, Ver=0x0003
        uint8_t val[] = {0x01, 0x45, 0x75, 0x71, 0x01, 0x00, 0x03};
        device_info_chr8 = gatts_chr_ro_add(device_info_svc, &uuid,
                            GATT_CHR_PROP_READ,
                            ATT_PERM_READ,
                            device_info_chr8_cbk, NULL,
                            val, sizeof(val));
    }
    gatts_svc_start(device_info_svc);
    return 0;
}
