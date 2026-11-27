/** @file ir_rc6_encode.c
 * @brief Gehua encoding follows the captured/validated 0x801026xx frames.
 * Start/mode=1110, double-width trailer=0, body MSB first; one=MARK/SPACE.
 * Only keys explicitly assigned in Gehua V2.5 section4.4 have fixed codes.
 */
#include "ir_rc6_encode.h"
#include "ir_raw.h"     /* IR_KEY_COUNT：物理键号上限，与按键扫描表保持一致 */

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
    /* 索引 = 物理键号（KEYSCAN_KEY1..29），值 = 歌华 RC6 命令码。
     * 来源：歌华有线新款蓝牙语音遥控器技术规范 V2.5 §4.4（RC-6 MODE 6A、
     * 载波 36KHz、系统码 0100110B=0x26）。数字键 0~9 的码为 0x00~0x09。
     * -1 = 该机型未定义此键，如实不发（语音无红外、静音/回看该规范无此键）。 */
    static const int16_t codes[IR_KEY_COUNT + 1u] = {
        -1,   /* [0]  未用                        */
        -1,   /* [1]  电视电源：歌华无此键         */
        -1,   /* [2]  直播业务：歌华无此键         */
        -1,   /* [3]  电视机业务：歌华无此键       */
        0x5C, /* [4]  确定                         */
        0x5A, /* [5]  左                           */
        0x5B, /* [6]  右                           */
        0x59, /* [7]  下                           */
        0x82, /* [8]  主页                         */
        0x0A, /* [9]  返回（本质为「退出」）       */
        0x54, /* [10] 菜单                         */
        0x20, /* [11] 频道+                        */
        0x10, /* [12] 音量+                        */
        0x21, /* [13] 频道-                        */
        0x11, /* [14] 音量-                        */
        -1,   /* [15] 语音：红外不发（规范为"-"）  */
        0x58, /* [16] 上                           */
        -1,   /* [17] 回看：歌华 V2.5 无此键       */
        0x0C, /* [18] 机顶盒电源（待机/唤醒）      */
        -1,   /* [19] 静音：歌华 V2.5 无此键       */
        0x01, /* [20] 数字1                        */
        0x02, /* [21] 数字2                        */
        0x03, /* [22] 数字3                        */
        0x04, /* [23] 数字4                        */
        0x05, /* [24] 数字5                        */
        0x06, /* [25] 数字6                        */
        0x07, /* [26] 数字7                        */
        0x08, /* [27] 数字8                        */
        0x09, /* [28] 数字9                        */
        0x00, /* [29] 数字0（注意码值是 0x00，非 -1） */
    };
    if (!command || key > IR_KEY_COUNT || key >= (uint8_t)(sizeof(codes) / sizeof(codes[0]))
        || codes[key] < 0) return false;
    *command = (uint8_t)codes[key];
    return true;
}
