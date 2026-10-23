#include "mock_merge.h"
typedef int gap_event_t;
typedef struct{peer_t peer;uint8_t result;} gap_conn_cmpl_t;
typedef struct{peer_t peer;uint8_t result;} gap_disc_cmpl_t;
#define GAP_HCI_STATUS_OK 0
#define CCCD_RETRY_INTERVAL_MS 500
#define CCCD_RETRY_MAX 6
static peer_t g_peer,dtss_peer,dtss_chr;
static unsigned connected_notify,disconnected_notify,updates,forced,restarts,stops;
static uint8_t bleConnectOk,bleSendEn,s_disconnect_work_pending;
static uint32_t conn_update_timer_id=BR_TIMER_INVALID_HANDLER;
static unsigned used[32];
static uint8_t timer_ev[32];
static void (*timer_cb[32])(uint8_t,void*);
static int g_last_good_params;
volatile uint16_t key_receive;
static pm_state_t state=PM_STATE_RECONNECTING;
peer_t ble_gap_get_peer(void){return g_peer;}
pm_state_t pm_state_get(void){return state;}
uint8_t peer_bonded_count(void){return 1;}
static int hid_kbd_is_any_cccd_enabled(void){return 0;}
static void hid_kbd_force_cccd_enable(peer_t p){assert(p==g_peer);++forced;}
static void ble_conn_params_apply(peer_t p,const int *v){(void)v;assert(p&&p==g_peer);++updates;}
void pm_notify_connected(void){++connected_notify;state=PM_STATE_WORKING;}
void pm_notify_disconnected(void){assert(!g_peer);++disconnected_notify;}
static void ble_rc_svc_on_connect(peer_t p){assert(p==g_peer);}
static int voice_ctrl_post_stop(void){++stops;return 1;}
static void ble_dtss_on_disconnect(void){}
static void task_keyscan_reset_on_ble_disconnect(void){key_receive=0;}
static void hid_kbd_on_disconnect(void){}
static void rc_trans_reset(void){}
static void rc_cat1_cancel_pending(void){}
static void ble_rc_svc_on_disconnect(void){}
static void ble_bas_on_disconnect(void){}
static int combo_key_unpair_in_progress(void){return 0;}
void ble_adv_stop(void){}
void le_conn_destory(peer_t p){(void)p;}
void ble_adv_start_by_bonding(void){++restarts;}
uint32_t sdk_timer_add(uint32_t ms,uint8_t ev,void(*cb)(uint8_t,void*),void *arg){unsigned i;(void)ms;(void)arg;for(i=0;i<32;i++)if(!used[i]){used[i]=1;timer_ev[i]=ev;timer_cb[i]=cb;return i;}assert(0);return 255;}
int sdk_timer_del(uint32_t id){assert(id<32&&used[id]);used[id]=0;return 0;}
#include "actual_ble_lifecycle.c"
int main(void){gap_conn_cmpl_t c={(peer_t)1,5};gap_disc_cmpl_t d={(peer_t)1,0};uint8_t old;unsigned i;
 ble_gap_connected(0,0,&c);assert(!g_peer&&!bleConnectOk&&!connected_notify);
 c.result=0;ble_gap_connected(0,0,&c);assert(g_peer&&bleConnectOk==0xa6&&conn_update_timer_id==0);
 old=s_connection_generation;used[conn_update_timer_id]=0;conn_parm_updata_handle(old,0);assert(updates==1&&s_cccd_retry_tid!=BR_TIMER_INVALID_HANDLER);
 ble_gap_disconnected(0,0,&d);assert(!g_peer&&!bleConnectOk&&disconnected_notify==1&&s_cccd_retry_tid==BR_TIMER_INVALID_HANDLER);
 state=PM_STATE_SLEEP;key_receive=18;ble_gap_process_disconnect_pending();assert(key_receive==18&&!restarts);
 state=PM_STATE_RECONNECTING;ble_gap_connected(0,0,&c);cccd_retry_handle(old,0);assert(updates==1&&!forced);
 for(i=0;i<100;i++){ble_gap_disconnected(0,0,&d);ble_gap_process_disconnect_pending();ble_gap_connected(0,0,&c);}
 {unsigned count=0;for(i=0;i<32;i++)count+=used[i];assert(count==1);}
 state=PM_STATE_SLEEP;ble_gap_disconnected(0,0,&d);ble_gap_connected(0,0,&c);assert(!g_peer);
 puts("BLE lifecycle: failed connect, 100 reconnects, slot-zero timer cleanup, stale CCCD callbacks, sleep disconnect preserves wake key PASS");return 0;}
