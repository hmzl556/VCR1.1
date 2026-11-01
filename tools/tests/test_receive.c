#include "mock_hw.h"
#include "ir_raw.c"
#include "ir_rc6_encode.c"
#include "ir_learn.c"
static bool tx_pending;static unsigned reports;static ir_learn_evt_t event;static ir_raw_t result;
bool ir_tx_pending(void){return tx_pending;}
static void callback(ir_learn_evt_t e,uint8_t u,uint8_t c){(void)u;(void)c;++reports;event=e;if(e==IR_LEARN_EVT_DONE)result=*ir_learn_wave();}
static uint32_t clock_us;
static void clock_to(uint32_t target)
{
    while(1){uint32_t wrap=(clock_us&~65535ul)+65536u,next=wrap;unsigned flags=TIM_FLAG_UPDATE;
        if(s_capture && s_capture->mark && (TIM3->DINTEN&TIM_INT_CC1)){
            uint32_t compare=s_capture->last_fall+100u;
            if(compare<next){next=compare;flags=TIM_FLAG_CC1;}
        }
        if(next>target)break;
        clock_us=next;TIM3->CNT=(uint16_t)next;TIM3->STS=flags;TIM3_IRQHandler();
    }
    clock_us=target;TIM3->CNT=(uint16_t)target;now_ms=target/1000u;
}
static void poll_at(uint32_t t){clock_to(t);portb.PID=GPIO_PIN_5;TIM3->STS=0;ir_learn_process();}
static void begin(void)
{
    ir_learn_stop();now_ms=0;clock_us=0;reports=0;ir_learn_set_target(17);
    assert(ir_learn_start());poll_at(21000);assert(s_capture->armed);
}
static void feed_edges(const ir_raw_t *w,unsigned period)
{
    unsigned i,j;uint32_t start;
    for(i=0;i<w->count;++i){start=clock_us;
        if(!(i&1u))for(j=0;j+9u<w->widths[i];j+=period){clock_to(start+j);TIM3->CCDAT2=(uint16_t)clock_us;TIM3->STS=TIM_FLAG_CC2;TIM3_IRQHandler();}
        clock_to(start+w->widths[i]);
    }
}
static void feed(const ir_raw_t *w,unsigned period)
{
    feed_edges(w,period);poll_at(clock_us+21000);
    if(s_active && s_first_count)poll_at(clock_us+110000);
    assert(reports==1&&event==IR_LEARN_EVT_DONE&&!s_active);
}
static void make_rc6(ir_raw_t *w,uint32_t f)
{
    uint16_t parts[80];unsigned i,n=ir_rc6_encode(f,parts,80);memset(w,0,sizeof(*w));
    if(!(parts[n-1]&IR_RC6_SEGMENT_MARK))--n;
    w->count=n;for(i=0;i<n;++i)w->widths[i]=parts[i]&IR_RC6_SEGMENT_US;
}
int main(void)
{
    ir_raw_t w;unsigned c,t;uint32_t f;
    ir_learn_init(callback);ir_learn_set_target(15);assert(!ir_learn_start());ir_learn_set_target(4);assert(ir_learn_start());ir_learn_stop();ir_learn_set_target(1);
    tx_pending=true;assert(!ir_learn_start());tx_pending=false;
    for(c=0;c<256;++c)for(t=0;t<2;++t){f=(0x80102600u|c)^(t?0x8000u:0);make_rc6(&w,f);begin();feed(&w,28);
        assert(result.kind==IR_GEHUA_KIND&&result.frame==(f&~0x8000u));}
    memset(&w,0,sizeof(w));w.count=67;w.widths[0]=9000;w.widths[1]=4500;w.widths[66]=562;
    for(c=0;c<256;++c){f=0x0000AC53u|(c<<16)|((c^255u)<<24);
        for(t=0;t<32;++t){w.widths[2+2*t]=562;w.widths[3+2*t]=(f&(1ul<<t))?1687:562;}
        begin();feed(&w,26);assert(result.kind==IR_NEC_KIND);
    }
    begin();feed(&w,18);assert(result.kind==IR_NEC_KIND&&result.period_ticks==18u*64u);
    /* Actual IRQ capture observes NEC short repeat and NEC2 full repeats. */
    for(t=0;t<2;++t){
        ir_protocol_t info;ir_raw_t second=w;uint32_t first_start;
        if(!t){second.count=3;second.widths[0]=9000;second.widths[1]=2250;second.widths[2]=562;}
        begin();first_start=clock_us;feed_edges(&w,26);poll_at(clock_us+21000);
        assert(!reports&&s_active&&s_first_count==67);
        clock_to(first_start+108000u);feed_edges(&second,26);poll_at(clock_us+21000);
        assert(reports==1&&event==IR_LEARN_EVT_DONE);ir_raw_analyze(&result,&info);
        assert(info.protocol==(t?IR_PROTO_NEC2:IR_PROTO_NEC));
        assert(info.repeat_count==(t?67u:3u));
    }
    /* A different next key is not learned as a repeat of the first key. */
    {ir_raw_t second=w;uint32_t first_start;
     second.widths[3]=second.widths[3]>1000?562:1687;
     begin();first_start=clock_us;feed_edges(&w,26);poll_at(clock_us+21000);
     clock_to(first_start+108000);feed_edges(&second,26);poll_at(clock_us+21000);
     assert(!reports&&s_active&&s_retry_count==1&&!s_capture->wave.count);}
    w.widths[3]=3000;begin();feed(&w,26);assert(result.kind==IR_RAW_KIND);
    begin();TIM3->STS=TIM_FLAG_CC2OF;TIM3_IRQHandler();TIM3->STS=0;ir_learn_process();assert(s_active&&!reports&&!s_capture->wave.count);assert(s_retry_count==1&&s_overcapture_count==1&&s_retry_errors==RX_ERR_OVERCAPTURE);
    poll_at(clock_us+21000);feed(&w,26);assert(result.kind==IR_RAW_KIND);
    begin();capture_push(79u);ir_learn_process();
    assert(s_retry_count==1&&s_overcapture_count==0&&s_retry_errors==RX_ERR_BOUNDS);
    for(c=0;c<20;c++){TIM3->STS=TIM_FLAG_CC2OF;TIM3_IRQHandler();TIM3->STS=0;ir_learn_process();}
    assert(s_retry_count==21&&s_overcapture_count==20&&s_retry_errors==(RX_ERR_OVERCAPTURE|RX_ERR_BOUNDS));
    ir_learn_stop();assert(!s_retry_count);
    begin();assert(!s_retry_count&&!s_overcapture_count&&!s_retry_errors);now_ms=15000;ir_learn_process();assert(event==IR_LEARN_EVT_TIMEOUT&&!s_capture);
    begin();ota_busy=1;ir_learn_process();assert(event==IR_LEARN_EVT_CANCELED&&!s_capture);ota_busy=0;
    ir_learn_stop();puts("capture IRQ: 512 RC6, 256 NEC, 56kHz, raw, wrap, overrun/retry, timeout, OTA, policy PASS");return 0;
}
