/**
 * @file upg_proto.c
 * @brief 升级协议状态机（GY/T 407-2024 E.1.1 命令集，E.3 下载流程的读取部分）
 *
 * P1 实现：双向认证、读取 rel0/usr1/usr2、升级标志、写指令配置。
 * 键值写入（0x12/0x13 落盘）留待 P2。
 * 全部状态放在借来的会话工作区（upg_ws_t.st），不占静态 RAM。
 */
#include "upg_proto.h"
#include "upg_session.h"
#include "upg_auth.h"
#include "upg_meta.h"
#include "ble_upg_svc.h"
#include "ble_gap.h"
#include "gmem.h"
#include "co_printf.h"
#include <string.h>

#define UPG_CMD_HOST     0xF9u
#define UPG_CMD_RC       0xFAu

#define UPG_OP_READ_START   0x11u
#define UPG_OP_WRITE_START  0x12u
#define UPG_OP_ABORT_IO     0x13u
#define UPG_OP_WRITE_DATA   0x14u
#define UPG_OP_GET_FLAG     0x15u
#define UPG_OP_SET_FLAG     0x16u
#define UPG_OP_DOWN_ACK     0x18u
#define UPG_OP_AUTH_HOST    0x20u
#define UPG_OP_AUTH_RC      0x21u
#define UPG_OP_WRITE_CFG    0x22u

#define UPG_RES_OK          0x00u
#define UPG_RES_BUSY        0xF0u
#define UPG_RES_NO_DATA     0xF2u

void upg_proto_init(void)
{
    /* 会话态在进入会话时清零，这里无需额外初始化 */
}

/** @brief 取会话态；未在会话中返回 NULL */
static upg_state_t *st(void)
{
    upg_ws_t *ws = upg_ws();

    return (ws != NULL) ? &ws->st : NULL;
}

/** @brief 判断 8 字节数据类型字段是否等于给定 ASCII 串（不足补 0） */
static uint8_t type_is(const uint8_t *f, const char *s)
{
    uint8_t n = (uint8_t)strlen(s);
    uint8_t i;

    if (memcmp(f, s, n) != 0) {
        return 0u;
    }
    for (i = n; i < 8u; ++i) {
        if (f[i] != 0u) {
            return 0u;
        }
    }
    return 1u;
}

/** @brief 组一帧命令字 0xFA 的短响应：FA op result */
static void send_result(uint8_t op, uint8_t result)
{
    uint8_t buf[3];

    buf[0] = UPG_CMD_RC;
    buf[1] = op;
    buf[2] = result;
    (void)ble_upg_notify(buf, 3u);
}

/** @brief 是否允许在未认证状态下执行该操作 */
static uint8_t auth_allows(uint8_t op)
{
#if UPG_REQUIRE_AUTH
    if (upg_auth_is_done()) {
        return 1u;
    }
    (void)op;
    return 0u;
#else
    (void)op;
    return 1u;
#endif
}

void upg_proto_on_link_ready(void)
{
    upg_ws_t *ws;

    /* 回调里只借工作区并置标志；发通知与停广播都留到任务上下文 */
    if (!upg_session_begin()) {
        co_printf("[UPG ] link ready but session denied\r\n");
        return;
    }
    ws = upg_ws();
    if (ws == NULL) {
        return;
    }
    ws->st.reading = 0u;
    ws->st.auth_pending = 1u;
}

/** @brief 在任务上下文发起双向认证第一步 */
static void start_auth(void)
{
    upg_ws_t *ws = upg_ws();
    uint16_t n;

    if (ws == NULL) {
        return;
    }
    upg_auth_reset();
    n = upg_auth_start(ws->io, UPG_IO_MAX);
    if (n != 0u) {
        (void)ble_upg_notify(ws->io, n);
        co_printf("[UPG ] auth start sent (%u)\r\n", (unsigned)n);
    }
}

/**
 * @brief  处理启动读请求：F9 11 pack_len type[8]
 * @note   先回 FA 11（地址 + 总长），随后由 poll 分包以 FA 14 发送
 */
