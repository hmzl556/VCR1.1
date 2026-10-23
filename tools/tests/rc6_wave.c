/* RC6-6A-32, approximately 447 us half-bits at 38 kHz (17 carrier cycles).
 * Header start/mode = 1110; double-width trailer bit = 0.
 * Body bits are MSB first, MARK/SPACE=1 and SPACE/MARK=0.
 * Kept independent of MCU headers so host tests exercise the actual encoder.
 */
#include "rc6_wave.h"
static unsigned put(uint32_t *out, unsigned count, unsigned cap, unsigned mark, unsigned units)
{
    uint32_t type=mark ? (1ul<<14) : 0ul;
    uint32_t duration=(uint32_t)units*RC6_HALF_CYCLES;
    if (count && (out[count-1]&(1ul<<14))==type) {
        out[count-1]+=duration; return count;
    }
    if (count>=cap) return 0u;
    out[count]=type|duration; return count+1u;
}
unsigned rc6_build(uint32_t frame, uint32_t *out, unsigned cap)
{
    unsigned n=0, i, bit;
    if (!out || cap<80u) return 0;
    n=put(out,n,cap,1,6); n=put(out,n,cap,0,2);
    for (i=0;i<4u;++i) {
        bit=(14u>>(3u-i))&1u;
        n=put(out,n,cap,bit,1); n=put(out,n,cap,!bit,1);
    }
    n=put(out,n,cap,0,2); n=put(out,n,cap,1,2);
    for (i=0;i<32u;++i) {
        bit=(frame>>(31u-i))&1u;
        n=put(out,n,cap,bit,1); n=put(out,n,cap,!bit,1);
    }
    n=put(out,n,cap,0,6);
    return n;
}
