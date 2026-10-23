/* Integrate the real OTA state machine, frame parser, CRC and NOR writer. */
#define udp_ota_session_busy mock_ota_session_busy
#define main ota_storage_cases_main
#include "test_ota_safety.c"
#undef main
#undef udp_ota_session_busy
volatile uint8_t g_audio_pcm_enabled;
static unsigned ir_cancels,learn_stops,voice_stops,hid_kept,restarted,system_resets,peer_connected=1;
void ir_tx_cancel(void){++ir_cancels;}
void ir_learn_stop(void){++learn_stops;}
int voice_ctrl_post_stop(void){++voice_stops;return 1;}
void Audio_Pcm_Stop(void){g_audio_pcm_enabled=0;}
void hid_kbd_force_cccd_enable(peer_t p){assert(p);++hid_kept;}
void ble_adv_stop(void){}
void ble_adv_start_by_bonding(void){++restarted;}
peer_t ble_gap_get_peer(void){return peer_connected?(peer_t)1:NULL;}
void pm_activity_notify(pm_activity_t p){(void)p;}
void cat1_skt_udp_force_fail_active(void){}
void cat1_skt_udp_log_recv_diag(uint16_t seq){(void)seq;}
int remote_get_local_sw_version_bcd(uint16_t *out){*out=0x0201;return 0;}
static void NVIC_SystemReset(void){++system_resets;}
#include "udp_ota.c"
static uint8_t data_byte(unsigned seq,unsigned off){return (uint8_t)(seq*23u+off*7u);}
static uint16_t program_sum(unsigned packets){uint16_t sum=0;unsigned seq,i;for(seq=1;seq<=packets;seq++)for(i=0;i<512;i++)sum=(uint16_t)(sum+data_byte(seq,i));return sum;}
static void build_chunk(uint8_t f[547],unsigned seq,unsigned packets){unsigned i;frame_header(f,547);ota_write_be16(f+11,(uint16_t)packets);ota_write_be16(f+13,(uint16_t)seq);ota_write_be32(f+15,FLASH_APP2_START);ota_write_be32(f+19,FLASH_APP2_START+packets*512u-1);ota_write_be16(f+23,0x0202);ota_write_be16(f+25,program_sum(packets));ota_write_be32(f+27,FLASH_APP2_START+(seq-1)*512u);ota_write_be16(f+31,512);for(i=0;i<512;i++)f[33+i]=data_byte(seq,i);f[545]=ota_frame_crc_to_cs(ota_frame_get_crc(f+3,542));}
static void request(void){const uint8_t *p;uint16_t n;assert(udp_ota_poll_tx_get(&p,&n)==1&&n==24);udp_ota_on_uplink_issued();}
static void receive(unsigned seq,unsigned packets){uint8_t f[547];request();build_chunk(f,seq,packets);assert(udp_ota_on_rx(f,547)==1);udp_ota_poll_work();assert(s_written_seq==seq);}
int main(void){unsigned i,n;uint8_t f[547];BOOTLOAD_PARA_STRUCT para;OTA_CHECKPOINT_STRUCT cp;
 reset();now_ms=1000;g_audio_pcm_enabled=1;udp_ota_init();udp_ota_on_tag11_task(0x0202,400);
 assert(udp_ota_session_busy()&&ir_cancels==1&&learn_stops==1&&voice_stops==1&&!g_audio_pcm_enabled&&hid_kept==1);
 memset(flash_image+FLASH_IR_RAW_START-0x01000000u,0x3c,0x6000);
 for(i=1;i<=400;i++){
  if(i==9){assert(udp_bootload_read_checkpoint(&cp)&&cp.written_seq==8);udp_ota_init();udp_ota_on_tag11_task(0x0202,400);assert(s_expect_seq==9&&s_written_seq==8);}
  if(i==17){build_chunk(f,16,400);assert(!udp_ota_on_rx(f,547)&&s_written_seq==16);build_chunk(f,18,400);assert(!udp_ota_on_rx(f,547)&&s_written_seq==16);}
  receive(i,400);
 }
 request();assert(s_dl_phase==OTA_DL_VERIFY);{const uint8_t *p;uint16_t len;assert(udp_ota_poll_tx_get(&p,&len)==-2);}
 assert(system_resets==1&&!udp_bootload_read_para(&para)&&para.prog_success_flag==OTA_UPGRADE_FLAG&&para.prog_length==204800);
 for(i=0;i<0x6000;i++)assert(flash_image[FLASH_IR_RAW_START-0x01000000u+i]==0x3c);
 reset();udp_ota_init();udp_ota_on_tag11_task(0x0202,9);for(i=1;i<=8;i++)receive(i,9);
 n=erases;for(i=0;i<CAT1_OTA_CHUNK_RETRY_MAX;i++){request();udp_ota_on_uplink_timeout();}
 assert(!udp_ota_is_active()&&udp_ota_session_busy());assert(udp_bootload_read_checkpoint(&cp)&&cp.written_seq==8);
 assert(udp_ota_resume_after_pause()&&s_expect_seq==9&&erases==n);receive(9,9);
 /* Read failure at final verification cannot commit an install flag. */
 peer_connected=0;request();fail_read=reads+1;{const uint8_t *p;uint16_t len;(void)udp_ota_poll_tx_get(&p,&len);}fail_read=0;
 assert(!udp_ota_session_busy()&&system_resets==1&&restarted==1);assert(!udp_bootload_read_para(&para)&&para.prog_success_flag!=OTA_UPGRADE_FLAG);
 reset();udp_ota_init();n=erases;udp_ota_on_tag11_task(0x0202,401);udp_ota_on_tag11_task(0x0201,10);assert(!udp_ota_session_busy()&&erases==n);
 puts("OTA integrated flow: complete 400-chunk image, reset/resume at chunk 8, duplicate/out-of-order rejection, PAUSE/retry, IR-bank preservation, final read failure forbids install PASS");return 0;}
