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
#include "ble_adv.h"
#include "ble_gap.h"    /* g_pairing_active */
#include "cs_core.h"
#include "qflash.h"     /* Qflash_Read */
#include "flash_layout.h" /* BLE_BONDING_STORE_ADDR */
#include "br1601x.h"

uint8_t local_bd[6];    // 本地蓝牙地址

// 从芯片读取本地蓝牙地址并初始化相关变量
void local_bd_init_from_chip(void)
{
    const uint8_t *mac = SystemGetMacAddr();
    uint8_t i;

    if (mac == NULL)
    {
        return;
    }
    for (i = 0u; i < 6u; i++)
    {
        local_bd[i] = mac[5u - i];
    }
}

// 通用遥控终端服务 UUID（128-bit，小端）
// 符合GY/T 407-2024 条款附录A
static const uint8_t k_remote_svc_uuid_le[16] = {
    0x00, 0xA0, 0x46, 0x3F, 0xC3, 0xEB, 0xE7, 0xE4,
    0x6F, 0x7E, 0xD3, 0x9A, 0xA4, 0x51, 0xEE, 0x19,
};

// 配对广播（广播类型1）
// 符合GY/T 407-2024 条款附录C.1、附录C.2
// 说明：该数组直接作为 Advertising Data，包含 Flags、HID UUID、Service Data 等字段。
static uint8_t adv_dat_pairing[] = {
    0x02, 0x01, 0x05, // Flags: LE Limited Discoverable + BR/EDR Not Supported
    0x03, 0x03, 0x12, 0x18, // HID服务UUID 0x1812
    0x14, 0x21, // AD Type 0x21：Service Data - 128-bit UUID
    0x00, 0xA0, 0x46, 0x3F, 0xC3, 0xEB, 0xE7, 0xE4,
    0x6F, 0x7E, 0xD3, 0x9A, 0xA4, 0x51, 0xEE, 0x19,
    0x00, // 指示信息=0x00（广播类型1）// 符合GY/T 407-2024 条款附录C.1
    0x08, // 基础能力=0x08（调试机顶盒专用）// 符合GY/T 407-2024 条款附录C.1
    0x02, // 发射功率=0x02（>=2dBm）// 符合GY/T 407-2024 条款5.1.2.3
};

// 回连广播（广播类型2）
// 符合GY/T 407-2024 条款附录C.1、附录C.2
// 说明：不可发现广播（Flags 不含 Discoverable），Service Data 内指示信息为 0x02。
static uint8_t adv_dat_reconnect[] = {
    0x02, 0x01, 0x04, // Flags: BR/EDR Not Supported（不可发现）
    0x03, 0x03, 0x12, 0x18, // HID服务UUID 0x1812
    0x12, 0x21, // AD Type 0x21：Service Data - 128-bit UUID
    0x00, 0xA0, 0x46, 0x3F, 0xC3, 0xEB, 0xE7, 0xE4,
    0x6F, 0x7E, 0xD3, 0x9A, 0xA4, 0x51, 0xEE, 0x19,
    0x02, // 指示信息=0x02（广播类型2）// 符合GY/T 407-2024 条款附录C.1
};

