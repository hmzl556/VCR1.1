/**
 * @file upg_md5.c
 * @brief MD5 实现（RFC 1321），参数与 GY/T 407-2024 附录 J.3 一致
 */
#include "upg_md5.h"
#include <string.h>

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac) \
    do { (a) += F(b, c, d) + (x) + (uint32_t)(ac); (a) = ROTL(a, s); (a) += (b); } while (0)
#define GG(a, b, c, d, x, s, ac) \
    do { (a) += G(b, c, d) + (x) + (uint32_t)(ac); (a) = ROTL(a, s); (a) += (b); } while (0)
#define HH(a, b, c, d, x, s, ac) \
    do { (a) += H(b, c, d) + (x) + (uint32_t)(ac); (a) = ROTL(a, s); (a) += (b); } while (0)
#define II(a, b, c, d, x, s, ac) \
    do { (a) += I(b, c, d) + (x) + (uint32_t)(ac); (a) = ROTL(a, s); (a) += (b); } while (0)

static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/** @brief 处理一个 64 字节分组 */
static void md5_block(upg_md5_ctx_t *ctx, const uint8_t block[64])
{
    uint32_t x[16];
    uint32_t a = ctx->state[0], b = ctx->state[1];
    uint32_t c = ctx->state[2], d = ctx->state[3];
    uint8_t i;

    for (i = 0; i < 16u; ++i) {
        x[i] = rd32le(block + (uint32_t)i * 4u);
    }

    FF(a, b, c, d, x[0], 7, 0xd76aa478UL);
    FF(d, a, b, c, x[1], 12, 0xe8c7b756UL);
    FF(c, d, a, b, x[2], 17, 0x242070dbUL);
    FF(b, c, d, a, x[3], 22, 0xc1bdceeeUL);
    FF(a, b, c, d, x[4], 7, 0xf57c0fafUL);
    FF(d, a, b, c, x[5], 12, 0x4787c62aUL);
    FF(c, d, a, b, x[6], 17, 0xa8304613UL);
    FF(b, c, d, a, x[7], 22, 0xfd469501UL);
    FF(a, b, c, d, x[8], 7, 0x698098d8UL);
    FF(d, a, b, c, x[9], 12, 0x8b44f7afUL);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1UL);
    FF(b, c, d, a, x[11], 22, 0x895cd7beUL);
    FF(a, b, c, d, x[12], 7, 0x6b901122UL);
    FF(d, a, b, c, x[13], 12, 0xfd987193UL);
    FF(c, d, a, b, x[14], 17, 0xa679438eUL);
    FF(b, c, d, a, x[15], 22, 0x49b40821UL);

    GG(a, b, c, d, x[1], 5, 0xf61e2562UL);
    GG(d, a, b, c, x[6], 9, 0xc040b340UL);
    GG(c, d, a, b, x[11], 14, 0x265e5a51UL);
    GG(b, c, d, a, x[0], 20, 0xe9b6c7aaUL);
    GG(a, b, c, d, x[5], 5, 0xd62f105dUL);
    GG(d, a, b, c, x[10], 9, 0x02441453UL);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681UL);
    GG(b, c, d, a, x[4], 20, 0xe7d3fbc8UL);
    GG(a, b, c, d, x[9], 5, 0x21e1cde6UL);
    GG(d, a, b, c, x[14], 9, 0xc33707d6UL);
    GG(c, d, a, b, x[3], 14, 0xf4d50d87UL);
    GG(b, c, d, a, x[8], 20, 0x455a14edUL);
    GG(a, b, c, d, x[13], 5, 0xa9e3e905UL);
    GG(d, a, b, c, x[2], 9, 0xfcefa3f8UL);
    GG(c, d, a, b, x[7], 14, 0x676f02d9UL);
    GG(b, c, d, a, x[12], 20, 0x8d2a4c8aUL);

    HH(a, b, c, d, x[5], 4, 0xfffa3942UL);
    HH(d, a, b, c, x[8], 11, 0x8771f681UL);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122UL);
    HH(b, c, d, a, x[14], 23, 0xfde5380cUL);
    HH(a, b, c, d, x[1], 4, 0xa4beea44UL);
    HH(d, a, b, c, x[4], 11, 0x4bdecfa9UL);
    HH(c, d, a, b, x[7], 16, 0xf6bb4b60UL);
    HH(b, c, d, a, x[10], 23, 0xbebfbc70UL);
    HH(a, b, c, d, x[13], 4, 0x289b7ec6UL);
    HH(d, a, b, c, x[0], 11, 0xeaa127faUL);
    HH(c, d, a, b, x[3], 16, 0xd4ef3085UL);
    HH(b, c, d, a, x[6], 23, 0x04881d05UL);
    HH(a, b, c, d, x[9], 4, 0xd9d4d039UL);
    HH(d, a, b, c, x[12], 11, 0xe6db99e5UL);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8UL);
    HH(b, c, d, a, x[2], 23, 0xc4ac5665UL);

    II(a, b, c, d, x[0], 6, 0xf4292244UL);
    II(d, a, b, c, x[7], 10, 0x432aff97UL);
    II(c, d, a, b, x[14], 15, 0xab9423a7UL);
    II(b, c, d, a, x[5], 21, 0xfc93a039UL);
    II(a, b, c, d, x[12], 6, 0x655b59c3UL);
    II(d, a, b, c, x[3], 10, 0x8f0ccc92UL);
    II(c, d, a, b, x[10], 15, 0xffeff47dUL);
    II(b, c, d, a, x[1], 21, 0x85845dd1UL);
    II(a, b, c, d, x[8], 6, 0x6fa87e4fUL);
    II(d, a, b, c, x[15], 10, 0xfe2ce6e0UL);
    II(c, d, a, b, x[6], 15, 0xa3014314UL);
    II(b, c, d, a, x[13], 21, 0x4e0811a1UL);
    II(a, b, c, d, x[4], 6, 0xf7537e82UL);
    II(d, a, b, c, x[11], 10, 0xbd3af235UL);
    II(c, d, a, b, x[2], 15, 0x2ad7d2bbUL);
    II(b, c, d, a, x[9], 21, 0xeb86d391UL);

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void upg_md5_init(upg_md5_ctx_t *ctx)
{
    ctx->count[0] = 0u;
    ctx->count[1] = 0u;
    ctx->state[0] = 0x67452301UL;
    ctx->state[1] = 0xefcdab89UL;
    ctx->state[2] = 0x98badcfeUL;
    ctx->state[3] = 0x10325476UL;
}

