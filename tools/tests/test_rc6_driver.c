#include "mock_hw.h"
#include "ir_rc6_encode.c"
#include "ir_raw.c"
#include "ir_rc6.c"
static bool learning, nec_busy;
bool ir_learn_is_active(void) { return learning; }
bool irc_is_busy(void) { return nec_busy; }
static void finish_frame(uint32_t frame)
{
    uint16_t expected[80];
    unsigned n=ir_rc6_encode(frame,expected,80u),i;
    assert(ir_rc6_send(frame));
    assert(s_active && TIM6->CTRL1 && TIM6->DINTEN==TIM_INT_UPDATE);
    assert(TIM3->PSC==0u && TIM3->AR==1777u && IRC->CTRL==0u);
    assert(!ir_rc6_send(frame));
    for(i=0;i<n;++i) {
        assert(s_index==i);
        assert(TIM6->AR==(unsigned)(expected[i]&IR_RC6_SEGMENT_US)-1u);
        assert(TIM3->CCDAT1==((expected[i]&IR_RC6_SEGMENT_MARK)?593u:0u));
        TIM6->CNT=i%3u?15u:0u;TIM6->STS=TIM_FLAG_UPDATE;TIM6_IRQHandler();
    }
    assert(!ir_rc6_is_busy() && TIM3->CCDAT1==0u);
    assert(!TIM6->DINTEN && !TIM6->CTRL1 && !TIM3->CTRL1);
    assert(IRC->CTRL==IR_CTRL_ENABLE_Msk);
}
int main(void)
{
    unsigned c,t;
    learning=true;assert(!ir_rc6_send(0x80102658u));learning=false;
    nec_busy=true;assert(!ir_rc6_send(0x80102658u));nec_busy=false;
    for(c=0;c<256;++c)for(t=0;t<2;++t)finish_frame(ir_rc6_gehua_frame((uint8_t)c,t!=0));
    assert(!ir_rc6_error_count());
    assert(ir_rc6_send(0x80102658u));TIM6->CNT=101u;TIM6->STS=TIM_FLAG_UPDATE;TIM6_IRQHandler();
    assert(!s_active && ir_rc6_error_count()==1 && IRC->CTRL==IR_CTRL_ENABLE_Msk);
    assert(ir_rc6_send(0x80102658u));now_ms+=81;
    assert(!ir_rc6_is_busy() && ir_rc6_error_count()==2);
    TIM6->STS=TIM_FLAG_UPDATE;TIM6_IRQHandler();assert(TIM3->CCDAT1==0 && !TIM6->DINTEN);
    now_ms=0xFFFFFFF0u;assert(ir_rc6_send(0x80102658u));now_ms+=81;assert(!ir_rc6_is_busy());
    {ir_raw_t w;unsigned i;memset(&w,0,sizeof(w));w.count=7;w.period_ticks=1143;
     for(i=0;i<7;++i)w.widths[i]=i?600:9000;w.widths[2]=40000;
     assert(ir_rc6_send_wave(&w,false));assert(TIM3->AR==1142);
     for(i=0;i<7;++i){assert(TIM6->AR==(unsigned)w.widths[i]-1u);TIM6->CNT=0;TIM6->STS=TIM_FLAG_UPDATE;TIM6_IRQHandler();}
     assert(!ir_rc6_is_busy()&&!s_wave);
     w.kind=IR_NEC_KIND;assert(ir_rc6_send_wave(&w,true));assert(s_length==3);
     for(i=0;i<3;++i){TIM6->CNT=0;TIM6->STS=TIM_FLAG_UPDATE;TIM6_IRQHandler();}
     assert(!ir_rc6_is_busy());}
    puts("RC6 driver: 512 nonblocking frames, PWM/segments, NEC/capture exclusion, mux restore, late IRQ/timeout/wrap PASS");return 0;
}
