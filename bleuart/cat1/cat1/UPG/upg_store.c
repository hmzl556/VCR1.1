/**
 * @file upg_store.c
 * @brief 键值下载覆盖层：原始文件 A/B 双扇区存储
 *
 * 写入流程（一次下载只写一个数据类型）：
 *   1. 选非生效份作为目标并整扇区擦除；
 *   2. 把生效份里"非本次类型"的记录逐条压实到目标份；
 *   3. 追加本次记录头，随后由协议层把 payload 流式写入；
 *   4. 收满后写记录尾 CRC，再写目标份扇区头 → 目标份生效。
 *
 * 任何一步断电都至少保留一份完整数据：步骤 2/3 断电时旧份仍有效，
 * 步骤 4 断电时目标份已完整，读取按 seq 取较大者即可。
 *
 * 静态 RAM 0 增长：不缓存整份数据，压实走小分块直读直写。
 */
#include "upg_store.h"
#include "flash_layout.h"
#include "qflash.h"
#include "co_printf.h"
#include <stdint.h>
#include <string.h>

#define UPG_REC_MAGIC   0x52454353u   /* 'RECS' */
#define UPG_SEC_MAGIC   0x53454331u   /* 'SEC1' */
#define UPG_REC_HDR_LEN 12u
#define UPG_SEC_HDR_LEN 16u
#define UPG_REC_CRC_LEN 2u
#define UPG_ALIGN4(x)   (((x) + 3u) & ~3u)

typedef struct {
    uint32_t magic;
    uint16_t seq;
    uint8_t  type;
    uint8_t  flags;
    uint16_t len;
    uint16_t hdr_crc;
} upg_rec_hdr_t;

typedef struct {
    uint32_t magic;
    uint16_t seq;
    uint16_t reserved;
    uint32_t reserved2;
    uint32_t reserved3;
} upg_sec_hdr_t;

uint16_t upg_crc16(const uint8_t *buf, uint32_t len, uint16_t crc_last);   /* upg_meta.c */

static const char *const k_type_name[UPG_ST_NUM] = {
    "fw01", "rel0", "usr1", "irtv", "irstb", "bletv", "blestb"
};

upg_store_type_t upg_store_type_of(const uint8_t *field8)
{
    uint8_t t;

    if (field8 == NULL) {
        return UPG_ST_NONE;
    }
    for (t = 0u; t < (uint8_t)UPG_ST_NUM; ++t) {
        const char *n = k_type_name[t];
        uint8_t nlen = (uint8_t)strlen(n);
        uint8_t i;

        if (nlen == 0u || nlen > 8u || memcmp(field8, n, nlen) != 0) {
            continue;
        }
        /* 不足 8 字节必须补 0 */
        for (i = nlen; i < 8u; ++i) {
            if (field8[i] != 0u) {
                break;
            }
        }
        if (i == 8u) {
            return (upg_store_type_t)t;
        }
    }
    return UPG_ST_NONE;
}

bool upg_store_type_writable(upg_store_type_t t)
{
    /* fw01 是固件包，走既有 UDP OTA；bletv 不实现——电视机只走红外，
     * 本机不与电视机建立任何蓝牙连接（产品范围，见 docs/待客户确认事项.md D1） */
    if (t >= UPG_ST_NUM) {
        return false;
    }
    return (t != UPG_ST_FW01) && (t != UPG_ST_BLETV);
}

static uint32_t sec_base(uint8_t idx)
{
    return (idx == 0u) ? FLASH_KEYSTORE_A_START : FLASH_KEYSTORE_B_START;
}

/** @brief 读扇区头；返回该份是否有效 */
static bool sec_valid(uint8_t idx, uint16_t *seq)
{
    upg_sec_hdr_t h;

    if (Qflash_Read(sec_base(idx), (uint8_t *)&h, sizeof(h)) != FlashOperationSuccess) {
        return false;
    }
    if (h.magic != UPG_SEC_MAGIC) {
        return false;
    }
    if (seq != NULL) {
        *seq = h.seq;
    }
    return true;
}

/** @brief 当前生效份下标；都无效返回 0xFF */
static uint8_t sec_active(void)
{
    uint16_t sa = 0u, sb = 0u;
    bool va = sec_valid(0u, &sa);
    bool vb = sec_valid(1u, &sb);

    if (va && vb) {
        return (sb > sa) ? 1u : 0u;
    }
    if (va) {
        return 0u;
    }
    if (vb) {
        return 1u;
    }
    return 0xFFu;
}

