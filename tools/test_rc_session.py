"""Exercise actual RC CONF apply + forced-session restore with deterministic mocks."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="vcr_rc_session_") as d:
 p=Path(d)
 (p/"gap.h").write_text("#include <stdint.h>\ntypedef void *peer_t;\n")
 (p/"gatt.h").write_text("#include <stdint.h>\ntypedef void *gatt_chr_t;\n")
 code=r'''#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef void *peer_t;
#include "gatt.h"
#include "rc_protocol.h"
static rc_status_t s_rc_status;
static rc_conf_t s_rc_conf;
static uint8_t s_rc_conf_valid;
static uint8_t s_rc_conf_session;
static peer_t s_rc_peer;
static uint8_t s_rc_cccd_subscribed;
static gatt_chr_t s_chr_status=(void*)0x1234,s_chr_key_notify=(void*)0x5678;
static unsigned notify_count,indicate_count,trans_resets,policy_fills;
static rc_status_t last_notified,last_indicated;
static peer_t last_notify_peer,last_indicate_peer;
void rc_trans_session_reset(void){++trans_resets;}
void rc_cat1_get_cloud_policy(uint8_t *dst){++policy_fills;*dst=0x5au;}
void ble_rc_svc_status_notify(peer_t peer,const rc_status_t *st){++notify_count;last_notify_peer=peer;memcpy(&last_notified,st,sizeof(*st));}
void ble_rc_svc_status_indicate(peer_t peer,const rc_status_t *st){++indicate_count;last_indicate_peer=peer;memcpy(&last_indicated,st,sizeof(*st));}
void remote_key_capture_on_ble_disconnect(void){}
static int mock_log(const char *fmt,...){(void)fmt;return 0;}
#define co_printf mock_log
'''
 source=(r/'bleuart/cat1/cat1/ble_rc_svc.c').read_text(encoding='utf-8')
 def extract(prefix):
  start=source.index(prefix); brace=source.index("{",start);level=1;i=brace+1
  while level:
   level+=(source[i]=="{")-(source[i]=="}");i+=1
  return source[start:i]
 code+=extract("static uint16_t rc_be16")+"\n"
 code+=extract("static const char *rc_state_name")+"\n"
 code+=extract("uint8_t ble_rc_svc_needs_restore")+"\n"
 code+=extract("static int rc_conf_apply")+"\n"
 code+=extract("void ble_rc_svc_on_connect")+"\n"
 code+=extract("void ble_rc_svc_force_cccd_enable")+"\n"
 code+=extract("void ble_rc_svc_on_disconnect")+"\n"
 code+=extract("uint8_t ble_rc_svc_is_subscribed")+"\n"
 code+=r'''
int main(void){
 static const uint8_t valid[RC_CONF_LEN]={0x30,0x30,0x30,0x31,0x01,0x00,0x80,0x03,0x01,0xF4,0,0,0,0,0,0};
 uint8_t bad[RC_CONF_LEN];peer_t p=(void*)1;
 /* 无有效 CONF: 连接/强开都不恢复会话, 强开只开通道 */
 ble_rc_svc_on_connect(p);
 assert(notify_count==0&&indicate_count==0&&s_rc_status.state==RC_STATE_IDLE);
 ble_rc_svc_force_cccd_enable(p);
 assert(s_rc_cccd_subscribed==1&&ble_rc_svc_is_subscribed()&&notify_count==0&&indicate_count==0&&s_rc_status.state==RC_STATE_IDLE);
 ble_rc_svc_on_disconnect();assert(!s_rc_cccd_subscribed&&!s_rc_conf_valid);
 /* 坏 CONF 全部拒绝且不置 valid */
 memcpy(bad,valid,sizeof(bad));
 assert(rc_conf_apply(NULL,RC_CONF_LEN,p)<0);
 assert(rc_conf_apply(valid,RC_CONF_LEN-1,p)<0);
 memcpy(bad,valid,sizeof(bad));bad[4]=2;   assert(rc_conf_apply(bad,RC_CONF_LEN,p)<0);
 memcpy(bad,valid,sizeof(bad));bad[5]=0;bad[6]=1; assert(rc_conf_apply(bad,RC_CONF_LEN,p)<0);
 memcpy(bad,valid,sizeof(bad));bad[7]=4;   assert(rc_conf_apply(bad,RC_CONF_LEN,p)<0);
 memcpy(bad,valid,sizeof(bad));bad[8]=0;bad[9]=0x63; assert(rc_conf_apply(bad,RC_CONF_LEN,p)<0);
 assert(!s_rc_conf_valid&&s_rc_status.state==RC_STATE_IDLE&&notify_count==0);
 /* 有效 CONF → READY + 推送 STATUS (Notify, 原正常路径) */
 assert(rc_conf_apply(valid,RC_CONF_LEN,p)==0);
 assert(s_rc_conf_valid&&s_rc_conf_session&&!ble_rc_svc_needs_restore()&&s_rc_status.state==RC_STATE_READY&&notify_count==1&&indicate_count==0&&trans_resets==1&&policy_fills==1);
 /* 普通断连: 复位会话状态, 但保留 valid 供下次恢复 (needs_restore 生效) */
 ble_rc_svc_on_disconnect();
 assert(!s_rc_cccd_subscribed&&s_rc_conf_valid&&!s_rc_conf_session&&ble_rc_svc_needs_restore()&&s_rc_status.state==RC_STATE_IDLE&&s_rc_status.pack_index==RC_PACK_INDEX_NONE);
 /* 回连 t=0: 连接即恢复 READY 并用 Indicate 推送 (规范 §2 结果须 Indicate) */
 ble_rc_svc_on_connect(p);
 assert(s_rc_peer==p&&indicate_count==1&&last_indicate_peer==p);
 assert(last_indicated.state==RC_STATE_READY&&last_indicated.result==RC_RESULT_OK&&last_indicated.retry_count==0&&policy_fills==2);
 /* 6s 强开: 幂等再恢复一次 (Indicate, 订阅标志置位) */
 ble_rc_svc_force_cccd_enable(p);
 assert(s_rc_cccd_subscribed==1&&indicate_count==2&&notify_count==1&&s_rc_status.state==RC_STATE_READY);
 /* 恢复后 STB 正常 CONF 仍可重入 */
 assert(rc_conf_apply(valid,RC_CONF_LEN,p)==0&&notify_count==2&&indicate_count==2&&!ble_rc_svc_needs_restore()&&s_rc_status.state==RC_STATE_READY);
 puts("RC session: invalid CONF atomic rejection, valid CONF READY, disconnect keeps valid, connect-time restore re-READYs via Indicate before first mirror, 6s force idempotent, session-scoped CONF flag, normal CONF re-entry PASS");
 return 0;
}
'''
 (p/"test.c").write_text(code)
 inc=['-I'+str(p),'-I'+str(r/'bleuart/cat1/cat1')]
 try:
  subprocess.run(['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter']+inc+[str(p/'test.c'),'-o',str(p/'t.exe')],check=True)
 except subprocess.CalledProcessError as e:
  subprocess.run(['gcc','-std=c99','-O0']+inc+[str(p/'test.c'),'-o',str(p/'t.exe')])
  raise e
 subprocess.run([str(p/'t.exe')],check=True)
