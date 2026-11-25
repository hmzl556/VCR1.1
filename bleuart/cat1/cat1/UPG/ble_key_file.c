/**
 * @file ble_key_file.c
 * @brief 蓝牙机顶盒键值（blestb，E.25）解析与查表
 *
 * 按需读 Flash，状态放栈上，零静态 RAM。结构说明见 ble_key_file.h。
 */
#include "ble_key_file.h"
#include "upg_store.h"
#include "ir_dl.h"          /* ir_dl_key_no：物理键 → 表 B.1 序号 */
#include "co_printf.h"
#include <string.h>

#define BLEDL_HDR_LEN       32u                                    /**< name+version+size+crc32+reserved */
#define BLEDL_DEV_TAB_OFF   44u                                    /**< device_nums(4) + version2(8) 之后 */
#define BLEDL_DEV_ENT_LEN   16u
#define BLEDL_ADV_LEN       148u                                   /**< 广播参数区 */
#define BLEDL_HID_MAX       48u                                    /**< fc2hid 固定槽位上限 */
#define BLEDL_HID_ENT_LEN    8u
#define BLEDL_HID_TAB_LEN  (BLEDL_HID_MAX * BLEDL_HID_ENT_LEN)     /**< 384 */
#define BLEDL_NO_MAX        48u
#define BLEDL_NO_ENT_LEN     3u
#define BLEDL_TYPE_STB      0x80000000u                            /**< device_type ≥ 此值为机顶盒 */
#define BLEDL_PAGE_KEYBOARD 0x07u
#define BLEDL_PAGE_CONSUMER 0x0Cu

/** 文件级上下文（栈上，10 字节） */
typedef struct {
    upg_store_type_t type;
    uint16_t len;        /**< 记录总长 */
    uint16_t hid_off;    /**< fc2hid 首个条目偏移（跳过 count 字节） */
    uint16_t no_off;     /**< fc2no 首个条目偏移 */
    uint8_t  hid_num;    /**< fc2hid 有效条目数 */
    uint8_t  no_num;     /**< fc2no 有效条目数 */
} bledl_t;

static uint16_t bl_rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t bl_rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/** @brief 读一段；越界返回 false */
static bool bl_rd(const bledl_t *d, uint16_t off, uint8_t *buf, uint16_t len)
{
    if ((uint32_t)off + len > d->len) {
        return false;
    }
    return upg_store_read_at(d->type, off, buf, len);
}

static bool bl_rd8(const bledl_t *d, uint16_t off, uint8_t *v)
{
    return bl_rd(d, off, v, 1u);
}

/**
 * @brief 打开文件：校验文件头，定位机顶盒设备详情里的两张表
 * @return false：无记录 / 名字不符 / 结构越界 / 没有机顶盒条目
 */
static bool bl_open(bledl_t *d)
{
    uint8_t h[BLEDL_HDR_LEN];
    uint8_t v[8];
    uint32_t nums;
    uint32_t i;
    uint32_t det_off = 0u;
    uint32_t det_size = 0u;
    uint8_t n;
    uint16_t hid_tab_end;

    if (d == NULL) {
        return false;
    }
    memset(d, 0, sizeof(*d));
    d->type = UPG_ST_BLESTB;
    d->len = upg_store_size(UPG_ST_BLESTB);
    if (d->len <= BLEDL_DEV_TAB_OFF) {
        return false;                       /* 没下载过 */
    }
    if (!bl_rd(d, 0u, h, BLEDL_HDR_LEN)) {
        return false;
    }
    /* name(8) 以 "blestb" 开头；版本串不校验（"ble1" 等后续可能变化） */
    if (memcmp(h, "blestb", 6u) != 0) {
        co_printf("[BLEDL] bad name\r\n");
        return false;
    }

    /* device_nums(4) @32 */
    {
        uint8_t b[4];

        if (!bl_rd(d, 32u, b, 4u)) {
            return false;
        }
        nums = bl_rd32(b);
    }
    if (nums == 0u || nums > 8u) {
        co_printf("[BLEDL] bad device_nums %lu\r\n", (unsigned long)nums);
        return false;
    }
    if (!bl_rd(d, 36u, v, 8u)) {
        return false;
    }

    /* 设备索引表：取第一个 device_type ≥ 0x80000000 的条目（机顶盒） */
    for (i = 0u; i < nums; ++i) {
        uint8_t e[BLEDL_DEV_ENT_LEN];
        uint32_t off = BLEDL_DEV_TAB_OFF + i * BLEDL_DEV_ENT_LEN;

        if (!bl_rd(d, (uint16_t)off, e, BLEDL_DEV_ENT_LEN)) {
            return false;
        }
        if (bl_rd32(e) < BLEDL_TYPE_STB) {
            continue;                       /* 电视机条目：跳过（D1 不做电视蓝牙） */
        }
        det_off = bl_rd32(e + 8);
        det_size = bl_rd32(e + 12);
        break;
    }
    if (det_off == 0u || det_size == 0u || det_off >= d->len) {
        co_printf("[BLEDL] no STB device entry\r\n");
        return false;
    }

    /* fc2hid 表位置：详情起点 + 广播参数区 */
    if (!bl_rd8(d, (uint16_t)(det_off + BLEDL_ADV_LEN), &n)) {
        return false;
    }
    if (n == 0u || n > BLEDL_HID_MAX) {
        co_printf("[BLEDL] bad fc2hid count %u\r\n", (unsigned)n);
        return false;
    }
    d->hid_num = n;
    d->hid_off = (uint16_t)(det_off + BLEDL_ADV_LEN + 1u);

    /* ★ fc2hid 固定占 48 槽，不是 n 条——必须整段跳过 */
    hid_tab_end = (uint16_t)(d->hid_off + BLEDL_HID_TAB_LEN);
    if ((uint32_t)hid_tab_end + 1u > d->len) {
        co_printf("[BLEDL] fc2hid table overflow\r\n");
        return false;
    }
    if (!bl_rd8(d, hid_tab_end, &n)) {
        return false;
    }
    if (n == 0u || n > BLEDL_NO_MAX) {
        co_printf("[BLEDL] bad fc2no count %u\r\n", (unsigned)n);
        return false;
    }
    d->no_num = n;
    d->no_off = (uint16_t)(hid_tab_end + 1u);
    if ((uint32_t)d->no_off + (uint32_t)n * BLEDL_NO_ENT_LEN > d->len) {
        co_printf("[BLEDL] fc2no table overflow\r\n");
        return false;
    }

    return true;
}

