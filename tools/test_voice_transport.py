"""Exercise actual GATT preflight and voice transport with deterministic mocks."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="vcr_voice_transport_") as d:
 p=Path(d)
 (p/"br1601x.h").write_text("#include <stdint.h>\n")
 (p/"gatt.h").write_text("""#ifndef MOCK_GATT
#define MOCK_GATT
#include <stdint.h>
typedef void *peer_t;typedef void *gatt_chr_t;typedef int gatt_rst_t;
#define GATT_OK 0
#define GATT_RES_INVALID_HANDLE 1
#define GATT_RES_INSUFFICIENT_RESOURCES 17
#endif
""")
 for name in ["cs_core.h","co_printf.h","gmem.h"]:(p/name).write_text("")
 code=r'''#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static uint32_t mask,now,notify_count,indicate_count,alloc_count;
static unsigned fail;static size_t probed;
static uint8_t bleConnectOk=0xa6,bleSendEn=0xa6;
static void *dtss_peer=(void*)1,*dtss_chr=(void*)2;
static uint32_t app_now_ms(void){return now;}
static uint32_t __get_PRIMASK(void){return mask;}
static void __disable_irq(void){mask=1;}
static void __set_PRIMASK(uint32_t m){mask=m;}
static void *mock_malloc(size_t n){assert(mask==1);++alloc_count;probed=n;return fail?NULL:malloc(n);}
static void mock_free(void *v){assert(mask==1);free(v);}
static int gatts_chr_val_notif(void *p,void *c,uint8_t *v,uint16_t n){assert(mask==1&&p&&c&&v&&n);++notify_count;return 0;}
static int gatts_chr_val_indicate(void *p,void *c,uint8_t *v,uint16_t n){assert(mask==1&&p&&c&&v&&n);++indicate_count;return 0;}
#define malloc mock_malloc
#define free mock_free
#define co_printf(...) ((void)0)
#include "blecomm.c"
void test_cccd(void);
int main(void){
 uint8_t data[64]={0};unsigned n;
 assert(app_gatt_notify(0,dtss_chr,data,64)==GATT_RES_INVALID_HANDLE&&!alloc_count);
 fail=1;assert(app_gatt_notify(dtss_peer,dtss_chr,data,64)==17&&!notify_count&&!mask&&probed==576);
 mask=1;assert(app_gatt_notify(dtss_peer,dtss_chr,data,64)==17&&mask==1);mask=0;fail=0;
 assert(app_gatt_indicate(dtss_peer,dtss_chr,data,64)==0&&indicate_count==1&&!mask);
 bleResetVoiceSbcSeq();bleSendEn=0;bleSendVoiceSbcFrame(data,64);assert(s_voice_no_link==1&&!notify_count);
 bleSendEn=0xa6;bleSendVoiceSbcFrame(data,64);assert(s_voice_submitted==1&&notify_count==1);
 for(n=0;n<100;n++)bleSendVoiceSbcFrame(data,64);
 assert(s_voice_submitted==1&&s_voice_dropped==100&&g_voice_sbc_ts==808);
 now=8;fail=1;bleSendVoiceSbcFrame(data,64);assert(s_voice_dropped==101&&s_voice_last_error==17&&notify_count==1);
 now=16;fail=0;bleSendVoiceSbcFrame(data,64);assert(s_voice_submitted==2&&notify_count==2);
 bleResetVoiceSbcSeq();now=0xfffffffcu;bleSendVoiceSbcFrame(data,64);now+=8;bleSendVoiceSbcFrame(data,64);assert(s_voice_submitted==2);
 bleResetVoiceSbcSeq();assert(!s_voice_submitted&&!s_voice_dropped&&!s_voice_no_link&&!s_voice_sent);
 test_cccd();
 puts("voice transport: IRQ restoration, low-memory rejection before SDK entry, indication/notification, subscription gate, 100-frame burst bound, source timestamps, wrap PASS");
 return 0;
}
'''

 source=(r/"cs/src/ble_dtss.c").read_text(encoding="utf-8")
 def extract(name, result="void"):
  start=source.index(result+" "+name+"("); brace=source.index("{",start);level=1;i=brace+1
  while level:
   level+=(source[i]=="{")-(source[i]=="}");i+=1
  return source[start:i]
 code+=r'''
#include "gatt.h"
typedef int gatt_event_t;
#define GATTS_EV_CHR_VALUE_READ_REQ 18
#define GATTS_EV_CHR_VALUE_SEND_START 21
#define GATT_SEND_VAL_NOTIFICATION 1
#define GATT_SEND_VAL_INDICATION 2
typedef struct{peer_t peer;gatt_chr_t chr;}gatt_chr_val_read_req_t;
typedef struct{peer_t peer;gatt_chr_t chr;int type;}gatt_chr_val_send_start_t;
#define BLE_DTSS_DATA_READY 1u
#define BLE_DTSS_CTRL_READY 2u
#define VOICE_CODEC_SBC 1u
#define VOICE_CTRL_STATE_IDLE 2u
#define VOICE_CTRL_RESP_CMD 1u
static unsigned stop_count;
static int voice_ctrl_post_stop(void){++stop_count;return 1;}
static peer_t active_peer=(void*)1;
static peer_t ble_gap_get_peer(void){return active_peer;}
static uint8_t voice_selected_codec,voice_control_state;
static uint8_t voice_capture_enabled,voice_chr2_value[8],voice_chr2_len=2;
static void *voice_indication_chr,*voice_indication_peer;
static void gatts_chr_val_read_rsp(void *p,void *c,int e,uint8_t*v,unsigned n){(void)p;(void)c;(void)e;(void)v;(void)n;}
'''
 code+=extract("dtss_chr0_cbk")+"\n"+extract("dtss_chr2_cbk")+"\n"+extract("ble_dtss_on_disconnect")+"\n"+extract("ble_dtss_ready_channels","uint8_t")
 code+=r'''
void test_cccd(void){
 gatt_chr_val_send_start_t in={(void*)1,(void*)2,1};
 dtss_chr0_cbk(21,0,&in);assert(bleSendEn==0xa6&&dtss_peer==in.peer&&dtss_chr==in.chr);
 in.type=0;dtss_chr0_cbk(21,0,&in);assert(!bleSendEn&&!dtss_peer&&!dtss_chr);
 in.type=2;dtss_chr2_cbk(21,0,&in);assert(voice_indication_chr&&voice_indication_peer);
 in.type=0;dtss_chr2_cbk(21,0,&in);assert(!voice_indication_chr&&!voice_indication_peer&&stop_count==2);
 /* Reconnect is not ready until BOTH channels subscribe on the new peer. */
 ble_dtss_on_disconnect();assert(!ble_dtss_ready_channels()&&!bleSendEn&&!dtss_peer&&!dtss_chr);
 active_peer=(void*)3;
 in.peer=(void*)1;in.type=1;dtss_chr0_cbk(21,0,&in);assert(!ble_dtss_ready_channels());
 in.peer=active_peer;dtss_chr0_cbk(21,0,&in);assert(ble_dtss_ready_channels()==1);
 in.type=2;dtss_chr2_cbk(21,0,&in);assert(ble_dtss_ready_channels()==3);
 in.peer=(void*)1;in.type=0;dtss_chr0_cbk(21,0,&in);dtss_chr2_cbk(21,0,&in);assert(ble_dtss_ready_channels()==3);
 bleConnectOk=0;assert(!ble_dtss_ready_channels());bleConnectOk=0xa6;
 voice_capture_enabled=1;voice_control_state=1;ble_dtss_on_disconnect();
 assert(!ble_dtss_ready_channels()&&!voice_capture_enabled&&voice_control_state==2);
 active_peer=(void*)4;in.peer=active_peer;in.type=2;dtss_chr2_cbk(21,0,&in);assert(ble_dtss_ready_channels()==2);
 in.type=1;dtss_chr0_cbk(21,0,&in);assert(ble_dtss_ready_channels()==3);
}
'''
 (p/"voice.c").write_text(code,encoding="utf-8")
 exe=p/"voice.exe"
 subprocess.run(["gcc","-std=c99","-O2","-I"+str(p),"-I"+str(r/"bleuart/inc"),"-I"+str(r/"bleuart/src"),str(p/"voice.c"),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