// 唤醒广播（UUID反转+Flags；28字节）
static uint8_t adv_dat_wakeup[] = {
    0x02, 0x01, 0x04,          // Flags: BR/EDR Not Supported（不可发现）
    0x18, 0x21,                // AD Type 0x21: Service Data - 128-bit UUID, 24字节数据
    0x19, 0xEE, 0x51, 0xA4, 0x9A, 0xD3, 0x7E, 0x6F,
    0xE4, 0xE7, 0xEB, 0xC3, 0x3F, 0x46, 0xA0, 0x00, // UUID (16B, 反转)
    0x15,                      // 指示信息: bit0-1=1(类型2) bit2=1(含地址) bit4=1(唤醒)
    0x00, 0x00, 0x00,          // 源地址(LSB在前, 索引22-24)
    0x00, 0x00, 0x00,          // 目标地址(LSB在前, 索引25-27)
};                             // 28 字节
// // 唤醒广播（128-bit UUID Service Data + 地址，28字节；HID UUID 放 Scan Response）
// static uint8_t adv_dat_wakeup[] = {
//     0x02, 0x01, 0x04,          // Flags: BR/EDR Not Supported（不可发现）
//     0x18, 0x21,                // AD Type 0x21: Service Data - 128-bit UUID, 24字节数据
//     0x00, 0xA0, 0x46, 0x3F, 0xC3, 0xEB, 0xE7, 0xE4,
//     0x6F, 0x7E, 0xD3, 0x9A, 0xA4, 0x51, 0xEE, 0x19, // UUID (16B)
//     0x15,                      // 指示信息: bit0-1=1(类型2, 按标准取值1) bit2=1(含地址) bit4=1(唤醒)
//     0x00, 0x00, 0x00,          // 源地址(LSB在前: local_bd[0][1][2], 索引22-24)
//     0x00, 0x00, 0x00,          // 目标地址(LSB在前: stb_mac[0][1][2], 索引25-27)
// };                             // 28 字节

// 唤醒广播 Scan Response（HID UUID，扫描响应不占 31 字节限制）
static uint8_t adv_dat_wakeup_scan_rsp[] = {
    0x03, 0x03, 0x12, 0x18,    // HID 服务 UUID 0x1812
};

static adv_data_t adv_dat_pairing_seg = {0};
static adv_data_t adv_dat_reconnect_seg = {0};
static adv_data_t adv_dat_wakeup_seg = {0};
static adv_data_t adv_dat_wakeup_scan_rsp_seg = {0};
static adv_data_t scan_rsp_adv_dat_seg = {0};
static adv_param_t adv_par_pairing = {0};
static adv_param_t adv_par_reconnect = {0};
static adv_param_t adv_par_wakeup = {0};
// 设备名称（扫描响应里返回 Complete Local Name）
// 可直接修改为自定义名称，例如 "MY_REMOTE"
#define BLE_ADV_NAME "GehuaRemote" // 设备名称，符合GY/T 407-2024 条款附录C.2
#define BLE_ADV_NAME_LEN (sizeof(BLE_ADV_NAME) - 1)// C 语言字符串末尾自动加一个'\0'结束符,减 1 是因为 AD 格式里的长度不包含结束符
// HID Appearance: Keyboard (0x03C1), 低字节在前
#define BLE_ADV_APPEARANCE_LO (0xC1u)
#define BLE_ADV_APPEARANCE_HI (0x03u)
// 扫描响应 AD 结构：Appearance + Name
static uint8_t scan_rsp_adv_dat[4 + 2 + BLE_ADV_NAME_LEN] = {0};

