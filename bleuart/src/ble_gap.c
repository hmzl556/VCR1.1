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
 * @file ble_gap.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include <stdio.h>
#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "ble_gap.h"
#include "ble_adv.h"
#include "br_timer.h"
#include "gmem.h"
#include "app_voice_ctrl.h"
#include "ble_dtss.h"
#include "userhard.h"
#include "task_keyscan.h"
#include "ble_hid_kbd.h"
#include "rc_trans_assembly.h"
#include "ble_rc_svc.h"
#include "ble_bas.h"
#include "rc_cat1_bridge.h"
#include "pm_state.h"       /* 电源管理状态机 — 连接/断连通知 */
#include "combo_key.h"      /* 组合键管理器 — 解配进行中判断     */
#include "udp_ota.h"
#ifndef BLE_CONN_PARAM_PERSIST_ENABLE
#define BLE_CONN_PARAM_PERSIST_ENABLE 0 // 默认不启用连接参数持久化，避免误操作导致 Flash 损耗
#endif

#if BLE_CONN_PARAM_PERSIST_ENABLE
#include "qflash.h"
#include "br_dfu_boot.h"
#endif

// ====== 配对信息持久化（Bonding Info Persistence）======
// 将首次连接后的配对信息（LTK/IRK等）保存到Flash，
// 保证重新上电后机顶盒可以根据保存的绑定信息自动连接遥控器
#ifndef BLE_BONDING_PERSIST_ENABLE
#define BLE_BONDING_PERSIST_ENABLE  1  // 默认使能配对信息持久化
#endif

// #if BLE_BONDING_PERSIST_ENABLE
// // 需要包含qflash和地址定义（无论连接参数持久化是否开启）
// #include "qflash.h"
// #include "br_dfu_boot.h"

// // 绑定数据存储在 APP1 区域末尾的最后一个扇区
// // APP1: 0x01004000 ~ 0x01020000 (112KB)，遥控器固件通常 < 60KB，末尾安全
// // 固件占用 0x01000000~0x0101F428（含 RW 初始化数据），APP1 末尾无空闲
// // 使用 APP2 末尾扇区（0x0103B000），与固件/OTA 区均不重叠
// #ifndef BLE_BONDING_STORE_ADDR
// #define BLE_BONDING_STORE_ADDR      (BR_APP2_START_ADDRESS + BR_APP2_DEFAULT_SIZE - FLASH_SECTOR_SIZE)
// #endif
// #define BLE_BONDING_STORE_MAGIC     0x424F4E44  // "BOND"
// #define BLE_BONDING_STORE_VER       2           // v2: 新增对端地址字段
// #define BLE_BONDING_MAX_DATA_SZ     256         // 最大绑定数据大小（实际通常<100字节）
#if BLE_BONDING_PERSIST_ENABLE
#include "qflash.h"
#include "flash_layout.h"//回连绑定信息存储在FLASH中，在此文件中定义了BLE_BONDING_STORE_ADDR
/* BLE 绑定扇区 @ 0x01069000（OTA 配置扇区 + 4KB） */
#define BLE_BONDING_STORE_MAGIC     0x424F4E44  // "BOND"
#define BLE_BONDING_STORE_VER       2           // v2: 新增对端地址字段
#define BLE_BONDING_MAX_DATA_SZ     256         // 最大绑定数据大小（实际通常<100字节）


// 绑定信息Flash存储头部（后面紧跟实际绑定数据）
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t data_sz;           // 有效绑定数据大小（字节）
    uint8_t  peer_addr_type;    // 对端身份地址类型（用于白名单）
    uint8_t  peer_addr[6];      // 对端 BD_ADDR（用于白名单回连过滤）
    uint32_t checksum;          // 对data的简单求和校验
    // 紧接着: uint8_t data[data_sz];
} ble_bonding_store_hdr_t;
#endif

peer_t g_peer = NULL; // 当前连接的 Peer 设备句柄，NULL 表示未连接
uint32_t conn_update_timer_id = BR_TIMER_INVALID_HANDLER;

static volatile uint8_t s_disconnect_work_pending = 0U;

#if BLE_CONN_PARAM_PERSIST_ENABLE
#ifndef BLE_CONN_PARAM_STORE_ADDR
#define BLE_CONN_PARAM_STORE_ADDR BR_APP_DATA_START_ADDRESS
#endif
#endif