static void on_read_start(const uint8_t *f, uint16_t len)
{
    upg_ws_t *ws = upg_ws();
    uint8_t resp[11];
    uint8_t pack;
    uint16_t total = 0u;
    uint16_t mtu_payload;

    if (ws == NULL || len < 11u) {
        send_result(UPG_OP_READ_START, UPG_RES_NO_DATA);
        return;
    }
    pack = f[2];
    if (pack != 16u && pack != 32u && pack != 64u && pack != 128u) {
        send_result(UPG_OP_READ_START, UPG_RES_NO_DATA);
        return;
    }
    /* 通知负载上限为 MTU-3，帧头 4 字节 */
    mtu_payload = (uint16_t)(ble_gap_get_mtu() - 3u);
    if ((uint16_t)pack + 4u > mtu_payload || (uint16_t)pack + 4u > UPG_IO_MAX) {
        send_result(UPG_OP_READ_START, UPG_RES_NO_DATA);
        return;
    }

    if (type_is(f + 3, UPG_TYPE_REL0)) {
        total = upg_meta_build_rel0(ws->scratch, UPG_SCRATCH_MAX);
    } else if (type_is(f + 3, UPG_TYPE_USR1)) {
        total = upg_meta_build_usr1(ws->scratch, UPG_SCRATCH_MAX);
    } else if (type_is(f + 3, UPG_TYPE_USR2)) {
        (void)upg_meta_idcode(ws->scratch);
        total = (uint16_t)UPG_IDCODE_LEN;
    } else {
        /* irtv/irstb/blestb/fw01 属于写入类型或本期不做，读取返回无数据 */
        send_result(UPG_OP_READ_START, UPG_RES_NO_DATA);
        return;
    }

    if (total == 0u) {
        send_result(UPG_OP_READ_START, UPG_RES_NO_DATA);
        return;
    }

    resp[0] = UPG_CMD_RC;
    resp[1] = UPG_OP_READ_START;
    resp[2] = UPG_RES_OK;
    resp[3] = 0u; resp[4] = 0u; resp[5] = 0u; resp[6] = 0u;   /* 数据地址 */
    resp[7] = (uint8_t)(total >> 24);
    resp[8] = (uint8_t)(total >> 16);
    resp[9] = (uint8_t)(total >> 8);
    resp[10] = (uint8_t)total;

    ws->st.pack_len = pack;
    ws->st.rd_len = total;
    ws->st.rd_off = 0u;
    ws->st.seq = 0u;
    ws->st.reading = 1u;

    (void)ble_upg_notify(resp, sizeof(resp));
    co_printf("[UPG ] read ready, total=%u pack=%u\r\n", (unsigned)total, (unsigned)pack);
}

/** @brief 分包发送读结果；发完补一帧终止数据传输（FA 14 00 00） */
static void poll_read(void)
{
    upg_ws_t *ws = upg_ws();
    uint16_t chunk;
    uint16_t i;
    uint8_t *out;

    if (ws == NULL || ws->st.reading == 0u) {
        return;
    }
    /* 组包复用 io 缓冲（此刻收包已处理完） */
    out = ws->io;

    if (ws->st.rd_off < ws->st.rd_len) {
        chunk = (uint16_t)(ws->st.rd_len - ws->st.rd_off);
        if (chunk > ws->st.pack_len) {
            chunk = ws->st.pack_len;
        }
        out[0] = UPG_CMD_RC;
        out[1] = UPG_OP_WRITE_DATA;
        out[2] = ws->st.seq++;
        out[3] = (uint8_t)chunk;
        for (i = 0u; i < chunk; ++i) {
            out[4u + i] = ws->scratch[ws->st.rd_off + i];
        }
        ws->st.rd_off = (uint16_t)(ws->st.rd_off + chunk);
        (void)ble_upg_notify(out, (uint16_t)(chunk + 4u));
        return;
    }

    out[0] = UPG_CMD_RC;
    out[1] = UPG_OP_WRITE_DATA;
    out[2] = 0u;
    out[3] = 0u;
    (void)ble_upg_notify(out, 4u);
    ws->st.reading = 0u;
    co_printf("[UPG ] read done\r\n");
}

static void on_flag_get(void)
{
    upg_state_t *s = st();
    uint8_t buf[7];

    if (s == NULL) {
        return;
    }
    buf[0] = UPG_CMD_RC;
    buf[1] = UPG_OP_GET_FLAG;
    buf[2] = UPG_RES_OK;
    buf[3] = (uint8_t)(s->flag >> 24);
    buf[4] = (uint8_t)(s->flag >> 16);
    buf[5] = (uint8_t)(s->flag >> 8);
    buf[6] = (uint8_t)s->flag;
    (void)ble_upg_notify(buf, sizeof(buf));
}

static void on_flag_set(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = st();

    if (s == NULL) {
        return;
    }
    if (len < 6u) {
        send_result(UPG_OP_SET_FLAG, UPG_RES_NO_DATA);
        return;
    }
    s->flag = ((uint32_t)f[2] << 24) | ((uint32_t)f[3] << 16)
            | ((uint32_t)f[4] << 8) | (uint32_t)f[5];
    send_result(UPG_OP_SET_FLAG, UPG_RES_OK);
    co_printf("[UPG ] flag=0x%08X\r\n", (unsigned)s->flag);
}