int ble_adv_init(void)
{
    // 组装扫描响应：Appearance + Complete Local Name
    // Appearance (AD type 0x19)
    scan_rsp_adv_dat[0] = 0x03;
    scan_rsp_adv_dat[1] = 0x19;
    scan_rsp_adv_dat[2] = BLE_ADV_APPEARANCE_LO;
    scan_rsp_adv_dat[3] = BLE_ADV_APPEARANCE_HI;
    // Complete Local Name (AD type 0x09)
    scan_rsp_adv_dat[4] = (uint8_t)(BLE_ADV_NAME_LEN + 1);
    scan_rsp_adv_dat[5] = 0x09;
    memcpy(&scan_rsp_adv_dat[6], BLE_ADV_NAME, BLE_ADV_NAME_LEN);

    // 绑定 adv data 到分片结构体
    adv_dat_pairing_seg.data = adv_dat_pairing;
    adv_dat_pairing_seg.len = sizeof(adv_dat_pairing);
    adv_dat_reconnect_seg.data = adv_dat_reconnect;
    adv_dat_reconnect_seg.len = sizeof(adv_dat_reconnect);

    // 绑定扫描响应数据
    scan_rsp_adv_dat_seg.data = scan_rsp_adv_dat;
    scan_rsp_adv_dat_seg.len = sizeof(scan_rsp_adv_dat);

    // 配对态参数：可连接 + 可扫描 + 受限可发现（通过 Flags 表达）
    adv_par_pairing = (adv_param_t){
        .adv_hdl = 0x01,// 用户自定义的广告句柄，范围 0x00~0xEF，后续 start/stop 接口通过该句柄控制
        .adv_ev_prop = (ADV_EV_PROP_BITS_LEGACY | ADV_EV_PROP_BITS_SCANNABLE | ADV_EV_PROP_BITS_CONNECTABLE),// 符合GY/T 407-2024 条款附录C.1 广播类型1：可连接 + 可发现
        .prim_adv_intv_min = 0x20, // 20ms // 符合GY/T 407-2024 条款附录C.2 广播间隔
        .prim_adv_intv_max = 0x20, // 20ms // 符合GY/T 407-2024 条款附录C.2
        .prim_adv_ch_map = (ADV_CH_MAP_37 | ADV_CH_MAP_38 | ADV_CH_MAP_39),//广播通道37、38、39
        .own_addr_type = ADDR_TYPE_PUBLIC,//蓝牙地址类型，使用公共地址
        .peer_addr_type = ADDR_TYPE_PUBLIC,// 定向广告时目标地址类型，非定向广告时置公共地址（不使用）
        .peer_addr = {0},// 定向广告的目标地址，非定向广告时置0
        .adv_filter_policy = ADV_POLICY_PROC_SCAN_CONN_ALL,// 广告过滤策略，对扫描和连接请求都不使用过滤（默认）
        .adv_tx_pwr = 0x7F,// 广播发射功率，0x7F表示主机不指定，由控制器决定使用合适的功率
        .prim_adv_phy = ADV_PHY_1M,// 主广播使用 1M PHY
        .scnd_adv_max_skip = 0x00,// 不跳过任何广告事件，保证扫描响应及时发送
        .scnd_adv_phy = ADV_PHY_1M,
        .adv_sid = 0,// 广告 SID，范围 0x00~0x0F，主广播和扫描响应使用同一个 SID
        .scan_req_notif_en = false,// 不使能扫描请求通知
        .adv_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,// 广告数据分片偏好，允许控制器分片
        .adv_data_frag_num = 1,// 广告数据分片数量为1，指向一个完整的广告数据结构体
        .adv_data_frag = &adv_dat_pairing_seg,
        .scn_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,// 扫描响应数据分片偏好，允许控制器分片
        .scn_data_frag_num = 1,// 扫描响应包含设备名称
        .scn_data_frag = &scan_rsp_adv_dat_seg,
    };

    // 回连态参数：可连接 + 不可发现（Flags 不包含 Discoverable，且不发 scan response）
    adv_par_reconnect = (adv_param_t){
        .adv_hdl = 0x01,
        .adv_ev_prop = (ADV_EV_PROP_BITS_LEGACY | ADV_EV_PROP_BITS_SCANNABLE | ADV_EV_PROP_BITS_CONNECTABLE),
        .prim_adv_intv_min = 0x20,
        .prim_adv_intv_max = 0x20,
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
        .adv_data_frag = &adv_dat_reconnect_seg,
        .scn_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,
        .scn_data_frag_num = 0,
        .scn_data_frag = NULL,
    };

    // 唤醒态参数：可连接 + 不可发现，Scan Response 放 HID UUID
    adv_dat_wakeup_seg.data = adv_dat_wakeup;
    adv_dat_wakeup_seg.len  = sizeof(adv_dat_wakeup);
    adv_dat_wakeup_scan_rsp_seg.data = adv_dat_wakeup_scan_rsp;
    adv_dat_wakeup_scan_rsp_seg.len  = sizeof(adv_dat_wakeup_scan_rsp);
    adv_par_wakeup = (adv_param_t){
        .adv_hdl = 0x01,
        .adv_ev_prop = (ADV_EV_PROP_BITS_LEGACY | ADV_EV_PROP_BITS_SCANNABLE | ADV_EV_PROP_BITS_CONNECTABLE),
        .prim_adv_intv_min = 0x20,
        .prim_adv_intv_max = 0x20,
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
        .adv_data_frag = &adv_dat_wakeup_seg,
        .scn_data_frag_pref = ADV_FRAG_PREF_CNTL_MAY_FRAG,
        .scn_data_frag_num = 1,
        .scn_data_frag = &adv_dat_wakeup_scan_rsp_seg,
    };

    // 默认先添加配对态广播集合，后续通过 start 函数切换/启用
    le_adv_add(&adv_par_pairing, NULL, NULL);

    return 0;
}