// 连接参数持久化数据的魔数/版本
#define BLE_CONN_PARAM_STORE_MAGIC 0x434F4E4E
#define BLE_CONN_PARAM_STORE_VER   1

// 连接间隔范围（单位 1.25ms）
#define BLE_CONN_INTV_MIN_5MS      0x0004//最小连接间隔 5ms
#define BLE_CONN_INTV_MAX_50MS     0x0028//最大连接间隔 50ms
#define BLE_CONN_INTV_MIN_LL       0x0001
#define BLE_CONN_INTV_MAX_LL       0x0004

// 从机延迟范围（单位：连接事件个数）
#define BLE_CONN_LATENCY_MIN       40 // 最小从机延迟 40 个连接事件（符合GY/T 407-2024 条款附录C.2）
#define BLE_CONN_LATENCY_MAX       100// 最大从机延迟 100 个连接事件（非强制要求，按 BLE 5.0 通用推荐值保留）

// 默认工作态参数（10ms / latency 40）
#define BLE_CONN_WORK_INTV         0x0008// 默认连接间隔 10ms，符合GY/T 407-2024 条款
#define BLE_CONN_WORK_LATENCY      40    // 默认从机延迟 40 个连接事件，符合GY/T 407-2024 条款附录C.2
#define BLE_CONN_WORK_TIMEOUT      800

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t reserved;
	uint16_t intv_min;
	uint16_t intv_max;
	uint16_t latency;
	uint16_t timeout;
	uint32_t checksum;
} ble_conn_param_store_t;

// 最近一次合规的连接参数（默认从持久化或内置值恢复）
static ble_conn_param_store_t g_last_good_params = {0};

static uint32_t ble_conn_param_checksum(const ble_conn_param_store_t *p)
{
	// 简单校验：异或字段，便于检测读写错误
	uint32_t sum = 0;
	sum ^= p->magic;
	sum ^= p->version;
	sum ^= p->intv_min;
	sum ^= p->intv_max;
	sum ^= p->latency;
	sum ^= p->timeout;
	return sum;
}

static bool ble_conn_params_is_work_range(uint16_t intv_min, uint16_t intv_max, uint16_t latency)
{
	// 强制范围检查：连接间隔 + 延迟数必须在规范区间内
	if (intv_min < BLE_CONN_INTV_MIN_5MS || intv_max > BLE_CONN_INTV_MAX_50MS) {
		return false;
	}
	if (intv_min > intv_max) {
		return false;
	}
	if (latency < BLE_CONN_LATENCY_MIN || latency > BLE_CONN_LATENCY_MAX) {
		return false;
	}
	return true;
}

static void ble_conn_params_set_default(ble_conn_param_store_t *out)
{
	// 默认工作态参数（合规）
	out->magic = BLE_CONN_PARAM_STORE_MAGIC;
	out->version = BLE_CONN_PARAM_STORE_VER;
	out->reserved = 0;
	out->intv_min = BLE_CONN_WORK_INTV;   // 符合GY/T 407-2024 条款5.1.2.3：10ms 在 5ms~50ms 范围内
	out->intv_max = BLE_CONN_WORK_INTV;   // 符合GY/T 407-2024 条款5.1.2.3：10ms 在 5ms~50ms 范围内
	out->latency = BLE_CONN_WORK_LATENCY; // 符合GY/T 407-2024 条款附录C.2：延迟数 40
	out->timeout = BLE_CONN_WORK_TIMEOUT; // 非强制参数：按 BLE 5.0 通用推荐值保留
	out->checksum = ble_conn_param_checksum(out);
}

static void ble_conn_params_save(const ble_conn_param_store_t *params)
{
#if BLE_CONN_PARAM_PERSIST_ENABLE
	// 写入 Flash（注意：仅在开启持久化且地址安全时使用）
	ble_conn_param_store_t tmp = *params;
	tmp.checksum = ble_conn_param_checksum(&tmp);
	Qflash_Erase_Sector(BLE_CONN_PARAM_STORE_ADDR);
	Qflash_Write(BLE_CONN_PARAM_STORE_ADDR, (uint8_t *)&tmp, sizeof(tmp));
#else
	(void)params;
#endif
}

