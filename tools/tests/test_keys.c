#include "mock_app.h"
#include "task_keyscan.c"
#define OTA_DL_IDLE 0
#define OTA_LED_BLINK_PERIOD_MS 100U
static unsigned s_dl_phase=2,s_led_blink_active,s_dl_led_on;
static uint32_t s_dl_led_last_ms;
static uint32_t udp_ota_now_ms(void){return now_ms;}
#include "actual_ota_led.h"
void combo_key_process(uint32_t t,const uint32_t hw[5]){(void)t;(void)hw;}
bool combo_key_is_idle(void){return combo_idle;}
bool ir_learn_is_active(void){return learn_active;}
bool ir_learn_start(void){learn_active=true;return true;}
void ir_learn_stop(void){if(learn_active){learn_active=false;task_keyscan_ir_learn_result(IR_LEARN_EVT_CANCELED,0,0);}}
void ir_learn_process(void){}
const ir_raw_t *ir_learn_wave(void){return 0;}
void ir_learn_discard(void){}
bool ir_wave_store_save(unsigned k,const ir_raw_t *w){(void)k;(void)w;return store_ok;}
static unsigned learned_test_key,tx_fail;
bool ir_wave_store_has(unsigned k){return k==learned_test_key;}
void ir_learn_set_target(uint8_t k){last_target=k;}
uint8_t ir_learn_get_target(void){return (uint8_t)last_target;}
bool ir_wave_store_clear(void){return clear_ok;}
bool ir_send_key(uint8_t k){last_ir_key=k;++ir_presses;return !tx_fail;}
void ir_tx_process(void){}
void ir_tx_repeat(uint8_t k){(void)k;++ir_repeats;}
void ir_tx_release(void){}
bool ir_tx_pending(void){return false;}
void ir_user_reset(void){}
static void scan(unsigned t,unsigned mask,unsigned event_key){now_ms=t;keys=mask;key_receive=event_key;task_keyscan_process();}
int main(void){unsigned i;
 pm_value=PM_STATE_RECONNECTING;
 scan(0,1u<<18,18);scan(20,1u<<18,0);
 assert(ir_presses==0&&hid_presses==1&&last_hid_key==1&&last_rc_key==1&&last_rc_usage==1&&wakeup_calls==1);
 scan(5020,1u<<18,0);assert(s_ir_learn_mode==IR_LEARN_MODE_OFF); /* left STB power never learns */
 scan(5030,0,0);scan(5050,0,0);
 scan(5100,2,1);scan(5120,2,0);assert(ir_presses==0&&hid_presses==1&&wakeup_calls==1);
 assert(led1==1&&led2==0);scan(5219,2,0);assert(led1==1);
 scan(5220,2,0);assert(led1==0);scan(10119,2,0);assert(led1==0);
 i=hid_releases;
 scan(10120,2,0);assert(s_ir_learn_mode==IR_LEARN_MODE_WAIT_KEY&&hid_releases==i&&led1==1);
 scan(10130,2,1);assert(!learn_active);
 scan(10140,0,0);scan(10150,2,1);assert(learn_active&&last_target==1);
 learn_active=false;modem_busy=1;task_keyscan_ir_learn_result(IR_LEARN_EVT_DONE,0x51,0xDC);
 assert(s_ir_learn_mode==IR_LEARN_MODE_SAVING);modem_busy=0;scan(10150,0,0);
 assert(s_ir_learn_mode==IR_LEARN_MODE_FEEDBACK&&s_ir_feedback_left==6);
 for(i=1;i<=6;i++)scan(10150+500*i,0,0);
 assert(s_ir_learn_mode==IR_LEARN_MODE_WAIT_KEY&&led1==1);
 scan(13160,1u<<17,17);assert(learn_active&&last_target==17);
 learn_active=false;store_ok=false;task_keyscan_ir_learn_result(IR_LEARN_EVT_DONE,0x51,0x37);
 assert(s_ir_feedback_left==10);for(i=1;i<=10;i++)scan(13160+100*i,0,0);
 assert(s_ir_learn_mode==IR_LEARN_MODE_WAIT_KEY);
 scan(14160,1u<<15,15);assert(!learn_active);scan(14160,0,0);
 scan(14161,1u<<12,12);assert(!learn_active);scan(14162,0,0);scan(14163,1u<<2,2);assert(learn_active);ota_busy=1;scan(14170,1u<<12,0);
 assert(!learn_active&&s_ir_learn_mode==IR_LEARN_MODE_OFF);ota_busy=0;scan(14180,0,0);
 /* TV short sends only on release, never repeats or sends on learning entry. */
 i=ir_presses;scan(14181,2,1);scan(14201,2,0);assert(ir_presses==i);
 scan(14202,0,0);scan(14232,0,0);assert(ir_presses==i+1&&last_ir_key==1);
 /* No action from released debounce candidates. */
 i=hid_presses;scan(14240,1u<<4,4);scan(14260,0,0);assert(hid_presses==i);
 /* Interrupted voice is stopped and releases its reports when combo takes over. */
 scan(14300,1u<<15,15);scan(14320,1u<<15,0);scan(14370,1u<<15,0);assert(voice_starts==1);
 combo_idle=false;scan(14380,(1u<<15)|(1u<<14),0);assert(voice_stops==1);combo_idle=true;scan(14390,0,0);
 /* Failed persistent reset must not clear BLE bonding. */
 clear_ok=false;scan(15100,(1u<<8)|(1u<<14),0);scan(18100,(1u<<8)|(1u<<14),0);
 scan(18110,0,0);scan(18120,1u<<4,4);assert(factory_rst_state==FACTORY_RST_CANCELLED&&bond_clears==0);
 modem_busy=1;hid_order_len=0;
 task_keyscan_hid_send_release(0);task_keyscan_hid_send_press(4);task_keyscan_hid_send_release(0);
 modem_busy=0;task_keyscan_hid_flush_deferred();
 assert(hid_order_len==3&&memcmp(hid_order,"RPR",3)==0);
 scan(30000,0,0);scan(30010,0,0);learned_test_key=15;
 i=voice_starts;scan(30100,1u<<15,15);scan(30120,1u<<15,0);scan(30200,1u<<15,0);assert(voice_starts==i+1);
 scan(30210,0,0);scan(30240,0,0);
 connected=false;learned_test_key=17;i=ir_repeats;scan(31000,1u<<17,17);scan(31020,1u<<17,0);scan(31300,1u<<17,0);assert(ir_repeats==i);
 scan(31420,1u<<17,0);assert(ir_repeats==i+1);scan(31430,0,0);scan(31460,0,0);learned_test_key=0;
 connected=false;i=ir_presses;scan(32000,1u<<18,18);scan(32020,1u<<18,0);
 assert(ir_presses==i+1&&last_ir_key==18);scan(32030,0,0);scan(32060,0,0);
 /* Even a stale voice record cannot suppress Bluetooth voice. No BLE
  * recipient means neither microphone nor voice IR is started. */
 connected=true;learned_test_key=15;tx_fail=1;i=voice_starts;
 scan(33000,1u<<15,15);scan(33020,1u<<15,0);scan(33100,1u<<15,0);assert(voice_starts==i+1);
 scan(33110,0,0);scan(33150,0,0);connected=false;learned_test_key=0;tx_fail=0;i=voice_starts;
 scan(34000,1u<<15,15);scan(34020,1u<<15,0);scan(34100,1u<<15,0);assert(voice_starts==i);
 scan(34110,0,0);scan(34150,0,0);
 /* Learned ordinary keys use HID when connected; IR only when disconnected. */
 connected=true;learned_test_key=17;i=ir_presses;
 {unsigned h=hid_presses;
 scan(35000,1u<<17,17);scan(35020,1u<<17,0);
 assert(hid_presses==h+1&&last_hid_key==17&&ir_presses==i);
 scan(35500,1u<<17,0);assert(ir_presses==i);
 scan(35510,0,0);scan(35550,0,0);}
 learned_test_key=0;
 /* Every remaining non-power learned key preserves its Bluetooth mapping. */
 {unsigned list[3]={2,3,17},n;
 for(n=0;n<3;n++){
  unsigned k=list[n],t=36000+n*1000,h=hid_presses,ir=ir_presses;
  connected=true;learned_test_key=k;
  scan(t,1u<<k,k);scan(t+20,1u<<k,0);
  assert(hid_presses==h+1&&last_hid_key==k&&ir_presses==ir);
  scan(t+30,0,0);scan(t+70,0,0);
  connected=false;
  scan(t+100,1u<<k,k);scan(t+120,1u<<k,0);
  assert(ir_presses==ir+1&&last_ir_key==k&&hid_presses==h+1);
  scan(t+130,0,0);scan(t+170,0,0);
 }
 learned_test_key=0;}
 /* A UART transaction may span several short key presses: preserve FIFO. */
 connected=true;modem_busy=1;key_active=0;hid_order_len=0;hid_defer_clear();
 for(i=0;i<4;i++){task_keyscan_hid_send_press(4+i);task_keyscan_hid_send_release(0);}
 assert(s_hid_count==8&&!hid_order_len);modem_busy=0;while(s_hid_count)task_keyscan_hid_flush_deferred();
 assert(hid_order_len==8&&!memcmp(hid_order,"PRPRPRPR",8));
 modem_busy=1;hid_order_len=0;
 for(i=0;i<20;i++){task_keyscan_hid_send_press(4);task_keyscan_hid_send_release(0);}
 assert(s_hid_reset_pending);modem_busy=0;while(s_hid_count||s_hid_reset_pending)task_keyscan_hid_flush_deferred();
 assert(hid_order[0]=='R'&&hid_order[1]=='R'&&hid_order[hid_order_len-1]=='R');
 /* A wake red LED survives outside the normal key state machine unless
  * combo cancellation/release explicitly clears it. Preserve blue ownership. */
 combo_idle=false;led1=1;led2=1;key_state=KEY_STATE_IDLE;
 scan(50000,(1u<<14)|(1u<<5),0);assert(s_ir_suppress_until_release);
 combo_idle=true;scan(50020,0,0);assert(!led1&&led2==1&&!s_ir_suppress_until_release);
 led1=1;scan(50030,0,0);assert(!led1&&led2==1);
 /* The same idle cleanup must not overwrite solid learning indication. */
 s_ir_learn_mode=IR_LEARN_MODE_WAIT_KEY;s_ir_learn_mode_ts_ms=50040;led1=0;
 scan(50040,0,0);assert(led1==1);s_ir_learn_mode=IR_LEARN_MODE_OFF;
 scan(50050,0,0);assert(!led1);
 /* During OTA, no-key cleanup, press/release and learning cancellation must
  * not shorten real OTA on/off phases. Re-run keyscan after every blink tick. */
 ota_busy=1;led1=0;led2=1;s_dl_phase=2;s_dl_led_on=0;s_dl_led_last_ms=60000;
 for(i=0;i<600;i++){
  unsigned mask=(i>=250&&i<280)?(1u<<4):0;
  unsigned expected=(i/100)%2;
  scan(60000+i,mask,i==250?4:0);udp_ota_download_led_poll();
  assert(led1==expected);
  scan(60000+i,mask,0);assert(led1==expected);
 }
 /* Direct ordinary on/off requests are suppressed equally while OTA owns red. */
 led1=0;led2=1;task_keyscan_red_led(true);assert(!led1&&led2==1);
 led1=1;task_keyscan_red_led(false);assert(led1);
 ota_busy=0;task_keyscan_red_led(false);assert(!led1);
 task_keyscan_red_led(true);assert(led1);scan(60700,0,0);assert(!led1);
 puts("keys: POWER learning, held-entry suppression, 18 keys, feedback/save failure, OTA cancel/100ms LED ownership, debounce, voice release, reset failure, deferred HID order PASS");return 0;}
