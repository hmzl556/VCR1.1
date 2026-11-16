#include "mock_merge.h"
#include "pm_state.c"
static unsigned bonds,entered,exited,sleeps,resumes,restarts;
static unsigned business_pending;
static bool ir_busy;static uint8_t hb_pending;
volatile bool g_pairing_active;
volatile uint8_t g_audio_pcm_enabled;
uint8_t peer_bonded_count(void){return bonds;}
bool task_keyscan_ir_busy(void){return ir_busy;}
uint8_t cat1_modem_is_busy(void){return modem_busy;}
uint8_t rc_cat1_has_work(void){return modem_busy || business_pending;}
uint8_t heartbeat_needs_cat1_poll(void){return hb_pending;}
void pm_standby_enter(void){++entered;}
void pm_standby_exit(void){++exited;}
uint8_t pm_sleep_enter(void){++sleeps;pm_notify_disconnected();return 1;}
void rc_cat1_init(void){}
void sdk_timer_resume(void){++resumes;}
void sdk_timer_pause(void){}
void ble_adv_start_by_bonding(void){++restarts;}
int main(void){
 pm_state_init();assert(pm_state_get()==PM_STATE_SILENT);
 now_ms=59000;pm_activity_notify(PM_ACTIVITY_KEY);now_ms=60001;pm_state_process();assert(!sleeps);
 bonds=1;pm_state_sync_after_ble_init();assert(pm_state_get()==PM_STATE_RECONNECTING);
 pm_notify_connected();assert(pm_state_get()==PM_STATE_WORKING);

 /* 硬阻止：红外 / OTA / 键值下载 / 语音 —— 无条件拒绝低功耗 */
 ir_busy=true;now_ms=100000;pm_state_process();assert(!entered);
 pm_state_transition(PM_STATE_SLEEP);assert(!sleeps);ir_busy=false;
 ota_busy=1;pm_state_transition(PM_STATE_SLEEP);assert(!sleeps);ota_busy=0;
 upg_busy_mock=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);
 pm_state_transition(PM_STATE_SLEEP);assert(!sleeps);
 now_ms+=600000;pm_state_process();assert(!entered&&pm_state_get()==PM_STATE_WORKING);
 upg_busy_mock=0;
 g_audio_pcm_enabled=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);g_audio_pcm_enabled=0;

 /* 软阻止（CAT1 待发）：先拒绝，等满 PM_CAT1_WAIT_BEFORE_SLEEP_MS 后放行，
  * 数据留在队列里，避免 4G 无网时永远不休眠 */
 modem_busy=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);
 now_ms+=PM_CAT1_WAIT_BEFORE_SLEEP_MS-1u;pm_state_transition(PM_STATE_STANDBY);assert(!entered);
 now_ms+=2u;pm_state_transition(PM_STATE_STANDBY);assert(entered==1);
 pm_activity_notify(PM_ACTIVITY_KEY);assert(exited==1&&pm_state_get()==PM_STATE_WORKING);

 /* 软阻止消失后窗口复位，下一次重新计时 */
 modem_busy=0;pm_state_transition(PM_STATE_STANDBY);assert(entered==2);
 pm_activity_notify(PM_ACTIVITY_KEY);assert(exited==2);
 hb_pending=1;pm_state_transition(PM_STATE_STANDBY);assert(entered==2);
 hb_pending=0;pm_state_transition(PM_STATE_STANDBY);assert(entered==3);
 pm_activity_notify(PM_ACTIVITY_KEY);assert(exited==3);

 /* 时间回绕 */
 now_ms=0xfffffff0u;pm_activity_notify(PM_ACTIVITY_KEY);now_ms=0x20;assert(pm_get_idle_ms()==48);
 pm_state_transition(PM_STATE_STANDBY);assert(entered==4);
 pm_activity_notify(PM_ACTIVITY_KEY);assert(exited==4&&pm_state_get()==PM_STATE_WORKING);

 pm_state_transition(PM_STATE_STANDBY);assert(entered==5);
 g_pairing_active=true;pm_state_transition(PM_STATE_SLEEP);
 assert(sleeps==1&&resumes==1&&restarts==1&&!g_pairing_active&&pm_state_get()==PM_STATE_RECONNECTING);
 puts("PM: SILENT/bond sync, IR/OTA/UPG/voice hard guards, CAT1 soft guard bounded then allowed, "
      "window reset, standby exit before sleep, retained-stack wake and reconnect PASS");
 return 0;}
