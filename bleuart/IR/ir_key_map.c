#include "app_heap.h"
/** @file ir_key_map.c
 * @brief Bounded task-context IR queue: Gehua RC6 STB, NEC TV/learned keys.
 * RC6 repeats resend the SAME frame/toggle. New accepted presses toggle once.
 */
#include "ir_key_map.h"
#include "ir_learn.h"
#include "irc.h"
#include "ir_rc6.h"
#include "ir_rc6_encode.h"
#include "gmem.h"
#include "ir_wave_store.h"
#include <stdlib.h>
#include "co_printf.h"
#include "udp_ota.h"
#define TX_CAPACITY 4u
#define TX_NEC 0u
#define TX_RC6 1u
#define TX_RAW 2u
#define TX_INTERVAL_MS 108u

typedef struct {
    uint8_t user, cmd, key, kind;
    uint32_t frame;
} tx_t;
static tx_t s_queue[TX_CAPACITY];
static uint8_t s_head, s_count, s_repeat_key, s_repeat_kind;
static uint32_t s_repeat_frame;
static bool s_toggle;
/* STB NEC address is retained for the explicit legacy NEC lookup API only.
 * It does not change the Gehua system address 0x26 or protocol. */
static uint8_t s_ir_user_stb = IRC_USER_STB;
static uint8_t s_ir_user_tv = IRC_USER_TV;
static uint32_t s_tx_ms;
static bool s_sent;
static uint32_t s_repeat_interval=108u;
static bool send_learned(uint8_t key,bool repeat,uint32_t frame)
{
    ir_raw_t *w=app_heap_alloc(sizeof(*w));bool ok=false;uint32_t us=0;unsigned i;
    if(!w)return false;
    if(ir_wave_store_get(key,w)){
        if(w->kind==IR_GEHUA_KIND)ok=ir_rc6_send(frame);
        else ok=ir_rc6_send_wave(w,repeat);
        for(i=0;i<w->count;++i)us+=w->widths[i];
        s_repeat_interval=(us+999u)/1000u+20u;if(s_repeat_interval<108u)s_repeat_interval=108u;
    }app_heap_free(w);return ok;
}
bool ir_user_switch_stb(void)
{
    s_ir_user_stb = s_ir_user_stb == IRC_USER_STB ? IRC_USER_STB_ALT : IRC_USER_STB;
    return s_ir_user_stb == IRC_USER_STB;
}
bool ir_user_switch_tv(void)
{
    s_ir_user_tv = s_ir_user_tv == IRC_USER_TV ? IRC_USER_TV_ALT : IRC_USER_TV;
    return s_ir_user_tv == IRC_USER_TV;
}
void ir_user_reset(void)
{
    s_ir_user_stb = IRC_USER_STB; s_ir_user_tv = IRC_USER_TV;
    s_head = s_count = s_repeat_key = 0u;
    /* Do not reset toggle while an already-started frame finishes. */
}
uint8_t ir_user_for_key(uint8_t key)
{ return (key == IR_KEY_TV_BUSINESS || key == IR_KEY_TV_POWER) ? s_ir_user_tv : s_ir_user_stb; }
static bool enqueue(uint8_t user, uint8_t cmd, uint8_t key, uint8_t kind)
{
    uint8_t i;
    if (s_count == TX_CAPACITY || ir_learn_is_active() || udp_ota_session_busy()) return false;
    i = (uint8_t)((s_head + s_count) % TX_CAPACITY);
    s_queue[i].user = user; s_queue[i].cmd = cmd;
    s_queue[i].key = key; s_queue[i].kind = kind;
    s_queue[i].frame = 0u;
    if(kind==TX_RAW){
        uint32_t frame;
        if(ir_wave_store_frame(key,&frame)){s_toggle=!s_toggle;s_queue[i].frame=(frame&~0x8000ul)^(s_toggle?0x8000ul:0u);}
    }
    if (kind == TX_RC6) {
        s_toggle = !s_toggle;
        s_queue[i].frame = ir_rc6_gehua_frame(cmd, s_toggle);
    }
    ++s_count;
    ir_tx_process();
    return true;
}
bool ir_send_key(uint8_t key)
{
    uint8_t cmd;
    s_repeat_key = 0u;
    /* Voice is never an IR/learning key, including stale stored data. */
    if (!key || key > IR_KEY_COUNT || key == 15u) return false;
    if(ir_wave_store_has(key))return enqueue(0u,0u,key,TX_RAW);
    if (key == IR_KEY_TV_POWER)
        return enqueue(s_ir_user_tv, ir_default_cmd(key), key, TX_NEC);
    /* No NEC fallback for unspecified Gehua keys: mute/TV business/replay/
     * voice stay IR-silent unless explicitly learned. BLE is independent. */
    if (!ir_rc6_gehua_command(key, &cmd)) return false;
    return enqueue(0u, cmd, key, TX_RC6);
}
void ir_tx_process(void)
{
    uint32_t now = app_now_ms();
    bool nec_busy = irc_is_busy();
    bool rc6_busy = ir_rc6_is_busy();
    tx_t *item;
    bool started;
    if (udp_ota_session_busy()) { ir_tx_cancel(); return; }
    if (!s_count || nec_busy || rc6_busy || ir_learn_is_active() ||
        (s_sent && now - s_tx_ms < s_repeat_interval)) return;
    item = &s_queue[s_head];
    started = item->kind == TX_RAW ? send_learned(item->key,false,item->frame) :
        (item->kind == TX_RC6 ? ir_rc6_send(item->frame) : irc_send(item->user, item->cmd));
    if(item->kind!=TX_RAW)s_repeat_interval=108u;
    if (started) {
        s_repeat_key = item->key; s_repeat_kind = item->kind;
        s_repeat_frame = item->frame;
        s_head = (uint8_t)((s_head + 1u) % TX_CAPACITY);
        --s_count;
        s_tx_ms = now; s_sent = true;
    } else {
        /* Idle start failure means invalid storage/allocation/configuration,
         * not a live transmitter. Drop it so sleep/learning cannot be wedged
         * forever behind a frame that can never start. */
        s_head=(uint8_t)((s_head+1u)%TX_CAPACITY);--s_count;s_repeat_key=0;
        co_printf("[IR] TX start failed; frame dropped\r\n");
    }
}
void ir_tx_repeat(uint8_t key)
{
    uint32_t now = app_now_ms();
    if (!key || key != s_repeat_key || s_count || ir_learn_is_active() || udp_ota_session_busy() ||
        now - s_tx_ms < s_repeat_interval || irc_is_busy() || ir_rc6_is_busy()) return;
    if(s_repeat_kind==TX_RAW){if(!send_learned(key,true,s_repeat_frame))return;}
    else if (s_repeat_kind == TX_RC6) {
        if (!ir_rc6_send(s_repeat_frame)) return;
    } else irc_send_repeat();
    s_tx_ms = now;
}
void ir_tx_release(void) { s_repeat_key = 0u; }
bool ir_tx_pending(void)
{
    bool nec_busy = irc_is_busy();
    bool rc6_busy = ir_rc6_is_busy();
    return s_count != 0u || nec_busy || rc6_busy;
}
void ir_send_pair_code_target(bool television)
{
    s_count = 0u; s_head = 0u; s_repeat_key = 0u;
    if (television) (void)enqueue(s_ir_user_tv, IRC_CMD_PAIR, 0u, TX_NEC);
    else (void)enqueue(0u, 0xADu, 0u, TX_RC6);
}
void ir_send_pair_code(void) { ir_send_pair_code_target(false); }

void ir_tx_cancel(void)
{
    s_head = s_count = s_repeat_key = 0u;
    ir_rc6_stop();
    irc_stop();
    (void)ir_rc6_is_busy(); /* Free any stopped raw buffer in task context. */
}
