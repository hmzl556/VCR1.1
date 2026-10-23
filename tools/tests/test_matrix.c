#include "mock_hw.h"
#include "../../bleuart/inc/app_keyscan.h"
int main(void){unsigned i,j;uint32_t raw[2]={0u,0x04000000u};
 assert(KEYSCAN_KEY18==18 && KEYSCAN_KEY_MAX==19);
 assert(sizeof(keyscan_map)/sizeof(keyscan_map[0])==KEYSCAN_KEY_MAX+1);
 assert(keyscan_map[18][0]==0&&keyscan_map[18][1]==0x04000000u);
 assert(keyscan_is_key_pressed(raw,18));
 for(i=1;i<KEYSCAN_KEY_MAX;i++){
  if(i!=18)assert(!keyscan_is_key_pressed(raw,(uint8_t)i));
  for(j=i+1;j<KEYSCAN_KEY_MAX;j++)assert(memcmp(keyscan_map[i],keyscan_map[j],sizeof(keyscan_map[i]))!=0);
 }
 /* Actual hardware bit masks for volume-minus plus mute. */
 raw[0]=0;raw[1]=keyscan_map[14][1]|keyscan_map[2][1];
 assert(raw[1]==0x81u&&keyscan_is_key_pressed(raw,14)&&keyscan_is_key_pressed(raw,2));
 for(i=1;i<KEYSCAN_KEY_MAX;i++)if(i!=14&&i!=2)assert(!keyscan_is_key_pressed(raw,(uint8_t)i));
 /* Hardware triangular matrix: node order PA0,PA1,PA2,PA3,PA6,
  * PB10,PB8,PB9; bit15 is reserved. Derive each real key's endpoints. */
 {unsigned nodes[19]={0},row,col,bit=0,k,n;
  unsigned pairs[6][2]={{14,5},{9,5},{12,9},{14,3},{3,9},{8,14}};
  for(row=1;row<8;row++)for(col=0;col<row;col++){
   if(bit==15)bit=16;
   for(k=1;k<19;k++)if(keyscan_map[k][1]==(1u<<bit))nodes[k]=(1u<<row)|(1u<<col);
   bit++;
  }
  for(n=0;n<6;n++)assert(nodes[pairs[n][0]]&&nodes[pairs[n][1]]&&!(nodes[pairs[n][0]]&nodes[pairs[n][1]]));
  assert(nodes[14]&nodes[2]);assert(nodes[9]&nodes[2]);assert(nodes[12]&nodes[10]);
 }
 puts("matrix: actual 18-key table, PB9--PA6 bit26, unique original keys, new key isolation PASS");return 0;}
