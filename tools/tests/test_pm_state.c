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
int main(void){pm_state_init();assert(pm_state_get()==PM_STATE_SILENT);now_ms=59000;pm_activity_notify(PM_ACTIVITY_KEY);now_ms=60001;pm_state_process();assert(!sleeps);bonds=1;pm_state_sync_after_ble_init();assert(pm_state_get()==PM_STATE_RECONNECTING);
 pm_notify_connected();assert(pm_state_get()==PM_STATE_WORKING);
 ir_busy=true;now_ms=100000;pm_state_process();assert(!entered);pm_state_transition(PM_STATE_SLEEP);assert(!sleeps);
 ir_busy=false;modem_busy=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);modem_busy=0;
 hb_pending=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);hb_pending=0;
 ota_busy=1;pm_state_transition(PM_STATE_SLEEP);assert(!sleeps);ota_busy=0;
 g_audio_pcm_enabled=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);g_audio_pcm_enabled=0;
 business_pending=1;pm_state_transition(PM_STATE_STANDBY);assert(!entered);business_pending=0;
 now_ms=0xfffffff0u;pm_activity_notify(PM_ACTIVITY_KEY);now_ms=0x20;assert(pm_get_idle_ms()==48);
 pm_state_transition(PM_STATE_STANDBY);assert(entered==1);pm_activity_notify(PM_ACTIVITY_KEY);assert(exited==1&&pm_state_get()==PM_STATE_WORKING);
 pm_state_transition(PM_STATE_STANDBY);g_pairing_active=true;pm_state_transition(PM_STATE_SLEEP);
 assert(exited==2&&sleeps==1&&resumes==1&&restarts==1&&!g_pairing_active&&pm_state_get()==PM_STATE_RECONNECTING);
 puts("PM: SILENT/bond sync, IR/OTA/CAT1/heartbeat sleep guards, standby exit before sleep, retained-stack wake and reconnect PASS");return 0;}