void upg_md5_update(upg_md5_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t idx = (ctx->count[0] >> 3) & 0x3Fu;
    uint32_t part;

    ctx->count[0] += len << 3;
    if (ctx->count[0] < (len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += len >> 29;

    if (idx != 0u) {
        part = 64u - idx;
        if (len < part) {
            memcpy(ctx->buffer + idx, data, len);
            return;
        }
        memcpy(ctx->buffer + idx, data, part);
        md5_block(ctx, ctx->buffer);
        data += part;
        len -= part;
    }

    while (len >= 64u) {
        md5_block(ctx, data);
        data += 64u;
        len -= 64u;
    }

    if (len != 0u) {
        memcpy(ctx->buffer, data, len);
    }
}

void upg_md5_final(upg_md5_ctx_t *ctx, uint8_t digest[16])
{
    static const uint8_t pad[64] = { 0x80u };
    uint8_t bits[8];
    uint32_t idx = (ctx->count[0] >> 3) & 0x3Fu;
    uint32_t pad_len = (idx < 56u) ? (56u - idx) : (120u - idx);
    uint8_t i;

    wr32le(bits, ctx->count[0]);
    wr32le(bits + 4, ctx->count[1]);
    upg_md5_update(ctx, pad, pad_len);
    upg_md5_update(ctx, bits, 8u);

    for (i = 0; i < 4u; ++i) {
        wr32le(digest + (uint32_t)i * 4u, ctx->state[i]);
    }
}

void upg_md5(const uint8_t *data, uint32_t len, uint8_t digest[16])
{
    upg_md5_ctx_t ctx;

    upg_md5_init(&ctx);
    upg_md5_update(&ctx, data, len);
    upg_md5_final(&ctx, digest);
}
