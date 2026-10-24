"""Actual manual-check bridge functions with deterministic lifecycle mocks."""
from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1]
s=(r/'bleuart/cat1/cat1/rc_cat1_bridge.c').read_text(encoding='utf-8')
def function(name):
 start=re.search(r'^(?:static )?(?:void|uint8_t) '+re.escape(name)+r'\(',s,re.M).start()
 brace=s.index('{',start);level=1;end=brace+1
 while level:
  if s[end]=='{':level+=1
  elif s[end]=='}':level-=1
  end+=1
 return s[start:end]+'\n'
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
#define co_printf(...) ((void)0)
static uint8_t s_manual_tag01_pending,s_udp_ready,s_tx_waiting,s_tag01_queued;
static uint8_t s_pending_tlv,s_cloud_save_pending,s_cat1_booting;
static uint16_t s_pending_tlv_len;
static uint32_t s_pending_pack_index;
static unsigned s_sktsend_fail_streak,s_silent_dl,s_silent_rebuild_pending;
static unsigned powered,busy,ota,queue_full,queued,ota_ready,heart_ready;
#define LTE_EN_ON() (powered=1)
#define LTE_EN_IS_ON() powered
static unsigned udp_ota_session_busy(void){return ota;}
static unsigned cat1_modem_is_busy(void){return busy;}
static int remote_queue_tag01_status(void){if(queue_full)return -1;++queued;return 0;}
static unsigned remote_report_queue_count(void){return queued;}
static void remote_tag10_enqueue_set_paused(unsigned n){(void)n;}
static void udp_ota_on_udp_ready(void){++ota_ready;}
static void heartbeat_on_udp_ready(void){++heart_ready;}
static void rc_cat1_init(void){s_manual_tag01_pending=0;s_pending_tlv=0;s_udp_ready=0;s_cat1_booting=1;}
"""
body=''.join(function(n) for n in ['rc_cat1_request_upgrade_check','rc_cat1_manual_check_poll','rc_cat1_on_setup_ok','rc_cat1_power_up','rc_cat1_has_work'])
post=r"""
static void reset(void){s_manual_tag01_pending=s_udp_ready=s_tx_waiting=s_tag01_queued=s_pending_tlv=s_cloud_save_pending=s_cat1_booting=0;powered=busy=ota=queue_full=queued=ota_ready=heart_ready=0;}
int main(void){SetErrorMode(SEM_NOGPFAULTERRORBOX);
 reset();CHECK(rc_cat1_request_upgrade_check()==1);CHECK(rc_cat1_has_work());CHECK(!rc_cat1_request_upgrade_check());
 rc_cat1_manual_check_poll();CHECK(queued==0 && s_manual_tag01_pending);
 s_pending_tlv=1;s_pending_tlv_len=23;s_pending_pack_index=42;rc_cat1_power_up();
 CHECK(powered&&s_manual_tag01_pending&&s_pending_tlv&&s_pending_tlv_len==23&&s_pending_pack_index==42);
 rc_cat1_on_setup_ok();CHECK(queued==1&&!s_manual_tag01_pending&&ota_ready==1&&heart_ready==1);
 rc_cat1_manual_check_poll();CHECK(queued==1); /* no duplicate after setup */
 reset();s_udp_ready=powered=1;s_tag01_queued=1;CHECK(rc_cat1_request_upgrade_check());busy=1;
 rc_cat1_manual_check_poll();CHECK(!queued&&s_manual_tag01_pending);busy=0;s_tx_waiting=1;
 rc_cat1_manual_check_poll();CHECK(!queued);s_tx_waiting=0;queue_full=1;
 rc_cat1_manual_check_poll();CHECK(s_manual_tag01_pending);queue_full=0;rc_cat1_manual_check_poll();
 CHECK(queued==1&&!s_manual_tag01_pending);rc_cat1_manual_check_poll();CHECK(queued==1);
 CHECK(rc_cat1_request_upgrade_check());rc_cat1_manual_check_poll();CHECK(queued==2);
 reset();CHECK(rc_cat1_request_upgrade_check());queue_full=1;rc_cat1_on_setup_ok();
 CHECK(s_manual_tag01_pending&&queued==0);queue_full=0;rc_cat1_manual_check_poll();CHECK(queued==1&&!s_manual_tag01_pending);
 reset();ota=1;CHECK(!rc_cat1_request_upgrade_check());CHECK(!s_manual_tag01_pending);
 ota=0;CHECK(rc_cat1_request_upgrade_check());ota=1;rc_cat1_manual_check_poll();CHECK(!s_manual_tag01_pending&&!queued);
 reset();rc_cat1_on_setup_ok();CHECK(queued==1&&s_tag01_queued&&ota_ready==1&&heart_ready==1); /* automatic path unchanged */
 puts("manual check: request coalescing, sleep guard, cold/warm link, preserved RC-TRANS, no duplicate setup Tag01, AT/queue retry, OTA exclusion, automatic path PASS");return 0;}
"""
# Integration guards exercised at compile time by the production build, plus
# check that the new pending flag is connected to poll wake and idle guards.
assert 's_pending_tlv != 0u || s_manual_tag01_pending != 0u' in s
assert '&& s_manual_tag01_pending == 0u' in s
assert 'rc_cat1_manual_check_poll();' in s
assert 's_manual_tag01_pending = 0u;' in function('rc_cat1_power_down')
with tempfile.TemporaryDirectory(prefix='vcr_manual_check_') as t:
 p=Path(t);(p/'test.c').write_text(prefix+body+post,encoding='utf-8')
 subprocess.run(['D:/w64devkit/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=15)
