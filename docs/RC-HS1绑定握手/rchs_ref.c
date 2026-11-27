/*
 * rchs_ref.c -- RC-HS1 模组侧握手算法参考实现（XTEA-64/128, 32 cycles）
 *
 * 用途：给 4G 模组厂（利尔达 NT26-FCN-C / 展锐 EC718）做实现对照与自测，
 *       对应指令 AT+RCHS=<idx>,<nonce>。
 * 编译：gcc -Wall -O2 rchs_ref.c -o rchs_ref
 * 运行：./rchs_ref   必须输出 SELF-TEST PASSED
 * 约束：纯 C89/C99，无动态内存、无查表，核心代码约 40 行，RAM < 32 B。
 *
 * 协议摘要
 *   请求：AT+RCHS=<idx>,<nonce>   nonce = 8 字节 → 16 个十六进制字符
 *   应答：+RCHS: <resp>           resp  = 8 字节 → 16 个十六进制字符
 *         OK
 *   resp = XTEA_encrypt_32cycles(key = K[idx][16], block = nonce[8])
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RC_HS_ROUNDS 32u

/* XTEA 分组加密：v[0..1] = 64 位数据块，k[0..3] = 128 位密钥 */
static void xtea_encrypt(uint32_t v[2], const uint32_t k[4])
{
    uint32_t v0 = v[0];
    uint32_t v1 = v[1];
    uint32_t sum = 0u;
    uint32_t i;

    for (i = 0u; i < RC_HS_ROUNDS; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3u]);
        sum += 0x9E3779B9u;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3u]);
    }
    v[0] = v0;
    v[1] = v1;
}

/* 大端 32 位读写 */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void put_be32(uint8_t *p, uint32_t w)
{
    p[0] = (uint8_t)(w >> 24);
    p[1] = (uint8_t)(w >> 16);
    p[2] = (uint8_t)(w >> 8);
    p[3] = (uint8_t)w;
}

/*
 * 计算握手应答
 *   key[16]  : 16 字节预置密钥（K[idx]）
 *   nonce[8] : 主机下发的 8 字节挑战，大端
 *   resp[8]  : 返回给主机的 8 字节应答，大端
 */
void rc_hs_response(const uint8_t key[16], const uint8_t nonce[8], uint8_t resp[8])
{
    uint32_t k[4];
    uint32_t v[2];
    uint8_t i;

    for (i = 0u; i < 4u; i++) {
        k[i] = be32(key + 4u * i);
    }
    v[0] = be32(nonce);
    v[1] = be32(nonce + 4u);

    xtea_encrypt(v, k);

    put_be32(resp, v[0]);
    put_be32(resp + 4u, v[1]);
}

/* ------------------------------------------------------------------ */
/* 以下仅为自测代码，移植到模组固件时不需要                              */
/* ------------------------------------------------------------------ */

static int hex_to_bytes(const char *hex, uint8_t *out, uint16_t out_cap)
{
    uint16_t n = 0u;

    while (hex[0] != '\0' && hex[1] != '\0') {
        unsigned v = 0u;
        int i;
        for (i = 0; i < 2; i++) {
            char c = (i == 0) ? hex[0] : hex[1];
            unsigned d;
            if (c >= '0' && c <= '9')      d = (unsigned)(c - '0');
            else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
            else return -1;
            v = (v << 4) | d;
        }
        if (n >= out_cap) return -1;
        out[n++] = (uint8_t)v;
        hex += 2;
    }
    return (int)n;
}

static void bytes_to_hex(const uint8_t *b, uint16_t len, char *out)
{
    static const char tab[] = "0123456789ABCDEF";
    uint16_t i;

    for (i = 0u; i < len; i++) {
        out[2u * i] = tab[b[i] >> 4];
        out[2u * i + 1u] = tab[b[i] & 0x0Fu];
    }
    out[2u * len] = '\0';
}

int main(void)
{
    static const struct {
        const char *key;
        const char *nonce;
        const char *expect;
        const char *note;
    } vt[] = {
        /* XTEA 官方自测向量（用于确认算法本身没写错） */
        { "00000000000000000000000000000000", "0000000000000000", "DEE9D4D8F7131ED9", "XTEA official #1" },
        { "0123456789ABCDEF0123456789ABCDEF", "0123456789ABCDEF", "27E795E076B2B537", "XTEA official #2" },
        /* RC-HS1 协议向量，测试密钥 = ASCII "RCHS-TEST-KEY-01" */
        { "524348532D544553542D4B45592D3031", "0000000000000000", "50D38D93C8049961", "RCHS vector #1" },
        { "524348532D544553542D4B45592D3031", "0102030405060708", "61AA3ADB1E6E9224", "RCHS vector #2" },
        { "524348532D544553542D4B45592D3031", "FFFFFFFFFFFFFFFF", "BA809F9A860D83C7", "RCHS vector #3" },
        { "524348532D544553542D4B45592D3031", "0011223344556677", "815A333B1F39EA7E", "RCHS vector #4" },
        { "524348532D544553542D4B45592D3031", "A5A5A5A5A5A5A5A5", "ED3121A623B7DA73", "RCHS vector #5" },
    };
    unsigned i;
    int fail = 0;

    for (i = 0u; i < (unsigned)(sizeof(vt) / sizeof(vt[0])); i++) {
        uint8_t key[16], nonce[8], resp[8];
        char hex[17];

        if (hex_to_bytes(vt[i].key, key, sizeof(key)) != 16 ||
            hex_to_bytes(vt[i].nonce, nonce, sizeof(nonce)) != 8) {
            printf("%-16s PARSE ERROR\n", vt[i].note);
            fail = 1;
            continue;
        }
        rc_hs_response(key, nonce, resp);
        bytes_to_hex(resp, 8u, hex);
        printf("%-16s got %s expect %s %s\n", vt[i].note, hex, vt[i].expect,
               strcmp(hex, vt[i].expect) == 0 ? "OK" : "FAIL");
        if (strcmp(hex, vt[i].expect) != 0) fail = 1;
    }
    printf("%s\n", fail ? "SELF-TEST FAILED" : "SELF-TEST PASSED");
    return fail;
}