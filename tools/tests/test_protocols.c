
#include "ir_raw.c"
#include <assert.h>
#include <stdio.h>
static void nec(ir_raw_t *w,uint32_t data,unsigned header){
 unsigned i;memset(w,0,sizeof(*w));w->count=67;w->period_ticks=1684;
 w->widths[0]=(uint16_t)header;w->widths[1]=4500;w->widths[66]=562;
 for(i=0;i<32;++i){w->widths[2+2*i]=562;w->widths[3+2*i]=(data&(1ul<<i))?1687:562;}
}
static void join(ir_raw_t *w,const ir_raw_t *other,unsigned period){
 unsigned n=w->count;w->widths[n]=(uint16_t)(period-wave_sum(w,n));
 memcpy(w->widths+n+1u,other->widths,other->count*sizeof(uint16_t));w->count+=(uint16_t)(other->count+1u);
}
static void manchester(ir_raw_t *w,uint32_t data,unsigned rc6){
 unsigned bits=rc6?21:14,i,j,u,last=0,d=0;uint8_t level[48];unsigned n=0;
 memset(w,0,sizeof(*w));w->period_ticks=1778;
 if(rc6){w->widths[0]=2664;w->widths[1]=888;w->count=2;}
 for(i=bits;i>0;--i){unsigned b=(data>>(i-1u))&1u;u=(rc6&&i==17u)?2:1;
  for(j=0;j<u;++j)level[n++]=(uint8_t)(rc6?b:!b);
  for(j=0;j<u;++j)level[n++]=(uint8_t)(rc6?!b:b);
 }
 for(i=rc6?0:1;i<n;++i){unsigned x=level[i];if(d&&x!=last){w->widths[w->count++]=(uint16_t)d;d=0;}last=x;d+=rc6?444:889;}
 if(last)w->widths[w->count++]=(uint16_t)d;
}
int main(void){
 ir_raw_t w,other,copy;ir_protocol_t p;unsigned cmd,addr,t,i;
 assert(sizeof(ir_raw_t)==524u); /* Flash ABI must not change. */
 for(addr=0;addr<256;++addr)for(cmd=0;cmd<256;++cmd){
  uint32_t b=addr|((addr^255u)<<8)|(cmd<<16)|((cmd^255u)<<24);
  nec(&w,b,9000);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_NEC&&p.data==b);
  copy=w;ir_raw_prepare(&copy,1,0);assert(copy.count==67); /* No repeat inferred from one frame. */
 }
 nec(&w,0xcb341234u,9000);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_NEC_EXT);
 other=w;join(&w,&other,108000);assert(ir_raw_valid(&w));ir_raw_analyze(&w,&p);
 assert(p.protocol==IR_PROTO_NEC2&&p.repeat_count==67&&p.interval_us==108000);
 copy=w;ir_raw_prepare(&copy,0,0);assert(copy.count==67&&!memcmp(copy.widths,other.widths,134));
 copy=w;ir_raw_prepare(&copy,1,0);assert(copy.count==67&&!memcmp(copy.widths,other.widths,134));
 nec(&w,0xcb34df20u,9000);memset(&other,0,sizeof(other));other.count=3;other.widths[0]=9000;other.widths[1]=2250;other.widths[2]=562;
 join(&w,&other,108000);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_NEC&&p.repeat_count==3);
 copy=w;ir_raw_prepare(&copy,1,0);assert(copy.count==3&&copy.widths[1]==2250);
 w.widths[69]=4500;ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_RAW);
 nec(&w,0xcb34df20u,4500);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_SAMSUNG);
 w.widths[10]=100;ir_raw_analyze(&w,&p);assert(p.protocol!=IR_PROTO_SAMSUNG);
 for(i=12;i<=20;i+=(i==12?3:5)){
  unsigned j;memset(&w,0,sizeof(w));w.count=(uint16_t)(1+2*i);w.widths[0]=2400;w.period_ticks=1600;
  for(j=0;j<i;++j){w.widths[1+2*j]=600;w.widths[2+2*j]=(j&1)?1200:600;}
  ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_SONY&&p.bits==i);
 }
 for(cmd=0;cmd<256;++cmd)for(t=0;t<2;++t){
  uint32_t b=0x3000u|(t<<11)|cmd;manchester(&w,b,0);assert(ir_raw_valid(&w));
  ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_RC5&&p.data==b);
  ir_raw_prepare(&w,0,!t);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_RC5&&p.data==(b^0x800u));
  b=0x100000u|(t<<16)|(0x26u<<8)|cmd;manchester(&w,b,1);assert(ir_raw_valid(&w));
  ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_RC6&&p.data==b);
  ir_raw_prepare(&w,0,!t);ir_raw_analyze(&w,&p);assert(p.protocol==IR_PROTO_RC6&&p.data==(b^0x10000u));
 }
 memset(&w,0,sizeof(w));w.count=7;w.period_ticks=1143;for(i=0;i<7;++i)w.widths[i]=i?600:1200;
 copy=w;ir_raw_prepare(&copy,1,1);assert(!memcmp(&copy,&w,sizeof(w)));
 puts("protocols: 65536 NEC pairs, extended/NEC2/observed short repeat, Samsung, Sony 12/15/20, 1024 Manchester/toggle cases, raw fallback and Flash ABI PASS");
 return 0;
}
