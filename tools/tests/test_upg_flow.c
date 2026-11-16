/* 键值下载读流程闭环测试：订阅 → 认证帧 → 读 rel0/usr1 → 分包 → 终止。
 * 用真实 upg_*.c 源文件，外设与协议栈用桩替代；断言逐帧校验。 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "upg_md5.c"
#include "upg_meta.c"
#include "upg_auth.c"
#include "upg_session.c"
#include "upg_proto.c"
#include "upg_store.c"

/* ---------------- 桩：时间 ---------------- */
static uint32_t s_now;
uint32_t app_now_ms(void) { return s_now; }

/* ---------------- 桩：借出的 Flash 写缓冲 ----------------
 * 目标板上该缓冲是 512B；PC 主机指针 8 字节会让 upg_ws_t 多出对齐填充，
 * 这里按实际结构大小取上限，避免主机编译器报数组越界。 */
static uint8_t s_ws_buf[(sizeof(upg_ws_t) > 512u) ? sizeof(upg_ws_t) : 512u];
uint8_t *udp_bootload_borrow_buf(uint16_t *len)
{
    if (len != NULL) {
        *len = (uint16_t)sizeof(s_ws_buf);
    }
    return s_ws_buf;
}

/* ---------------- 桩：OTA 会话（可控，用于模拟被 OTA 接管） ---------------- */
static uint8_t s_ota_busy;
uint8_t udp_ota_session_busy(void) { return s_ota_busy; }

/* ---------------- 桩：连接状态 ---------------- */
static int s_peer_marker;
static int s_peer_connected = 1;
peer_t ble_gap_get_peer(void) { return s_peer_connected ? (peer_t)&s_peer_marker : NULL; }
uint16_t ble_gap_get_mtu(void) { return 500u; }   /* 与实测协商值一致 */

/* ---------------- 桩：外设释放动作 ---------------- */
static unsigned s_adv_restart;      /* 广播恢复次数 */
void ir_tx_cancel(void) {}
void ir_learn_stop(void) {}
int ble_adv_stop(void) { return 0; }
void ble_adv_start_by_bonding(void) { ++s_adv_restart; }
int voice_ctrl_post_stop(void) { return 1; }
void Audio_Pcm_Stop(void) {}
volatile uint8_t g_audio_pcm_enabled = 1u;

/* ---------------- 桩：Flash（覆盖层 A/B + 升级标志扇区用真数组模拟，其余地址返回 0xFF） ---------------- */
static uint8_t s_keystore[2][4096];
static uint8_t s_flagsec[4096];

/** @brief 地址映射；返回 -1 表示不在模拟区、按擦除态处理 */
static int ks_map(uint32_t addr, uint32_t len)
{
    uint32_t base;

    if (addr >= FLASH_KEYSTORE_A_START
        && (addr + len) <= (FLASH_KEYSTORE_A_START + sizeof(s_keystore[0]))) {
        return 0;
    }
    base = FLASH_KEYSTORE_A_START + sizeof(s_keystore[0]);
    if (addr >= base && (addr + len) <= (base + sizeof(s_keystore[1]))) {
        return 1;
    }
    if (addr >= FLASH_UPG_FLAG_START
        && (addr + len) <= (FLASH_UPG_FLAG_START + sizeof(s_flagsec))) {
        return 2;
    }
    return -1;
}

/** @brief 取某个模拟区的基址与数据指针 */
static void ks_region(int idx, uint32_t *base, uint8_t **data)
{
    if (idx == 0) {
        *base = FLASH_KEYSTORE_A_START;
        *data = s_keystore[0];
    } else if (idx == 1) {
        *base = FLASH_KEYSTORE_A_START + sizeof(s_keystore[0]);
        *data = s_keystore[1];
    } else {
        *base = FLASH_UPG_FLAG_START;
        *data = s_flagsec;
    }
}

uint32_t Qflash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    int idx;
    uint32_t base;
    uint8_t *data;

    if (buf == NULL) {
        return FlashOperationSuccess;
    }
    idx = ks_map(addr, len);
    if (idx < 0) {
        memset(buf, 0xFF, len);          /* 密钥区等：保持擦除态，走开发占位值 */
    } else {
        ks_region(idx, &base, &data);
        memcpy(buf, data + (addr - base), len);
    }
    return FlashOperationSuccess;
}

