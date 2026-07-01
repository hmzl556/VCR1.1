/**
 * @file aes_transport.c
 * @brief UDP data field AES-128-ECB + PKCS5Padding（tiny-AES-c，静态 RAM）。
 */
#include "aes_transport.h"
#include "cat1_udp_config.h"

#include <string.h>

#if RC_UDP_AES_ENABLE
#include "tiny-aes/aes.h"

#define RC_UDP_AES_BLOCK  16u

static struct AES_ctx s_aes_ctx;
static uint8_t s_aes_work[CAT1_UDP_MAX_UDP_PDU];
static const uint8_t s_aes_key[16] = RC_UDP_AES_KEY_BYTES;

/**
 * @brief  将 plain 做 PKCS5 填充写入 s_aes_work，返回填充后长度
 * @return 0 成功；-1 超长或参数非法
 */
static int rc_udp_aes_pkcs5_pad(const uint8_t *plain, uint16_t plain_len, uint16_t *padded_len)
{
    uint16_t pad;
    uint16_t total;
    uint16_t i;
    if (plain == NULL || padded_len == NULL || plain_len == 0u
        || plain_len > RC_UDP_AES_PLAIN_DATA_MAX)
    {
        return -1;
    }
    pad = (uint16_t)(RC_UDP_AES_BLOCK - (plain_len % RC_UDP_AES_BLOCK));
    if (pad == 0u)
    {
        pad = RC_UDP_AES_BLOCK;
    }
    total = (uint16_t)(plain_len + pad);
    if (total > (uint16_t)sizeof(s_aes_work))
    {
        return -1;
    }
    memcpy(s_aes_work, plain, plain_len);
    for (i = plain_len; i < total; i++)
    {
        s_aes_work[i] = (uint8_t)pad;
    }
    *padded_len = total;
    return 0;
}

/**
 * @brief  校验 PKCS5 填充并写入 plain 输出缓冲
 * @return 0 成功；-1 失败
 */
static int rc_udp_aes_pkcs5_unpad(const uint8_t *padded, uint16_t padded_len, uint8_t *plain,
                                  uint16_t plain_cap, uint16_t *plain_len)
{
    uint8_t pad;
    uint16_t i;
    if (padded == NULL || plain == NULL || plain_len == NULL
        || padded_len == 0u || (padded_len % RC_UDP_AES_BLOCK) != 0u)
    {
        return -1;
    }
    pad = padded[padded_len - 1u];
    if (pad == 0u || pad > RC_UDP_AES_BLOCK || pad > padded_len)
    {
        return -1;
    }
    for (i = 0u; i < pad; i++)
    {
        if (padded[padded_len - 1u - i] != pad)
        {
            return -1;
        }
    }
    if ((uint16_t)(padded_len - pad) > plain_cap)
    {
        return -1;
    }
    memcpy(plain, padded, (size_t)(padded_len - pad));
    *plain_len = (uint16_t)(padded_len - pad);
    return 0;
}

/**
 * @brief  对 s_aes_work 中 padded_len 字节按块 ECB 加/解密
 */
static void rc_udp_aes_ecb_crypt_blocks(uint8_t *buf, uint16_t len, uint8_t encrypt)
{
    uint16_t i;
    for (i = 0u; i < len; i = (uint16_t)(i + RC_UDP_AES_BLOCK))
    {
        if (encrypt != 0u)
        {
            AES_ECB_encrypt(&s_aes_ctx, buf + i);
        }
        else
        {
            AES_ECB_decrypt(&s_aes_ctx, buf + i);
        }
    }
}
#endif /* RC_UDP_AES_ENABLE */

int rc_udp_aes_init(void)
{
#if RC_UDP_AES_ENABLE
    AES_init_ctx(&s_aes_ctx, s_aes_key);
#endif
    return 0;
}

int rc_udp_aes_encrypt_data_field(const uint8_t *plain, uint16_t plain_len,
                                  uint8_t *cipher, uint16_t cipher_cap, uint16_t *cipher_len)
{
#if RC_UDP_AES_ENABLE
    uint16_t padded_len;
    if (cipher == NULL || cipher_len == NULL)
    {
        return -1;
    }
    if (rc_udp_aes_pkcs5_pad(plain, plain_len, &padded_len) != 0)
    {
        return -1;
    }
    rc_udp_aes_ecb_crypt_blocks(s_aes_work, padded_len, 1u);
    if (padded_len > cipher_cap)
    {
        return -1;
    }
    memcpy(cipher, s_aes_work, padded_len);
    *cipher_len = padded_len;
    return 0;
#else
    (void)plain;
    (void)plain_len;
    (void)cipher;
    (void)cipher_cap;
    (void)cipher_len;
    return -1;
#endif
}

int rc_udp_aes_decrypt_data_field(const uint8_t *cipher, uint16_t cipher_len,
                                  uint8_t *plain, uint16_t plain_cap, uint16_t *plain_len)
{
#if RC_UDP_AES_ENABLE
    if (cipher == NULL || plain == NULL || plain_len == NULL
        || cipher_len == 0u || (cipher_len % RC_UDP_AES_BLOCK) != 0u
        || cipher_len > (uint16_t)sizeof(s_aes_work))
    {
        return -1;
    }
    memcpy(s_aes_work, cipher, cipher_len);
    rc_udp_aes_ecb_crypt_blocks(s_aes_work, cipher_len, 0u);
    return rc_udp_aes_pkcs5_unpad(s_aes_work, cipher_len, plain, plain_cap, plain_len);
#else
    (void)cipher;
    (void)cipher_len;
    (void)plain;
    (void)plain_cap;
    (void)plain_len;
    return -1;
#endif
}
