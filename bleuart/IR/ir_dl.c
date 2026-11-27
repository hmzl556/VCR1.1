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
#include "ir_key_map.h"     /* IR_KEY_COUNT */
#include "irc.h"            /* irc_is_busy — 诊断用 */
#include "ir_learn.h"       /* ir_learn_is_active — 诊断用 */
#include "gmem.h"
#include "co_printf.h"
#include "etv.h"
#include "upg_store.h"
#include <string.h>

/**
 * @brief 物理键 → 表 B.1 功能序号
 * @note  与 ir_key_map.h 的 ir_default_cmd_table 同源（那张表按 KEY1..18
 *        给出表 B.1 的红外键值，此处取其序号）。
 *          KEY1 电视机电源 0xDC→1   KEY2 静音 0x9C→16   KEY3 电视 0x88→3
 *          KEY4 确认 0xCE→5         KEY5 左 0x99→10     KEY6 右 0xC1→11
 *          KEY7 下 0xD2→9           KEY8 主页 0x88→4    KEY9 返回 0x95→6
 *          KEY10 菜单 0x82→7        KEY11 频道+ 0x85→18 KEY12 音量+ 0x80→14
 *          KEY13 频道- 0x86→19      KEY14 音量- 0x81→15 KEY15 语音 0xE5→13
 *          KEY16 上 0xCA→8          KEY17 回看 0x37→20  KEY18 机顶盒电源→1
 */
static const uint8_t s_key_no[IR_KEY_COUNT + 1u] =
{
    0u,   /* [0]  未用 */
    1u,   /* [1]  KEY1  电视机电源   */
    2u,   /* [2]  KEY2  直播业务（目标形态取消静音键，见 task_keyscan.h） */
    3u,   /* [3]  KEY3  电视机业务   */
    5u,   /* [4]  KEY4  确认         */
    10u,  /* [5]  KEY5  左           */
    11u,  /* [6]  KEY6  右           */
    9u,   /* [7]  KEY7  下           */
    4u,   /* [8]  KEY8  主页         */
    6u,   /* [9]  KEY9  返回         */
    7u,   /* [10] KEY10 菜单         */
    18u,  /* [11] KEY11 频道+        */
    14u,  /* [12] KEY12 音量+        */
    19u,  /* [13] KEY13 频道-        */
    15u,  /* [14] KEY14 音量-        */
    13u,  /* [15] KEY15 语音         */
    8u,   /* [16] KEY16 上           */
    20u,  /* [17] KEY17 回看         */
    1u,   /* [18] KEY18 机顶盒电源   */
    16u,  /* [19] KEY19 静音（v1.3 原理图丝印误写为"点播"，实物功能为静音） */
    21u,  /* [20] KEY20 数字1        */
    22u,  /* [21] KEY21 数字2        */
    23u,  /* [22] KEY22 数字3        */
    24u,  /* [23] KEY23 数字4        */
    25u,  /* [24] KEY24 数字5        */
    26u,  /* [25] KEY25 数字6        */
    27u,  /* [26] KEY26 数字7        */
    28u,  /* [27] KEY27 数字8        */
    29u,  /* [28] KEY28 数字9        */
    30u,  /* [29] KEY29 数字0        */
};

/**
 * @brief 物理键 → 按键表条目
 * @note  真实链路：物理键 → 表 B.1 序号 → 文件映射表 → key_id → 按键表。
 *        文件未带映射表时，退回把表 B.1 序号直接当 key_id。
 */
static bool dl_find(const etv_file_t *f, uint8_t key, etv_key_t *k)
{
    uint16_t id;

    if (key == 0u || key > IR_KEY_COUNT) {
        return false;
    }
    if (etv_map_lookup(f, s_key_no[key], &id)) {
        return etv_find_key(f, id, k);
    }
    return etv_find_key(f, (uint16_t)s_key_no[key], k);
}

/** @brief 该按键查哪份文件：电视机键查 irtv，其余查 irstb */
static upg_store_type_t file_of(bool tv)
{
    return tv ? UPG_ST_IRTV : UPG_ST_IRSTB;
}

uint8_t ir_dl_key_no(uint8_t key)
{
    if (key == 0u || key > IR_KEY_COUNT) {
        return 0u;
    }
    return s_key_no[key];
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
    return dl_find(&f, key, &k);
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
    if (!dl_find(&f, key, &k)) {
        co_printf("[IR] no entry for key %u (type=%s, map=%u)\r\n",
                  (unsigned)key, tv ? "irtv" : "irstb", (unsigned)f.map_num);
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
            /* 逐项拆开 ir_raw_valid 的判据，定位到底是哪一条不满足 */
            uint32_t total = 0u;
            uint16_t mn = 0xFFFFu;
            uint16_t i;

            for (i = 0u; i < w->count && i < IR_RAW_MAX; ++i) {
                total += w->widths[i];
                if (w->widths[i] < mn) {
                    mn = w->widths[i];
                }
            }
            co_printf("[IR] diag c=%u p=%u k=%lu f=%08lX w0=%u mn=%u tot=%lu | fail c=%d p=%d k=%d mn=%d t=%d\r\n",
                      (unsigned)w->count, (unsigned)w->period_ticks,
                      (unsigned long)w->kind, (unsigned long)w->frame,
                      (unsigned)w->widths[0], (unsigned)mn, (unsigned long)total,
                      (w->count < 7u || w->count > IR_RAW_MAX || !(w->count & 1u)) ? 1 : 0,
                      (w->period_ticks < 1067u || w->period_ticks > 3200u) ? 1 : 0,
                      (w->kind > IR_NEC_KIND) ? 1 : 0,
                      (mn < 80u) ? 1 : 0,
                      (total > 250000u || w->widths[0] < 500u) ? 1 : 0);
        }
    } else {
        co_printf("[IR] encode fail: enc=%u bits=%u col=%u scale=%u\r\n",
                  (unsigned)t.enc, (unsigned)t.data_bit_num,
                  (unsigned)t.col_num, (unsigned)f.scale);
    }
    app_heap_free(w);
    if (ok) {
        co_printf("[IR] download key %u sent (type=%s)\r\n",
                  (unsigned)key, tv ? "irtv" : "irstb");
    }
    return ok;
}
