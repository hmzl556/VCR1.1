#ifndef IR_RAW_H
#define IR_RAW_H
#include <stdint.h>
#define IR_RAW_MAX 255u
#define IR_KEY_COUNT 18u
#define IR_RAW_KIND 0u
#define IR_GEHUA_KIND 1u
#define IR_NEC_KIND 2u
enum {IR_RX_OK,IR_RX_WIDTH,IR_RX_OVERFLOW,IR_RX_TIMEOUT,IR_RX_CARRIER_FEW,
      IR_RX_CARRIER_SPREAD,IR_RX_CARRIER_RANGE,IR_RX_FRAME,IR_RX_RC6_DAMAGED};
typedef struct {
    uint16_t count, period_ticks;
    uint32_t kind, frame;
    uint16_t widths[IR_RAW_MAX];
} ir_raw_t;
typedef struct {
    ir_raw_t wave;
    uint16_t periods[65];
    uint32_t started, mark_start, mark_end, last_low, last_fall, quiet_at;
    unsigned armed, mark, previous, seen_fall, failed;
    uint32_t last_poll,max_poll_us,bad_width;
    unsigned error,carrier_peak,carrier_samples,carrier_accepted;
} ir_capture_t;
void ir_capture_init(ir_capture_t *c,uint32_t now,unsigned level);
/* 0 waiting, 1 complete, -1 malformed/overflow. Call without logging. */
int ir_capture_poll(ir_capture_t *c,uint32_t now,unsigned level);
int ir_capture_complete(ir_capture_t *c);
int ir_raw_valid(const ir_raw_t *w);
int ir_raw_decode_gehuarc6(const ir_raw_t *w,uint32_t *frame);
/* 0 decoded, 1 leader, 2 run width, 3 length, 4 prefix, 5 bit pair, 6 vendor. */
unsigned ir_raw_diagnose_rc6(const ir_raw_t *w,uint32_t *frame,unsigned *position,unsigned *halves);
unsigned ir_raw_rc6_header(const ir_raw_t *w);
unsigned ir_raw_rc6_damaged(const ir_raw_t *w);
int ir_raw_is_nec(const ir_raw_t *w);
#endif