static void on_write_cfg(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = st();
    uint8_t cmd;
    uint8_t n;

    if (s == NULL) {
        return;
    }
    if (len < 4u) {
        send_result(UPG_OP_WRITE_CFG, UPG_RES_NO_DATA);
        return;
    }
    cmd = f[2];
    n = f[3];
    if ((uint16_t)(4u + n) > len) {
        send_result(UPG_OP_WRITE_CFG, UPG_RES_NO_DATA);
        return;
    }
    if (cmd == 0x01u && n >= 1u) {
        s->cfg_target = f[4];
        s->cfg_has = 1u;
        co_printf("[UPG ] cfg target=%u (apply in P5)\r\n", (unsigned)s->cfg_target);
    } else if (cmd == 0x02u) {
        co_printf("[UPG ] cfg live-key seq, n=%u (apply in P5)\r\n", (unsigned)n);
    } else {
        send_result(UPG_OP_WRITE_CFG, UPG_RES_NO_DATA);
        return;
    }
    send_result(UPG_OP_WRITE_CFG, UPG_RES_OK);
}

/** @brief 下行数据响应：FA 18 seq len（P1 不落盘，仅回执避免主机重发） */
static void on_downlink_data(const uint8_t *f, uint16_t len)
{
    uint8_t buf[4];

    if (len < 4u) {
        return;
    }
    buf[0] = UPG_CMD_RC;
    buf[1] = UPG_OP_DOWN_ACK;
    buf[2] = f[2];
    buf[3] = f[3];
    (void)ble_upg_notify(buf, sizeof(buf));
}

void upg_proto_on_frame(const uint8_t *data, uint16_t len)
{
    upg_ws_t *ws;
    uint8_t op;
    uint16_t out_len;

    if (data == NULL || len < 2u) {
        return;
    }
    if (data[0] != UPG_CMD_HOST) {
        return;   /* 只接受主机命令字 0xF9 */
    }
    if (!upg_session_busy() && !upg_session_begin()) {
        return;
    }
    upg_session_touch();
    ws = upg_ws();
    if (ws == NULL) {
        return;
    }

    op = data[1];
    co_printf("[UPG ] rx op=0x%02X len=%u\r\n", (unsigned)op, (unsigned)len);

    if (op != UPG_OP_AUTH_HOST && op != UPG_OP_AUTH_RC && !auth_allows(op)) {
        send_result(op, UPG_RES_NO_DATA);
        return;
    }

    switch (op) {
    case UPG_OP_READ_START:
        on_read_start(data, len);
        break;
    case UPG_OP_WRITE_START:
    case UPG_OP_ABORT_IO:
        /* 键值写入在 P2 实现 */
        send_result(op, UPG_RES_NO_DATA);
        break;
    case UPG_OP_WRITE_DATA:
        on_downlink_data(data, len);
        break;
    case UPG_OP_GET_FLAG:
        on_flag_get();
        break;
    case UPG_OP_SET_FLAG:
        on_flag_set(data, len);
        break;
    case UPG_OP_AUTH_HOST:
        if (len < 18u) {
            break;
        }
        out_len = upg_auth_on_host_hash(data + 2, (uint16_t)(len - 2u), ws->io, UPG_IO_MAX);
        if (out_len != 0u) {
            (void)ble_upg_notify(ws->io, out_len);
        }
        break;
    case UPG_OP_AUTH_RC:
        out_len = upg_auth_on_rc_request(data, len, ws->io, UPG_IO_MAX);
        if (out_len != 0u) {
            (void)ble_upg_notify(ws->io, out_len);
        } else if (upg_auth_is_done()) {
            co_printf("[UPG ] auth done\r\n");
        }
        break;
    case UPG_OP_WRITE_CFG:
        on_write_cfg(data, len);
        break;
    default:
        co_printf("[UPG ] unsupported op 0x%02X\r\n", (unsigned)op);
        break;
    }
}

void upg_proto_poll(void)
{
    upg_ws_t *ws;

    if (!upg_session_busy()) {
        return;
    }
    /* 连接已断开则立即结束会话，恢复 CAT1 与广播 */
    if (ble_gap_get_peer() == NULL) {
        upg_session_end();
        return;
    }
    ws = upg_ws();
    if (ws == NULL) {
        return;
    }
    upg_session_poll();          /* 首次调用时在任务上下文获取独占资源 */

    if (ws->st.auth_pending != 0u) {
        ws->st.auth_pending = 0u;
        start_auth();
    }
    poll_read();
}
