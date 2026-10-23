#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ir_rc6_encode.c"

static void check_frame(uint32_t frame)
{
    uint16_t segments[IR_RC6_MAX_SEGMENTS];
    uint8_t levels[90];
    unsigned count, i, j, n = 0u;
    uint32_t decoded = 0u;
    count = ir_rc6_encode(frame, segments, IR_RC6_MAX_SEGMENTS);
    assert(count && count <= 77u);
    for (i = 0u; i < count; ++i) {
        unsigned width = segments[i] & IR_RC6_SEGMENT_US;
        assert(width && width % IR_RC6_HALF_US == 0u);
        if (i) assert((segments[i] ^ segments[i-1]) & IR_RC6_SEGMENT_MARK);
        for (j = 0u; j < width / IR_RC6_HALF_US; ++j) {
            assert(n < sizeof(levels));
            levels[n++] = !!(segments[i] & IR_RC6_SEGMENT_MARK);
        }
    }
    assert(n == 90u); /* 39960us including final idle. */
    for (i = 0u; i < 6u; ++i) assert(levels[i] == 1u);
    assert(!levels[6] && !levels[7]);
    for (i = 0u; i < 4u; ++i) {
        unsigned bit = (14u >> (3u-i)) & 1u;
        assert(levels[8u+2u*i] == bit && levels[9u+2u*i] != bit);
    }
    assert(!levels[16] && !levels[17] && levels[18] && levels[19]);
    for (i = 0u; i < 32u; ++i) {
        assert(levels[20u+2u*i] != levels[21u+2u*i]);
        decoded = (decoded << 1) | levels[20u+2u*i];
    }
    assert(decoded == frame);
    for (i = 84u; i < 90u; ++i) assert(!levels[i]);
}

int main(void)
{
    uint16_t out[80], before[80];
    unsigned cmd, toggle, i, mapped = 0u;
    uint8_t code;
    memset(out, 0xA5, sizeof(out)); memcpy(before, out, sizeof(out));
    assert(!ir_rc6_encode(0u, NULL, 80u));
    assert(!ir_rc6_encode(0u, out, 79u));
    assert(!memcmp(out, before, sizeof(out)));
    for (cmd = 0u; cmd < 256u; ++cmd)
        for (toggle = 0u; toggle < 2u; ++toggle)
            check_frame(ir_rc6_gehua_frame((uint8_t)cmd, toggle != 0u));
    for (i = 0u; i < 19u; ++i)
        if (ir_rc6_gehua_command((uint8_t)i, &code)) ++mapped;
    assert(mapped == 13u);
    assert(ir_rc6_gehua_command(18u, &code) && code == 0x0C);
    assert(!ir_rc6_gehua_command(1u, &code));
    assert(!ir_rc6_gehua_command(2u, &code));
    assert(!ir_rc6_gehua_command(3u, &code));
    assert(!ir_rc6_gehua_command(15u, &code));
    assert(!ir_rc6_gehua_command(17u, &code));
    assert(!ir_rc6_gehua_command(255u, &code));
    assert(!ir_rc6_gehua_command(4u, NULL));
    puts("RC6 encoder: 512 command/toggle round trips, header, trailer, 39960us, 13 mapped keys, unmapped rejection PASS");
    return 0;
}
