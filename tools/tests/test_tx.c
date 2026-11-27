#include "mock_hw.h"
#include "ir_rc6_encode.c"
#include "ir_key_map.c"
static bool nec_busy,rc6_busy,learning;
static bool start_failure;
static unsigned raw_duration=600u,store_reads;
static unsigned nec_sent,nec_repeats,rc6_sent,raw_sent;static bool raw_repeat;static unsigned learned_kind;
static uint8_t user,command,learned_key,learned_user,learned_cmd;
static uint32_t frame;
bool irc_is_busy(void){return nec_busy;}
void irc_stop(void){nec_busy=false;}
void ir_rc6_stop(void){rc6_busy=false;}
bool ir_rc6_is_busy(void){return rc6_busy;}
bool irc_send(uint8_t u,uint8_t c){if(nec_busy||rc6_busy)return false;user=u;command=c;++nec_sent;nec_busy=true;return true;}
bool ir_rc6_send(uint32_t f){if(nec_busy||rc6_busy)return false;frame=f;++rc6_sent;rc6_busy=true;return true;}
void irc_send_repeat(void){assert(!nec_busy&&!rc6_busy);++nec_repeats;nec_busy=true;}
bool ir_learn_is_active(void){return learning;}
bool ir_wave_store_frame(unsigned k,uint32_t *f){if(learned_kind!=1 || !learned_key || k!=learned_key)return false;*f=0x80102658u;return true;}
bool ir_wave_store_has(unsigned k){return learned_key && k==learned_key;}
bool ir_wave_store_get(unsigned k,ir_raw_t *w){assert(!rc6_busy&&!nec_busy);++store_reads;if(!ir_wave_store_has(k))return false;memset(w,0,sizeof(*w));w->count=7;w->period_ticks=1684;w->widths[0]=(uint16_t)raw_duration;w->kind=learned_kind;w->frame=0x80102658u;return true;}
bool ir_rc6_send_wave(const ir_raw_t *w,bool repeat,uint8_t duty){(void)duty;if(start_failure)return false;(void)w;++raw_sent;raw_repeat=repeat;rc6_busy=true;return true;}
static void advance(unsigned ms){now_ms+=ms;nec_busy=rc6_busy=false;ir_tx_process();}
int main(void)
{
 uint32_t first;unsigned n;
 assert(!ir_send_key(0)&&!ir_send_key(19));
 assert(!ir_send_key(2)&&!ir_send_key(3)&&!ir_send_key(15)&&!ir_send_key(17));assert(!nec_sent&&!rc6_sent);
 assert(ir_send_key(18));first=frame;assert(rc6_sent==1&&(frame&0xFFFF7FFFu)==0x8010260Cu&&!nec_sent);
 ir_tx_repeat(18);assert(rc6_sent==1);advance(107);ir_tx_repeat(18);assert(rc6_sent==1);
 advance(1);ir_tx_repeat(18);assert(rc6_sent==2&&frame==first&&!nec_repeats);
 ir_tx_release();advance(108);ir_tx_repeat(18);assert(rc6_sent==2);
 assert(ir_send_key(18));assert(frame==(first^0x8000u));ir_tx_release();advance(108);
 assert(ir_send_key_tv(1));assert(nec_sent==1&&user==0x51&&command==0xDC);
 advance(108);ir_tx_repeat(1);assert(nec_repeats==1&&rc6_sent==3);ir_tx_release();advance(108);
 assert(!ir_user_switch_tv());assert(ir_send_key_tv(1));assert(user==0x53&&command==0xDC);ir_tx_release();advance(108);
 learned_kind=2;learned_key=2;learned_user=0x51;learned_cmd=0;
 assert(ir_send_key(2));assert(raw_sent==1&&!raw_repeat);advance(108);ir_tx_repeat(2);assert(raw_sent==2&&raw_repeat);ir_tx_release();advance(108);learned_key=0;
 /* 三层查表优先级（规范 7.4：以最后更新的为准）：下载层 vs 学习层按各自写入序号比较 */
 ir_dl_mock=1;learned_kind=2;learned_key=3;           /* 同一个键两层都有 */
 ir_dl_seq_mock=5;ir_learn_seq_mock=3;                /* 下载更新 */
 assert(ir_send_key(3));assert(ir_dl_sent==1&&raw_sent==2);advance(108);ir_tx_release();
 ir_dl_seq_mock=3;ir_learn_seq_mock=3;                /* 序号相同 → 学习值优先（用户显式操作） */
 assert(ir_send_key(3));assert(ir_dl_sent==1&&raw_sent==3);advance(108);ir_tx_release();
 ir_dl_seq_mock=2;ir_learn_seq_mock=4;                /* 学习更新 */
 assert(ir_send_key(3));assert(ir_dl_sent==1&&raw_sent==4);advance(108);ir_tx_release();
 ir_dl_mock=0;learned_key=0;ir_dl_seq_mock=0;ir_learn_seq_mock=0;
 /* 只下载了电视机键值时，非电视专属键也要改走电视红外（联调期要求）：
  * 否则按音量/方向只会发机顶盒 RC6，电视机收不到。 */
 {unsigned dl=ir_dl_sent,nr=rc6_sent,nn=nec_sent;
  while(s_count)advance(108);ir_tx_release();
  ir_dl_tv_only=1;assert(ir_send_key_tv(4));      /* KEY4 确认：irstb 没有 → 改走 irtv */
  assert(ir_dl_sent==dl+1&&ir_dl_last_tv);     /* 发的是下载值，且走电视通道 */
  assert(rc6_sent==nr&&nec_sent==nn);          /* 没有退回 ROM 表 */
  advance(108);ir_tx_release();
  /* 两份都下载时仍以"键归属"为准：普通键走机顶盒 */
  ir_dl_tv_only=0;ir_dl_mock=1;
  assert(ir_send_key(4));assert(ir_dl_sent==dl+2&&!ir_dl_last_tv);
  advance(108);ir_tx_release();
  /* 电视专属键即便机顶盒也有值，仍走电视 */
  assert(ir_send_key_tv(1));assert(ir_dl_sent==dl+3&&ir_dl_last_tv);
  advance(108);ir_tx_release();
  ir_dl_mock=0;}
 nec_busy=true;assert(ir_send_key(12));assert(s_count==1);ir_tx_repeat(12);assert(rc6_sent==3);advance(108);
 assert((frame&0xFFFF7FFFu)==0x80102610u&&rc6_sent==4);ir_tx_release();advance(108);
 ir_send_pair_code_target(false);assert((frame&0xFFFF7FFFu)==0x801026ADu);
 n=rc6_sent;advance(108);ir_tx_repeat(12);assert(rc6_sent==n);
 ir_send_pair_code_target(true);assert(user==0x53&&command==0x7D);ir_tx_release();advance(108);
 learning=true;n=rc6_sent;assert(!ir_send_key(4));assert(rc6_sent==n);learning=false;
 rc6_busy=true;assert(ir_send_key(4)&&ir_send_key(5)&&ir_send_key(6)&&ir_send_key(7));
 {bool toggle=s_toggle;assert(!ir_send_key(16));assert(s_toggle==toggle);}
 while(s_count)advance(108);advance(108);ir_tx_release();ir_user_reset();
 assert(ir_user_for_key(1)==0x51&&ir_user_for_key(18)==0x3A);
 now_ms=0xFFFFFFF0u;assert(ir_send_key(4));first=frame;now_ms+=108;rc6_busy=false;ir_tx_repeat(4);assert(frame==first);
 learned_key=17;learned_kind=1;advance(108);assert(ir_send_key(17));first=frame;
 advance(108);ir_tx_repeat(17);assert(frame==first);ir_tx_release();advance(108);assert(ir_send_key(17));assert(frame==(first^0x8000u));
 ir_tx_release();advance(108);learned_kind=0;learned_key=17;raw_duration=60000;
 assert(ir_send_key(17));assert(s_repeat_interval==108);ir_tx_release();advance(108);
 /* Acceptance while another frame is active must never read Flash. */
 {unsigned reads=store_reads;rc6_busy=true;assert(ir_send_key(17));assert(store_reads==reads);rc6_busy=false;ir_tx_process();}
 advance(108);rc6_busy=false;start_failure=true;assert(ir_send_key(17));assert(!s_count&&!ir_tx_pending()&&!s_repeat_key);start_failure=false;
 rc6_busy=true;assert(ir_send_key(4));ota_busy=1;ir_tx_process();assert(!s_count&&!ir_tx_pending()&&!s_repeat_key&&!ir_send_key(18));
 ota_busy=0;advance(108);assert(ir_send_key(18));ir_tx_cancel();assert(!ir_tx_pending());
 /* 29 键扩展：点播借序号17设置码，数字借表 B.1 序号21~30 的码 */
 assert(ir_default_cmd(19u)==0x9Cu&&ir_default_cmd(20u)==0x92u&&ir_default_cmd(29u)==0x87u);
 assert(ir_default_cmd(30u)==0x00u);
 {unsigned before=nec_sent;assert(ir_send_key_tv(20u));advance(108);
  assert(nec_sent==before+1&&command==0x92u);ir_tx_release();advance(108);} puts("TX routing: RC6 STB/toggle/repeats, NEC TV/learned/zero, undefined silence, queue/full, pair targets, wrap PASS");return 0;
}
