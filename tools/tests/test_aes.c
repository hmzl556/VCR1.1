#include "mock_merge.h"
static const uint8_t *SystemGetMacAddr(void){static uint8_t m[6];return m;}
#include "tiny-aes/aes.c"
#include "aes_transport.c"
#include "remote_reports.c"
int cat1_time_get_bcd6_offset(uint8_t out[6],int8_t tz){(void)tz;memset(out,0x11,6);return 0;}
int cat1_time_sync_from_bcd6_fallback(const uint8_t b[6],int8_t tz){(void)b;(void)tz;return 0;}
void heartbeat_set_period_s(uint16_t s){(void)s;}
void udp_ota_on_tag11_task(uint16_t v,uint16_t n){(void)v;(void)n;}
const uint8_t *ble_rc_svc_get_read(void){static uint8_t b[RC_READ_LEN];return b;}
int cat1_iccid_get_raw(uint8_t b[20],uint8_t *n){memset(b,0,20);*n=0;return 0;}
uint8_t bat_get_level_percent(void){return 77;}
int main(void){
 const uint8_t key[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
 const uint8_t expected[16]={0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
 struct AES_ctx ctx;uint8_t block[16],plain[560],cipher[608],restored[560],frame[608];
 uint16_t n,cn,pn,fn;unsigned i;uint8_t rc;uint32_t pack;
 for(i=0;i<16;i++)block[i]=(uint8_t)(i*17);AES_init_ctx(&ctx,key);AES_ECB_encrypt(&ctx,block);assert(!memcmp(block,expected,16));
 AES_ECB_decrypt(&ctx,block);for(i=0;i<16;i++)assert(block[i]==i*17);
 rc_udp_aes_init();for(i=0;i<sizeof(plain);i++)plain[i]=(uint8_t)(i*113+17);
 for(n=1;n<=560;n++){
  assert(!rc_udp_aes_encrypt_data_field(plain,n,cipher,sizeof(cipher),&cn)&&cn==(n/16+1)*16);
  assert(!rc_udp_aes_decrypt_data_field(cipher,cn,restored,sizeof(restored),&pn)&&pn==n&&!memcmp(plain,restored,n));
 }
 memset(block,0,sizeof(block));AES_ECB_encrypt(&s_aes_ctx,block);assert(rc_udp_aes_decrypt_data_field(block,16,restored,560,&pn)<0);
 assert(rc_udp_aes_encrypt_data_field(plain,0,cipher,608,&cn)<0);
 assert(rc_udp_aes_encrypt_data_field(plain,560,cipher,575,&cn)<0);
 plain[0]=2;plain[1]=0x30;plain[2]=0x22;plain[3]=2;plain[4]=0x2b; /* 560B container, opaque 555B value */
 remote_reports_init();assert(!remote_queue_tlv_blob(plain,560,42));
 assert(!remote_report_poll(frame,sizeof(frame),&fn,&rc,&pack)&&fn==597&&rc&&pack==42);
 assert(!rc_udp_aes_decrypt_data_field(frame+19,576,restored,sizeof(restored),&pn)&&pn==560&&!memcmp(plain,restored,560));
 puts("AES/report boundary: NIST AES-128 vector, 1..560-byte PKCS round trips, bad padding/caps, actual 597-byte full-size RC-TRANS PASS");return 0;
}
