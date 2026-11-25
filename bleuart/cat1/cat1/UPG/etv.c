/**
 * @file etv.c
 * @brief ETV（红外遥控基础信息文件）解析骨架，GY/T 407-2024 表 E.24
 *
 * 全部数据按需从覆盖层 Flash 读取，不整块载入；无静态 RAM。
 * 结构遍历只看字段本身，**不用 file_size 判断结尾**（附录 I 样本与结构累计值不一致，
 * 已在 docs/待客户确认事项.md 的 Q2 记录）。
 */
#include "etv.h"
#include "co_printf.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/** @brief 读一段；越界返回 false */
static bool rd(const etv_file_t *f, uint16_t off, uint8_t *buf, uint16_t len)
{
    if ((uint32_t)off + len > f->len) {
        return false;
    }
    return upg_store_read_at(f->type, off, buf, len);
}

/** @brief 读 1 字节 */
static bool rd8(const etv_file_t *f, uint16_t off, uint8_t *v)
{
    return rd(f, off, v, 1u);
}

/** @brief 由协议表头部（enc..col_num 共 8 字节）算出整表长度 */
static uint16_t table_len(uint8_t col_num)
{
    return (uint16_t)(ETV_TABLE_FIXED + (uint16_t)col_num * ETV_COL_LEN);
}

bool etv_open(etv_file_t *f, upg_store_type_t type)
{
    uint8_t h[ETV_HDR_LEN];
    uint16_t off;
    uint8_t i;

    if (f == NULL) {
        return false;
    }
    memset(f, 0, sizeof(*f));
    f->type = type;
    f->len = upg_store_size(type);
    if (f->len < ETV_HDR_LEN) {
        return false;         /* 没有下载记录 */
    }
    if (!rd(f, 0u, h, ETV_HDR_LEN)) {
        return false;
    }
    /* tag = "ETV\0" */
    if (h[0] != 'E' || h[1] != 'T' || h[2] != 'V' || h[3] != 0u) {
        co_printf("[ETV ] bad tag\r\n");
        return false;
    }
    /* 文件头偏移（表 E.24）：tag4 + spec2 + reserved6 + file_version4 + file_size4 = 20 */
    f->ratio_freq = h[20];
    f->scale = h[22];
    f->table_num = h[23];
    f->key_num = rd16(h + 24);
    f->repeat_mode = h[26];
    f->table_off = ETV_HDR_LEN;

    /* 顺序跳过协议表，得到按键表偏移 */
    off = f->table_off;
    for (i = 0u; i < f->table_num; ++i) {
        uint8_t col;
        uint16_t tlen;

        if (!rd8(f, (uint16_t)(off + 7u), &col)) {
            return false;
        }
        if (col == 0u || col > 8u) {
            co_printf("[ETV ] bad col_num %u\r\n", (unsigned)col);
            return false;
        }
        tlen = table_len(col);
        if ((uint32_t)off + tlen > f->len) {
            return false;
        }
        off = (uint16_t)(off + tlen);
    }
    f->key_off = off;

    if ((uint32_t)f->key_off + (uint32_t)f->key_num * ETV_KEY_LEN > f->len) {
        co_printf("[ETV ] key table overflow (%u keys)\r\n", (unsigned)f->key_num);
        return false;
    }
    return true;
}

