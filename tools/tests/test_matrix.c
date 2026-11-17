#include "mock_hw.h"
#include "../../bleuart/inc/app_keyscan.h"
/* 新板 29 键 = SDK 44 键映射 (KEY_1..44); 矩阵 28 对 + 独立键 1 个。
 * 键号→线对 来自 2026-11-17 飞线枚举 (见 doc/BR1601键盘扫描应用笔记勘误.md)。
 * 8 条线: PA0,PA1,PA2,PA3,PA6,PB8,PB9,PB10 (位 0..7)。 */
static unsigned lines(unsigned key)
{
    static const unsigned pairs[30][2] = {
        {0,0},{0,1},{0,2},{1,2},{0,3},{1,3},{2,3},{0,4},{1,4},{2,4},
        {3,4},{0,7},{1,7},{2,7},{3,7},{4,7},{0,5},{1,5},{2,5},{3,5},
        {4,5},{5,7},{0,6},{1,6},{2,6},{3,6},{4,6},{7,6},{5,6},{0,0},
    };
    return key < 30u ? (1u << pairs[key][0]) | (1u << pairs[key][1]) : 0u;
}
int main(void){unsigned i,j;
 assert(KEYSCAN_KEY29==29 && KEYSCAN_KEY_MAX==45);
 assert(sizeof(keyscan_map)/sizeof(keyscan_map[0])==KEYSCAN_KEY_MAX+1);
 /* 实测锚点: (PA1,PA3)=KEY_5=word1 bit4; 独立键 PA0 低有效=KEY_29=word0 bit0 */
 assert(keyscan_map[5][0]==0&&keyscan_map[5][1]==0x10u);
 assert(keyscan_map[29][0]==1u&&keyscan_map[29][1]==0);
 /* 全部键值唯一 */
 for(i=1;i<KEYSCAN_KEY_MAX;i++)
  for(j=i+1;j<KEYSCAN_KEY_MAX;j++)
   assert(memcmp(keyscan_map[i],keyscan_map[j],sizeof(keyscan_map[i]))!=0);
 /* 矩阵键都在 word1, 独立键在 word0 */
 for(i=1;i<=28;i++)assert(keyscan_map[i][0]==0&&keyscan_map[i][1]!=0);
 for(i=29;i<45;i++)assert(keyscan_map[i][0]!=0&&keyscan_map[i][1]==0);
 /* 组合键共线检查 (GY/T 407-2024 §7.2 与出厂/用户码组合):
  * 每对必须 4 线独立 (非同行非同列)。 */
 {unsigned pairs[6][2]={{7,12},{19,12},{7,23},{19,23},{13,7},{10,18}};
  for(i=0;i<6;i++){unsigned a=lines(pairs[i][0]),b=lines(pairs[i][1]);
   assert(a&&b&&!(a&b));}}
 /* 锚点线对自检: KEY5=(PA1,PA3)=线1|线3 */
 assert(lines(5)==((1u<<1)|(1u<<3)));
 puts("matrix: 44-key SDK map, KEY5=(PA1,PA3) bit4, KEY29=PA0 independent bit0, unique entries, standard combo line isolation PASS");return 0;}