/** @brief 遍历一份扇区的记录 */
typedef void (*rec_visit_fn)(uint32_t rec_addr, const upg_rec_hdr_t *h, void *ctx);

static void scan_records(uint8_t idx, rec_visit_fn cb, void *ctx)
{
    uint32_t off = UPG_SEC_HDR_LEN;
    upg_rec_hdr_t h;

    while ((off + UPG_REC_HDR_LEN) <= UPG_STORE_SEC_SIZE) {
        uint32_t addr = sec_base(idx) + off;

        if (Qflash_Read(addr, (uint8_t *)&h, sizeof(h)) != FlashOperationSuccess) {
            return;
        }
        if (h.magic != UPG_REC_MAGIC) {
            return;
        }
        if (h.hdr_crc != upg_crc16((const uint8_t *)&h, 10u, 0u)) {
            return;
        }
        if (h.len > UPG_STORE_DATA_MAX) {
            return;
        }
        if ((off + UPG_REC_HDR_LEN + h.len + UPG_REC_CRC_LEN) > UPG_STORE_SEC_SIZE) {
            return;
        }
        cb(addr, &h, ctx);
        off = UPG_ALIGN4(off + UPG_REC_HDR_LEN + h.len + UPG_REC_CRC_LEN);
    }
}

/* ---- seq 最大值扫描 ---- */
static void seq_visit(uint32_t rec_addr, const upg_rec_hdr_t *h, void *ctx)
{
    uint16_t *max = (uint16_t *)ctx;

    (void)rec_addr;
    if (h->seq > *max) {
        *max = h->seq;
    }
}

uint16_t upg_store_seq_next(void)
{
    uint16_t max = 0u;
    uint8_t i;

    for (i = 0u; i < 2u; ++i) {
        if (sec_valid(i, NULL)) {
            scan_records(i, seq_visit, &max);
        }
    }
    if (max >= 0xFFFEu) {
        return 1u;      /* 回绕：两份都已到顶时从头来过 */
    }
    return (uint16_t)(max + 1u);
}

