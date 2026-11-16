/**
 * @file ble_upg_svc.c
 * @brief 三模通用遥控器升级服务的 GATT 注册与收发
 *
 * 静态 RAM 只保留服务与特征句柄；对端句柄、订阅状态放在借来的会话工作区。
 */
#include "ble_upg_svc.h"
#include "upg_proto.h"
#include "upg_session.h"
#include "pm_state.h"       /* pm_activity_notify — 对端数据下发，退出待机 */
#include "gatt.h"
#include "gap.h"
#include "co_printf.h"
#include <string.h>

/* 10111213-1415-1617-1819-1a1b1c1d1931（本 SDK 的 uuid128 为反序存放） */
static const uint8_t k_svc_uuid[16] = {
    0x31u, 0x19u, 0x1Du, 0x1Cu, 0x1Bu, 0x1Au, 0x19u, 0x18u,
    0x17u, 0x16u, 0x15u, 0x14u, 0x13u, 0x12u, 0x11u, 0x10u
};
/* 10111213-1415-1617-1819-1a1b1c1d2b31 */
static const uint8_t k_chr_uuid[16] = {
    0x31u, 0x2Bu, 0x1Du, 0x1Cu, 0x1Bu, 0x1Au, 0x19u, 0x18u,
    0x17u, 0x16u, 0x15u, 0x14u, 0x13u, 0x12u, 0x11u, 0x10u
};

static gatt_svc_t s_svc;
static gatt_chr_t s_chr;

static void upg_svc_cbk(gatt_event_t event, void *context, void *param)
{
    if (event == GATTS_EV_SERVICE_START) {
        gatt_svc_start_t *in = (gatt_svc_start_t *)param;
        co_printf("[UPG ] svc start=%X\r\n", in->result);
    }
}

static void upg_chr_cbk(gatt_event_t event, void *context, void *param)
{
    static const uint8_t empty[1] = { 0u };

    if (event == GATTS_EV_CHR_VALUE_WRITE_REQ || event == GATTS_EV_CHR_VALUE_WRITE) {
        gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
        upg_ws_t *ws;

        if (in->val == NULL || in->val_len == 0u || in->val_len > UPG_IO_MAX) {
            if (event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr,
                                        GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH);
            }
            return;
        }
        /* 对端（APP）下发数据 → 刷新空闲计时并退出待机，与 HID OUT 写入同一处理 */
        pm_activity_notify(PM_ACTIVITY_PEER_DATA);
        if (!upg_session_begin()) {
            if (event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr, GATT_RES_WRITE_NOT_PERMITTED);
            }
            return;
        }
        ws = upg_ws();
        if (ws == NULL) {
            if (event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
                gatts_chr_val_write_rsp(in->peer, in->chr, GATT_RES_WRITE_NOT_PERMITTED);
            }
            return;
        }
        ws->st.peer = in->peer;
        memcpy(ws->io, in->val, in->val_len);
        if (event == GATTS_EV_CHR_VALUE_WRITE_REQ) {
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
        }
        /* P1 无 Flash 写入，解析在回调内完成；P2 落盘时改为投递到任务 */
        upg_proto_on_frame(ws->io, in->val_len);
    } else if (event == GATTS_EV_CHR_VALUE_READ_REQ) {
        gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
        gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, (uint8_t *)empty, 1u);
    } else if (event == GATTS_EV_CHR_VALUE_SEND_START) {
        gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
        upg_ws_t *ws;

        co_printf("[UPG ] send start type=%d\r\n", (int)in->type);
        /* 订阅动作也属于对端交互：立即退出待机，否则待机分支不会推进协议状态机 */
        pm_activity_notify(PM_ACTIVITY_PEER_DATA);
        if (!upg_session_begin()) {
            return;
        }
        ws = upg_ws();
        if (ws == NULL) {
            return;
        }
        ws->st.peer = in->peer;
        ws->st.cccd_on = (in->type == GATT_SEND_VAL_NOTIFICATION) ? 1u : 0u;
        if (ws->st.cccd_on != 0u) {
            upg_proto_on_link_ready();
        } else {
            /* 取消订阅：清掉待发动作，避免向未订阅端发通知；会话由空闲超时回收 */
            ws->st.auth_pending = 0u;
            ws->st.reading = 0u;
        }
    }
}

int ble_upg_svc_init(void)
{
    uuid_t svc_uuid;
    uuid_t chr_uuid;

    memset(&svc_uuid, 0, sizeof(svc_uuid));
    svc_uuid.len = UUID_LEN_128;
    memcpy(svc_uuid.val.uuid128, k_svc_uuid, sizeof(k_svc_uuid));
    s_svc = gatts_svc_create(&svc_uuid, upg_svc_cbk, NULL);

    memset(&chr_uuid, 0, sizeof(chr_uuid));
    chr_uuid.len = UUID_LEN_128;
    memcpy(chr_uuid.val.uuid128, k_chr_uuid, sizeof(k_chr_uuid));
    s_chr = gatts_chr_add(s_svc, &chr_uuid,
                          GATT_CHR_PROP_READ
                          | GATT_CHR_PROP_WRITE
                          | GATT_CHR_PROP_WRITE_WITHOUT_RSP
                          | GATT_CHR_PROP_NOTIFY,
                          ATT_PERM_READ | ATT_PERM_WRITE,
                          upg_chr_cbk, NULL);
    gatts_dsc_ccc_add(s_chr, GATT_CHR_PROP_NOTIFY,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(s_svc);
    upg_proto_init();
    return 0;
}

bool ble_upg_notify(const uint8_t *buf, uint16_t len)
{
    upg_ws_t *ws;
    peer_t peer;

    if (buf == NULL || len == 0u) {
        return false;
    }
    ws = upg_ws();
    if (ws == NULL || ws->st.peer == NULL || ws->st.cccd_on == 0u) {
        return false;
    }
    peer = (peer_t)ws->st.peer;
    return (gatts_chr_val_notif(peer, s_chr, (uint8_t *)buf, len) == GATT_OK);
}