bool etv_read_table(const etv_file_t *f, uint8_t index, etv_table_t *t)
{
    uint8_t b[ETV_TABLE_FIXED];
    uint16_t off;
    uint8_t i;

    if (f == NULL || t == NULL || index >= f->table_num) {
        return false;
    }
    off = f->table_off;
    for (i = 0u; i < index; ++i) {
        uint8_t col;

        if (!rd8(f, (uint16_t)(off + 7u), &col) || col == 0u || col > 8u) {
            return false;
        }
        off = (uint16_t)(off + table_len(col));
    }
    if (!rd(f, off, b, ETV_TABLE_FIXED)) {
        return false;
    }

    memset(t, 0, sizeof(*t));
    t->enc = b[0];
    t->unit = rd16(b + 1);
    t->has_start = b[3];
    t->has_toggle_bit = b[4];
    t->data_bit_num = b[5];
    t->header2_pos = b[6];
    t->col_num = b[7];
    t->b0[0] = rd16(b + 8);
    t->b0[1] = rd16(b + 10);
    t->b1[0] = rd16(b + 12);
    t->b1[1] = rd16(b + 14);
    t->b2[0] = rd16(b + 16);
    t->b2[1] = rd16(b + 18);
    t->b3[0] = rd16(b + 20);
    t->b3[1] = rd16(b + 22);
    t->header[0] = rd16(b + 24);
    t->header[1] = rd16(b + 26);
    t->header2[0] = rd16(b + 28);
    t->header2[1] = rd16(b + 30);

    if (t->col_num == 0u || t->col_num > 8u) {
        return false;
    }
    /* 当前只处理单列：多列语义（ref_col_index 如何选列）需真实文件确认 */
    if (t->col_num != 1u) {
        co_printf("[ETV ] multi-column (%u) not supported yet\r\n", (unsigned)t->col_num);
        return false;
    }
    {
        uint8_t c[ETV_COL_LEN];

        if (!rd(f, (uint16_t)(off + ETV_TABLE_FIXED), c, ETV_COL_LEN)) {
            return false;
        }
        t->bit_num = c[0];
        t->ref_col_index = c[1];
        t->has_header = c[2];
        t->burst[0] = rd16(c + 3);
        t->burst[1] = rd16(c + 5);
    }
    return true;
}

bool etv_find_key(const etv_file_t *f, uint16_t key_id, etv_key_t *k)
{
    uint16_t i;

    if (f == NULL || k == NULL) {
        return false;
    }
    for (i = 0u; i < f->key_num; ++i) {
        uint8_t rec[ETV_KEY_LEN];
        uint16_t off = (uint16_t)(f->key_off + (uint16_t)i * ETV_KEY_LEN);

        if (!rd(f, off, rec, ETV_KEY_LEN)) {
            return false;
        }
        if (rd16(rec) != key_id) {
            continue;
        }
        k->table_index = rec[2];
        memcpy(k->value, rec + 3, 8u);
        return true;
    }
    return false;
}

uint32_t etv_carrier_hz(uint8_t ratio_freq)
{
    switch ((ratio_freq >> 4) & 0x0Fu) {
    case 1u: return 36000u;
    case 2u: return 37000u;
    case 3u: return 38000u;
    case 4u: return 39000u;
    case 5u: return 40000u;
    case 6u: return 56000u;
    default: return 0u;
    }
}

uint8_t etv_duty_percent(uint8_t ratio_freq)
{
    switch (ratio_freq & 0x0Fu) {
    case 1u: return 50u;
    case 2u: return 33u;
    case 3u: return 25u;
    case 4u: return 20u;
    default: return 0u;
    }
}

uint16_t etv_period_ticks(uint32_t carrier_hz)
{
    if (carrier_hz == 0u) {
        return 0u;
    }
    /* 定时器 64 MHz：38 kHz → 1684 tick，与学习通路实测一致 */
    return (uint16_t)((64000000ul + carrier_hz / 2ul) / carrier_hz);
}

/* ---------------- 波形生成 ---------------- */

typedef struct {
    uint16_t *w;
    uint16_t  cap;
    uint16_t  n;
    uint8_t   level;      /**< 上一条的电平：1=mark 0=space；0xFF 表示还没写过 */
    bool      ok;
} wave_buf_t;

/** @brief 追加一段；与上一段同电平时合并（表 I.7 的相邻同电平合并规则） */
static void wpush(wave_buf_t *b, uint8_t level, uint32_t dur)
{
    if (!b->ok) {
        return;
    }
    if (dur == 0u) {
        return;
    }
    if (dur > 0xFFFFu) {
        dur = 0xFFFFu;
    }
    if (b->level == level && b->n > 0u) {
        uint32_t sum = (uint32_t)b->w[b->n - 1u] + dur;

        b->w[b->n - 1u] = (uint16_t)((sum > 0xFFFFu) ? 0xFFFFu : sum);
        return;
    }
    if (b->n >= b->cap) {
        b->ok = false;
        return;
    }
    b->w[b->n++] = (uint16_t)dur;
    b->level = level;
}

/** @brief 取 key_value 的第 bit 位（低字节低位先发） */
static uint8_t bit_of(const uint8_t value[8], uint16_t bit)
{
    uint8_t byte = value[(bit >> 3) & 7u];

    return (uint8_t)((byte >> (bit & 7u)) & 1u);
}