/** @brief 按"不跨页"切块读 Flash */
static bool flash_read_span(uint32_t addr, uint8_t *buf, uint32_t len)
{
    while (len > 0u) {
        uint32_t chunk = FLASH_PAGE_SIZE - (addr & (FLASH_PAGE_SIZE - 1u));

        if (chunk > len) {
            chunk = len;
        }
        if (Qflash_Read(addr, buf, chunk) != FlashOperationSuccess) {
            return false;
        }
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
    return true;
}

/** @brief 按"不跨页"切块写 Flash */
static bool flash_write_span(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    while (len > 0u) {
        uint32_t chunk = FLASH_PAGE_SIZE - (addr & (FLASH_PAGE_SIZE - 1u));

        if (chunk > len) {
            chunk = len;
        }
        if (Qflash_Write(addr, (uint8_t *)(uintptr_t)buf, chunk) != FlashOperationSuccess) {
            return false;
        }
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
    return true;
}

/* ---- 压实 ---- */
typedef struct {
    uint32_t dst;          /* 目标扇区内的绝对写地址 */
    upg_store_type_t skip;       /* 本次要覆盖的类型 */
    bool     ok;
} compact_ctx_t;

static void compact_visit(uint32_t rec_addr, const upg_rec_hdr_t *h, void *ctx)
{
    compact_ctx_t *c = (compact_ctx_t *)ctx;
    uint8_t chunk[64];             /* 栈上中转小块：不新增静态 RAM */
    uint32_t total = UPG_REC_HDR_LEN + h->len + UPG_REC_CRC_LEN;
    uint32_t done = 0u;
    uint32_t src = rec_addr;

    if (c->skip != UPG_ST_NONE && (uint8_t)c->skip == h->type) {
        return;                    /* 同类型旧记录不再搬运 */
    }
    while (done < total) {
        uint32_t n = total - done;

        if (n > sizeof(chunk)) {
            n = sizeof(chunk);
        }
        if (!flash_read_span(src + done, chunk, n)
            || !flash_write_span(c->dst + done, chunk, n)) {
            c->ok = false;
            return;
        }
        done += n;
    }
    c->dst = c->dst + UPG_ALIGN4(total);
}

bool upg_store_begin(upg_store_wr_t *w, upg_store_type_t type, uint16_t len, uint32_t *addr_out)
{
    uint8_t act;
    uint8_t dst;
    compact_ctx_t c;
    upg_rec_hdr_t h;

    if (w == NULL || w->busy != 0u || !upg_store_type_writable(type)
        || len == 0u || len > UPG_STORE_DATA_MAX || addr_out == NULL) {
        return false;
    }
    act = sec_active();
    dst = (act == 0u) ? 1u : 0u;

    if (Qflash_Erase_Sector(sec_base(dst)) != FlashOperationSuccess) {
        co_printf("[UPG ] store erase fail\r\n");
        return false;
    }

    /* 压实：把生效份里其它类型的记录搬到目标份，保证新份自洽 */
    c.dst = sec_base(dst) + UPG_SEC_HDR_LEN;
    c.skip = type;
    c.ok = true;
    if (act != 0xFFu) {
        scan_records(act, compact_visit, &c);
    }
    if (!c.ok) {
        co_printf("[UPG ] store compact fail\r\n");
        (void)Qflash_Erase_Sector(sec_base(dst));
        return false;
    }

    w->seq = upg_store_seq_next();
    memset(&h, 0, sizeof(h));
    h.magic = UPG_REC_MAGIC;
    h.seq = w->seq;
    h.type = (uint8_t)type;
    h.flags = 0u;
    h.len = len;
    h.hdr_crc = upg_crc16((const uint8_t *)&h, 10u, 0u);

    if (!flash_write_span(c.dst, (const uint8_t *)&h, sizeof(h))) {
        co_printf("[UPG ] store hdr fail\r\n");
        (void)Qflash_Erase_Sector(sec_base(dst));
        return false;
    }
    w->sec = sec_base(dst);
    w->pay_addr = c.dst + UPG_REC_HDR_LEN;
    w->total = len;
    w->written = 0u;
    w->busy = 1u;
    *addr_out = w->pay_addr;
    return true;
}

bool upg_store_write(upg_store_wr_t *w, const uint8_t *data, uint16_t len)
{
    if (w == NULL || w->busy == 0u || data == NULL
        || (uint32_t)w->written + len > w->total) {
        return false;
    }
    if (!flash_write_span(w->pay_addr + w->written, data, len)) {
        return false;
    }
    w->written = (uint16_t)(w->written + len);
    return true;
}

bool upg_store_commit(upg_store_wr_t *w, uint16_t crc16)
{
    upg_sec_hdr_t sh;
    uint8_t crc_buf[UPG_REC_CRC_LEN];

    if (w == NULL || w->busy == 0u || w->written != w->total) {
        return false;
    }
    crc_buf[0] = (uint8_t)(crc16 >> 8);
    crc_buf[1] = (uint8_t)(crc16 & 0xFFu);
    if (!flash_write_span(w->pay_addr + w->total, crc_buf, sizeof(crc_buf))) {
        return false;
    }

    /* 扇区头最后写：magic 落下才算这一份有效 */
    memset(&sh, 0, sizeof(sh));
    sh.magic = UPG_SEC_MAGIC;
    sh.seq = w->seq;
    if (!flash_write_span(w->sec, (const uint8_t *)&sh, sizeof(sh))) {
        return false;
    }
    w->busy = 0u;
    co_printf("[UPG ] store commit type ok, len=%u\r\n", (unsigned)w->total);
    return true;
}

void upg_store_abort(upg_store_wr_t *w)
{
    if (w == NULL || w->busy == 0u) {
        return;
    }
    (void)Qflash_Erase_Sector(w->sec);   /* 目标份作废，旧份仍生效 */
    w->busy = 0u;
    co_printf("[UPG ] store abort\r\n");
}

/* ---- 读取 ---- */
typedef struct {
    upg_store_type_t want;
    uint16_t   best_seq;
    uint32_t   best_pay;
    uint16_t   best_len;
    uint16_t   best_crc;
    bool       found;
} find_ctx_t;

static void find_visit(uint32_t rec_addr, const upg_rec_hdr_t *h, void *ctx)
{
    find_ctx_t *f = (find_ctx_t *)ctx;

    if ((upg_store_type_t)h->type != f->want) {
        return;
    }
    if (h->len < UPG_REC_CRC_LEN) {
        return;
    }
    if (f->found && h->seq <= f->best_seq) {
        return;
    }
    f->found = true;
    f->best_seq = h->seq;
    f->best_pay = rec_addr + UPG_REC_HDR_LEN;
    f->best_len = h->len;
}

/** @brief 定位某类型 seq 最大的记录；返回 payload 地址与长度 */
static bool find_best(upg_store_type_t type, uint32_t *pay, uint16_t *len, uint16_t *crc)
{
    find_ctx_t f;
    uint8_t i;

    memset(&f, 0, sizeof(f));
    f.want = type;
    for (i = 0u; i < 2u; ++i) {
        if (sec_valid(i, NULL)) {
            scan_records(i, find_visit, &f);
        }
    }
    if (!f.found) {
        return false;
    }
    {
        uint8_t buf[UPG_REC_CRC_LEN];

        if (!flash_read_span(f.best_pay + f.best_len, buf, sizeof(buf))) {
            return false;
        }
        *crc = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    }
    *pay = f.best_pay;
    *len = f.best_len;
    return true;
}

uint16_t upg_store_load(upg_store_type_t type, uint8_t *buf, uint16_t cap)
{
    uint32_t pay;
    uint16_t len;
    uint16_t crc;
    uint16_t calced = 0u;

    if (buf == NULL || cap == 0u || !find_best(type, &pay, &len, &crc)) {
        return 0u;
    }
    if (len > cap) {
        return 0u;
    }
    if (!flash_read_span(pay, buf, len)) {
        return 0u;
    }
    calced = upg_crc16(buf, len, 0u);
    if (calced != crc) {
        co_printf("[UPG ] store crc mismatch %04X/%04X\r\n",
                  (unsigned)crc, (unsigned)calced);
        return 0u;
    }
    return len;
}

bool upg_store_has(upg_store_type_t type)
{
    uint32_t pay;
    uint16_t len;
    uint16_t crc;

    return find_best(type, &pay, &len, &crc);
}

uint16_t upg_store_size(upg_store_type_t type)
{
    uint32_t pay;
    uint16_t len;
    uint16_t crc;

    return find_best(type, &pay, &len, &crc) ? len : 0u;
}

bool upg_store_read_at(upg_store_type_t type, uint16_t off, uint8_t *buf, uint16_t len)
{
    uint32_t pay;
    uint16_t total;
    uint16_t crc;

    if (buf == NULL || len == 0u || !find_best(type, &pay, &total, &crc)) {
        return false;
    }
    if ((uint32_t)off + len > total) {
        return false;
    }
    return flash_read_span(pay + off, buf, len);
}

bool upg_store_erase_all(void)
{
    bool ok = true;

    if (Qflash_Erase_Sector(FLASH_KEYSTORE_A_START) != FlashOperationSuccess) {
        ok = false;
    }
    if (Qflash_Erase_Sector(FLASH_KEYSTORE_B_START) != FlashOperationSuccess) {
        ok = false;
    }
    co_printf("[UPG ] store factory reset %s\r\n", ok ? "ok" : "fail");
    return ok;
}

/* ---------------- 升级标志（独立扇区） ---------------- */
typedef struct {
    uint32_t magic;
    uint32_t flag;
    uint32_t reserved;
    uint16_t crc;          /* 覆盖前 12 字节 */
    uint16_t reserved2;
} upg_flag_rec_t;

uint32_t upg_flag_load(void)
{
    upg_flag_rec_t r;

    if (Qflash_Read(FLASH_UPG_FLAG_START, (uint8_t *)&r, sizeof(r)) != FlashOperationSuccess) {
        return UPG_FLAG_NONE;
    }
    if (r.magic != UPG_FLAG_MAGIC) {
        return UPG_FLAG_NONE;
    }
    if (r.crc != upg_crc16((const uint8_t *)&r, 12u, 0u)) {
        return UPG_FLAG_NONE;
    }
    return r.flag;
}

bool upg_flag_save(uint32_t flag)
{
    upg_flag_rec_t r;

    memset(&r, 0, sizeof(r));
    r.magic = UPG_FLAG_MAGIC;
    r.flag = flag;
    r.crc = upg_crc16((const uint8_t *)&r, 12u, 0u);

    if (Qflash_Erase_Sector(FLASH_UPG_FLAG_START) != FlashOperationSuccess) {
        co_printf("[UPG ] flag erase fail\r\n");
        return false;
    }
    if (!flash_write_span(FLASH_UPG_FLAG_START, (const uint8_t *)&r, sizeof(r))) {
        co_printf("[UPG ] flag write fail\r\n");
        return false;
    }
    co_printf("[UPG ] flag saved 0x%08X\r\n", (unsigned)flag);
    return true;
}
