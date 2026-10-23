#include "app_heap.h"
/* Hardware capture: IRQ only collects; task validates/saves/retries. */
#include "ir_learn.h"
#include "ir_key_map.h"
#include "ir_key_policy.h"
#include "br1601x.h"
#include "rcc.h"
#include "gpio.h"
#include "tim.h"
#include "gmem.h"
#include "udp_ota.h"
#include "cat1_skt_udp.h"
#include "co_printf.h"
#include <stdlib.h>
static ir_capture_t *s_capture;
static volatile bool s_active;
static volatile uint8_t s_error;
static volatile uint32_t s_high;
static uint32_t s_start_ms;
static uint8_t s_target;
static ir_learn_cb_t s_cb;
#define RX_ERR_OVERCAPTURE 1u
#define RX_ERR_BOUNDS     2u
static uint16_t s_retry_count, s_overcapture_count;
static uint8_t s_retry_errors;
/* Task context only: one diagnostic summary per capture attempt, not a
 * blocking UART print for every retry while receiving carrier edges. */
static void report_retries(void)
{
    if (s_retry_count) {
        co_printf("[IR] KEY%u RX retry=%u hw=%u flags=%u\r\n",
                  (unsigned)s_target, (unsigned)s_retry_count,
                  (unsigned)s_overcapture_count, (unsigned)s_retry_errors);
        s_retry_count=0;
    }
}
static void capture_off(void)
{ TIM3->DINTEN=0;NVIC_DisableIRQ(TIM3_IRQn);TIM_Enable(TIM3,DISABLE);s_active=false; }
void ir_learn_discard(void){if(!s_active){app_heap_free(s_capture);s_capture=0;}}
const ir_raw_t *ir_learn_wave(void){return s_capture?&s_capture->wave:0;}
void ir_learn_init(ir_learn_cb_t cb){s_cb=cb;}
bool ir_learn_is_active(void){return s_active;}
uint8_t ir_learn_get_target(void){return s_target;}
void ir_learn_set_target(uint8_t key){s_target=key;}
static void configure(void)
{
    GPIO_InitType g;TIM_TimeBaseInitType tb;TIM_ICInitType ic;
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3,ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB|RCC_APB2_PERIPH_AFIO,ENABLE);
    capture_off();TIM3->CCEN=0;TIM3->CCMOD1=0;
    GPIO_InitStruct(&g);g.Pin=GPIO_PIN_5;g.GPIO_Mode=GPIO_MODE_AF_PP;
    g.GPIO_Alternate=GPIO_AF2_TIM3;g.GPIO_Pull=GPIO_PULL_UP;GPIO_InitPeripheral(GPIOB,&g);
    TIM_InitTimBaseStruct(&tb);tb.Prescaler=63;tb.Period=65535;tb.CntMode=TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM3,&tb);TIM_ConfigArPreload(TIM3,DISABLE);
    TIM_InitIcStruct(&ic);ic.Channel=TIM_CH_2;ic.IcPolarity=TIM_IC_POLARITY_FALLING;
    ic.IcPrescaler=TIM_IC_PSC_DIV1;ic.IcSelection=TIM_IC_SELECTION_DIRECTTI;ic.IcFilter=0;
    TIM_ICInit(TIM3,&ic);NVIC_SetPriority(TIM3_IRQn,3);
}
static void reset_frame(uint32_t now){ir_capture_init(s_capture,now,1);s_error=0;}
bool ir_learn_start(void)
{
    if(s_active || !ir_key_learnable(s_target) || udp_ota_session_busy() || ir_tx_pending() || cat1_modem_is_busy())return false;
    ir_learn_discard();s_capture=app_heap_alloc(sizeof(*s_capture));if(!s_capture)return false;
    configure();s_high=0;TIM_SetCnt(TIM3,0);TIM3->STS=0;
    reset_frame(0);s_retry_count=0;s_overcapture_count=0;s_retry_errors=0;
    s_start_ms=app_now_ms();s_active=true;
    NVIC_ClearPendingIRQ(TIM3_IRQn);TIM3->DINTEN=TIM_INT_UPDATE|TIM_INT_CC2;
    NVIC_EnableIRQ(TIM3_IRQn);TIM_Enable(TIM3,ENABLE);return true;
}
void ir_learn_stop(void)
{
    bool active=s_active;capture_off();if(active)report_retries();ir_learn_discard();
    if(active && s_cb)s_cb(IR_LEARN_EVT_CANCELED,0,0);
}
static uint32_t capture_now(void)
{
    uint32_t hi;uint16_t lo;uint32_t flags;
    NVIC_DisableIRQ(TIM3_IRQn);hi=s_high;lo=TIM3->CNT;flags=TIM3->STS;
    if((flags&TIM_FLAG_UPDATE) && lo<32768u)hi+=65536u;
    NVIC_EnableIRQ(TIM3_IRQn);return hi+lo;
}
static void capture_push(uint32_t width)
{
    if(width<80u || width>65535u || s_capture->wave.count>=IR_RAW_MAX){s_error|=RX_ERR_BOUNDS;return;}
    s_capture->wave.widths[s_capture->wave.count++]=(uint16_t)width;
}
void TIM3_IRQHandler(void)
{
    uint16_t flags=TIM3->STS,cap=TIM3->CCDAT2;uint32_t base=s_high,t,d;
    ir_capture_t *c=s_capture;
    if(!s_active || !c){TIM3->STS=0;return;}
    TIM3->STS=(uint16_t)~flags;
    if(flags&TIM_FLAG_UPDATE){s_high+=65536u;if(cap<32768u)base=s_high;}
    if(flags&TIM_FLAG_CC2OF)s_error|=RX_ERR_OVERCAPTURE;
    if(s_error){TIM3->DINTEN=TIM_INT_UPDATE;return;}
    if(flags&TIM_FLAG_CC2){
        t=base+cap;if(!c->armed){c->quiet_at=t;return;}
        if(c->seen_fall){d=t-c->last_fall;if(d>=10u && d<=64u && c->periods[d]<65535u)++c->periods[d];}
        if(c->mark && t-c->last_fall>100u){c->mark_end=c->last_fall+8u;capture_push(c->mark_end-c->mark_start);c->mark=0;}
        if(!c->mark){if(c->wave.count)capture_push(t-c->mark_end);else c->started=t;c->mark_start=t;c->mark=1;}
        c->last_fall=t;c->seen_fall=1;
        TIM3->CCDAT1=(uint16_t)(cap+100u);TIM3->DINTEN|=TIM_INT_CC1;
    }else if((flags&TIM_FLAG_CC1) && c->mark){
        c->mark_end=c->last_fall+8u;capture_push(c->mark_end-c->mark_start);c->mark=0;TIM3->DINTEN&=~TIM_INT_CC1;
    }
}
void ir_learn_process(void)
{
    uint32_t now;bool complete,failed;int ok;ir_capture_t *c=s_capture;
    if(!s_active || !c)return;
    if(udp_ota_session_busy()){ir_learn_stop();return;}
    if(app_now_ms()-s_start_ms>=15000u){capture_off();report_retries();ir_learn_discard();if(s_cb)s_cb(IR_LEARN_EVT_TIMEOUT,0,0);return;}
    now=capture_now();NVIC_DisableIRQ(TIM3_IRQn);
    if(!c->armed && now-c->quiet_at>=20000u && (GPIOB->PID&GPIO_PIN_5))c->armed=1;
    complete=c->seen_fall && !c->mark && c->wave.count && now-c->last_fall>=20000u;
    failed=s_error || (c->seen_fall && now-c->started>270000u);
    if(complete || failed)TIM3->DINTEN=TIM_INT_UPDATE;
    NVIC_EnableIRQ(TIM3_IRQn);if(!complete && !failed)return;
    ok=!failed && ir_capture_complete(c)==1;
    if(ok){capture_off();report_retries();if(s_cb)s_cb(IR_LEARN_EVT_DONE,0,0);}
    else{
        if(s_retry_count<65535u)++s_retry_count;
        if((s_error&RX_ERR_OVERCAPTURE) && s_overcapture_count<65535u)++s_overcapture_count;
        s_retry_errors|=s_error;
        NVIC_DisableIRQ(TIM3_IRQn);reset_frame(now);s_capture->quiet_at=now;
        TIM3->STS=0;TIM3->DINTEN=TIM_INT_UPDATE|TIM_INT_CC2;NVIC_EnableIRQ(TIM3_IRQn);
    }
}
