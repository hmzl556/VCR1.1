/**
 * @file upg_proto.c
 * @brief 升级协议状态机（GY/T 407-2024 E.1.1 命令集，E.3 下载流程）
 *
 * P1：双向认证、读取 rel0/usr1/usr2、升级标志、写指令配置。
 * P2：键值写入链路（0x12 启动写 / 0x14 下行数据 / 0x18 回执 / FA 14 00 00 接收完毕 /
 *     0x13 终止 + CRC16 双向比对），数据经 upg_store 落 A/B 覆盖层。
 *
 * 约定：GATT 回调里只解析参数与置标志，**所有 Flash 操作都在任务上下文**
 * （upg_proto_poll → poll_write），与现有 OTA / 学习波形一致。
 * 全部状态放在借来的会话工作区（upg_ws_t.st），不占静态 RAM。
 */
#include "upg_proto.h"
#include "upg_session.h"
#include "upg_auth.h"
#include "upg_meta.h"
#include "upg_store.h"
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
static upg_state_t *proto_st(void)
{
    upg_ws_t *ws = upg_ws();

    return (ws != NULL) ? &ws->st : NULL;
}

/** @brief 每次读写的有效包长只允许 16/32/64/128（表 E.2 / E.4） */
static uint8_t pack_valid(uint8_t pack)
{
    return (pack == 16u || pack == 32u || pack == 64u || pack == 128u) ? 1u : 0u;
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
    if (pack_valid(pack) == 0u) {
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
        /* 下载过版本信息就以覆盖层里的为准（E.3.3：回读要反映最新版本） */
        total = upg_store_load(UPG_ST_USR1, ws->scratch, UPG_SCRATCH_MAX);
        if (total == 0u) {
            total = upg_meta_build_usr1(ws->scratch, UPG_SCRATCH_MAX);
        }
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

/** @brief 分包发送读结果；发完补一帧终止数据传输（FA 14 00 00）
 *  @note  发送失败时不推进偏移，下一轮重试，避免丢包。 */
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
        out[2] = ws->st.seq;
        out[3] = (uint8_t)chunk;
        for (i = 0u; i < chunk; ++i) {
            out[4u + i] = ws->scratch[ws->st.rd_off + i];
        }
        if (!ble_upg_notify(out, (uint16_t)(chunk + 4u))) {
            return;   /* 未订阅或协议栈忙：本轮不发，也不推进 */
        }
        ws->st.seq++;
        ws->st.rd_off = (uint16_t)(ws->st.rd_off + chunk);
        return;
    }

    out[0] = UPG_CMD_RC;
    out[1] = UPG_OP_WRITE_DATA;
    out[2] = 0u;
    out[3] = 0u;
    if (!ble_upg_notify(out, 4u)) {
        return;
    }
    ws->st.reading = 0u;
    co_printf("[UPG ] read done\r\n");
}

static void on_flag_get(void)
{
    uint8_t buf[7];
    uint32_t flag = upg_flag_load();

    buf[0] = UPG_CMD_RC;
    buf[1] = UPG_OP_GET_FLAG;
    buf[2] = UPG_RES_OK;
    buf[3] = (uint8_t)(flag >> 24);
    buf[4] = (uint8_t)(flag >> 16);
    buf[5] = (uint8_t)(flag >> 8);
    buf[6] = (uint8_t)flag;
    (void)ble_upg_notify(buf, sizeof(buf));
}

/** @brief 设置升级标志：只记录，落盘与应答在任务上下文（擦扇区不能进回调） */
static void on_flag_set(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = proto_st();

    if (s == NULL) {
        return;
    }
    if (len < 6u) {
        send_result(UPG_OP_SET_FLAG, UPG_RES_NO_DATA);
        return;
    }
    s->flag = ((uint32_t)f[2] << 24) | ((uint32_t)f[3] << 16)
            | ((uint32_t)f[4] << 8) | (uint32_t)f[5];
    s->flag_pending = 1u;
}