static bool ble_conn_params_load(ble_conn_param_store_t *out)
{
#if BLE_CONN_PARAM_PERSIST_ENABLE
	// 从 Flash 读取并做完整性与范围校验
	Qflash_Read(BLE_CONN_PARAM_STORE_ADDR, (uint8_t *)out, sizeof(*out));
	if (out->magic != BLE_CONN_PARAM_STORE_MAGIC || out->version != BLE_CONN_PARAM_STORE_VER) {
		return false;
	}
	if (out->checksum != ble_conn_param_checksum(out)) {
		return false;
	}
	if (!ble_conn_params_is_work_range(out->intv_min, out->intv_max, out->latency)) {
		return false;
	}
	return true;
#else
	(void)out;
	return false;
#endif
}

static void ble_conn_params_apply(peer_t peer, const ble_conn_param_store_t *params)
{
	// 向控制器发起连接参数更新请求
	le_conn_update_t par;
	par.peer = peer;
	par.intv_min = params->intv_min;
	par.intv_max = params->intv_max;
	par.latency_max = params->latency;
	par.timeout = params->timeout;
	par.ce_len_min = 0;
	par.ce_len_max = 0;
	le_conn_update(&par);
}

// CCCD 自旋重试：每 500ms 检查一次，最多 6 次（3s），STB 写了就停
#define CCCD_RETRY_INTERVAL_MS  500
#define CCCD_RETRY_MAX          6

static uint32_t s_cccd_retry_tid = BR_TIMER_INVALID_HANDLER;
static uint8_t s_cccd_retry_count, s_connection_generation;

static void cccd_retry_handle(uint8_t event, void *arg)
{
    (void)arg;
    if (event != s_connection_generation || g_peer == NULL) return;
    s_cccd_retry_tid = BR_TIMER_INVALID_HANDLER;
    if (!peer_bonded_count() || hid_kbd_is_any_cccd_enabled()) {
        s_cccd_retry_count = 0u;
        return;
    }
    if (++s_cccd_retry_count >= CCCD_RETRY_MAX) {
        hid_kbd_force_cccd_enable(g_peer);
        s_cccd_retry_count = 0u;
    } else s_cccd_retry_tid = sdk_timer_add(CCCD_RETRY_INTERVAL_MS,
                         s_connection_generation, cccd_retry_handle, NULL);
}

static void conn_parm_updata_handle(uint8_t event,void *arg)
{
    (void)arg;
    if (event != s_connection_generation || g_peer == NULL) return;
    conn_update_timer_id = BR_TIMER_INVALID_HANDLER;
    ble_conn_params_apply(g_peer, &g_last_good_params);
    if (peer_bonded_count()) cccd_retry_handle(event, NULL);
}

