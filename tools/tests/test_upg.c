/* Host test for the key-download protocol primitives: MD5, CRC16 and info builders. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

/* --- upg_meta.c 所需的最小桩：只关心逻辑，不关心 Flash 地址 --- */
#define FlashOperationSuccess 0u
static uint8_t s_flash[512];
static uint32_t Qflash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    (void)addr;
    if (buf == NULL || len == 0u || len > sizeof(s_flash)) return 1u;
    memcpy(buf, s_flash, len);
    return 0u;
}

#include "upg_md5.c"
#include "upg_meta.c"

static void hex16(const uint8_t d[16], char out[33])
{
    static const char h[] = "0123456789abcdef";
    unsigned i;
    for (i = 0u; i < 16u; ++i) {
        out[i * 2u] = h[d[i] >> 4];
        out[i * 2u + 1u] = h[d[i] & 0x0Fu];
    }
    out[32] = '\0';
}

static void md5_expect(const uint8_t *data, uint32_t len, const char *want)
{
    uint8_t d[16];
    char got[33];

    upg_md5(data, len, d);
    hex16(d, got);
    if (strcmp(got, want) != 0) {
        printf("MD5 mismatch: got %s want %s\n", got, want);
        assert(0);
    }
}

int main(void)
{
    uint8_t d[16], d2[16];
    char hex[33];
    uint8_t buf[256];
    uint16_t n;
    uint16_t body_len, crc_field;
    const char digits[] = "123456789";
    const char sixty4[] =
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890";

    /* RFC 1321 标准测试向量 */
    md5_expect((const uint8_t *)"", 0u, "d41d8cd98f00b204e9800998ecf8427e");
    md5_expect((const uint8_t *)"a", 1u, "0cc175b9c0f1b6a831c399e269772661");
    md5_expect((const uint8_t *)"abc", 3u, "900150983cd24fb0d6963f7d28e17f72");
    md5_expect((const uint8_t *)"message digest", 14u, "f96b697d7cb7938d525a2f31aaf161d0");
    md5_expect((const uint8_t *)"abcdefghijklmnopqrstuvwxyz", 26u,
               "c3fcd3d76192e4007dfb496cca67e13b");
    md5_expect((const uint8_t *)sixty4, 80u, "57edf4a22be3c955ac49da2e2107b67a");

    /* 分片更新必须与一次性计算结果一致（认证按 key/rand/idcode 三段拼接） */
    {
        upg_md5_ctx_t ctx;
        upg_md5_init(&ctx);
        upg_md5_update(&ctx, (const uint8_t *)"abc", 3u);
        upg_md5_final(&ctx, d);
        upg_md5_update(&ctx, (const uint8_t *)"abc", 3u); /* 复用 ctx 前先重置 */
        upg_md5_init(&ctx);
        upg_md5_update(&ctx, (const uint8_t *)"a", 1u);
        upg_md5_update(&ctx, (const uint8_t *)"bc", 2u);
        upg_md5_final(&ctx, d2);
        hex16(d, hex);
        assert(strcmp(hex, "900150983cd24fb0d6963f7d28e17f72") == 0);
        assert(memcmp(d, d2, 16u) == 0);
    }

    /* 附录 J 的 CRC16：反射式多项式 0xA001，初值 0（CRC-16/ARC） */
    assert(upg_crc16((const uint8_t *)digits, 9u, 0u) == 0xBB3Du);
    assert(upg_crc16((const uint8_t *)"", 0u, 0u) == 0x0000u);
    /* 分段累加等价于一次性计算 */
    assert(upg_crc16((const uint8_t *)digits + 4, 5u,
                     upg_crc16((const uint8_t *)digits, 4u, 0u)) == 0xBB3Du);

    /* rel0：idcode 文本，未写入密钥时使用开发占位值 */
    memset(s_flash, 0xFF, sizeof(s_flash));
    n = upg_meta_build_rel0(buf, sizeof(buf));
    assert(n == 7u + UPG_IDCODE_LEN + 1u);          /* "idcode:" + 32 字符 + '\n' */
    assert(memcmp(buf, "idcode:VCR1DEVIDCODE", 20u) == 0);   /* 7 + 13 字符前缀 */
    assert(buf[n - 1u] == '\n');

    /* usr1：CRC16(2) + 长度(2) + 键值文本，均高字节在前 */
    n = upg_meta_build_usr1(buf, sizeof(buf));
    assert(n > 4u);
    body_len = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    assert(body_len == n - 4u);
    assert(memcmp(buf + 4, "fw:", 3u) == 0);
    crc_field = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    assert(crc_field == upg_crc16(buf + 4, body_len, 0u));

    /* 写入合法密钥记录后，idcode/key 应返回该记录并报告已产测 */
    memset(s_flash, 0xFF, sizeof(s_flash));
    {
        upg_authkey_t k;
        uint8_t idc[UPG_IDCODE_LEN], key[UPG_KEY_LEN];
        memset(&k, 0, sizeof(k));
        k.magic = AUTHKEY_MAGIC;
        k.version = AUTHKEY_VER;
        k.key_count = 1u;
        memset(k.idcode, 'A', UPG_IDCODE_LEN);
        memset(k.keys[0], 'K', UPG_KEY_LEN);
        memcpy(s_flash, &k, sizeof(k));

        assert(upg_meta_idcode(idc) == true);
        assert(idc[0] == 'A');
        assert(upg_meta_key(0u, key) == true);
        assert(key[0] == 'K');
        /* 越界索引回落到第 0 组 */
        assert(upg_meta_key(9u, key) == true);
        assert(key[0] == 'K');
    }

    puts("UPG primitives: 6 MD5 RFC vectors, chunked update, CRC16/ARC, rel0/usr1 layout, "
         "provisioned key load PASS");
    return 0;
}