int ble_adv_start(void)
{
    co_printf("Starting BLE Advertising...\n");
    // 使能 adv handle 0x01（无持续时间限制）
    adv_en_param_t en_par = {
        .hdl = 0x01,
        .duration = 0,
        .max_events = 0,
    };
    le_adv_enable(1, &en_par, NULL, NULL);
    return 0;
}

int ble_adv_stop(void)
{
    uint8_t adv_hadl = 0x01;
    // 关闭 adv handle 0x01
    le_adv_disable(1, &adv_hadl, NULL, NULL);

    return 0;
}

void ble_adv_start_pairing(void)
{
    // 配对态：可连接非定向、受限可发现广播
    // 符合GY/T 407-2024 条款附录C.1
    co_printf("Starting BLE Advertising for Pairing...\n");
    // 先移除旧 handle，再添加配对态参数并启用
    le_adv_remove(0x01, NULL, NULL);
    le_adv_add(&adv_par_pairing, NULL, NULL);
    ble_adv_start();
}

void ble_adv_start_reconnect(void)
{
    // 回连态：可连接非定向、不可发现广播
    // 符合GY/T 407-2024 条款附录C.1
    co_printf("Starting BLE Advertising for Reconnection...\n");
    // 先移除旧 handle，再添加回连态参数并启用
    le_adv_remove(0x01, NULL, NULL);
    le_adv_add(&adv_par_reconnect, NULL, NULL);
    ble_adv_start();
}

void ble_adv_start_wakeup(void)
{
    /* SDK Flash reads round up to a 32-bit word; reserve eight bytes. */
    uint8_t stb_mac[8] = {0};
    uint32_t addr = BLE_BONDING_STORE_ADDR + 13;

    co_printf("Starting BLE Advertising for Wakeup...\n");

    // 读 STB MAC
    if (Qflash_Read(addr, stb_mac, sizeof(stb_mac)) != FlashOperationSuccess) {
        co_printf("Wakeup address read failed\r\n");
        return;
    }

    // 源地址（遥控器 MAC 低 3 字节，LSB 在前）→ 索引 22-24
    adv_dat_wakeup[22] = local_bd[0];
    adv_dat_wakeup[23] = local_bd[1];
    adv_dat_wakeup[24] = local_bd[2];
    // 目标地址（STB MAC 低 3 字节，LSB 在前）→ 索引 25-27
    adv_dat_wakeup[25] = stb_mac[0];
    adv_dat_wakeup[26] = stb_mac[1];
    adv_dat_wakeup[27] = stb_mac[2];

    co_printf("Wakeup raw[18]:");
    for (uint8_t j = 0; j < sizeof(adv_dat_wakeup); j++)
        co_printf(" %02X", adv_dat_wakeup[j]);
    co_printf("\r\n");

    // 切换广播
    ble_adv_stop();
    cs_schedule();
    cs_schedule();
    le_adv_remove(0x01, NULL, NULL);
    cs_schedule();
    le_adv_add(&adv_par_wakeup, NULL, NULL);
    ble_adv_start();

    // 等一个广播间隔(20ms)，确保首个唤醒广播包已发出
    for (uint8_t k = 0; k < 5; k++) {
        cs_schedule();
        delay_n_10us(400U);  // 4ms × 5 = 20ms
    }
}

void ble_adv_start_by_bonding(void)
{
    if (peer_bonded_count() > 0) {
        ble_adv_start_reconnect();
    } else if (g_pairing_active) {
        // KEY14+KEY3 组合键已触发，启动可发现广播
        ble_adv_start_pairing();
    } else {
        // 无绑定且未触发配对（静默态）：不广播，由组合键手动发起
        co_printf("No bonding, skip advertising. Press volume- + left (KEY14+KEY5) to pair.\r\n");
    }
}
