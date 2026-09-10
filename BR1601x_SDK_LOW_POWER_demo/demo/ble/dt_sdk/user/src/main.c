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
 * @file main.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include <stdio.h>
#include "br1601x.h"
#include "it.h"
#include "cs_core.h"
#include "task.h"
#include "cfg.h"
#include "flash_manage.h"
#include "atprocess.h"
#include "rbuf_data.h"
#include "app_ble.h"
#include "app_power.h"
#include "app_whitelist.h"
#include "app_power.h"
#include "app_task.h"
#include "app_gpio.h"
#include "co_printf.h"
#include "porting.h"
#include "log.h"

#ifdef CFG_STACKTRACE
#include "cm_backtrace.h"
#endif

/**
 * GLOBAL VARIABLES DECLARATION
 ****************************************************************************************
 */

/**
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
void host_init_done(HANDLE hdl, void *context, void *arg, UINT16 len) 
{
    extern void sdk_timer_ext_init(bool (*cbk)(uint16_t tid));
    sdk_timer_ext_init(timer_handle);
#if defined(BR1601A01) || defined(BR1601A02)
    br_modem_set(PHY_1MBPS);
#endif
    cmd_phy_pref_t dft_phy;
    cfg_reserver_get(PHYPREF_OFF, &dft_phy, PHYPREF_LEN);
    ble_set_def_phy(dft_phy.tx_phys, dft_phy.rx_phys);
    user_ble_event_cb(BLE_READY, NULL);
}

int main(void)
{
    delay_n_ms(50);
    /// 日志初始化
    log_init();
#ifdef CFG_STACKTRACE
    extern void (*user_HardFault_Handler)(void);
    user_HardFault_Handler = user_hardfault;
    cm_backtrace_init("dt", "BR1601x", BR_FIRMWARE_VERSION); 
#endif 
    /// 任务模块初始化
    task_init();
    /// flash管理初始化
    flash_manage_init();
    /// 配置模块初始化
    cfg_init();
    /// 发射功率控制
    uint8_t txlevel;
    cfg_get(CFG_TYPE_TXPOWER, &txlevel, sizeof(uint8_t));
	br_tx_power_set(txlevel);
	// 功耗模块初始化 
    app_power_init((power_evt_cb_t *)user_power_event_cb); 
    /// 校准频偏
    uint32_t trim = br_radio_freq_offset_get();
    radio_freq_cal_set((trim == 8000000) ? 2402042035 : (uint32_t)((trim/8.0)*2402));
    /// 蓝牙初始化
    app_ble_init((app_ble_event_cbk_t)user_ble_event_cb);
    /// 串口模块初始化
    cfg_get(CFG_TYPE_UART_BAUD, &trans_init.baudrate, sizeof(trans_init.baudrate));
    cfg_get(CFG_TYPE_UART_FC, &trans_init.flow_ctrl, sizeof(trans_init.flow_ctrl));
    cfg_get(CFG_TYPE_IND_PREFIX, &trans_init.prefix_type, sizeof(trans_init.prefix_type));
    cfg_get(CFG_TYPE_IND_SUFFIX, &trans_init.suffix_type, sizeof(trans_init.suffix_type));
    cfg_reserver_get(SILENT_OFF, &trans_init.btmode, SILENT_LEN);
    trans_uart_init(&trans_init);
    trans_uart_cb_update((trans_uart_rcv_callback_t)cmd_parse);
    trans_uart_send_enable(false);
    // 指令模块初始化
    cmd_pkt_done_set((cmd_pkt_proc_done_t)trans_uart_pkt_proc_done);
    cmd_rsp_out_set((cmd_rsp_fmt_out_t)trans_uart_event_ind);
    cmd_callback_set(user_atcmd_evt_ck);
    /// 白名单初始化
    user_while_list_init();
    /// 其余io初始化
    ble_disc_io_init();
    ble_conn_io_init();
    ble_adv_io_init();
    while (1)
    {
        cs_schedule();
        uint8_t task_busy = task_schedule();
        user_ble_sleep_task();
        
        if(task_busy == 0 && (!BLE_SLEEP_LOCK_MASK_GET())) { 
            br_sleep();
        }
        user_ble_wakeup_task();
    }
}
