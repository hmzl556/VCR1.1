/**
 * @file upg_meta.c
 * @brief 硬件信息 / 版本信息 / 认证密钥
 *
 * 密钥存放于 FLASH_AUTHKEY_START 扇区（产测写入）。扇区为空时回退到开发占位值，
 * 便于 P1 阶段联调；量产前必须由产测工具写入真实 idcode/key。
 */
#include "upg_meta.h"
#include "flash_layout.h"
#include "qflash.h"
#include "co_printf.h"
#include <string.h>

#define UPG_META_APP_BUILD   "02010000"   /* 与版本号保持同步，联调阶段使用 */

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t key_count;
    uint8_t  idcode[UPG_IDCODE_LEN];
    uint8_t  keys[UPG_KEY_MAX][UPG_KEY_LEN];
    uint32_t crc;
} upg_authkey_t;

/** 开发占位值：仅用于未写入密钥时的联调，量产前必须替换 */
static void dev_defaults(upg_authkey_t *k)
{
    memset(k, 0, sizeof(*k));
    k->magic = AUTHKEY_MAGIC;
    k->version = AUTHKEY_VER;
    k->key_count = 1u;

    memset(k->idcode, '0', UPG_IDCODE_LEN);
    memcpy(k->idcode, "VCR1DEVIDCODE", 13u);

    memset(k->keys[0], '0', UPG_KEY_LEN);
    memcpy(k->keys[0], "VCR1DEVKEY", 10u);
}

/** @brief 从 Flash 读取密钥表；无效时填入开发占位值。返回是否已产测写入 */
static bool meta_load(upg_authkey_t *k)
{
    if (Qflash_Read(FLASH_AUTHKEY_START, (uint8_t *)k, sizeof(*k)) == FlashOperationSuccess
        && k->magic == AUTHKEY_MAGIC
        && k->version == AUTHKEY_VER
        && k->key_count != 0u
        && k->key_count <= UPG_KEY_MAX) {
        return true;
    }
    dev_defaults(k);
    return false;
}

bool upg_meta_idcode(uint8_t out[UPG_IDCODE_LEN])
{
    upg_authkey_t k;
    bool ok = meta_load(&k);

    memcpy(out, k.idcode, UPG_IDCODE_LEN);
    return ok;
}

bool upg_meta_key(uint8_t index, uint8_t out[UPG_KEY_LEN])
{
    upg_authkey_t k;
    bool ok = meta_load(&k);

    if (index >= k.key_count) {
        index = 0u;
    }
    memcpy(out, k.keys[index], UPG_KEY_LEN);
    if (!ok) {
        co_printf("[UPG ] warning: dev auth key in use\r\n");
    }
    return ok;
}

uint16_t upg_crc16(const uint8_t *buf, uint32_t len, uint16_t crc_last)
{
    uint16_t crc = crc_last;
    uint32_t j;
    uint8_t  i;

    for (j = len; j > 0u; --j) {
        uint8_t ds = *buf++;
        for (i = 0u; i < 8u; ++i) {
            crc = (uint16_t)((crc >> 1) ^ (((crc ^ ds) & 1u) ? 0xA001u : 0u));
            ds >>= 1;
        }
    }
    return crc;
}

uint16_t upg_meta_build_rel0(uint8_t *buf, uint16_t cap)
{
    static const char prefix[] = "idcode:";
    uint16_t n;
    uint8_t  idcode[UPG_IDCODE_LEN];
    uint16_t i;

    if (buf == NULL || cap < (uint16_t)(sizeof(prefix) - 1u + UPG_IDCODE_LEN + 1u)) {
        return 0u;
    }
    (void)upg_meta_idcode(idcode);
    memcpy(buf, prefix, sizeof(prefix) - 1u);
    n = (uint16_t)(sizeof(prefix) - 1u);
    for (i = 0u; i < UPG_IDCODE_LEN; ++i) {
        buf[n++] = idcode[i];
    }
    buf[n++] = '\n';
    return n;
}

/** @brief 追加字符串，返回新的写入位置 */
static uint16_t append_str(uint8_t *buf, uint16_t cap, uint16_t pos, const char *s)
{
    while (*s != '\0' && pos < cap) {
        buf[pos++] = (uint8_t)(*s++);
    }
    return pos;
}

uint16_t upg_meta_build_usr1(uint8_t *buf, uint16_t cap)
{
    uint16_t body, pos;
    uint16_t crc;

    if (buf == NULL || cap < 8u) {
        return 0u;
    }
    /* 先占位 CRC16(2) + 长度(2)，数据体从偏移 4 开始 */
    pos = 4u;
    pos = append_str(buf, cap, pos, "fw:" UPG_META_APP_BUILD "\n");
    pos = append_str(buf, cap, pos, "tbi:0\n");
    pos = append_str(buf, cap, pos, "tmi:0\n");
    pos = append_str(buf, cap, pos, "sbi:0\n");
    pos = append_str(buf, cap, pos, "smi:0\n");
    pos = append_str(buf, cap, pos, "irtv:0\n");
    pos = append_str(buf, cap, pos, "irstb:0\n");
    pos = append_str(buf, cap, pos, "bletv:0\n");
    pos = append_str(buf, cap, pos, "blestb:0\n");

    body = (uint16_t)(pos - 4u);
    crc = upg_crc16(buf + 4u, body, 0u);

    buf[0] = (uint8_t)(crc >> 8);          /* 高字节在前 */
    buf[1] = (uint8_t)(crc & 0xFFu);
    buf[2] = (uint8_t)(body >> 8);
    buf[3] = (uint8_t)(body & 0xFFu);
    return pos;
}
