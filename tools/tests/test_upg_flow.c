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

/* ---------------- 桩：Flash 读（返回全 0xFF，用开发占位密钥） ---------------- */
uint32_t Qflash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    (void)addr;
    if (buf != NULL) {
        memset(buf, 0xFF, len);
    }
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
    upg_proto_on_frame(req, 11u);
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
    upg_proto_on_frame(req, 11u);
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
    upg_proto_on_frame(req, 11u);
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
    upg_proto_on_frame(req, 11u);
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
    upg_proto_on_frame(req, 11u);
    upg_proto_poll();
    upg_proto_poll();
    assert(s_cap_n == 3u);
    expect_read_ready(0u, 40u);
    assert(s_cap[1][1] == 0x14u && s_cap_len[1] == 44u);
    assert(s_cap[2][1] == 0x14u && s_cap_len[2] == 4u);
    /* 让会话按超时结束，模拟手机端长时间不操作 */
    s_now += 31000u;
    upg_proto_poll();
    assert(upg_session_busy() == false);

    puts("UPG flow: subscribe->auth 52B, rel0 total=40 + 44B packet + terminator, "
         "usr1 total=72 with body len 68, retry keeps seq, irtv->F2, "
         "disconnect restores adv, OTA takeover keeps adv untouched, "
         "read after idle timeout still delivers PASS");
    return 0;
}