static void on_write_cfg(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = proto_st();
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
    upg_state_t *s = proto_st();
    uint8_t ack[4];

    if (s == NULL || len < 4u) {
        return;
    }
    ack[0] = UPG_CMD_RC;
    ack[1] = UPG_OP_DOWN_ACK;
    ack[2] = f[2];
    ack[3] = f[3];

    if (s->writing == 0u) {
        /* 无写会话（例如读会话中收到数据包）：按规范回执，避免主机空等 */
        (void)ble_upg_notify(ack, sizeof(ack));
        return;
    }
    /* 包序/长度/越界校验通过后只置标志；落盘与回执都在任务上下文，
     * 数据此刻就留在 ws->io 里，主机在收到 0x18 前不会发下一包 */
    if (f[2] != s->wr_expect || f[3] == 0u || len < (uint16_t)(4u + (uint16_t)f[3])
        || ((uint32_t)s->wo.written + (uint16_t)f[3]) > (uint32_t)s->wr_len) {
        co_printf("[UPG ] bad data pkt seq=%u exp=%u len=%u/%u\r\n",
                  (unsigned)f[2], (unsigned)s->wr_expect,
                  (unsigned)f[3], (unsigned)s->wr_len);
        return;
    }
    s->wr_pkt_ready = 1u;
}

/** @brief 启动写：只校验参数并记录，擦扇区/压实留到任务上下文 */
static void on_write_start(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = proto_st();
    upg_store_type_t t;
    uint32_t dlen;

    if (s == NULL) {
        return;
    }
    if (len < 19u || pack_valid(f[2]) == 0u) {
        send_result(UPG_OP_WRITE_START, UPG_RES_NO_DATA);
        return;
    }
    if (s->writing != 0u || s->reading != 0u || s->wr_start != 0u) {
        send_result(UPG_OP_WRITE_START, UPG_RES_BUSY);
        return;
    }
    t = upg_store_type_of(f + 3);
    if (!upg_store_type_writable(t)) {
        send_result(UPG_OP_WRITE_START, UPG_RES_NO_DATA);
        return;
    }
    dlen = ((uint32_t)f[15] << 24) | ((uint32_t)f[16] << 16)
         | ((uint32_t)f[17] << 8) | (uint32_t)f[18];
    if (dlen == 0u || dlen > UPG_STORE_DATA_MAX) {
        send_result(UPG_OP_WRITE_START, UPG_RES_NO_DATA);
        return;
    }
    s->pack_len = f[2];
    s->wr_type = (uint8_t)t;
    s->wr_len = (uint16_t)dlen;
    s->wr_crc = 0u;
    memset(&s->wo, 0, sizeof(s->wo));
    s->wr_expect = 0u;
    s->wr_start = 1u;
}

