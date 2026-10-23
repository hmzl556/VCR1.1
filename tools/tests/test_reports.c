#include "mock_merge.h"
static const uint8_t *SystemGetMacAddr(void){static uint8_t m[6];return m;}
#include "remote_reports.c"
#include "captured_tag01_fixture.h"
static unsigned period,updates,ota_tasks;static uint8_t read_data[RC_READ_LEN];
const uint8_t *ble_rc_svc_get_read(void){return read_data;}
uint8_t bat_get_level_percent(void){return 77;}
int cat1_iccid_get_raw(uint8_t out[20],uint8_t *len){memcpy(out,"898604F41525D1986801",20);*len=20;return 0;}
int cat1_time_get_bcd6_offset(uint8_t out[6],int8_t tz){(void)tz;memset(out,0x11,6);return 0;}
int cat1_time_sync_from_bcd6_fallback(const uint8_t b[6],int8_t tz){(void)b;(void)tz;return 0;}
void heartbeat_set_period_s(uint16_t p){period=p;++updates;}
void udp_ota_on_tag11_task(uint16_t sw,uint16_t n){(void)sw;(void)n;++ota_tasks;}
int rc_udp_aes_init(void){return 0;}
int rc_udp_aes_encrypt_data_field(const uint8_t *p,uint16_t n,uint8_t *o,uint16_t cap,uint16_t *len){assert(cap>=n);memcpy(o,p,n);*len=n;return 0;}
int rc_udp_aes_decrypt_data_field(const uint8_t *p,uint16_t n,uint8_t *o,uint16_t cap,uint16_t *len){assert(cap>=n);memcpy(o,p,n);*len=n;return 0;}
int main(void){uint8_t value[64],policy[6]={0},good[]={0,1,6,0,30},bad[]={6,0,1,0x7f},shortv[]={6,0};uint16_t n;
 memset(read_data,'0',sizeof(read_data));
 read_data[RC_READ_OFF_DEVICE_STATUS]=0;
 memcpy(read_data+RC_READ_OFF_DEVICE_ACTIVE_TIME,"20260101100800",14);
 memcpy(read_data+RC_READ_OFF_DEVICE_INACTIVE_TIME,"20360101100800",14);
 assert(!remote_build_tag01_value(value,&n)&&n==50&&value[27]==7&&!memcmp(value+28,"898604F41525D1986801",20)&&value[48]==8&&value[49]==77);
 {
  const uint8_t expected[6]={0x26,1,1,0x10,8,0};
  uint8_t blob[128], saved[128], out[608], rc;uint16_t size, framed;uint32_t pack;unsigned i;
  assert(value[11]==4&&!memcmp(value+12,expected,6)&&value[18]==5&&value[19]==0x36&&value[24]==0);
  /* Date validity, leap years, BCD nibbles and seconds. */
  {uint8_t date[6]={0x24,2,0x29,0x23,0x59,0x59};assert(remote_date6_valid(date));
   date[0]=0x23;assert(!remote_date6_valid(date));date[0]=0;assert(remote_date6_valid(date));
   date[1]=0x13;assert(!remote_date6_valid(date));date[1]=1;date[5]=0x60;assert(!remote_date6_valid(date));
   date[5]=0x1a;assert(!remote_date6_valid(date));}
  read_data[RC_READ_OFF_DEVICE_STATUS]=1;
  assert(!remote_build_tag01_value(value,&n)&&n==50&&value[11]==4&&value[18]==5&&value[19]==0x36);
  read_data[RC_READ_OFF_DEVICE_STATUS]=2;assert(!remote_build_tag01_value(value,&n)&&n==50&&value[11]==4&&value[18]==5);
  read_data[RC_READ_OFF_DEVICE_STATUS]=0;
  memcpy(read_data+RC_READ_OFF_DEVICE_ACTIVE_TIME,"00000000000000",14);
  assert(!remote_build_tag01_value(value,&n)&&n==43&&value[11]==5);
  memcpy(read_data+RC_READ_OFF_DEVICE_ACTIVE_TIME,"20260230100800",14);assert(remote_build_tag01_value(value,&n)<0);
  memcpy(read_data+RC_READ_OFF_DEVICE_ACTIVE_TIME,"20260101100837",14);
  /* Reproduce captured STB dates: YYYYMMDDhhmm instead of YYMMDDhhmmss.
   * The missing 37 seconds are recovered from RC-READ, not guessed zero. */
  {const uint8_t bad[]={0,40,0x22,0,1,0xaa,1,0,27,0,0x37,1,0,1,1,2,2,1,3,0,4,0x20,0x26,1,1,0x10,8,5,0x20,0x36,1,1,0x10,8,6,0,0x23,0,1,0xbb};
   memcpy(blob,bad,sizeof(bad));size=sizeof(bad);assert(size==40);
   assert(remote_validate_tlv(blob,size,0)<0);
   assert(!remote_normalize_tlv_blob(blob,&size)&&size==40&&blob[1]==40);
   assert(blob[6]==1&&blob[8]==27&&blob[21]==0x26&&blob[26]==0x37);
   assert(!memcmp(blob+2,bad+2,4)&&!memcmp(blob+36,bad+36,4));
   assert(!remote_validate_tlv(blob,size,0));memcpy(saved,blob,size);
   assert(!remote_normalize_tlv_blob(blob,&size)&&size==40&&!memcmp(saved,blob,size));
   remote_reports_init();assert(!remote_queue_tlv_blob(blob,size,123));
   assert(!remote_report_poll(out,sizeof(out),&framed,&rc,&pack)&&rc&&pack==123);
   assert(!memcmp(out+19,blob,size)); /* mock AES is identity */
   memcpy(blob,bad,sizeof(bad));blob[1]=38;size=40;assert(!remote_normalize_tlv_blob(blob,&size)&&size==40&&blob[1]==38);
   /* Unknown malformed date fails atomically; unrelated data is not half-modified. */
   memcpy(blob,bad,sizeof(bad));blob[22]=0x27;size=40;memcpy(saved,blob,size);
   assert(remote_normalize_tlv_blob(blob,&size)==REMOTE_QUEUE_PARAM&&size==40&&!memcmp(blob,saved,size));
   assert(remote_queue_tlv_blob(blob,size,124)==REMOTE_QUEUE_PARAM&&!remote_report_queue_count());
   /* Invalid trailing TLV also prevents modifying the recoverable earlier date. */
   memcpy(blob,bad,sizeof(bad));blob[38]=9;size=40;memcpy(saved,blob,size);
   assert(remote_normalize_tlv_blob(blob,&size)==REMOTE_QUEUE_PARAM&&!memcmp(blob,saved,size));
   /* Both dates are retained and corrected for status 0, 1 and 2. */
   memcpy(blob,bad,sizeof(bad));blob[19]=1;size=40;
   assert(!remote_normalize_tlv_blob(blob,&size)&&size==40&&blob[20]==4&&blob[27]==5&&blob[28]==0x36&&blob[33]==0);
   memcpy(blob,bad,sizeof(bad));blob[19]=2;size=40;
   assert(!remote_normalize_tlv_blob(blob,&size)&&size==40&&blob[20]==4&&blob[27]==5);
   /* Duplicate DataID and missing status are rejected, with no partial rewrite. */
   memcpy(blob,bad,sizeof(bad));blob[27]=4;size=40;
   assert(remote_normalize_tlv_blob(blob,&size)==REMOTE_QUEUE_PARAM);
   memcpy(blob,bad,sizeof(bad));blob[18]=6;size=40;
   assert(remote_normalize_tlv_blob(blob,&size)==REMOTE_QUEUE_PARAM);
  }
  assert(!remote_build_tag01_value(value,&n));
  blob[0]=0;blob[1]=(uint8_t)(n+5);blob[2]=1;blob[3]=0;blob[4]=(uint8_t)n;memcpy(blob+5,value,n);size=(uint16_t)(n+5);
  memcpy(saved,blob,size);assert(!remote_normalize_tlv_blob(blob,&size)&&!memcmp(saved,blob,size));
  for(i=0;i<16;i++){ /* every short header/partial date must remain bounded */
   uint16_t truncated=(uint16_t)i;assert(remote_normalize_tlv_blob(saved,&truncated)==REMOTE_QUEUE_PARAM);
  }
  memcpy(read_data+RC_READ_OFF_DEVICE_ACTIVE_TIME,"20260101100800",14);
  {
   uint8_t replay[sizeof(captured_tag01_fixture)];uint16_t replay_len=sizeof(replay);
   memcpy(replay,captured_tag01_fixture,sizeof(replay));assert(replay_len==186);
   assert(!remote_normalize_tlv_blob(replay,&replay_len)&&replay_len==186&&replay[1]==186);
   assert(!memcmp(replay+2,captured_tag01_fixture+2,70));
   assert(!memcmp(replay+102,captured_tag01_fixture+102,84));
   assert(replay[74]==27&&replay[87]==0x26&&replay[92]==0&&replay[94]==0x36&&replay[99]==0);
   assert(!remote_validate_tlv(replay,replay_len,0));
  }
 }
 assert(!remote_build_tag0A_value(value,&n)&&n==30&&value[0]==0&&value[7]==1&&!memcmp(value+8,"898604F41525D1986801",20)&&value[28]==2&&value[29]==77);
 assert(!remote_parse_tag11_payload(good,sizeof(good),policy)&&period==30&&updates==1&&!ota_tasks);
 assert(remote_parse_tag11_payload(bad,sizeof(bad),policy)<0&&period==30&&updates==1);
 assert(remote_parse_tag11_payload(shortv,sizeof(shortv),policy)<0&&updates==1);
 {uint8_t badcap[]={1,1,0x7f},badpolicy[]={2,0,9,0x7f},halfota[]={4,2,2,6,0,5},otaonly[]={4,2,2,5,0,2};
  uint8_t before[6];memcpy(before,policy,6);remote_set_key_capture_enable(2);
  assert(remote_parse_tag11_payload(badcap,sizeof(badcap),policy)<0&&!remote_is_key_capture_enabled());
  assert(remote_parse_tag11_payload(badpolicy,sizeof(badpolicy),policy)<0&&!memcmp(before,policy,6));
  assert(remote_parse_tag11_payload(halfota,sizeof(halfota),policy)<0&&updates==1&&!ota_tasks);
  assert(!remote_parse_tag11_payload(otaonly,sizeof(otaonly),policy)&&ota_tasks==1);
 }
 {
  uint8_t plain[6]={0,4,0x22,0,1,0x11},a[592],b[592],out[592];
  uint16_t an,bn,on;uint8_t rc;uint32_t pack;unsigned j;
  remote_reports_init();assert(!remote_queue_tlv_blob(plain,6,1));plain[5]=0x22;assert(!remote_queue_tlv_blob(plain,6,2));
  an=s_queue[s_q_head].len;memcpy(a,s_queue[s_q_head].buf,an);bn=s_queue[(s_q_head+1)%2].len;memcpy(b,s_queue[(s_q_head+1)%2].buf,bn);
  plain[5]=0x33;assert(remote_queue_tlv_blob(plain,6,3)==REMOTE_QUEUE_FULL);
  assert(s_q_tail<2&&!memcmp(a,s_queue[s_q_head].buf,an)&&!memcmp(b,s_queue[(s_q_head+1)%2].buf,bn));
  assert(!remote_report_drop_all_tag10()&&s_q_tail<2);
  assert(!remote_report_poll(out,sizeof(out),&on,&rc,&pack)&&pack==1);assert(!remote_report_poll(out,sizeof(out),&on,&rc,&pack)&&pack==2);
  for(j=0;j<1000;j++){
   remote_report_clear_queue();plain[5]=0x11;assert(!remote_queue_tlv_blob(plain,6,77));
   plain[5]=0x22;assert(!remote_queue_tlv_blob(plain,6,RC_PACK_INDEX_NONE));s_queue[(s_q_head+1)%2].is_tag10=1;
   plain[5]=0x33;assert(!remote_queue_tlv_blob(plain,6,88));assert(s_q_count==2&&s_q_tail<2);
   assert(!remote_report_poll(out,sizeof(out),&on,&rc,&pack)&&pack==77);
   assert(!remote_report_poll(out,sizeof(out),&on,&rc,&pack)&&pack==88);
  }
 }
 puts("reports: captured STB date recovery, missing-second preservation, calendar/BCD validation, both dates retained across states, atomic rejection, opaque TLV preservation, Tag01/Tag0A lengths, 20-byte ASCII ICCID, battery, Tag11 heartbeat-only acceptance, malformed update atomicity PASS");return 0;}
