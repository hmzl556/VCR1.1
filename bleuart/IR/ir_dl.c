/**
 * @file ir_dl.c
 * @brief 红外「下载层」：用 APP 下发的 ETV 键值发码
 *
 * 数据流：upg_store（Flash 覆盖层）→ etv 解析 → etv_encode 波形 → ir_rc6 发送
 * 全程按需读 Flash，ir_raw_t 从堆上临时申请（与学习值发送同一套路），零静态 RAM。
 */
#include "ir_dl.h"
#include "app_heap.h"       /* app_heap_alloc / app_heap_free */
#include "ir_raw.h"
#include "ir_rc6.h"
#include "gmem.h"
#include "co_printf.h"
#include "etv.h"
#include "upg_store.h"
#include <string.h>

/**
 * @brief 物理键 → 表 B.1 功能序号
 * @note  占位实现：暂按物理键号一一对应。表 B.1 的真实序号需客户确认
 *        （见 docs/待客户确认事项.md 的 Q7），确认后只改这一张表。
 */
static uint16_t keyid_of(uint8_t key)
{
    return (uint16_t)key;
}

/** @brief 该按键查哪份文件：电视机键查 irtv，其余查 irstb */
static upg_store_type_t file_of(bool tv)
{
    return tv ? UPG_ST_IRTV : UPG_ST_IRSTB;
}

bool ir_dl_available(uint8_t key, bool tv)
{
    etv_file_t f;
    etv_key_t k;

    if (key == 0u) {
        return false;
    }
    if (!etv_open(&f, file_of(tv))) {
        return false;
    }
    return etv_find_key(&f, keyid_of(key), &k);
}

uint16_t ir_dl_seq(bool tv)
{
    return upg_store_seq_of(file_of(tv));
}

bool ir_dl_send(uint8_t key, bool tv)
{
    etv_file_t f;
    etv_table_t t;
    etv_key_t k;
    ir_raw_t *w;
    uint16_t period;
    bool ok = false;

    if (key == 0u || !etv_open(&f, file_of(tv))) {
        return false;
    }
    if (!etv_find_key(&f, keyid_of(key), &k)) {
        return false;
    }
    if (!etv_read_table(&f, k.table_index, &t)) {
        return false;
    }
    period = etv_period_ticks(etv_carrier_hz(f.ratio_freq));
    if (period < 1067u || period > 3200u) {
        period = 1684u;              /* 载波未知/越界时按 38 kHz 兜底 */
    }

    w = app_heap_alloc(sizeof(*w));
    if (w == NULL) {
        return false;
    }
    memset(w, 0, sizeof(*w));
    if (etv_encode(&t, &k, f.scale, w->widths, IR_RAW_MAX, &w->count)) {
        w->period_ticks = period;
        w->kind = IR_RAW_KIND;
        w->frame = 0u;
        ok = ir_rc6_send_wave(w, false);
        if (!ok) {
            co_printf("[IR] download frame rejected (kind=%u count=%u period=%u)\r\n",
                      (unsigned)w->kind, (unsigned)w->count, (unsigned)w->period_ticks);
        }
    }
    app_heap_free(w);
    if (ok) {
        co_printf("[IR] download key %u sent (type=%s)\r\n",
                  (unsigned)key, tv ? "irtv" : "irstb");
    }
    return ok;
}
