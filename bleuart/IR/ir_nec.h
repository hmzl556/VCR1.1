/** @file ir_nec.h @brief Pure NEC envelope validation, shared by receiver and host tests. */
#ifndef IR_NEC_H
#define IR_NEC_H
#include <stdint.h>
#include <stdbool.h>
#define IR_NEC_WIDTHS 67u
static bool ir_nec_range(uint16_t v, uint16_t lo, uint16_t hi)
{ return v >= lo && v <= hi; }
static bool ir_nec_decode(const volatile uint16_t *w, uint8_t count,
                          uint8_t *user, uint8_t *cmd)
{
    uint32_t bits = 0u;
    uint8_t i;
    if (!w || !user || !cmd || count != IR_NEC_WIDTHS ||
        !ir_nec_range(w[0], 7000u, 11000u) ||
        !ir_nec_range(w[1], 3500u, 5500u) ||
        !ir_nec_range(w[66], 350u, 900u)) return false;
    for (i = 0u; i < 32u; ++i) {
        if (!ir_nec_range(w[2u+2u*i], 350u, 900u)) return false;
        if (ir_nec_range(w[3u+2u*i], 1250u, 2100u)) bits |= 1UL << i;
        else if (!ir_nec_range(w[3u+2u*i], 350u, 900u)) return false;
    }
    if (((uint8_t)bits ^ (uint8_t)(bits >> 8)) != 0xFFu ||
        ((uint8_t)(bits >> 16) ^ (uint8_t)(bits >> 24)) != 0xFFu) return false;
    *user = (uint8_t)bits;
    *cmd = (uint8_t)(bits >> 16);
    return true;
}
#endif
