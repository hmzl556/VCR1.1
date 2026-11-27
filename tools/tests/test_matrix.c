#include "mock_hw.h"
#include "../../bleuart/inc/app_keyscan.h"
int main(void){unsigned i,j,k;uint32_t raw[2];
 assert(KEYSCAN_KEY29==29 && KEYSCAN_KEY_MAX==30);
 assert(sizeof(keyscan_map)/sizeof(keyscan_map[0])==KEYSCAN_KEY_MAX+1);
 /* Every real key resolves from its own scan word and from nothing else. */
 for(i=1;i<KEYSCAN_KEY_MAX;i++){
  raw[0]=keyscan_map[i][0];raw[1]=keyscan_map[i][1];
  assert((raw[0]|raw[1])!=0u);
  for(k=1;k<KEYSCAN_KEY_MAX;k++){
   if(k==i)assert(keyscan_is_key_pressed(raw,(uint8_t)k));
   else assert(!keyscan_is_key_pressed(raw,(uint8_t)k));
  }
 }
 /* Digit 0 is the only key on the second scan word (PA0-GND). */
 raw[0]=keyscan_map[29][0];raw[1]=keyscan_map[29][1];
 assert(raw[0]==1u&&raw[1]==0u&&keyscan_is_key_pressed(raw,29));
 for(i=1;i<29;i++)assert(!keyscan_is_key_pressed(raw,(uint8_t)i));
 /* 29 keys must occupy 29 distinct matrix positions. */
 for(i=1;i<KEYSCAN_KEY_MAX;i++)
  for(j=i+1;j<KEYSCAN_KEY_MAX;j++)
   assert(memcmp(keyscan_map[i],keyscan_map[j],sizeof(keyscan_map[i]))!=0);
 /* A chord resolves by containment: a digit plus confirm stay independent. */
 raw[0]=0u;raw[1]=keyscan_map[20][1]|keyscan_map[4][1];
 assert(keyscan_is_key_pressed(raw,20)&&keyscan_is_key_pressed(raw,4));
 for(i=1;i<KEYSCAN_KEY_MAX;i++)if(i!=20&&i!=4)assert(!keyscan_is_key_pressed(raw,(uint8_t)i));
 /* Empty scan matches no key. */
 raw[0]=0u;raw[1]=0u;
 for(i=1;i<KEYSCAN_KEY_MAX;i++)assert(!keyscan_is_key_pressed(raw,(uint8_t)i));
 puts("matrix: 29-key table, 29 unique positions, digit0 on word0, per-key isolation and chord containment PASS");return 0;}
