#include <string.h>
#include "rc_trans_assembly.h"
#include "ble_rc_svc.h"
#include "rc_cat1_bridge.h"

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
static uint16_t s_asm_len;
static uint32_t s_asm_pack_index;
static uint8_t  s_asm_active;
static uint8_t  s_assembled_ready;
static uint32_t s_last_ok_pack_index;
static uint8_t  s_have_last_ok;

static uint16_t rc_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

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

static void rc_trans_clear_slots(void)
{
    memset(s_slot_used, 0, sizeof(s_slot_used));
    s_asm_len = 0u;
}

static void rc_trans_fail(peer_t peer, uint8_t result, uint32_t pack_index)
{
    rc_status_t st;

    memcpy(&st, ble_rc_svc_get_status(), sizeof(st));
    st.state = RC_STATE_ERROR;
    st.result = result;
    st.pack_index = pack_index;
    ble_rc_svc_status_notify(peer, &st);
}

/**
 * @brief  按 SubPackIndex 倒序拼接到 slot0 区域（省去独立 asm_buf）
 */
static int rc_trans_build_from_slots(void)
{
    uint8_t max_sub = 0;
    uint8_t i;
    uint16_t total = 0;
    uint8_t sub0_bak[RC_TRANS_PAYLOAD_MAX];

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

    memcpy(sub0_bak, s_slot_data[0], s_slot_len[0]);
    s_asm_len = 0u;
    for (i = max_sub; i > 0u; i--)
    {
        memcpy(s_slot_data[0] + s_asm_len, s_slot_data[i], s_slot_len[i]);
        s_asm_len = (uint16_t)(s_asm_len + s_slot_len[i]);
    }
    memcpy(s_slot_data[0] + s_asm_len, sub0_bak, s_slot_len[0]);
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
    rc_trans_clear_slots();
}

/**
 * @brief  UDP 发送成功后记录 PackIndex
 */
void rc_trans_note_upload_ok(uint32_t pack_index)
{
    s_last_ok_pack_index = pack_index;
    s_have_last_ok = 1u;
}

const uint8_t *rc_trans_get_assembled(uint16_t *len)
{
    if (!s_assembled_ready || len == NULL)
    {
        return NULL;
    }
    *len = s_asm_len;
    return s_slot_data[0];
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

    if (s_assembled_ready)
    {
        rc_trans_fail(peer, RC_RESULT_BUSY_REJECT, s_asm_pack_index);
        return -1;
    }

    cur = ble_rc_svc_get_status();
    if (cur->state == RC_STATE_BUSY)
    {
        rc_trans_fail(peer, RC_RESULT_BUSY_REJECT, cur->pack_index);
        return -1;
    }
    if (cur->state != RC_STATE_READY && cur->state != RC_STATE_DONE &&
        cur->state != RC_STATE_ERROR)
    {
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
    }
    else if (pack_index != s_asm_pack_index)
    {
        rc_trans_fail(peer, RC_RESULT_BUSY_REJECT, s_asm_pack_index);
        return -1;
    }

    if (s_slot_used[sub_index])
    {
        rc_trans_fail(peer, RC_RESULT_DUPLICATE, pack_index);
        return -1;
    }

    s_slot_used[sub_index] = 1u;
    s_slot_len[sub_index] = (uint8_t)payload_len;
    memcpy(s_slot_data[sub_index], val + RC_TRANS_OFF_PAYLOAD, payload_len);

    if (sub_index != 0x00u)
    {
        return 0;
    }

    if (rc_trans_build_from_slots() != 0)
    {
        rc_trans_fail(peer, RC_RESULT_LEN_ERROR, pack_index);
        rc_trans_reset();
        return -1;
    }

    s_assembled_ready = 1u;
    memcpy(&st, cur, sizeof(st));
    st.state = RC_STATE_BUSY;
    st.result = RC_RESULT_NONE;
    st.pack_index = pack_index;
    ble_rc_svc_status_notify(peer, &st);

    rc_cat1_on_tlv_ready(s_asm_len, pack_index);
    return 0;
}