// 连接参数更新完成回调，成功时保存参数，失败时保持之前的好参数不变
#if BLE_BONDING_PERSIST_ENABLE 
static void ble_gap_peer_bonded(gap_event_t ev, void *ctx, peer_inf_update_t *arg)
{
    co_printf("peer bonded! inf_sz:%d", (int)arg->inf_sz);
    if (arg->inf != NULL && arg->inf_sz > 0 && arg->inf_sz <= BLE_BONDING_MAX_DATA_SZ)
    {
        uint32_t hdr_sz = sizeof(ble_bonding_store_hdr_t);
        uint32_t total_sz = hdr_sz + arg->inf_sz;
        uint8_t buf[sizeof(ble_bonding_store_hdr_t) + BLE_BONDING_MAX_DATA_SZ] = {0};

        ble_bonding_store_hdr_t *hdr = (ble_bonding_store_hdr_t *)buf;
        hdr->magic = BLE_BONDING_STORE_MAGIC;
        hdr->version = BLE_BONDING_STORE_VER;
        hdr->reserved = 0;
        hdr->data_sz = arg->inf_sz;

        // 获取对端身份地址，回连广播白名单过滤使用
        uint8_t addr_type = ADDR_TYPE_PUBLIC;
        bd_addr_t addr = {0};
        if (smp_get_ident_addr(arg->peer, &addr_type, addr) ||
            peer_addr_get(arg->peer, &addr_type, addr)) {
            hdr->peer_addr_type = addr_type;
            memcpy(hdr->peer_addr, addr, 6);
            co_printf("peer ident addr: type=%d %02X:%02X:%02X:%02X:%02X:%02X",
                addr_type, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        } else {
            co_printf("bonding address unavailable, not persisted");
            return;
        }

        memcpy(buf + hdr_sz, arg->inf, arg->inf_sz);

        uint32_t sum = 0;
        for (uint32_t i = 0; i < arg->inf_sz; i++) {
            sum += arg->inf[i];
        }
        hdr->checksum = sum;

        if (Qflash_Erase_Sector(BLE_BONDING_STORE_ADDR) != FlashOperationSuccess ||
            Qflash_Write(BLE_BONDING_STORE_ADDR, buf, total_sz) != FlashOperationSuccess) {
            co_printf("bonding flash write failed");
        } else co_printf("bonding info saved to flash");
    }
}


static void ble_gap_peer_recover_cmpl(gap_event_t ev, void *ctx, peer_recover_cmpl_t *arg)
{
    
    co_printf("peer recover result:%d", arg->result);
}
#endif

void ble_gap_connected(gap_event_t ev, void *ctx, gap_conn_cmpl_t *arg)
{
    if (arg == NULL || arg->result != GAP_HCI_STATUS_OK || arg->peer == NULL) {
        co_printf("[GAP  ] failed connection ignored\r\n");
        return;
    }
    if (pm_state_get() == PM_STATE_SLEEP) {
        le_conn_destory(arg->peer);
        return;
    }
    ++s_connection_generation;
    s_cccd_retry_count = 0u;
	//app_usart_dma_enable(ENABLE);
	co_printf("conn complete, %08X, %02X", (uint32_t)arg->peer, arg->result);
	bleConnectOk = 0xa6;	// 连接成功标志，语音notify需要！！！！！！
	g_peer = arg->peer;
	co_printf("Connected peer: %08X", (uint32_t)g_peer);		
	ble_adv_stop(); // 符合GY/T 407-2024 条款附录C.2
	ble_rc_svc_on_connect(g_peer);
	// 通知电源管理状态机：连接建立，由配对/回连态切换到工作态
	pm_notify_connected();
	// 连接后延时触发参数更新，避免刚连接时立即修改导致失败
	conn_update_timer_id = sdk_timer_add(3000, s_connection_generation, conn_parm_updata_handle, NULL); // 符合GY/T 407-2024 条款5.1.2.3
}

void ble_gap_disconnected(gap_event_t ev, void *ctx, gap_disc_cmpl_t *arg)
{
	(void)ev;
	(void)ctx;

    ++s_connection_generation;
    s_cccd_retry_count = 0u;
    if (s_cccd_retry_tid != BR_TIMER_INVALID_HANDLER) {
        (void)sdk_timer_del(s_cccd_retry_tid);
        s_cccd_retry_tid = BR_TIMER_INVALID_HANDLER;
    }
	//app_usart_dma_enable(DISABLE);
	// 断开后清理定时器
	if (conn_update_timer_id != BR_TIMER_INVALID_HANDLER) {
		sdk_timer_del(conn_update_timer_id);
		conn_update_timer_id = BR_TIMER_INVALID_HANDLER;
	}

	/*
	 * 通知电源管理状态机：连接断开。
	 * PM 根据是否有绑定记录自动决定进入回连态或配对态。
	 * 注意：此调用必须在 bleConnectOk/g_peer 清零之前执行，
	 *       因为 pm_notify_disconnected 内部需要查询 peer_bonded_count()。
	 */
	g_peer = NULL; /* Do not update connection parameters through a dead peer. */
	pm_notify_disconnected();

	//ble_adv_start_by_bonding();

	(void)voice_ctrl_post_stop();
	bleConnectOk = 0U;
	bleSendEn = 0U;
	dtss_peer = NULL;
	dtss_chr = NULL;
	ble_dtss_on_disconnect();
	g_peer = NULL;
	s_disconnect_work_pending = 1U;
	co_printf("\r\n[DISCONNECT] OK! %08X, %02X\r\n\r\n", (uint32_t)arg->peer, arg->result);
}

/**
* @brief 处理 BLE 断开连接的 pending 事件
*/
void ble_gap_process_disconnect_pending(void)
{
	if (s_disconnect_work_pending == 0U) {
		return;
	}
	s_disconnect_work_pending = 0U;
    {
        uint16_t wake_key = key_receive;
        task_keyscan_reset_on_ble_disconnect();
        if (pm_state_get() == PM_STATE_SLEEP) key_receive = wake_key;
    }
	hid_kbd_on_disconnect();
	rc_trans_reset();
	rc_cat1_cancel_pending();
	ble_rc_svc_on_disconnect();
	ble_bas_on_disconnect();
    if (pm_state_get() == PM_STATE_SLEEP) return;

	// 解配进行中时跳过广播重启，由 combo_key 状态机处理后续
	if (combo_key_unpair_in_progress()) {
		co_printf("Unpair in progress, skip advertising restart.\r\n");
		return;
	}

	/* OTA 会话期间禁止断连后重启广播（广播干扰 Cat.1 OTA 接收） */
	if (udp_ota_session_busy() != 0u) {
		co_printf("[GAP  ] OTA session busy, skip advertising restart.\r\n");
		return;
	}

	ble_adv_start_by_bonding();
}

static void ble_conn_read_rssi(gap_event_t ev, void *ctx, rssi_t *arg)
{
	co_printf("%s %d rssi:%d state:%02x peer:%08x\r\n",__FUNCTION__,__LINE__,arg->rssi,arg->status,arg->peer);
}

static void ble_gap_peer_conn_par_req(gap_event_t ev, void *ctx, le_peer_conn_par_req_t *arg)
{
	// USART1_Init(ENABLE);//
	// printf("_le_peer_conn_par_req, %08X\n", (uint32_t)arg->peer);
	// USART1_Init(DISABLE);
	//co_printf("_le_peer_conn_par_req, %08X\r\n", (uint32_t)arg->peer);
	// 对端请求的连接参数，先按请求值回应
	le_conn_update_t par = {
		.peer = arg->peer,
		.intv_min = arg->intv_min,
		.intv_max = arg->intv_max,
		.latency_max = arg->lantency_max,
		.timeout = arg->timeout,
		.ce_len_min = 0,
		.ce_len_max = 0,
	};
	if (!ble_conn_params_is_work_range(par.intv_min, par.intv_max, par.latency_max)) {
		// 请求超出规范范围：回退为最近一次合规参数
		//co_printf("conn param req out of range, use last good params\r\n");
		par.intv_min = g_last_good_params.intv_min;
		par.intv_max = g_last_good_params.intv_max;
		par.latency_max = g_last_good_params.latency;
		par.timeout = g_last_good_params.timeout;
	}
	le_peer_conn_par_reply(&par, NULL, NULL);
	le_conn_read_rssi(arg->peer);
	gatts_exchange_mtu(arg->peer,515);
}

static void ble_gap_encrypt_cmpl(gap_event_t ev, void *ctx, encrypt_cmpl_t *arg)
{
    co_printf("encrypt complete, peer:%08X status:%02X", (uint32_t)arg->peer, arg->status);
}

void ble_peer_pair_req(gap_event_t ev, void *ctx, pairing_req_t *arg)
{
	#if 1
	co_printf("Just Works pairing, auto accept\r\n");
	smp_pair_accept(arg->peer,true);
	#else
	co_printf("User key confirmation \n");
	smp_pair_accept(arg->peer,?);
	#endif
}

void ble_gap_mtu_exchange(gap_event_t ev, void *ctx, att_mtu_ntf_t *arg)
{
	co_printf("[MTU] mtu_exchange_complete: %d\r\n", arg->mtu);
	//ble_att_mtu = arg->mtu;
}

static void ble_gap_conn_update_cmpl(gap_event_t ev, void *ctx, le_peer_conn_update_cmpl_t *arg)
{
	// 连接参数更新结果处理：成功则保存为“最新合规参数”
	if (arg->result == GAP_HCI_STATUS_OK) {
		if (ble_conn_params_is_work_range(arg->conn_intv, arg->conn_intv, arg->lantency_max)) {
			g_last_good_params.intv_min = arg->conn_intv;
			g_last_good_params.intv_max = arg->conn_intv;
			g_last_good_params.latency = arg->lantency_max;
			g_last_good_params.timeout = arg->timeout;
			g_last_good_params.checksum = ble_conn_param_checksum(&g_last_good_params);
			ble_conn_params_save(&g_last_good_params); // 符合GY/T 407-2024 条款7.2：参数持久化
		}
	} else {
		// 更新失败：保留上一次合规参数，不做越界配置
		co_printf("conn param update failed:%02X, keep last good params", arg->result);
	}
}

static gap_ev_cbk_t ble_gap_ev_cbk[] = {
    {GAP_EV_CONN_CMPL,              (gap_cbk_t *)ble_gap_connected        },//连接成功
    {GAP_EV_DISC_CMPL,              (gap_cbk_t *)ble_gap_disconnected     },// 连接断开
    {GAP_EV_LE_PEER_CONN_PAR_REQ,   (gap_cbk_t *)ble_gap_peer_conn_par_req},// 对端发来的参数
    {GAP_EV_READ_RSSI_CMPL,         (gap_cbk_t *)ble_conn_read_rssi       },
    {GAP_EV_PAIRING_REQ,            (gap_cbk_t *)ble_peer_pair_req        },// 对端发起的配对请求
    {GAP_EV_ENCRYPT_CMPL,           (gap_cbk_t *)ble_gap_encrypt_cmpl     },// 链路加密完成
    {GAP_EV_ATT_MTU_NOTIFY,         (gap_cbk_t *)ble_gap_mtu_exchange     },
    {GAP_EV_LE_CONN_UPDATE_CMPL,    (gap_cbk_t *)ble_gap_conn_update_cmpl },// 连接参数更新完成
#if BLE_BONDING_PERSIST_ENABLE
    {GAP_EV_PEER_BONDED,            (gap_cbk_t *)ble_gap_peer_bonded      },// 绑定完成，保存配对信息
    {GAP_EV_PEER_RECOVER_CMPL,      (gap_cbk_t *)ble_gap_peer_recover_cmpl},// 绑定恢复完成
#endif
};


int ble_gap_init(void)
{
	gap_init();
#if BLE_CONN_PARAM_PERSIST_ENABLE || BLE_BONDING_PERSIST_ENABLE
	// 初始化 Flash 访问接口（仅在启用持久化时）
	Qflash_Init();
#endif
	// 先尝试从持久化读取，失败则使用默认合规参数
	if (!ble_conn_params_load(&g_last_good_params)) {
		ble_conn_params_set_default(&g_last_good_params); // 符合GY/T 407-2024 条款7.2：默认合规参数
		ble_conn_params_save(&g_last_good_params);
	}
	// 配对/安全配置：Just Works，无输入输出
	gap_set_io_cap(IO_CAPABILITY_NOINPUTNOOUTPUT);
	gap_set_bondable(true); // HID兼容性：开启绑定，便于机顶盒配对记忆
	gap_set_sec_level(GAP_SECURITY_MODE1_LEVEL2); // HID兼容性：要求加密但不强制MITM，符合大多数机顶盒的配对流程
	gap_set_mitm_protect(false); // HID兼容性：禁用MITM，避免PIN/确认交互
	// 订阅 GAP 事件
	gap_ev_subscribe(ble_gap_ev_cbk, sizeof(ble_gap_ev_cbk) / sizeof(gap_ev_cbk_t));

// 连接成功后，机顶盒会基于之前的绑定信息自动重连，尝试从 Flash 恢复绑定信息，如果恢复成功则自动连接到之前绑定的设备，无需重新配对
#if BLE_BONDING_PERSIST_ENABLE
    // 尝试从Flash恢复绑定信息，使机顶盒能基于之前的配对自动重连
    {
        uint32_t hdr_sz = sizeof(ble_bonding_store_hdr_t);
        uint8_t bond_buf[sizeof(ble_bonding_store_hdr_t) + BLE_BONDING_MAX_DATA_SZ] = {0};

        if (Qflash_Read(BLE_BONDING_STORE_ADDR, bond_buf, hdr_sz) != FlashOperationSuccess)
            memset(bond_buf, 0, sizeof(bond_buf));
        ble_bonding_store_hdr_t *bond_hdr = (ble_bonding_store_hdr_t *)bond_buf;

        if (bond_hdr->magic == BLE_BONDING_STORE_MAGIC
            && bond_hdr->version == BLE_BONDING_STORE_VER
            && bond_hdr->data_sz > 0
            && bond_hdr->data_sz <= BLE_BONDING_MAX_DATA_SZ) {

            // 读取完整数据
            if (Qflash_Read(BLE_BONDING_STORE_ADDR, bond_buf, hdr_sz + bond_hdr->data_sz) != FlashOperationSuccess)
                bond_hdr->magic = 0u;

            // 校验和验证
            uint32_t sum = 0;
            for (uint32_t i = 0; i < bond_hdr->data_sz; i++) {
                sum += bond_buf[hdr_sz + i];
            }
            
            // 校验和验证通过才恢复绑定信息，避免错误数据导致连接问题
            if (bond_hdr->magic == BLE_BONDING_STORE_MAGIC && sum == bond_hdr->checksum) {
                co_printf("bonding found, recovering %d bytes...", (int)bond_hdr->data_sz);
                peer_recover(bond_buf + hdr_sz, bond_hdr->data_sz, NULL, NULL);
                co_printf("bonding info recovered OK");

                // 将对端地址加入白名单，回连广播仅允许此设备接入
                if (bond_hdr->version >= 2) {
                    le_wl_clear();
                    le_wl_add((addr_type_t)bond_hdr->peer_addr_type, bond_hdr->peer_addr);
                    co_printf("wl add peer: type=%d %02X:%02X:%02X:%02X:%02X:%02X",
                        bond_hdr->peer_addr_type,
                        bond_hdr->peer_addr[5], bond_hdr->peer_addr[4],
                        bond_hdr->peer_addr[3], bond_hdr->peer_addr[2],
                        bond_hdr->peer_addr[1], bond_hdr->peer_addr[0]);
                }
            }
        }
    }
#endif
    return 0;
}

// 符合GY/T 407-2024 条款附录C.3：低时延模式流程框架预留
static void ble_low_latency_query(peer_t peer)
{
	(void)peer;
	// 1. 机顶盒查询低时延能力
	// 2. 遥控器返回支持的最小连接间隔
}

static void ble_low_latency_enter(peer_t peer)
{
	(void)peer;
	// 3. 机顶盒请求进入低时延态
	// 4. 双方同步切换连接间隔至 1ms~5ms (单位 1.25ms)
}

static void ble_low_latency_exit(peer_t peer)
{
	(void)peer;
	// 5. 机顶盒请求退出低时延态，恢复普通工作态连接参数
}

peer_t ble_gap_get_peer(void)
{
	return g_peer;
}



// 配对广播 LED2 快闪指示（200ms 周期）
// 仅 KEY14+KEY5 组合键触发后 (g_pairing_active=true) 才闪烁
#define PAIRING_LED_FLASH_MS 200U
static uint32_t g_pairing_led_ts_ms = 0;
static bool     g_pairing_led_state = false;

volatile bool g_pairing_active = false;

void ble_gap_led_indicate(void)
{
	if (g_pairing_active && ble_gap_get_peer() == NULL && peer_bonded_count() == 0) {
		uint32_t now = app_now_ms();
		if ((now - g_pairing_led_ts_ms) >= PAIRING_LED_FLASH_MS) {
			g_pairing_led_ts_ms = now;
			g_pairing_led_state = !g_pairing_led_state;
			if (g_pairing_led_state) SETLED2; else CLRLED2;
		}
	}
	else {
		if (g_pairing_led_state) {
			g_pairing_led_state = false;
			CLRLED2;
		}
	}
}

// 清除 Flash 中的配对与绑定信息（恢复出厂设置时调用）
// 绑定位于独立0x01069000扇区；写0使magic失效，避免解配时整扇区擦除。
void ble_gap_clear_bonding_info(void)
{
    peer_t peer;

    // 清除配对广播活跃标志
    g_pairing_active = false;

    // 先断开当前连接
    peer = ble_gap_get_peer();
    if (peer != NULL) {
        le_conn_destory(peer);
        co_printf("Disconnecting before clearing bonding...\r\n");
    }

#if BLE_BONDING_PERSIST_ENABLE
    {
        uint32_t invalid_magic[1] = {0x00000000};
        Qflash_Write(BLE_BONDING_STORE_ADDR, (uint8_t *)invalid_magic, sizeof(invalid_magic));
        co_printf("bonding info invalidated\r\n");

        peer_bonded_clear();
        co_printf("bonding RAM cleared, count=%d\r\n", peer_bonded_count());
    }
#endif
}