/** @brief 终止读写：读流程立即应答；写流程只记录 CRC1，提交在任务上下文 */
static void on_abort_io(const uint8_t *f, uint16_t len)
{
    upg_state_t *s = proto_st();
    uint8_t resp[5];

    if (s == NULL) {
        return;
    }
    if (s->writing == 0u) {
        s->reading = 0u;
        resp[0] = UPG_CMD_RC;
        resp[1] = UPG_OP_ABORT_IO;
        resp[2] = UPG_RES_OK;
        resp[3] = 0u;
        resp[4] = 0u;
        (void)ble_upg_notify(resp, sizeof(resp));
        return;
    }
    if (len < 4u) {
        send_result(UPG_OP_ABORT_IO, UPG_RES_NO_DATA);
        return;
    }
    s->wr_crc1 = (uint16_t)(((uint16_t)f[2] << 8) | (uint16_t)f[3]);
    s->wr_abort = 1u;
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
        on_write_start(data, len);
        break;
    case UPG_OP_ABORT_IO:
        on_abort_io(data, len);
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

/**
 * @brief 任务上下文推进写入：擦扇区/压实 → 回 FA 12；落盘 + 回执；提交或回滚
 * @note  Flash 操作绝不能放在 GATT 回调里（擦扇区几十毫秒会打乱连接间隔）
 */
static void poll_write(void)
{
    upg_state_t *s = proto_st();
    upg_ws_t *ws = upg_ws();
    uint8_t out[11];

    if (s == NULL || ws == NULL) {
        return;
    }

    /* 0) 升级标志落盘（表 E.14）：擦扇区只能在任务上下文 */
    if (s->flag_pending != 0u) {
        s->flag_pending = 0u;
        send_result(UPG_OP_SET_FLAG,
                    upg_flag_save(s->flag) ? UPG_RES_OK : UPG_RES_NO_DATA);
        return;
    }

    /* 1) 启动写：此处才擦扇区并压实其余记录，成功后再回 FA 12 */
    if (s->wr_start != 0u) {
        uint32_t addr = 0u;

        s->wr_start = 0u;
        if (!upg_store_begin(&s->wo, (upg_store_type_t)s->wr_type, s->wr_len, &addr)) {
            send_result(UPG_OP_WRITE_START, UPG_RES_NO_DATA);
            co_printf("[UPG ] write start fail\r\n");
            return;
        }
        s->writing = 1u;
        out[0] = UPG_CMD_RC;
        out[1] = UPG_OP_WRITE_START;
        out[2] = UPG_RES_OK;
        out[3] = (uint8_t)(addr >> 24);
        out[4] = (uint8_t)(addr >> 16);
        out[5] = (uint8_t)(addr >> 8);
        out[6] = (uint8_t)addr;
        out[7] = 0u;
        out[8] = 0u;
        out[9] = (uint8_t)(s->wr_len >> 8);
        out[10] = (uint8_t)s->wr_len;
        (void)ble_upg_notify(out, sizeof(out));
        co_printf("[UPG ] write ready, total=%u pack=%u\r\n",
                  (unsigned)s->wr_len, (unsigned)s->pack_len);
        return;
    }

    /* 2) 一包数据：落盘 → 累计 CRC2 → 回 0x18 → 收满则发 FA 14 00 00 */
    if (s->wr_pkt_ready != 0u && s->writing != 0u) {
        uint8_t seq = ws->io[2];
        uint8_t n = ws->io[3];
        uint8_t ack[4];

        s->wr_pkt_ready = 0u;
        if (!upg_store_write(&s->wo, ws->io + 4u, n)) {
            co_printf("[UPG ] store write fail\r\n");
            upg_store_abort(&s->wo);
            s->writing = 0u;
            return;
        }
        s->wr_crc = upg_crc16(ws->io + 4u, n, s->wr_crc);

        s->wr_expect++;

        ack[0] = UPG_CMD_RC;
        ack[1] = UPG_OP_DOWN_ACK;
        ack[2] = seq;
        ack[3] = n;
        (void)ble_upg_notify(ack, sizeof(ack));

        if (s->wo.written >= s->wr_len) {
            /* 数据传输完毕：由遥控器主动通知（E.3.4 g），等主机发 0x13 */
            uint8_t done[4];

            done[0] = UPG_CMD_RC;
            done[1] = UPG_OP_WRITE_DATA;
            done[2] = 0u;
            done[3] = 0u;
            (void)ble_upg_notify(done, sizeof(done));
            co_printf("[UPG ] rx %u bytes done, wait abort\r\n", (unsigned)s->wo.written);
        }
        return;
    }

    /* 3) 终止读写：比对 CRC1/CRC2，一致才提交 */
    if (s->wr_abort != 0u) {
        bool ok = (s->wo.written == s->wr_len) && (s->wr_crc == s->wr_crc1);

        s->wr_abort = 0u;
        if (ok) {
            ok = upg_store_commit(&s->wo, s->wr_crc);
        }
        if (!ok) {
            upg_store_abort(&s->wo);
        }
        s->writing = 0u;
        out[0] = UPG_CMD_RC;
        out[1] = UPG_OP_ABORT_IO;
        out[2] = UPG_RES_OK;
        out[3] = (uint8_t)(s->wr_crc >> 8);
        out[4] = (uint8_t)(s->wr_crc & 0xFFu);
        (void)ble_upg_notify(out, 5u);
        co_printf("[UPG ] %s crc2=%04X crc1=%04X\r\n",
                  ok ? "write commit" : "write rejected",
                  (unsigned)s->wr_crc, (unsigned)s->wr_crc1);
    }
}

void upg_proto_poll(void)
{
    upg_ws_t *ws;

    /* 断链后订阅状态失效：即使会话已结束也要清，否则重连后 notification 会被误判 */
    if (ble_gap_get_peer() == NULL) {
        ble_upg_on_link_lost();
        if (upg_session_busy()) {
            upg_session_end();      /* 恢复广播，须在任务上下文 */
        }
        return;
    }
    if (!upg_session_busy()) {
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
    poll_write();
    poll_read();
}