/** @brief PW 编码：按 b0..b3 输出 */
static bool encode_pw(const etv_table_t *t, const etv_key_t *k, uint8_t scale,
                      wave_buf_t *b, uint16_t bits)
{
    bool two_bit = (t->b2[0] | t->b2[1] | t->b3[0] | t->b3[1]) != 0u;
    uint16_t i;

    for (i = 0u; i < bits; ) {
        const uint16_t *pair;

        if (two_bit) {
            uint8_t v = (uint8_t)(bit_of(k->value, i) | (bit_of(k->value, (uint16_t)(i + 1u)) << 1));

            switch (v) {
            case 0u:  pair = t->b0; break;
            case 1u:  pair = t->b1; break;
            case 2u:  pair = t->b2; break;
            default:  pair = t->b3; break;
            }
            i = (uint16_t)(i + 2u);
        } else {
            pair = bit_of(k->value, i) ? t->b1 : t->b0;
            i = (uint16_t)(i + 1u);
        }
        wpush(b, 1u, (uint32_t)pair[0] * scale);
        wpush(b, 0u, (uint32_t)pair[1] * scale);
    }
    return b->ok;
}

/** @brief BP 编码：1→1P1S，0→0S0P；toggle 位翻倍 */
static bool encode_bp(const etv_table_t *t, const etv_key_t *k, uint8_t scale,
                      wave_buf_t *b, uint16_t bits)
{
    uint16_t i;

    if (t->has_start != 0u) {
        wpush(b, 1u, (uint32_t)t->unit * scale);
    }
    for (i = 0u; i < bits; ++i) {
        uint32_t d = (uint32_t)t->unit * scale;

        /* has_toggle_bit：第 4 位（0 基）时长翻倍，等同 RC6 的 trailer */
        if (t->has_toggle_bit != 0u && i == 4u) {
            d *= 2u;
        }
        if (bit_of(k->value, i) != 0u) {
            wpush(b, 1u, d);      /* 1P */
            wpush(b, 0u, d);      /* 1S */
        } else {
            wpush(b, 0u, d);      /* 0S */
            wpush(b, 1u, d);      /* 0P */
        }
    }
    return b->ok;
}

bool etv_encode(const etv_table_t *t, const etv_key_t *k, uint8_t scale,
                uint16_t *w, uint16_t cap, uint16_t *count)
{
    wave_buf_t b;
    uint16_t bits;

    if (t == NULL || k == NULL || w == NULL || count == NULL || cap < 7u) {
        return false;
    }
    if (scale == 0u || t->data_bit_num == 0u || t->data_bit_num > 64u) {
        return false;
    }
    bits = t->data_bit_num;
    memset(&b, 0, sizeof(b));
    b.w = w;
    b.cap = cap;
    b.level = 0xFFu;
    b.ok = true;

    /* 引导码 */
    if (t->has_header != 0u && (t->header[0] != 0u || t->header[1] != 0u)) {
        wpush(&b, 1u, (uint32_t)t->header[0] * scale);
        wpush(&b, 0u, (uint32_t)t->header[1] * scale);
    }

    if (t->enc == 1u) {
        (void)encode_pw(t, k, scale, &b, bits);
    } else if (t->enc == 2u) {
        (void)encode_bp(t, k, scale, &b, bits);
    } else {
        return false;
    }

    /* 结束码：burst[0]/burst[1] 都为 0 时无效。
     * 只输出 burst[0]（高电平），**不输出 burst[1]**：
     *   - ir_raw_valid 要求 count 为奇数，即最后一段必须是 mark；
     *     结尾的 space 由发送器停止后的空闲天然形成，与学习通路一致。
     *   - 规范"burst[0]>0、burst[1]=0 为低电平"这种纯低电平结束码本来
     *     也不需要额外发码（引脚本来就是低），直接忽略。 */
    if (t->burst[0] != 0u) {
        wpush(&b, 1u, (uint32_t)t->burst[0] * scale);
    }

    if (!b.ok || b.n < 7u) {
        return false;
    }
    /* 必须停在 mark 上（count 奇数）；偶数说明结构异常，宁可不发 */
    if ((b.n & 1u) == 0u) {
        co_printf("[ETV ] encode: count %u is even, refuse\r\n", (unsigned)b.n);
        return false;
    }
    *count = b.n;
    return true;
}
