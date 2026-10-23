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
    static const int16_t codes[19] = {
        -1, -1, -1, -1, 0x5C, 0x5A, 0x5B, 0x59, 0x82, 0x0A,
        0x54, 0x20, 0x10, 0x21, 0x11, -1, 0x58, -1, 0x0C
    };
    if (!command || key >= 19u || codes[key] < 0) return false;
    *command = (uint8_t)codes[key];
    return true;
}
