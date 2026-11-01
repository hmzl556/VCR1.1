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

/* Protocol analysis uses the stored envelope itself, not new Flash fields.
 * A learned exchange may contain two frames separated by an idle space. */
static uint32_t wave_sum(const ir_raw_t *w,unsigned n)
{unsigned i;uint32_t t=0;for(i=0;i<n;++i)t+=w->widths[i];return t;}
static int pd_bits(const ir_raw_t *w,unsigned start,unsigned n,unsigned hm,unsigned hs,uint32_t *bits)
{
    unsigned i;uint32_t b=0;
    if(n!=67u || !near(w->widths[start],hm,hm/5u) || !near(w->widths[start+1u],hs,hs/5u))return 0;
    for(i=0;i<32u;++i){
        if(!near(w->widths[start+2u+2u*i],562u,160u))return 0;
        if(near(w->widths[start+3u+2u*i],1687u,340u))b|=1ul<<i;
        else if(!near(w->widths[start+3u+2u*i],562u,160u))return 0;
    }
    if(!near(w->widths[start+66u],562u,160u))return 0;
    *bits=b;return 1;
}
static int biphase(const ir_raw_t *w,unsigned n,unsigned start,unsigned unit,unsigned rc6,uint32_t *value)
{
    uint8_t h[48];unsigned i,j,k=0,u,target=rc6?44u:28u,at=0,b;uint32_t v=0;
    if(!rc6)h[k++]=0; /* RC5 starts with an unrecorded SPACE half-bit. */
    for(i=start;i<n;++i){
        u=(w->widths[i]+unit/2u)/unit;
        if(!u || u>3u || !near(w->widths[i],u*unit,unit/4u) || k+u>target)return 0;
        for(j=0;j<u;++j)h[k++]=(uint8_t)(!(i&1u));
    }
    if(k+1u==target)h[k++]=0; /* Capture ends at last mark. */
    if(k!=target)return 0;
    for(i=0;i<(rc6?21u:14u);++i){
        u=(rc6 && i==4u)?2u:1u;
        if(h[at]==h[at+u])return 0;
        if(u==2u && (h[at]!=h[at+1u] || h[at+2u]!=h[at+3u]))return 0;
        b=rc6?h[at]:!h[at];v=(v<<1)|b;at+=2u*u;
    }
    if(rc6){if((v>>17)!=8u)return 0;} /* RC6 mode 0 only. Other modes stay raw. */
    else if(!(v&0x2000u))return 0;
    *value=v;return 1;
}
/* Two timing clusters; weighted mean instead of selecting one noisy sample.
 * Reject ambiguous outliers, never force an arbitrary waveform into bits. */