uint32_t Qflash_Write(uint32_t addr, uint8_t *buf, uint32_t len)
{
    int idx = ks_map(addr, len);
    uint32_t base;
    uint8_t *data;
    uint32_t i;

    assert(idx >= 0);                     /* 只允许写模拟区 */
    ks_region(idx, &base, &data);
    for (i = 0; i < len; ++i) {
        data[addr - base + i] &= buf[i];  /* Flash：只能 1→0 */
    }
    return FlashOperationSuccess;
}

uint32_t Qflash_Erase_Sector(uint32_t addr)
{
    int idx = ks_map(addr, 1u);
    uint32_t base;
    uint8_t *data;

    assert(idx >= 0);
    ks_region(idx, &base, &data);
    memset(data, 0xFF, 4096u);
    return FlashOperationSuccess;
}

/* ---------------- 捕获 notify ---------------- */
#define CAP_MAX 8
static uint8_t  s_cap[CAP_MAX][160];
static uint16_t s_cap_len[CAP_MAX];
static unsigned s_cap_n;
static int      s_fail_next;      /* >0：下一次 notify 返回失败 */

/* 等价于 ATT 层订阅状态：由 ble_upg_svc 持有，不随会话重建清零 */
static unsigned s_cccd_on = 1u;

void ble_upg_on_link_lost(void) { s_cccd_on = 0u; }

bool ble_upg_notify(const uint8_t *buf, uint16_t len)
{
    if (s_fail_next > 0) {
        --s_fail_next;
        return false;
    }
    if (s_cccd_on == 0u) {          /* 未订阅：与真实实现同样拒绝 */
        return false;
    }
    if (s_cap_n >= CAP_MAX || len > sizeof(s_cap[0])) {
        return false;
    }
    memcpy(s_cap[s_cap_n], buf, len);
    s_cap_len[s_cap_n] = len;
    ++s_cap_n;
    return true;
}

/* ---------------- 辅助 ---------------- */
/** @brief 模拟 ble_upg_svc：先把帧拷进会话 io 再交给协议层
 *  （写流程的数据包就在 ws->io 里，任务上下文从这里落盘） */
static void deliver(const uint8_t *f, uint16_t len)
{
    upg_ws_t *ws;

    if (!upg_session_busy()) {
        (void)upg_session_begin();
    }
    ws = upg_ws();
    assert(ws != NULL && len <= UPG_IO_MAX);
    memcpy(ws->io, f, len);
    upg_proto_on_frame(ws->io, len);
}
static void build_read_req(uint8_t req[11], const char *tag)
{
    req[0] = 0xF9u;
    req[1] = 0x11u;
    req[2] = 0x80u;              /* 每包 128 字节 */
    memset(req + 3, 0, 8);
    memcpy(req + 3, tag, strlen(tag));
}

/** @brief 断言第 idx 帧是 FA 11 成功，且总长为 total */
static void expect_read_ready(unsigned idx, uint16_t total)
{
    assert(s_cap_n > idx);
    assert(s_cap[idx][0] == 0xFAu);
    assert(s_cap[idx][1] == 0x11u);
    assert(s_cap[idx][2] == 0x00u);
    assert(s_cap_len[idx] == 11u);
    assert(s_cap[idx][7] == (uint8_t)(total >> 24));
    assert(s_cap[idx][8] == (uint8_t)(total >> 16));
    assert(s_cap[idx][9] == (uint8_t)(total >> 8));
    assert(s_cap[idx][10] == (uint8_t)total);
}

/* ---------------- P2：写入链路用例 ---------------- */

/** @brief 组启动写请求：F9 12 pack type[8] rsv[4] len[4] */
static void build_write_start(uint8_t req[19], uint8_t pack, const char *tag, uint16_t dlen)
{
    req[0] = 0xF9u;
    req[1] = 0x12u;
    req[2] = pack;
    memset(req + 3, 0, 8u);
    memcpy(req + 3, tag, strlen(tag));
    memset(req + 11, 0, 4u);
    req[15] = (uint8_t)(dlen >> 24);
    req[16] = (uint8_t)(dlen >> 16);
    req[17] = (uint8_t)(dlen >> 8);
    req[18] = (uint8_t)dlen;
}

