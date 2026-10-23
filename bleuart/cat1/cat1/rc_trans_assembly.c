#include <string.h>
#include "rc_trans_assembly.h"
#include "ble_rc_svc.h"
#include "rc_cat1_bridge.h"
#include "co_printf.h"
#include "udp_ota.h"
#include "gmem.h"
#include "pm_state.h"   /* pm_state_get / PM_STATE_STANDBY */
#include "FreeRTOS.h"
#include "task.h"

#define RC_TRANS_OFF_HEADER      0u
#define RC_TRANS_OFF_FRAME_LEN   6u
#define RC_TRANS_OFF_PACK_INDEX  8u
#define RC_TRANS_OFF_SUB_IDX     12u
#define RC_TRANS_OFF_PAYLOAD_LEN 13u
#define RC_TRANS_OFF_PAYLOAD     14u
#define RC_TRANS_HDR_MIN         17u

static uint8_t  s_slot_used[RC_TRANS_MAX_SUBPACK];
static uint8_t  s_slot_len[RC_TRANS_MAX_SUBPACK];
static uint8_t  s_slot_data[RC_TRANS_MAX_SUBPACK][RC_TRANS_PAYLOAD_MAX];
static uint8_t  s_asm_buf[RC_TRANS_ASSEMBLY_MAX];
static uint16_t s_asm_len;
static uint32_t s_asm_pack_index;
static uint8_t  s_asm_active;
static uint8_t  s_assembled_ready;
static uint32_t s_last_ok_pack_index;
static uint8_t  s_have_last_ok;
static uint32_t s_asm_last_ms;

/**
 * @brief  获取当前时间（ms）
 */
static uint32_t rc_trans_now_ms(void)
{
    return app_now_ms();
}

static void rc_trans_touch_activity(void)
{
    s_asm_last_ms = rc_trans_now_ms();
}

/**
 * @brief  从大端字节流读取 Uint16
 */
static uint16_t rc_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/**
 * @brief  从大端字节流读取 Uint32
 */
static uint32_t rc_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * @brief  计算 CRC-16/CCITT-FALSE（MD §3.3）
 */
