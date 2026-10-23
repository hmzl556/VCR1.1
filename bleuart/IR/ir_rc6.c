#include "app_heap.h"
/** @file ir_rc6.c
 * @brief PWM carrier plus interrupt-driven RC6 envelope; no task blocking.
 * Uses the validated diagnostic's 36kHz, 1/3 duty, 444us half-bit waveform.
 * TIM6 reloads run without resetting CNT on each ISR, so ISR entry latency
 * does not accumulate across the frame. Late service aborts the current TX.
 */
#include "ir_rc6.h"
#include "ir_rc6_encode.h"
#include "ir_learn.h"
#include "irc.h"
#include "br1601x.h"
#include "rcc.h"
#include "tim.h"
#include "gmem.h"
#include <stdlib.h>
#include <string.h>

#define CARRIER_PERIOD 1778u /* 64MHz / 1778 = 35995.5Hz */
#define CARRIER_HIGH    593u
#define MAX_ISR_LATE_US 100u
#define FRAME_TIMEOUT_MS 80u

static uint16_t s_segments[IR_RC6_MAX_SEGMENTS];
static uint16_t s_length;
static ir_raw_t *s_wave;
static uint16_t s_carrier_high = CARRIER_HIGH;
static uint16_t s_carrier_period = CARRIER_PERIOD;
static uint32_t s_timeout_ms = FRAME_TIMEOUT_MS;
static volatile uint16_t s_index;
static volatile bool s_active;
static uint32_t s_start_ms;
static volatile uint32_t s_errors;

void ir_rc6_stop(void)
{
    if (!s_active) return;
    TIM6->DINTEN = 0u;
    NVIC_DisableIRQ(TIM6_IRQn);
    TIM_Enable(TIM6, DISABLE);
    TIM6->STS = 0u;
    NVIC_ClearPendingIRQ(TIM6_IRQn);
    TIM3->CCDAT1 = 0u;
    TIM_Enable(TIM3, DISABLE);
    /* Return PB4 AF2 to NEC IRC, whose frequency/timing registers survive. */
    IRC->CTRL = IR_CTRL_ENABLE_Msk;
    s_active = false;
}

bool ir_rc6_is_busy(void)
{
    if (s_active && app_now_ms() - s_start_ms > s_timeout_ms) {
        ++s_errors;
        ir_rc6_stop();
    }
    /* Heap ownership stays in task context, never free from TIM6 ISR. */
    if (!s_active && s_wave) { app_heap_free(s_wave); s_wave=0; }
    return s_active;
}

uint32_t ir_rc6_error_count(void) { return s_errors; }

static bool start_wave(void)
{
    TIM_TimeBaseInitType tb;
    OCInitType oc;
    if (s_active || ir_learn_is_active() || irc_is_busy()) return false;


    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3 | RCC_APB1_PERIPH_TIM6, ENABLE);
    NVIC_DisableIRQ(TIM3_IRQn);
    NVIC_DisableIRQ(TIM6_IRQn);
    TIM3->DINTEN = 0u;
    TIM6->DINTEN = 0u;
    TIM_Enable(TIM3, DISABLE);
    TIM_Enable(TIM6, DISABLE);
    TIM3->CCEN = 0u;
    TIM3->CCMOD1 = 0u;
    /* IRC_ENABLE selects IRC versus TIM3_CH1 on the same PB4 AF2 pin. */
    IRC->CTRL = 0u;

    TIM_InitTimBaseStruct(&tb);
    tb.Prescaler = 0u;
    tb.Period = s_carrier_period - 1u;
    tb.CntMode = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM3, &tb);
    TIM_InitOcStruct(&oc);
    oc.OcMode = TIM_OCMODE_PWM1;
    oc.OutputState = TIM_OUTPUT_STATE_ENABLE;
    oc.OcPolarity = TIM_OC_POLARITY_HIGH;
    oc.Pulse = 0u;
    TIM_InitOc1(TIM3, &oc);
    TIM_EnableOc1Preload(TIM3, TIM_OC_PRE_LOAD_DISABLE);
    TIM3->STS = 0u;
    TIM_SetCnt(TIM3, 0u);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    TIM_Enable(TIM3, ENABLE);

    TIM_InitTimBaseStruct(&tb);
    tb.Prescaler = 63u;
    tb.Period = (s_wave ? s_wave->widths[0] : (s_segments[0] & IR_RC6_SEGMENT_US)) - 1u;
    tb.CntMode = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM6, &tb);
    TIM_ConfigArPreload(TIM6, DISABLE);
    TIM_SetCnt(TIM6, 0u);
    TIM6->STS = 0u;
    NVIC_ClearPendingIRQ(TIM6_IRQn);
    NVIC_SetPriority(TIM6_IRQn, 2u); /* BLE and CAT1 UART keep higher priority. */
    s_index = 0u;
    s_start_ms = app_now_ms();
    s_active = true;
    TIM3->CCDAT1 = s_carrier_high; /* First segment is the 6T leader MARK. */
    TIM6->DINTEN = TIM_INT_UPDATE;
    NVIC_EnableIRQ(TIM6_IRQn);
    TIM_Enable(TIM6, ENABLE);
    return true;
}

void TIM6_IRQHandler(void)
{
    uint16_t segment,width; bool mark;
    if (!(TIM6->STS & TIM_FLAG_UPDATE)) return;
    TIM6->STS = (uint16_t)~TIM_FLAG_UPDATE;
    if (!s_active) { TIM6->DINTEN = 0u; return; }
    if (TIM6->CNT > MAX_ISR_LATE_US) {
        ++s_errors;
        ir_rc6_stop();
        return;
    }
    ++s_index;
    if (s_index >= s_length) { ir_rc6_stop(); return; }
    segment = s_wave ? s_wave->widths[s_index] : s_segments[s_index];
    width = s_wave ? segment : (segment & IR_RC6_SEGMENT_US);
    mark = s_wave ? !(s_index & 1u) : (segment & IR_RC6_SEGMENT_MARK)!=0;
    if(TIM6->CNT>=width){++s_errors;ir_rc6_stop();return;}
    TIM3->CCDAT1 = mark ? s_carrier_high : 0u;
    TIM6->AR = width - 1u;
}

bool ir_rc6_send(uint32_t frame)
{
    if(ir_rc6_is_busy() || ir_learn_is_active() || irc_is_busy())return false;
    s_length=ir_rc6_encode(frame,s_segments,IR_RC6_MAX_SEGMENTS);
    s_carrier_high=CARRIER_HIGH;s_carrier_period=CARRIER_PERIOD;s_timeout_ms=FRAME_TIMEOUT_MS;
    return s_length && start_wave();
}
bool ir_rc6_send_wave(const ir_raw_t *wave,bool repeat)
{
    uint32_t duration=0;unsigned i;
    if(ir_rc6_is_busy() || ir_learn_is_active() || irc_is_busy() || !wave || !ir_raw_valid(wave))return false;
    s_wave=app_heap_alloc(sizeof(*s_wave));if(!s_wave)return false;*s_wave=*wave;
    if(repeat && wave->kind==IR_NEC_KIND){s_wave->count=3;s_wave->widths[0]=9000;s_wave->widths[1]=2250;s_wave->widths[2]=562;}
    s_length=s_wave->count;s_carrier_period=s_wave->period_ticks;s_carrier_high=(s_carrier_period+1u)/3u;
    for(i=0;i<s_length;++i)duration+=s_wave->widths[i];
    s_timeout_ms=(duration+999u)/1000u+30u;
    if(start_wave())return true;
    app_heap_free(s_wave);s_wave=0;return false;
}