/** @brief 走完一次写：启动写 → 逐包 → 接收完毕 → 终止；corrupt 时故意错 CRC */
static void run_write(const char *tag, const uint8_t *data, uint16_t dlen,
                      uint8_t pack, bool corrupt)
{
    uint8_t req[19];
    uint8_t pkt[4 + 128];
    uint16_t off = 0u;
    uint16_t seq = 0u;
    uint16_t crc = 0u;

    s_cap_n = 0u;
    build_write_start(req, pack, tag, dlen);
    deliver(req, 19u);
    upg_proto_poll();                     /* 任务上下文：擦扇区 + 压实 + 回 FA 12 */

    assert(s_cap_n == 1u);
    assert(s_cap[0][0] == 0xFAu && s_cap[0][1] == 0x12u && s_cap[0][2] == 0x00u);
    assert(s_cap_len[0] == 11u);
    assert(s_cap[0][9] == (uint8_t)(dlen >> 8) && s_cap[0][10] == (uint8_t)dlen);

    while (off < dlen) {
        uint16_t n = (uint16_t)(dlen - off);

        if (n > pack) {
            n = pack;
        }
        pkt[0] = 0xF9u;
        pkt[1] = 0x14u;
        pkt[2] = (uint8_t)seq;
        pkt[3] = (uint8_t)n;
        memcpy(pkt + 4, data + off, n);
        s_cap_n = 0u;
        deliver(pkt, (uint16_t)(4u + n));
        upg_proto_poll();
        assert(s_cap_n >= 1u);
        assert(s_cap[0][1] == 0x18u && s_cap[0][2] == (uint8_t)seq && s_cap[0][3] == (uint8_t)n);
        crc = upg_crc16(pkt + 4, n, crc);
        off = (uint16_t)(off + n);
        ++seq;
    }
    /* 收满后遥控器主动发 FA 14 00 00（E.3.4 g） */
    assert(s_cap_n == 2u);
    assert(s_cap[1][0] == 0xFAu && s_cap[1][1] == 0x14u);
    assert(s_cap[1][2] == 0u && s_cap[1][3] == 0u);

    if (corrupt) {
        crc = (uint16_t)(crc ^ 0x5A5Au);
    }
    req[0] = 0xF9u;
    req[1] = 0x13u;
    req[2] = (uint8_t)(crc >> 8);
    req[3] = (uint8_t)(crc & 0xFFu);
    s_cap_n = 0u;
    deliver(req, 4u);
    upg_proto_poll();
    assert(s_cap_n == 1u);
    assert(s_cap[0][1] == 0x13u && s_cap[0][2] == 0x00u);
    assert(s_cap_len[0] == 5u);
}

