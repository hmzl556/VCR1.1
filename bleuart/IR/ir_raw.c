#include "ir_raw.h"
#include <string.h>
#define MERGE_US 100u
#define GAP_US 20000u
static unsigned near(uint32_t x,uint32_t wanted,uint32_t tolerance)
{ return x+tolerance>=wanted && x<=wanted+tolerance; }
void ir_capture_init(ir_capture_t *c,uint32_t now,unsigned level)
{
    memset(c,0,sizeof(*c));c->previous=level;c->quiet_at=now;c->last_poll=now;
}
static int push(ir_capture_t *c,uint32_t duration)
{
    if(duration<80u || duration>65535u || c->wave.count==IR_RAW_MAX){
        c->failed=1;c->bad_width=duration;
        c->error=c->wave.count==IR_RAW_MAX?IR_RX_OVERFLOW:IR_RX_WIDTH;return 0;
    }
    c->wave.widths[c->wave.count++]=(uint16_t)duration;return 1;
}
int ir_raw_valid(const ir_raw_t *w)
{
    unsigned i;uint32_t total=0;
    if(w->count<7u || w->count>IR_RAW_MAX || !(w->count&1u) ||
       w->period_ticks<1067u || w->period_ticks>3200u || w->kind>IR_NEC_KIND)return 0;
    for(i=0;i<w->count;++i){if(w->widths[i]<80u)return 0;total+=w->widths[i];}
    if(total>250000u || w->widths[0]<500u)return 0;
    if(w->kind==IR_GEHUA_KIND && (w->frame>>16)!=0x8010u)return 0;
    return 1;
}
unsigned ir_raw_diagnose_rc6(const ir_raw_t *w,uint32_t *frame,unsigned *position,unsigned *halves)
{
    uint8_t half[76];unsigned i,j,n=0,units;uint32_t f=0;
    static const uint8_t prefix[12]={1,0,1,0,1,0,0,1,0,0,1,1};
    *position=0;*halves=0;*frame=0;
    if(w->count<5 || !near(w->widths[0],2664u,400u) || !near(w->widths[1],888u,220u))return 1;
    for(i=2;i<w->count;++i){
        units=(w->widths[i]+222u)/444u;
        *position=i;*halves=n;
        if(!units || units>3u || !near(w->widths[i],units*444u,180u) || n+units>76u)return 2;
        for(j=0;j<units;++j)half[n++]=(uint8_t)(!(i&1u));
    }
    /* A final SPACE half-bit is not part of the captured last MARK. */
    if(n==75u)half[n++]=0;
    *halves=n;
    if(n!=76u)return 3;
    for(i=0;i<12;++i)if(half[i]!=prefix[i]){*position=i;return 4;}
    for(i=12;i<76;i+=2){if(half[i]==half[i+1]){*position=i;return 5;}f=(f<<1)|half[i];}
    *frame=f;
    if((f>>16)!=0x8010u)return 6;
    return 0;
}
int ir_raw_decode_gehuarc6(const ir_raw_t *w,uint32_t *frame)
{unsigned position,halves;return ir_raw_diagnose_rc6(w,frame,&position,&halves)==0;}
unsigned ir_raw_rc6_header(const ir_raw_t *w)
{
    static const uint8_t prefix[12]={1,0,1,0,1,0,0,1,0,0,1,1};
    unsigned i,j,n=0,units;
    if(w->count<5 || !near(w->widths[0],2664u,400u) || !near(w->widths[1],888u,220u))return 0;
    for(i=2;i<w->count && n<12u;++i){
        units=(w->widths[i]+222u)/444u;
        if(!units || units>3u || !near(w->widths[i],units*444u,180u))return 0;
        for(j=0;j<units && n<12u;++j)if(prefix[n++]!=(uint8_t)(!(i&1u)))return 0;
    }
    return n==12u;
}
unsigned ir_raw_rc6_damaged(const ir_raw_t *w)
{
    unsigned position,halves,reason,i,expanded=0;uint32_t frame;
    if(!ir_raw_rc6_header(w))return 0;
    reason=ir_raw_diagnose_rc6(w,&frame,&position,&halves);
    for(i=2;i<w->count;++i)expanded+=(w->widths[i]+222u)/444u;
    /* Shorter RC6-6A variants are not claimed supported by this validator.
     * Reject malformed 32-bit-sized captures, preserve shorter raw protocols. */
    return reason!=0u && reason!=6u && expanded>=75u && expanded<=80u;
}
int ir_raw_is_nec(const ir_raw_t *w)
{
    unsigned i;uint32_t bits=0;
    if(w->count!=67u || !near(w->widths[0],9000u,1800u) || !near(w->widths[1],4500u,900u))return 0;
    for(i=0;i<32;++i){
        if(!near(w->widths[2+i*2],562u,220u))return 0;
        if(near(w->widths[3+i*2],1687u,400u))bits|=1ul<<i;
        else if(!near(w->widths[3+i*2],562u,220u))return 0;
    }
    return near(w->widths[66],562u,220u) && (((bits>>16)^(bits>>24))&255u)==255u;
}
int ir_capture_poll(ir_capture_t *c,uint32_t now,unsigned level)
{
    uint32_t delta=now-c->last_poll;
    if(c->seen_fall && delta>c->max_poll_us)c->max_poll_us=delta;
    c->last_poll=now;
    if(c->failed)return -1;
    if(!c->armed){
        if(!level)c->quiet_at=now;
        if(level && (uint32_t)(now-c->quiet_at)>=GAP_US){c->armed=1;c->previous=1;}
        return 0;
    }
    if(!level){
        if(c->previous){
            if(c->seen_fall){uint32_t d=now-c->last_fall;if(d>=10u && d<=64u && c->periods[d]<65535u)++c->periods[d];}
            c->last_fall=now;c->seen_fall=1;
        }
        if(!c->mark){
            if(c->wave.count){if(!push(c,now-c->mark_end))return -1;}
            else c->started=now;
            c->mark_start=now;c->mark=1;
        }
        c->last_low=now;
    }else if(c->mark && (uint32_t)(now-c->last_low)>=MERGE_US){
        c->mark_end=c->last_low+1u;
        if(!push(c,c->mark_end-c->mark_start))return -1;
        c->mark=0;
    }
    c->previous=level;
    if(c->seen_fall && (uint32_t)(now-c->started)>270000u){c->error=IR_RX_TIMEOUT;return -1;}
    if(c->mark || !c->wave.count || (uint32_t)(now-c->mark_end)<GAP_US)return 0;
    return ir_capture_complete(c);
}
int ir_capture_complete(ir_capture_t *c)
{
    unsigned i,peak=10,accepted=0,total=0;uint32_t sum=0,frame;
    for(i=10;i<=64;++i){total+=c->periods[i];if(c->periods[i]>c->periods[peak])peak=i;}
    for(i=peak>3?peak-3:10;i<=peak+3 && i<=64;++i){accepted+=c->periods[i];sum+=i*c->periods[i];}
    c->carrier_peak=peak;c->carrier_samples=total;c->carrier_accepted=accepted;
    if(accepted<32u){c->error=IR_RX_CARRIER_FEW;return -1;}
    if(accepted*100u<total*80u){c->error=IR_RX_CARRIER_SPREAD;return -1;}
    c->wave.period_ticks=(uint16_t)((sum*64u+accepted/2u)/accepted);
    if(c->wave.period_ticks<1067u || c->wave.period_ticks>3200u){c->error=IR_RX_CARRIER_RANGE;return -1;}
    c->wave.kind=IR_RAW_KIND;
    if(ir_raw_decode_gehuarc6(&c->wave,&frame)){
        c->wave.kind=IR_GEHUA_KIND;c->wave.frame=frame&~0x8000ul;
    }else if(ir_raw_rc6_damaged(&c->wave)){c->error=IR_RX_RC6_DAMAGED;return -1;}
    else if(ir_raw_is_nec(&c->wave))c->wave.kind=IR_NEC_KIND;
    if(!ir_raw_valid(&c->wave)){c->error=IR_RX_FRAME;return -1;}
    return 1;
}
