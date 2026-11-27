#include "mock_app.h"
#include "combo_key.c"
void ir_send_pair_code_target(bool tv){if(!tv){assert(bond_clears>0u);assert(g_pairing_active);}++pair_calls;last_pair_tv=tv;}
bool ir_user_switch_stb(void){++switch_calls;return false;}
bool ir_user_switch_tv(void){++switch_calls;return true;}
static unsigned mode_calls,last_mode;
void task_keyscan_set_control_mode(uint8_t m){++mode_calls;last_mode=m;}
uint8_t task_keyscan_control_mode(void){return 0u;}
static void scan(unsigned t,unsigned mask){uint32_t hw[5]={mask,0,0,0,0};now_ms=t;combo_key_process(t,hw);}
int main(void){unsigned i;unsigned tv=(1u<<14)|(1u<<3),stb=tv;
 unsigned unpair=(1u<<9)|(1u<<3);
 pm_value=PM_STATE_WORKING;combo_key_init();
 assert(COMBO_COUNT==4);
 assert(g_combos[COMBO_IDX_USER_STB].key1==12&&g_combos[COMBO_IDX_USER_STB].key2==8);
 assert(g_combos[COMBO_IDX_USER_TV].key1==12&&g_combos[COMBO_IDX_USER_TV].key2==10);
 assert(g_combos[COMBO_IDX_PAIR_STB].key1==14&&g_combos[COMBO_IDX_PAIR_STB].key2==3);
 assert(g_combos[COMBO_IDX_UNPAIR_STB].key1==9&&g_combos[COMBO_IDX_UNPAIR_STB].key2==3);
 scan(0,tv);scan(2999,tv);assert(pair_calls==0);scan(3000,tv);
 assert(pair_calls==1&&!last_pair_tv&&bond_clears==1&&adv_pair_calls==1&&g_pairing_active);
 for(i=3010;i<12000;i+=10)scan(i,tv);assert(pair_calls==1);scan(12010,0);assert(combo_key_is_idle());
 /* Pairing also works disconnected, without duplicate TV IR actions. */
 connected=false;scan(12100,stb);scan(15100,stb);
 assert(pair_calls==2&&!last_pair_tv&&bond_clears==2&&adv_pair_calls==2);scan(15110,0);
 scan(26000,tv|(1u<<2));scan(30000,tv|(1u<<2));assert(pair_calls==2);scan(30010,0);
 ota_busy=1;scan(31000,tv);scan(34000,tv);ota_busy=0;scan(35000,tv);assert(pair_calls==2);
 scan(35010,0);scan(36000,tv);scan(39000,tv);assert(pair_calls==3);scan(39010,0);
 /* New unpair combo clears once, waits for disconnect, then sleeps. */
 {unsigned before=bond_clears;
  combo_key_init();connected=true;pm_value=PM_STATE_WORKING;
  scan(130000,unpair);scan(132999,unpair);assert(bond_clears==before);
  scan(133000,unpair);assert(bond_clears==before+1&&combo_key_unpair_in_progress());
  scan(133010,unpair);assert(bond_clears==before+1);
  connected=false;scan(135000,unpair);assert(pm_value==PM_STATE_SLEEP);
  scan(135010,0);assert(combo_key_is_idle());}
 /* Missing a single sample must not repeatedly cancel a held pair. */
 {unsigned before=pair_calls;
  combo_key_init();scan(200000,tv);
  for(i=200008;i<203000;i+=8)scan(i,(i%32==8)?(1u<<14):tv);
  scan(203000,tv);assert(pair_calls==before+1);
  scan(210000,tv);assert(pair_calls==before+1);scan(210010,0);
 }
 /* A real release cancels; a new press needs a new full 3-second hold. */
 {unsigned before=pair_calls;
  combo_key_init();scan(220000,tv);scan(220008,0);scan(220107,0);
  assert(g_combos[COMBO_IDX_PAIR_STB].state==COMBO_HOLDING);
  scan(220108,0);assert(combo_key_is_idle());
  scan(220200,tv);scan(223199,tv);assert(pair_calls==before);
  scan(223200,tv);assert(pair_calls==before+1);scan(223210,0);
 }
 /* A missing snapshot at 3 seconds cannot trigger; long gaps cannot join. */
 {unsigned before=pair_calls;
  combo_key_init();scan(230000,tv);scan(232999,0);scan(233000,0);
  assert(pair_calls==before);scan(233008,tv);assert(pair_calls==before+1);scan(233010,0);
  combo_key_init();scan(240000,tv);scan(240008,0);scan(243000,tv);
  assert(pair_calls==before+1);scan(245999,tv);assert(pair_calls==before+1);
  scan(246000,tv);assert(pair_calls==before+2);scan(246010,0);
 }
 /* The filter never allows a third key to trigger the held combo. */
 {unsigned before=pair_calls;
  combo_key_init();scan(250000,tv);scan(252999,tv|(1u<<2));
  scan(253000,tv|(1u<<2));scan(253100,tv|(1u<<2));assert(pair_calls==before);
  scan(253110,0);assert(combo_key_is_idle());
 }
 /* The STB pairing path also tolerates scan dropout and fires once. */
 {unsigned before=pair_calls,clear=bond_clears,adv=adv_pair_calls;
  combo_key_init();connected=false;scan(260000,stb);scan(260008,1u<<14);
  scan(260040,stb);scan(263000,stb);scan(270000,stb);
  assert(pair_calls==before+1&&!last_pair_tv&&bond_clears==clear+1&&adv_pair_calls==adv+1);
  scan(270010,0);
 }
 /* Tick wrap preserves both hold and release thresholds. */
 {unsigned before=pair_calls;uint32_t start=0xfffffff0u;
  combo_key_init();scan(start,tv);scan(start+8u,0);scan(start+40u,tv);
  scan(start+3000u,tv);assert(pair_calls==before+1);scan(start+3010u,0);
 }
 /* User-code combos: 3s hold switches code once, then slow-2 / fast-3 feedback. */
 {unsigned before=switch_calls;
  /* VOL+ & MENU -> TV, stub returns default -> slow blink (4 half-periods x 500ms) */
  combo_key_init();scan(290000,(1u<<12)|(1u<<10));
  assert(switch_calls==before);
  scan(292999,(1u<<12)|(1u<<10));assert(switch_calls==before);
  scan(293000,(1u<<12)|(1u<<10));
  assert(switch_calls==before+1);
  scan(293001,(1u<<12)|(1u<<10));assert(switch_calls==before+1);
  assert(g_combos[COMBO_IDX_USER_TV].state==COMBO_FLASHING);
  assert(led1==1&&led2==1);
  scan(293501,(1u<<12)|(1u<<10));assert(led1==0&&led2==0);
  scan(294001,(1u<<12)|(1u<<10));assert(led1==1&&led2==1);
  scan(294501,(1u<<12)|(1u<<10));assert(led1==0&&led2==0);
   /* 4th half-period ends the slow blink: LEDs off, state WAIT_RELEASE */
   scan(295001,(1u<<12)|(1u<<10));assert(led1==0&&led2==0);
   assert(g_combos[COMBO_IDX_USER_TV].state==COMBO_WAIT_RELEASE);
  scan(296000,0);assert(combo_key_is_idle());
  /* VOL+ & HOME -> STB, stub returns backup -> fast blink (6 half-periods x 200ms) */
  combo_key_init();scan(300000,(1u<<12)|(1u<<8));
  scan(303000,(1u<<12)|(1u<<8));assert(switch_calls==before+2);
  assert(g_combos[COMBO_IDX_USER_STB].state==COMBO_FLASHING&&led1==1&&led2==1);
  {unsigned k;
   for(k=1;k<=5;k++)scan(303000u+200u*k,(1u<<12)|(1u<<8));
   assert(g_combos[COMBO_IDX_USER_STB].state==COMBO_FLASHING);
   scan(303000u+200u*6u,(1u<<12)|(1u<<8));
   assert(g_combos[COMBO_IDX_USER_STB].state==COMBO_WAIT_RELEASE&&led1==0&&led2==0);
  }
  scan(306000,0);assert(combo_key_is_idle());
  /* A third key cancels the hold before any switch. */
  combo_key_init();scan(310000,(1u<<12)|(1u<<8)|(1u<<2));
  scan(313000,(1u<<12)|(1u<<8)|(1u<<2));assert(switch_calls==before+2);
  scan(313100,0);
  /* An active OTA/UPG session blocks the switch. */
  combo_key_init();ota_busy=1;scan(320000,(1u<<12)|(1u<<8));
  scan(323000,(1u<<12)|(1u<<8));assert(switch_calls==before+2);ota_busy=0;scan(323010,0);
  combo_key_init();upg_busy_mock=1;scan(330000,(1u<<12)|(1u<<10));
  scan(333000,(1u<<12)|(1u<<10));assert(switch_calls==before+2);upg_busy_mock=0;scan(333010,0);
 }
 /* Previous, electrically overlapping aliases no longer execute. */
 {unsigned before=pair_calls,clear=bond_clears,sw=switch_calls,n;
  unsigned old[5]={(1u<<14)|(1u<<2),(1u<<9)|(1u<<2),
   (1u<<14)|(1u<<5),(1u<<9)|(1u<<5),(1u<<12)|(1u<<9)};
  for(n=0;n<5;n++){combo_key_init();scan(280000+n*5000,old[n]);scan(283000+n*5000,old[n]);scan(283010+n*5000,0);}
  assert(pair_calls==before&&bond_clears==clear&&switch_calls==sw);
 }
 puts("combo: KEY14+KEY3 STB pair/RC6, KEY9+KEY3 unpair, KEY12+KEY8/KEY12+KEY10 3s user-code switch with slow2/fast3 LED, five obsolete aliases disabled; 100ms release filter/dropout/repress/threshold/wrap; IR target, one action per hold, all user-switch aliases disabled, triple-key exclusion, OTA release latch PASS");return 0;}