static int clusters(const ir_raw_t *w,unsigned n,unsigned offset,uint16_t *short_us,uint16_t *long_us)
{
    unsigned i,min=65535u,max=0,lo=0,hi=0;uint32_t ls=0,hs=0;unsigned split;
    for(i=offset;i<n-1u;i+=2u){unsigned x=w->widths[i];if(x<min)min=x;if(x>max)max=x;}
    if(!max)return 0;
    if(max<=min+min/3u){
        for(i=offset;i<n-1u;i+=2u){ls+=w->widths[i];++lo;}
        *short_us=(uint16_t)((ls+lo/2u)/lo);*long_us=*short_us;return 1;
    }
    split=(min+max)/2u;
    for(i=offset;i<n-1u;i+=2u){unsigned x=w->widths[i];if(x<split){ls+=x;++lo;}else{hs+=x;++hi;}}
    if(!lo || !hi)return 0;
    *short_us=(uint16_t)((ls+lo/2u)/lo);*long_us=(uint16_t)((hs+hi/2u)/hi);
    for(i=offset;i<n-1u;i+=2u){unsigned x=w->widths[i];
        if(!near(x,*short_us,*short_us/4u) && !near(x,*long_us,*long_us/4u))return 0;}
    return 1;
}
void ir_raw_analyze(const ir_raw_t *w,ir_protocol_t *p)
{
    unsigned i,n,second;uint32_t b=0,b2;
    memset(p,0,sizeof(*p));
    if(!w || w->count<7u || w->count>IR_RAW_MAX || !(w->count&1u))return;
    n=w->count;
    for(i=3;i<w->count;i+=2u)if(w->widths[i]>=8000u){n=i;break;}
    p->first_count=n;p->repeat_count=n;p->interval_us=108000u;
    second=n+1u;
    if(second<w->count){p->repeat_offset=second;p->repeat_count=w->count-second;p->interval_us=wave_sum(w,second);}
    if(pd_bits(w,0,n,9000,4500,&b) && (((b>>16)^(b>>24))&255u)==255u){
        p->data=b;p->bits=32u;
        p->protocol=((b^(b>>8))&255u)==255u?IR_PROTO_NEC:IR_PROTO_NEC_EXT;
        if(second<w->count){
            if(pd_bits(w,second,w->count-second,9000,4500,&b2) && b==b2)p->protocol=IR_PROTO_NEC2;
            else if(w->count-second!=3u || !near(w->widths[second],9000,1800) ||
                    !near(w->widths[second+1u],2250,450) || !near(w->widths[second+2u],562,160)){
                p->protocol=IR_PROTO_RAW;p->repeat_offset=0;p->repeat_count=w->count;p->first_count=w->count;
            }
        }
        return;
    }
    if(pd_bits(w,0,n,4500,4500,&b) && (((b>>16)^(b>>24))&255u)==255u){p->protocol=IR_PROTO_SAMSUNG;p->bits=32;p->data=b;return;}
    if((n==25u || n==31u || n==41u) && near(w->widths[0],2400,480)){
        p->bits=(n-1u)/2u;
        for(i=0;i<p->bits;++i){
            if(!near(w->widths[1u+2u*i],600,150))break;
            if(near(w->widths[2u+2u*i],1200,240))b|=1ul<<i;
            else if(!near(w->widths[2u+2u*i],600,150))break;
        }
        if(i==p->bits){p->protocol=IR_PROTO_SONY;p->data=b;if(!p->repeat_offset)p->interval_us=45000u;return;}
    }
    if(biphase(w,n,0,889u,0,&b)){p->protocol=IR_PROTO_RC5;p->bits=14;p->data=b;p->interval_us=114000u;return;}
    if(near(w->widths[0],2666,400) && near(w->widths[1],889,180) && biphase(w,n,2,444u,1,&b)){
        p->protocol=IR_PROTO_RC6;p->bits=21;p->data=b;return;
    }
    if(n>=19u && clusters(w,n,2,&p->mark0,&p->mark1) && clusters(w,n,3,&p->space0,&p->space1)){
        if(p->mark0==p->mark1 && p->space0!=p->space1)p->protocol=IR_PROTO_DISTANCE;
        else if(p->space0==p->space1 && p->mark0!=p->mark1)p->protocol=IR_PROTO_WIDTH;
        /* Unsupported combined-distance/width encodings stay unmodified. */
        if(p->protocol){
            p->bits=(n-3u)/2u;
            /* Data preview uses LSB-first internally; raw envelope remains
             * authoritative, so this choice never changes the wire bit order. */
            for(i=0;i<p->bits && i<32u;++i){
                unsigned x=w->widths[(p->protocol==IR_PROTO_DISTANCE?3u:2u)+2u*i];
                unsigned threshold=p->protocol==IR_PROTO_DISTANCE?(p->space0+p->space1)/2u:(p->mark0+p->mark1)/2u;
                if(x>threshold)p->data|=1ul<<i;
            }
        }
    }
}
static void append_level(ir_raw_t *w,unsigned mark,unsigned us)
{
    if(!w->count){if(mark)w->widths[w->count++]=(uint16_t)us;return;}
    if(mark==((w->count&1u)!=0))w->widths[w->count-1u]+=(uint16_t)us;
    else if(w->count<IR_RAW_MAX)w->widths[w->count++]=(uint16_t)us;
}
void ir_raw_prepare(ir_raw_t *w,unsigned repeat,unsigned toggle)
{
    ir_protocol_t p;unsigned i,bit,unit,mark;uint32_t v;
    ir_raw_analyze(w,&p);
    if(!p.first_count || p.protocol==IR_PROTO_RAW || p.protocol==IR_PROTO_DISTANCE || p.protocol==IR_PROTO_WIDTH)return;
    if(p.protocol==IR_PROTO_RC5 || p.protocol==IR_PROTO_RC6){
        v=p.data;
        if(p.protocol==IR_PROTO_RC5)v=(v&~0x800ul)|(toggle?0x800ul:0);else v=(v&~0x10000ul)|(toggle?0x10000ul:0);
        w->count=0;
        if(p.protocol==IR_PROTO_RC6){append_level(w,1,2664);append_level(w,0,888);}
        for(i=p.bits;i>0;--i){
            bit=(v>>(i-1u))&1u;unit=p.protocol==IR_PROTO_RC5?889u:444u;
            if(p.protocol==IR_PROTO_RC6 && i==17u)unit*=2u;
            mark=p.protocol==IR_PROTO_RC6?bit:!bit;
            append_level(w,mark,unit);append_level(w,!mark,unit);
        }
        if(!(w->count&1u))--w->count;
        return;
    }
    /* Only observed short repeat frames authorize NEC short repeats.
     * Legacy single-frame NEC records repeat the full frame conservatively. */
    if(repeat && p.repeat_offset){
        memmove(w->widths,w->widths+p.repeat_offset,p.repeat_count*sizeof(w->widths[0]));w->count=(uint16_t)p.repeat_count;
    }else w->count=(uint16_t)p.first_count;
    if(p.protocol==IR_PROTO_NEC || p.protocol==IR_PROTO_NEC_EXT || p.protocol==IR_PROTO_NEC2 || p.protocol==IR_PROTO_SAMSUNG){
        if(w->count==3u){w->widths[0]=9000;w->widths[1]=2250;w->widths[2]=562;return;}
        if(w->count!=67u)return;
        w->widths[0]=p.protocol==IR_PROTO_SAMSUNG?4500:9000;w->widths[1]=4500;
        for(i=0;i<32u;++i){w->widths[2u+2u*i]=562;w->widths[3u+2u*i]=(p.data&(1ul<<i))?1687:562;}
        w->widths[66]=562;
    }else if(p.protocol==IR_PROTO_SONY){
        w->widths[0]=2400;
        for(i=0;i<p.bits;++i){w->widths[1u+2u*i]=600;w->widths[2u+2u*i]=(p.data&(1ul<<i))?1200:600;}
    }else if(p.protocol==IR_PROTO_DISTANCE || p.protocol==IR_PROTO_WIDTH){
        /* Do not rewrite unknown timing without an independent matching frame.
         * Analysis remains available, while raw replay retains the evidence. */
    }
}
const char *ir_raw_protocol_name(const ir_raw_t *w)
{
    static const char *const names[]={"RAW","NEC","NECext","NEC2","Sony","Samsung","RC5","RC6","Distance","Width"};
    ir_protocol_t p;if(w && w->kind==IR_GEHUA_KIND)return "GehuaRC6";
    ir_raw_analyze(w,&p);return names[p.protocol];
}
