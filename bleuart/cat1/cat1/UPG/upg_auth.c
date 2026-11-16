/**
 * @file upg_auth.c
 * @brief 双向 MD5 认证实现（GY/T 407-2024 E.1.2）
 *
 * 帧格式：
 *   FA 20 00 key_index rand[16] idcode[32]   遥控器 → 主机（发起）
 *   F9 20 hash[16]                           主机 → 遥控器（回应）
 *   FA 20 01|02                              遥控器 → 主机（通过/失败）
 *   F9 21 00 rand[16] key_index              主机 → 遥控器（发起）
 *   FA 21 hash[16]                           遥控器 → 主机（回应）
 *   F9 21 01                                 主机 → 遥控器（通过）
 *
 * 状态放在借来的会话工作区（upg_ws_t.st），不占静态 RAM。
 */
#include "upg_auth.h"
#include "upg_md5.h"
#include "upg_meta.h"
#include "upg_session.h"
#include "gmem.h"
#include <string.h>

#define UPG_CMD_RC       0xFAu
#define UPG_OP_AUTH_HOST 0x20u
#define UPG_OP_AUTH_RC   0x21u

#define UPG_AUTH_STATUS_START   0x00u
#define UPG_AUTH_STATUS_PASS    0x01u
#define UPG_AUTH_STATUS_FAIL    0x02u

/** @brief 取会话态；未在会话中返回 NULL */
static upg_state_t *auth_st(void)
{
    upg_ws_t *ws = upg_ws();

    return (ws != NULL) ? &ws->st : NULL;
}

void upg_auth_rand(uint8_t *out, uint16_t len)
{
    upg_state_t *s = auth_st();
    uint16_t i;

    if (s == NULL || out == NULL) {
        return;
    }
    if (s->rng == 0u) {
        s->rng = (uint32_t)app_now_ms() ^ 0x9E3779B9u;
        if (s->rng == 0u) {
            s->rng = 0x12345678u;
        }
    }
    for (i = 0u; i < len; ++i) {
        /* xorshift32：仅用于联调，量产应改用硬件随机源 */
        s->rng ^= s->rng << 13;
        s->rng ^= s->rng >> 17;
        s->rng ^= s->rng << 5;
        out[i] = (uint8_t)s->rng;
    }
}

void upg_auth_reset(void)
{
    upg_state_t *s = auth_st();

    if (s != NULL) {
        s->auth_state = (uint8_t)UPG_AUTH_IDLE;
        s->key_index = 0u;
        memset(s->hash1, 0, sizeof(s->hash1));
    }
}

/** @brief hash = MD5(key || rand || idcode) */
static void auth_hash(const uint8_t *rand, uint8_t out[16])
{
    upg_md5_ctx_t ctx;
    upg_state_t *s = auth_st();
    uint8_t key[UPG_KEY_LEN];
    uint8_t idcode[UPG_IDCODE_LEN];
    uint8_t idx = (s != NULL) ? s->key_index : 0u;

    (void)upg_meta_key(idx, key);
    (void)upg_meta_idcode(idcode);

    upg_md5_init(&ctx);
    upg_md5_update(&ctx, key, UPG_KEY_LEN);
    upg_md5_update(&ctx, rand, 16u);
    upg_md5_update(&ctx, idcode, UPG_IDCODE_LEN);
    upg_md5_final(&ctx, out);

    memset(key, 0, sizeof(key));
}

uint16_t upg_auth_start(uint8_t *out, uint16_t cap)
{
    upg_state_t *s = auth_st();
    uint8_t idcode[UPG_IDCODE_LEN];
    uint8_t rand1[16];

    if (s == NULL || out == NULL || cap < 52u) {
        return 0u;
    }
    s->key_index = 0u;
    upg_auth_rand(rand1, sizeof(rand1));
    auth_hash(rand1, s->hash1);

    out[0] = UPG_CMD_RC;
    out[1] = UPG_OP_AUTH_HOST;
    out[2] = UPG_AUTH_STATUS_START;
    out[3] = s->key_index;
    memcpy(out + 4, rand1, 16u);
    (void)upg_meta_idcode(idcode);
    memcpy(out + 20, idcode, UPG_IDCODE_LEN);

    s->auth_state = (uint8_t)UPG_AUTH_WAIT_HOST_HASH;
    return 52u;
}

uint16_t upg_auth_on_host_hash(const uint8_t *hash, uint16_t len, uint8_t *out, uint16_t cap)
{
    upg_state_t *s = auth_st();
    uint8_t diff = 0u;
    uint16_t i;

    if (s == NULL || out == NULL || cap < 3u) {
        return 0u;
    }
    out[0] = UPG_CMD_RC;
    out[1] = UPG_OP_AUTH_HOST;

    if (s->auth_state != (uint8_t)UPG_AUTH_WAIT_HOST_HASH || hash == NULL || len != 16u) {
        s->auth_state = (uint8_t)UPG_AUTH_FAILED;
        out[2] = UPG_AUTH_STATUS_FAIL;
        return 3u;
    }
    for (i = 0u; i < 16u; ++i) {
        diff |= (uint8_t)(hash[i] ^ s->hash1[i]);
    }
    if (diff != 0u) {
        s->auth_state = (uint8_t)UPG_AUTH_FAILED;
        out[2] = UPG_AUTH_STATUS_FAIL;
        return 3u;
    }
    s->auth_state = (uint8_t)UPG_AUTH_WAIT_RC_REQ;
    out[2] = UPG_AUTH_STATUS_PASS;
    return 3u;
}

uint16_t upg_auth_on_rc_request(const uint8_t *frame, uint16_t len, uint8_t *out, uint16_t cap)
{
    upg_state_t *s = auth_st();
    uint8_t status;

    if (s == NULL || frame == NULL || len < 3u) {
        return 0u;
    }
    status = frame[2];

    if (status == UPG_AUTH_STATUS_PASS) {
        if (s->auth_state == (uint8_t)UPG_AUTH_WAIT_RC_REQ) {
            s->auth_state = (uint8_t)UPG_AUTH_DONE;
        }
        return 0u;
    }
    if (status != UPG_AUTH_STATUS_START) {
        s->auth_state = (uint8_t)UPG_AUTH_FAILED;
        return 0u;
    }
    /* 主机发起遥控器认证：F9 21 00 rand[16] key_index */
    if (len < 20u || s->auth_state != (uint8_t)UPG_AUTH_WAIT_RC_REQ) {
        s->auth_state = (uint8_t)UPG_AUTH_FAILED;
        return 0u;
    }
    if (out == NULL || cap < 18u) {
        return 0u;
    }
    s->key_index = frame[19];
    auth_hash(frame + 3, out + 2);
    out[0] = UPG_CMD_RC;
    out[1] = UPG_OP_AUTH_RC;
    return 18u;
}

upg_auth_state_t upg_auth_state(void)
{
    upg_state_t *s = auth_st();

    return (s != NULL) ? (upg_auth_state_t)s->auth_state : UPG_AUTH_IDLE;
}

bool upg_auth_is_done(void)
{
    upg_state_t *s = auth_st();

    return (s != NULL) && (s->auth_state == (uint8_t)UPG_AUTH_DONE);
}