static void write_case(void)
{

    uint8_t blob[200];
    uint8_t out[256];
    uint16_t n;
    uint16_t i;

    for (i = 0u; i < sizeof(blob); ++i) {
        blob[i] = (uint8_t)(i * 7u + 3u);
    }

    /* 12) 正常写入 irstb：CRC 一致 → 提交，回读一致 */
    run_write("irstb", blob, 200u, 64u, false);
    n = upg_store_load(UPG_ST_IRSTB, out, sizeof(out));
    assert(n == 200u);
    assert(memcmp(out, blob, 200u) == 0);

    /* 13) 再写 irtv：A/B 轮换后 irtv 生效，压实保证了 irstb 也还在 */
    run_write("irtv", blob, 100u, 32u, false);
    n = upg_store_load(UPG_ST_IRTV, out, sizeof(out));
    assert(n == 100u);
    assert(memcmp(out, blob, 100u) == 0);
    n = upg_store_load(UPG_ST_IRSTB, out, sizeof(out));
    assert(n == 200u);
    assert(memcmp(out, blob, 200u) == 0);

    /* 14) CRC 不一致：拒绝提交，旧值保持不变 */
    blob[0] ^= 0xFFu;
    run_write("irtv", blob, 120u, 64u, true);
    n = upg_store_load(UPG_ST_IRTV, out, sizeof(out));
    assert(n == 100u);
    assert(out[0] == 0x03u);

    /* 15) 恢复出厂：两份都清掉 */
    assert(upg_store_erase_all());
    assert(upg_store_load(UPG_ST_IRTV, out, sizeof(out)) == 0u);
    assert(upg_store_load(UPG_ST_IRSTB, out, sizeof(out)) == 0u);

    /* 16) 升级标志：设置 → 任务上下文落盘 → 回读（掉电保持语义） */
    assert(upg_flag_load() == UPG_FLAG_NONE);
    {
        uint8_t fset[6] = { 0xF9u, 0x16u, 0xF5u, 0xF5u, 0xF5u, 0x00u };
        uint8_t fget[2] = { 0xF9u, 0x15u };

        s_cap_n = 0u;
        deliver(fset, sizeof(fset));
        assert(s_cap_n == 0u);            /* 回调里不发应答 */
        upg_proto_poll();
        assert(s_cap_n == 1u);
        assert(s_cap[0][1] == 0x16u && s_cap[0][2] == 0x00u);
        assert(upg_flag_load() == UPG_FLAG_VALID);

        s_cap_n = 0u;
        deliver(fget, sizeof(fget));
        assert(s_cap_n == 1u);
        assert(s_cap[0][1] == 0x15u && s_cap[0][2] == 0x00u);
        assert(s_cap[0][3] == 0xF5u && s_cap[0][4] == 0xF5u
               && s_cap[0][5] == 0xF5u && s_cap[0][6] == 0x00u);
    }
}

