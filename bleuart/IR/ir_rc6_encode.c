/** @file ir_rc6_encode.c
 * @brief Gehua encoding follows the captured/validated 0x801026xx frames.
 * Start/mode=1110, double-width trailer=0, body MSB first; one=MARK/SPACE.
 * Only keys explicitly assigned in Gehua V2.5 section4.4 have fixed codes.
 */
#include "ir_rc6_encode.h"

static void append(uint16_t *out, uint8_t *count, bool mark, uint8_t units)
{
    uint16_t type = mark ? IR_RC6_SEGMENT_MARK : 0u;
    uint16_t width = (uint16_t)(units * IR_RC6_HALF_US);
    if (*count && (out[*count - 1u] & IR_RC6_SEGMENT_MARK) == type)
        out[*count - 1u] += width;
    else out[(*count)++] = type | width;
}

uint8_t ir_rc6_encode(uint32_t frame, uint16_t *out, uint8_t capacity)
{
    uint8_t count = 0u, i;
    bool bit;
    /* Unmerged frame needs at most 77 entries. Require the public fixed cap
     * before writing, so failure never leaves a partially written waveform. */
    if (!out || capacity < IR_RC6_MAX_SEGMENTS) return 0u;
    append(out, &count, true, 6u);
    append(out, &count, false, 2u);
    for (i = 0u; i < 4u; ++i) {
        bit = ((14u >> (3u - i)) & 1u) != 0u;
        append(out, &count, bit, 1u);
        append(out, &count, !bit, 1u);
    }
    append(out, &count, false, 2u);
    append(out, &count, true, 2u);
    for (i = 0u; i < 32u; ++i) {
        bit = ((frame >> (31u - i)) & 1u) != 0u;
        append(out, &count, bit, 1u);
        append(out, &count, !bit, 1u);
    }
    append(out, &count, false, 6u);
    return count;
}

uint32_t ir_rc6_gehua_frame(uint8_t command, bool toggle)
{
    return (0x80102600UL | command) ^ (toggle ? 0x00008000UL : 0u);
}

bool ir_rc6_gehua_command(uint8_t key, uint8_t *command)
{
    /* 新板键号 (SDK KEY_n, 2026-11-17 枚举) 对应的歌华 RC6 命令码;
     * 未定义键返回失败 (电视侧键与新增键不发 RC6) */
    static const int16_t codes[30] = {
        -1,    /*  0 未用 */
        -1,    /*  1 电视机电源 */
        0x0C,  /*  2 机顶盒电源 */
        0x5C,  /*  3 确定 */
        0x21,  /*  4 频道- */
        0x58,  /*  5 上 */
        0x59,  /*  6 下 */
        0x11,  /*  7 音量- */
        0x5A,  /*  8 左 */
        0x5B,  /*  9 右 */
        0x10,  /* 10 音量+ */
        -1,    /* 11 回看 */
        -1,    /* 12 直播业务 */
        0x82,  /* 13 主页 */
        -1,    /* 14 点播 */
        -1,    /* 15 数字1 */
        -1,    /* 16 语音 */
        0x20,  /* 17 频道+ */
        0x54,  /* 18 菜单 */
        0x0A,  /* 19 返回 */
        -1,    /* 20 数字2 */
        -1,    /* 21 数字3 */
        -1,    /* 22 数字4 */
        -1,    /* 23 电视机业务 */
        -1,    /* 24 数字5 */
        -1,    /* 25 数字6 */
        -1,    /* 26 数字7 */
        -1,    /* 27 数字8 */
        -1,    /* 28 数字9 */
        -1,    /* 29 数字0 */
    };
    if (!command || key >= 30u || codes[key] < 0) return false;
    *command = (uint8_t)codes[key];
    return true;
}