static uint16_t rc_crc16_ccitt_false(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu;
    uint32_t i;
    uint8_t j;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (j = 0; j < 8u; j++)
        {
            if (crc & 0x8000u)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  清空汇聚槽
 */
static void rc_trans_clear_slots(void)
{
    memset(s_slot_used, 0, sizeof(s_slot_used));
    s_asm_len = 0u;
}

/**
 * @brief  打印 STB 写入的 RC-TRANS 帧（字段 + 原始 hex）
 */
static void rc_trans_log_rx(const uint8_t *val, uint16_t len)
{
    uint16_t i;
    uint16_t dump_len;
    uint16_t frame_len;
    uint32_t pack_index;
    uint8_t sub_index;
    uint8_t payload_len;
    uint16_t recv_crc;

    if (val == NULL || len == 0u)
    {
        return;
    }

    co_printf("[RC   ] TRANS rx len=%u\r\n", (unsigned)len);

    if (len >= RC_TRANS_HDR_MIN)
    {
        frame_len = rc_be16(val + RC_TRANS_OFF_FRAME_LEN);
        pack_index = rc_be32(val + RC_TRANS_OFF_PACK_INDEX);
        sub_index = val[RC_TRANS_OFF_SUB_IDX];
        payload_len = val[RC_TRANS_OFF_PAYLOAD_LEN];
        recv_crc = rc_be16(val + len - 2u);
        co_printf("[RC   ] TRANS rx: Magic=0x%04X FrameLen=%u PackIndex=0x%08X Sub=%u PayloadLen=%u CRC=0x%04X\r\n",
                  (unsigned)rc_be16(val + RC_TRANS_OFF_HEADER),
                  (unsigned)frame_len,
                  (unsigned)pack_index,
                  (unsigned)sub_index,
                  (unsigned)payload_len,
                  (unsigned)recv_crc);
        if (payload_len > 0u &&
            (uint16_t)(RC_TRANS_OFF_PAYLOAD + payload_len) <= len)
        {
            co_printf("[RC   ] TRANS payload:");
            for (i = 0u; i < payload_len; i++)
            {
                co_printf(" %02X", (unsigned)val[RC_TRANS_OFF_PAYLOAD + i]);
            }
            co_printf("\r\n");
        }
    }

    dump_len = len;
    if (dump_len > RC_TRANS_FRAME_MAX)
    {
        dump_len = RC_TRANS_FRAME_MAX;
    }
    co_printf("[RC   ] TRANS raw:");
    for (i = 0u; i < dump_len; i++)
    {
        co_printf(" %02X", (unsigned)val[i]);
    }
    co_printf("\r\n");
}

static void rc_trans_fail(peer_t peer, uint8_t result, uint32_t pack_index)
{
    rc_status_t st;

    co_printf("[RC   ] TRANS fail: result=0x%02X pack=0x%08X asm_active=%u\r\n",
              (unsigned)result, (unsigned)pack_index, (unsigned)s_asm_active);
    memcpy(&st, ble_rc_svc_get_status(), sizeof(st));
    st.state = RC_STATE_ERROR;
    st.result = result;
    st.pack_index = pack_index;
    ble_rc_svc_status_notify(peer, &st);
    rc_trans_reset();
}

/**
 * @brief  按 SubPackIndex 倒序拼接到 s_asm_buf：Sub=1..N 在前，Sub=0 在后
 */
static int rc_trans_build_from_slots(void)
{
    uint8_t max_sub = 0;
    uint8_t i;
    uint16_t total = 0;

    for (i = 0; i < RC_TRANS_MAX_SUBPACK; i++)
    {
        if (s_slot_used[i] && i >= max_sub)
        {
            max_sub = i;
        }
    }
    if (!s_slot_used[0])
    {
        return -1;
    }
    for (i = 0; i <= max_sub; i++)
    {
        if (!s_slot_used[i])
        {
            return -1;
        }
        total = (uint16_t)(total + s_slot_len[i]);
    }
    if (total > RC_TRANS_ASSEMBLY_MAX)
    {
        return -1;
    }

    s_asm_len = 0u;
    for (i = max_sub; i > 0u; i--)
    {
        memcpy(s_asm_buf + s_asm_len, s_slot_data[i], s_slot_len[i]);
        s_asm_len = (uint16_t)(s_asm_len + s_slot_len[i]);
    }
    memcpy(s_asm_buf + s_asm_len, s_slot_data[0], s_slot_len[0]);
    s_asm_len = (uint16_t)(s_asm_len + s_slot_len[0]);
    return 0;
}

/**
 * @brief  重置 RC-TRANS 汇聚状态
 */
void rc_trans_reset(void)
{
    s_asm_pack_index = RC_PACK_INDEX_NONE;
    s_asm_active = 0u;
    s_assembled_ready = 0u;
    s_asm_last_ms = 0u;
    rc_trans_clear_slots();
}

void rc_trans_session_reset(void)
{
    s_have_last_ok = 0u;
    s_last_ok_pack_index = RC_PACK_INDEX_NONE;
    rc_trans_reset();
}

/**
 * @brief  半包汇聚超时：清 slot，避免旧 PackIndex 拖死后续大包
 */
void rc_trans_poll(void)
{
    uint32_t now_ms;

    if (!s_asm_active || s_assembled_ready)
    {
        return;
    }

    now_ms = rc_trans_now_ms();
    if ((now_ms - s_asm_last_ms) >= (uint32_t)RC_TRANS_ASM_TIMEOUT_MS)
    {
        co_printf("[RC   ] TRANS asm timeout pack=0x%08X\r\n",
                  (unsigned)s_asm_pack_index);
        rc_trans_reset();
    }
}

/**
 * @brief  UDP 发送成功后记录 PackIndex
 */
void rc_trans_note_upload_ok(uint32_t pack_index)
{
    s_last_ok_pack_index = pack_index;
    s_have_last_ok = 1u;
}

uint8_t *rc_trans_get_assembled(uint16_t *len)
{
    if (!s_assembled_ready || len == NULL)
    {
        return NULL;
    }
    *len = s_asm_len;
    return s_asm_buf;
}

void rc_trans_release(void)
{
    rc_trans_reset();
}

/**
 * @brief  处理 STB 写入的 RC-TRANS 小包
 */
int rc_trans_on_write(peer_t peer, const uint8_t *val, uint16_t len)
{
    uint16_t frame_len;
    uint16_t payload_len;
    uint16_t calc_crc;
    uint16_t recv_crc;
    uint32_t pack_index;
    uint8_t sub_index;
    const rc_status_t *cur;
    rc_status_t st;

    if (val == NULL || len < RC_TRANS_HDR_MIN)
    {
        rc_trans_fail(peer, RC_RESULT_LEN_ERROR, RC_PACK_INDEX_NONE);
        return -1;
    }

    /* OTA 会话期间拒绝 STB 下发的 RC-TRANS（Cat.1 模组留给 OTA 拉片） */
    if (udp_ota_session_busy() != 0u) {
        rc_trans_fail(peer, RC_RESULT_BUSY_REJECT, RC_PACK_INDEX_NONE);
        return -1;
    }

    /*
     * 收到 RC-TRANS → 通知 PM 有对端数据活动。
     * 待机态下会退出到工作态, CAT1 由主循环检测 s_pending_tlv 后上电。
     */
    pm_activity_notify(PM_ACTIVITY_PEER_DATA);

    rc_trans_log_rx(val, len);

    cur = ble_rc_svc_get_status();

    /*
     * 在途上传（assembled_ready / BUSY）时拒收新包：只回 Result，不改 State。
     * 若此处 rc_trans_fail→ERROR，会提前解锁并打乱仍在 UDP 队列中的原包。
     */
    if (s_assembled_ready || cur->state == RC_STATE_BUSY)
    {
        rc_status_t rej;

        co_printf("[RC   ] TRANS reject: %s pack=0x%08X (keep BUSY)\r\n",
                  s_assembled_ready ? "assembled_ready" : "state BUSY",
                  (unsigned)(s_assembled_ready ? s_asm_pack_index : cur->pack_index));
        memcpy(&rej, cur, sizeof(rej));
        rej.result = RC_RESULT_BUSY_REJECT;
        if (s_assembled_ready)
        {
            rej.pack_index = s_asm_pack_index;
        }
        ble_rc_svc_status_notify(peer, &rej);
        return -1;
    }
    if (cur->state != RC_STATE_READY && cur->state != RC_STATE_DONE &&
        cur->state != RC_STATE_ERROR)
    {
        co_printf("[RC   ] TRANS reject: state=%s(0x%02X) need CONF/READY\r\n",
                  (cur->state == RC_STATE_IDLE) ? "IDLE" :
                  (cur->state == RC_STATE_BUSY) ? "BUSY" : "?",
                  (unsigned)cur->state);
        rc_trans_fail(peer, RC_RESULT_PARAM_ERROR, RC_PACK_INDEX_NONE);
        return -1;
    }

    if (rc_be16(val + RC_TRANS_OFF_HEADER) != RC_TRANS_MAGIC)
    {
        rc_trans_fail(peer, RC_RESULT_LEN_ERROR, RC_PACK_INDEX_NONE);
        return -1;
    }

    frame_len = rc_be16(val + RC_TRANS_OFF_FRAME_LEN);
    if (frame_len != len || frame_len > RC_TRANS_FRAME_MAX)
    {
        rc_trans_fail(peer, RC_RESULT_LEN_ERROR, RC_PACK_INDEX_NONE);
        return -1;
    }

    pack_index = rc_be32(val + RC_TRANS_OFF_PACK_INDEX);
    sub_index = val[RC_TRANS_OFF_SUB_IDX];
    payload_len = val[RC_TRANS_OFF_PAYLOAD_LEN];
    if (sub_index >= RC_TRANS_MAX_SUBPACK)
    {
        rc_trans_fail(peer, RC_RESULT_PARAM_ERROR, pack_index);
        return -1;
    }
    if (payload_len > RC_TRANS_PAYLOAD_MAX ||
        (uint16_t)(RC_TRANS_OFF_PAYLOAD + payload_len + 2u) != len)
    {
        rc_trans_fail(peer, RC_RESULT_LEN_ERROR, pack_index);
        return -1;
    }

    calc_crc = rc_crc16_ccitt_false(val, (uint32_t)(len - 2u));
    recv_crc = rc_be16(val + len - 2u);
    if (calc_crc != recv_crc)
    {
        rc_trans_fail(peer, RC_RESULT_CRC_ERROR, pack_index);
        return -1;
    }

    if (s_have_last_ok)
    {
        /* PackIndex 递增：小于已成功上传序号视为 DUPLICATE（0xFFFFFFFE→0 环绕例外） */
        if (pack_index < s_last_ok_pack_index &&
            !(s_last_ok_pack_index == 0xFFFFFFFEu && pack_index == 0u))
        {
            rc_trans_fail(peer, RC_RESULT_DUPLICATE, pack_index);
            return -1;
        }
    }

    if (!s_asm_active)
    {
        s_asm_active = 1u;
        s_asm_pack_index = pack_index;
        rc_trans_clear_slots();
        rc_trans_touch_activity();
    }
    else if (pack_index != s_asm_pack_index)
    {
        co_printf("[RC   ] TRANS reject: pack mismatch active=0x%08X incoming=0x%08X\r\n",
                  (unsigned)s_asm_pack_index, (unsigned)pack_index);
        rc_trans_fail(peer, RC_RESULT_BUSY_REJECT, s_asm_pack_index);
        return -1;
    }
    else
    {
        rc_trans_touch_activity();
    }

    if (s_slot_used[sub_index])
    {
        if (sub_index == 0u && pack_index == s_asm_pack_index)
        {
            co_printf("[RC   ] TRANS sub0 retry pack=0x%08X\r\n",
                      (unsigned)pack_index);
        }
        else
        {
            rc_trans_fail(peer, RC_RESULT_DUPLICATE, pack_index);
            return -1;
        }
    }
    else
    {
        s_slot_used[sub_index] = 1u;
    }
    s_slot_len[sub_index] = (uint8_t)payload_len;
    memcpy(s_slot_data[sub_index], val + RC_TRANS_OFF_PAYLOAD, payload_len);

    if (sub_index != 0x00u)
    {
        return 0;
    }

    if (rc_trans_build_from_slots() != 0)
    {
        /* 保留已收中间子包，仅清 Sub=0，便于 STB 同 PackIndex 重试 0 包 */
        s_slot_used[0] = 0u;
        s_slot_len[0] = 0u;
        co_printf("[RC   ] TRANS sub0 asm incomplete pack=0x%08X\r\n",
                  (unsigned)pack_index);
        return -1;
    }

    s_assembled_ready = 1u;
    if (rc_cat1_on_tlv_ready(s_asm_len, pack_index) != 0)
    {
        rc_trans_fail(peer, RC_RESULT_PARAM_ERROR, pack_index);
        return -1;
    }

    memcpy(&st, cur, sizeof(st));
    st.state = RC_STATE_BUSY;
    st.result = RC_RESULT_NONE;
    st.pack_index = pack_index;
    ble_rc_svc_status_notify(peer, &st);
    return 0;
}
