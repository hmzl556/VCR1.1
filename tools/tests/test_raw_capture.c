#include "ir_raw.c"
#include "rc6_wave.c"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ir_raw.h"
#include "rc6_wave.h"
static ir_capture_t c;
static uint32_t words[80];
static void record(ir_raw_t *w,unsigned period,unsigned step){
 uint32_t now=0,begin,end,ticks=period;unsigned i,level;int result=0;
 ir_capture_init(&c,0,1);
 for(now=0;now<30000;now+=step)assert(ir_capture_poll(&c,now,1)==0);
 for(i=0;i<w->count;++i){
  begin=now;end=begin+w->widths[i];
  for(;now<end;now+=step){
   level=(i&1u)?1u:((now-begin)*64u%ticks>=ticks/3u);
   result=ir_capture_poll(&c,now,level);assert(!result);
  }
 }
 for(i=0;i<30000;i+=step){now+=step;result=ir_capture_poll(&c,now,1);if(result)break;}
 assert(result==1);assert(ir_raw_valid(&c.wave));
 assert(c.wave.count==w->count);
 assert(c.wave.period_ticks+96u>=period && c.wave.period_ticks<=period+96u);
 for(i=0;i<w->count;++i){int d=(int)c.wave.widths[i]-w->widths[i];assert(d>=-80 && d<=80);}
}
int main(void){
 unsigned i,j,n,toggle,cmd,periods[]={1778,1684,1143};ir_raw_t w;uint32_t frame;
 for(cmd=0x58;cmd<=0x5c;++cmd)for(toggle=0;toggle<2;++toggle){
  memset(&w,0,sizeof(w));frame=(0x80102600u|cmd)^(toggle?0x8000u:0);
  n=rc6_build(frame,words,80);if(!(words[n-1]&(1u<<14)))--n;
  w.count=n;for(i=0;i<n;++i)w.widths[i]=(words[i]&0x3fff)/RC6_HALF_CYCLES*444;
  assert(ir_raw_decode_gehuarc6(&w,&frame));
  for(j=1;j<=4;++j){record(&w,1778,j);assert(c.wave.kind==IR_GEHUA_KIND);assert(c.wave.frame==(frame&~0x8000u));}
 }
 memset(&w,0,sizeof(w));w.count=67;w.widths[0]=9000;w.widths[1]=4500;
 frame=0xcb345aa5u;
 for(i=0;i<32;++i){w.widths[2+2*i]=562;w.widths[3+2*i]=frame&(1u<<i)?1687:562;}
 w.widths[66]=562;
 assert(ir_raw_is_nec(&w));
 for(j=0;j<3;++j){record(&w,periods[j],2);assert(c.wave.kind==IR_NEC_KIND);}
 w.widths[3]=3000;assert(!ir_raw_is_nec(&w));record(&w,1600,2);assert(c.wave.kind==IR_RAW_KIND);
 /* Silence never becomes a successful learning result. */
 ir_capture_init(&c,0,1);for(i=0;i<100000;i+=10)assert(!ir_capture_poll(&c,i,1));
 /* Continuous low and excessive frame lengths are rejected. */
 ir_capture_init(&c,0,1);for(i=0;i<30000;i+=10)ir_capture_poll(&c,i,1);
 for(i=30000;i<310000;i+=10){if(ir_capture_poll(&c,i,0)<0)break;}assert(i<310000);assert(c.error==IR_RX_TIMEOUT);
 /* Overflow must fail, never store a truncated frame. */
 ir_capture_init(&c,0,1);for(i=0;i<30000;i+=10)ir_capture_poll(&c,i,1);
 c.wave.count=IR_RAW_MAX;c.mark=1;c.mark_start=31000;c.last_low=31500;
 assert(ir_capture_poll(&c,32000,1)==-1);assert(c.error==IR_RX_OVERFLOW);
 /* Detailed reasons distinguish carrier problems from envelope failure. */
 ir_capture_init(&c,0,1);c.armed=1;c.seen_fall=1;c.started=0;c.wave.count=7;c.mark_end=100;
 for(i=0;i<7;++i)c.wave.widths[i]=i?600:2400;
 assert(ir_capture_poll(&c,21000,1)==-1 && c.error==IR_RX_CARRIER_FEW);
 c.periods[27]=40;c.periods[40]=40;
 assert(ir_capture_poll(&c,22000,1)==-1 && c.error==IR_RX_CARRIER_SPREAD);
 memset(c.periods,0,sizeof(c.periods));c.periods[55]=100;
 assert(ir_capture_poll(&c,23000,1)==-1 && c.error==IR_RX_CARRIER_RANGE);
 memset(c.periods,0,sizeof(c.periods));c.periods[27]=100;c.wave.count=6;
 assert(ir_capture_poll(&c,24000,1)==-1 && c.error==IR_RX_FRAME);
 {
  const unsigned observed[]={2605,933,374,490,372,490,376,928,406,901,1207,933,401,463,399,463,399,463,403,459,398,463,404,458,399,463,399,463,815,464,792,932,402,459,400,463,402,458,400,464,397,463,788,937,815,463,788,490,372,937,401,459,793,932,792,485,372,937,398,463,400,463,398};
  unsigned pos,halves;memset(&w,0,sizeof(w));w.count=63;w.period_ticks=1781;
  for(i=0;i<63;++i)w.widths[i]=observed[i];
  assert(ir_raw_rc6_header(&w));
  assert(ir_raw_diagnose_rc6(&w,&frame,&pos,&halves)==2 && pos==61 && halves==76);
  assert(ir_raw_rc6_damaged(&w)); /* Do not save or invent missing data bits. */
  n=rc6_build(0x80102658u,words,80);if(!(words[n-1]&(1u<<14)))--n;
  w.count=n;for(i=0;i<n;++i)w.widths[i]=(words[i]&0x3fff)/RC6_HALF_CYCLES*444;
  assert(!ir_raw_rc6_damaged(&w));assert(ir_raw_decode_gehuarc6(&w,&frame)&&frame==0x80102658u);
 }
 puts("PASS: detailed RX failure reasons; 40 RC6 capture/toggle cases, NEC at 36/38/56kHz, generic waveform, quiet/noise/overflow rejection");
 return 0;
}
