#ifndef MOCK_APP_H
#define MOCK_APP_H
#include "mock_hw.h"
static uint32_t keys;
static bool connected=true,combo_idle=true,learn_active,store_ok=true,clear_ok=true;
static unsigned hid_presses,hid_releases,rc_releases,voice_starts,voice_stops,ir_presses,ir_repeats,bond_clears,pair_calls,switch_calls;
static unsigned last_pair_tv,led1,led2,last_target,last_ir_key;
static int pm_value;
static unsigned adv_pair_calls;
static unsigned wakeup_calls,last_hid_key,last_rc_key,last_rc_usage;
static char hid_order[64];static unsigned hid_order_len;
#define SETLED1 do{led1=1;}while(0)
#define CLRLED1 do{led1=0;}while(0)
#define SETLED2 do{led2=1;}while(0)
#define CLRLED2 do{led2=0;}while(0)
#define PM_STATE_PAIRING 0
#define PM_STATE_RECONNECTING 1
#define PM_STATE_WORKING 2
#define PM_STATE_SLEEP 4
#define PM_ACTIVITY_KEY 0
#define COMBO_KEY_HOLD_MS 3000u
#define COMBO_LED_FLASH_MS 200u
#define COMBO_UNPAIR_LED_MIN_MS 2000u
#define COMBO_UNPAIR_TIMEOUT_MS 30000u
static bool g_pairing_active;
static unsigned modem_busy;
static unsigned cat1_modem_is_busy(void){return modem_busy;}
static void cs_schedule(void){}
static void *ble_gap_get_peer(void){return connected?(void*)1:NULL;}
static void ble_gap_clear_bonding_info(void){++bond_clears;}
static void ble_adv_start_pairing(void){++adv_pair_calls;}
static void ble_adv_start_wakeup(void){++wakeup_calls;}
static void pm_activity_notify(unsigned x){(void)x;}
static int pm_state_get(void){return pm_value;}
static void pm_state_transition(int x){pm_value=x;}
static bool keyscan_is_key_pressed(const uint32_t *hw,unsigned k){return (hw[0]&(1u<<k))!=0;}
static void KEYSCAN_ReadKeyData(uint32_t *hw){hw[0]=keys;hw[1]=0;}
static uint8_t app_keyscan_read_key_id(void){unsigned k;if(keys&(keys-1u))return 0;for(k=1;k<=18;k++)if(keys&(1u<<k))return k;return 0;}
static bool hid_kbd_map_key_to_usage(unsigned k,uint16_t *u,bool *c){*u=(uint16_t)k;*c=false;return true;}
static void hid_kbd_send_key_id_current(unsigned k){last_hid_key=k;++hid_presses;hid_order[hid_order_len++]='P';}
static void hid_kbd_send_release_current(void){++hid_releases;hid_order[hid_order_len++]='R';}
static void hid_kbd_send_consumer_release_current(void){++hid_releases;hid_order[hid_order_len++]='R';}
static void rc_key_input_event(unsigned t,unsigned a,unsigned u,unsigned k){(void)t;last_rc_key=k;last_rc_usage=u;if(!a)++rc_releases;}
static int voice_ctrl_post_start(void){++voice_starts;return 0;}
static int voice_ctrl_post_stop(void){++voice_stops;return 0;}
#endif