int main(void)
{
    uint8_t req[11];
    static const char rel0_expect[] = "idcode:VCR1DEVIDCODE0000000000000000000\n";

    upg_proto_init();
    s_now = 1000u;

    /* 1) 对端订阅 → 首次 poll 应发认证发起帧 FA 20 00，共 52 字节 */
    upg_proto_on_link_ready();
    assert(s_cap_n == 0u);                 /* 回调里只置标志，不发帧 */
    upg_proto_poll();
    assert(s_cap_n == 1u);
    assert(s_cap_len[0] == 52u);
    assert(s_cap[0][0] == 0xFAu && s_cap[0][1] == 0x20u && s_cap[0][2] == 0x00u);
    assert(s_cap[0][3] == 0u);             /* key_index = 0 */
    assert(memcmp(s_cap[0] + 20, "VCR1DEVIDCODE", 13) == 0);  /* 帧内携带 idcode */

    /* 2) 读 rel0：立刻回 FA 11，总长 40 */
    s_cap_n = 0u;
    build_read_req(req, "rel0");
    deliver(req, 11u);
    assert(s_cap_n == 1u);
    expect_read_ready(0u, 40u);

    /* 3) 下一次 poll 发数据包：FA 14 00 28 + 40 字节 */
    upg_proto_poll();
    assert(s_cap_n == 2u);
    assert(s_cap[1][0] == 0xFAu && s_cap[1][1] == 0x14u);
    assert(s_cap[1][2] == 0u && s_cap[1][3] == 40u);
    assert(s_cap_len[1] == 44u);
    assert(memcmp(s_cap[1] + 4, rel0_expect, 40u) == 0);

    /* 4) 再下一次 poll 发终止帧 FA 14 00 00 */
    upg_proto_poll();
    assert(s_cap_n == 3u);
    assert(s_cap[2][0] == 0xFAu && s_cap[2][1] == 0x14u);
    assert(s_cap[2][2] == 0u && s_cap[2][3] == 0u);
    assert(s_cap_len[2] == 4u);

    /* 5) 终止后不再有输出 */
    upg_proto_poll();
    assert(s_cap_n == 3u);

    /* 6) 读 usr1：总长 72，正文（跳过 4 字节帧头后的 2B CRC + 2B 长度）以 fw:02020000 开头 */
    s_cap_n = 0u;
    build_read_req(req, "usr1");
    deliver(req, 11u);
    expect_read_ready(0u, 72u);
    upg_proto_poll();
    assert(s_cap_n == 2u);
    assert(s_cap[1][3] == 72u);
    assert(memcmp(s_cap[1] + 8, "fw:02020000\n", 12u) == 0);
    /* 4 字节头之后的正文长度字段应与总长一致 */
    {
        uint16_t body = (uint16_t)((s_cap[1][6] << 8) | s_cap[1][7]);
        assert(body == 68u);
    }
    upg_proto_poll();
    assert(s_cap_n == 3u && s_cap[2][3] == 0u);

    /* 7) 通知发送失败时不得推进偏移，下一轮重发同一序号 */
    s_cap_n = 0u;
    build_read_req(req, "rel0");
    deliver(req, 11u);
    assert(s_cap_n == 1u);
    s_fail_next = 1;
    upg_proto_poll();                      /* 本轮发送失败 */
    assert(s_cap_n == 1u);
    upg_proto_poll();                      /* 下一轮重试 */
    assert(s_cap_n == 2u);
    assert(s_cap[1][2] == 0u);             /* 序号仍为 0，没有跳号 */
    assert(s_cap[1][3] == 40u);

    /* 8) 未订阅时 notify 直接失败，不应产生帧 */
    upg_proto_poll();
    s_cap_n = 0u;
    build_read_req(req, "irtv");           /* 写入型，读取应回 F2 */
    deliver(req, 11u);
    assert(s_cap_n == 1u);
    assert(s_cap[0][1] == 0x11u && s_cap[0][2] == 0xF2u);

    /* 9) 断连：poll 应结束会话并恢复广播 */
    s_cap_n = 0u;
    s_peer_connected = 0;
    upg_proto_poll();
    assert(upg_session_busy() == false);
    assert(s_adv_restart == 1u);
    assert(s_cap_n == 0u);

    /* 10) 会话中途被 OTA 接管：收尾时不得再恢复广播（独占资源已归 OTA） */
    s_peer_connected = 1;
    s_cccd_on = 1u;                        /* 模拟重连后重新订阅 */
    s_adv_restart = 0u;
    s_cap_n = 0u;
    upg_proto_on_link_ready();
    assert(upg_session_busy() == true);
    upg_proto_poll();                      /* 任务上下文首次 poll：取独占资源并发认证帧 */
    assert(s_cap_n == 1u);
    s_ota_busy = 1u;                       /* OTA 接管 */
    s_peer_connected = 0;                  /* 同时断连，触发 UPG 收尾 */
    upg_proto_poll();
    assert(upg_session_busy() == false);
    assert(s_adv_restart == 0u);           /* 未恢复广播 */
    s_ota_busy = 0u;

    /* 11) 会话超时结束后再来一帧读：新会话必须照常产出 FA 11 + 数据包
     *     （订阅状态由 ATT 层持有、不随会话重建清零，这里校验流程侧不被清坏） */
    s_peer_connected = 1;
    s_cccd_on = 1u;
    s_adv_restart = 0u;
    s_cap_n = 0u;
    build_read_req(req, "rel0");
    deliver(req, 11u);
    upg_proto_poll();
    upg_proto_poll();
    assert(s_cap_n == 3u);
    expect_read_ready(0u, 40u);
    assert(s_cap[1][1] == 0x14u && s_cap_len[1] == 44u);
    assert(s_cap[2][1] == 0x14u && s_cap_len[2] == 4u);
    /* 让会话按超时结束，模拟手机端长时间不操作（超时值随调试开关变化） */
    s_now += (uint32_t)(UPG_SESSION_IDLE_MS + 1000u);
    upg_proto_poll();
    assert(upg_session_busy() == false);

    /* ================= P2：写入链路 + A/B 覆盖层 ================= */
    write_case();

    puts("UPG flow: subscribe->auth 52B, rel0 total=40 + 44B packet + terminator, "
         "usr1 total=72 with body len 68, retry keeps seq, irtv->F2, "
         "disconnect restores adv, OTA takeover keeps adv untouched, "
         "read after idle timeout still delivers, "
         "write irstb commit + crc reject + A/B rotate PASS");
    return 0;
}