bool ble_key_file_available(void)
{
    bledl_t d;

    return bl_open(&d);
}

uint16_t ble_key_file_seq(void)
{
    return upg_store_seq_of(UPG_ST_BLESTB);
}

/**
 * @brief 打印整份 blestb 映射（联调期核对用）
 *
 * 关键字序号按表 B.1 的 1~30 逐个查询，把解析结果打到串口，
 * 便于和 APP 下发的原始 753 字节逐条对照。
 * 由 BLEDL_DEBUG_LOG 控制，量产该开关置 0 后本函数不会被调用（链接器回收）。
 */
void ble_key_file_dump(void)
{
    bledl_t d;
    uint8_t k;
    uint8_t hit = 0u;

    if (!bl_open(&d)) {
        co_printf("[BLEDL] no usable blestb record\r\n");
        return;
    }
    co_printf("[BLEDL] %lu bytes, fc2hid=%u fc2no=%u\r\n",
              (unsigned long)d.len, (unsigned)d.hid_num, (unsigned)d.no_num);
    co_printf("[BLEDL] ---- blestb mapping (key_no -> page/mod/usage) ----\r\n");
    for (k = 1u; k <= 30u; ++k) {
        ble_hid_ent_t e;

        if (ble_key_file_lookup(k, &e)) {
            co_printf("[BLEDL] %2u -> %02X/%02X/%04X\r\n",
                      (unsigned)k, (unsigned)e.page,
                      (unsigned)e.mod, (unsigned)e.usage);
            ++hit;
        }
    }
    co_printf("[BLEDL] ---- end (%u/30) ----\r\n", (unsigned)hit);
}

bool ble_key_file_lookup_phys(uint8_t phys_key, ble_hid_ent_t *e)
{
    uint8_t kno = ir_dl_key_no(phys_key);

    if (kno == 0u) {
        return false;
    }
    return ble_key_file_lookup(kno, e);
}

bool ble_key_file_lookup(uint8_t key_no, ble_hid_ent_t *e)
{
    bledl_t d;
    uint16_t fcode = 0u;
    uint8_t i;
    bool found = false;

    if (e == NULL || key_no == 0u) {
        return false;
    }
    memset(e, 0, sizeof(*e));
    if (!bl_open(&d)) {
        return false;
    }

    /* 第一级：表 B.1 序号 → key_fcode2 */
    for (i = 0u; i < d.no_num; ++i) {
        uint8_t rec[BLEDL_NO_ENT_LEN];

        if (!bl_rd(&d, (uint16_t)(d.no_off + (uint16_t)i * BLEDL_NO_ENT_LEN),
                rec, BLEDL_NO_ENT_LEN)) {
            return false;
        }
        if (rec[2] == key_no) {
            fcode = bl_rd16(rec);
            found = true;
            break;
        }
    }
    if (!found) {
#if BLEDL_DEBUG_LOG
        co_printf("[BLEDL] key_no=%u not in fc2no\r\n", (unsigned)key_no);
#endif
        return false;
    }

    /* 第二级：key_fcode → HID 键值 */
    for (i = 0u; i < d.hid_num; ++i) {
        uint8_t rec[BLEDL_HID_ENT_LEN];

        if (!bl_rd(&d, (uint16_t)(d.hid_off + (uint16_t)i * BLEDL_HID_ENT_LEN),
                rec, BLEDL_HID_ENT_LEN)) {
            return false;
        }
        if (bl_rd16(rec) != fcode) {
            continue;
        }
        e->page = rec[2];
        e->mod = rec[3];
        /* 键盘页是两个独立单字节键码，取第一个；消费类页是 2 字节大端 */
        e->usage = (e->page == BLEDL_PAGE_KEYBOARD) ? (uint16_t)rec[4] : bl_rd16(rec + 4u);
        return true;
    }

#if BLEDL_DEBUG_LOG
    co_printf("[BLEDL] key_no=%u fcode=%04X not in fc2hid\r\n",
              (unsigned)key_no, (unsigned)fcode);
#endif
    return false;
}
