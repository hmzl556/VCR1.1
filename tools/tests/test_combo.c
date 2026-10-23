#include "mock_app.h"
#include "combo_key.c"
void ir_send_pair_code_target(bool tv){if(!tv){assert(bond_clears>0u);assert(g_pairing_active);}++pair_calls;last_pair_tv=tv;}
bool ir_user_switch_stb(void){++switch_calls;return false;}
bool ir_user_switch_tv(void){++switch_calls;return true;}
static void scan(unsigned t,unsigned mask){uint32_t hw[5]={mask,0,0,0,0};now_ms=t;combo_key_process(t,hw);}
int main(void){unsigned i;unsigned tv=(1u<<14)|(1u<<3),stb=(1u<<14)|(1u<<5),user=(1u<<12)|(1u<<9);
 pm_value=PM_STATE_WORKING;combo_key_init();scan(0,tv);scan(3000,tv);assert(pair_calls==1&&last_pair_tv==1);
 assert(!bond_clears&&!adv_pair_calls&&!g_pairing_active&&pm_value==PM_STATE_WORKING&&connected);
 for(i=3010;i<12000;i+=10)scan(i,tv);assert(pair_calls==1);scan(12010,0);assert(combo_key_is_idle());
 scan(12100,stb);scan(15100,stb);assert(pair_calls==2&&last_pair_tv==0&&bond_clears==1&&adv_pair_calls==1&&g_pairing_active);scan(15110,0);
 scan(16000,user);scan(19000,user);assert(switch_calls==1);for(i=19200;i<=22000;i+=200)scan(i,user);assert(switch_calls==1);scan(22010,0);
 scan(22100,(1u<<12)|(1u<<8));scan(25100,(1u<<12)|(1u<<8));assert(switch_calls==1);scan(25110,0);
 scan(26000,tv|(1u<<2));scan(30000,tv|(1u<<2));assert(pair_calls==2);scan(30010,0);
 ota_busy=1;scan(31000,tv);scan(34000,tv);ota_busy=0;scan(35000,tv);assert(pair_calls==2);scan(35010,0);scan(36000,tv);scan(39000,tv);assert(pair_calls==3);
 /* TV operations preserve an existing STB connection, advertising and bond
  * for both connected/disconnected and active/inactive pairing states. */
 for(i=0;i<4;i++){
  unsigned before_pair=pair_calls,before_clear=bond_clears,before_adv=adv_pair_calls;
  unsigned unpair=(1u<<3)|(1u<<9);bool active=(i&1)!=0;
  combo_key_init();connected=(i&2)!=0;g_pairing_active=active;
  pm_value=connected?PM_STATE_WORKING:PM_STATE_RECONNECTING;
  {int before_pm=pm_value;
   scan(40000,tv);scan(43000,tv);scan(80000,tv);
   assert(pair_calls==before_pair+1&&last_pair_tv);
   assert(bond_clears==before_clear&&adv_pair_calls==before_adv&&g_pairing_active==active&&pm_value==before_pm);
   scan(80010,0);scan(80100,unpair);scan(83100,unpair);scan(120000,unpair);
   assert(!combo_key_unpair_in_progress());
   assert(bond_clears==before_clear&&adv_pair_calls==before_adv&&g_pairing_active==active&&pm_value==before_pm);
   scan(120010,0);assert(combo_key_is_idle());
  }
 }
 /* Actual STB unpair still clears binding and follows disconnect/sleep. */
 {unsigned before=bond_clears;unsigned unpair=(1u<<9)|(1u<<5);
  combo_key_init();connected=true;pm_value=PM_STATE_WORKING;
  scan(130000,unpair);scan(133000,unpair);
  assert(bond_clears==before+1&&combo_key_unpair_in_progress());
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
  assert(g_combos[COMBO_IDX_PAIR_TV].state==COMBO_HOLDING);
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
 /* Previous, electrically overlapping aliases no longer execute. */
 {unsigned before=pair_calls,clear=bond_clears,sw=switch_calls,n;
  unsigned old[3]={(1u<<14)|(1u<<2),(1u<<9)|(1u<<2),(1u<<12)|(1u<<10)};
  for(n=0;n<3;n++){combo_key_init();scan(280000+n*5000,old[n]);scan(283000+n*5000,old[n]);scan(283010+n*5000,0);}
  assert(pair_calls==before&&bond_clears==clear&&switch_calls==sw);
 }
 puts("combo: TV operations preserve STB bond/advertising/PM; STB pair/unpair retained; 100ms release filter/dropout/repress/threshold/wrap; IR target, one action per hold, user-switch latch, triple-key exclusion, OTA release latch PASS");return 0;}
