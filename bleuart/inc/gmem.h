#ifndef _GMEM_H_
#define _GMEM_H_

#include "gap.h"
#include "gatt.h"

extern gatt_chr_t dtss_chr;
extern peer_t dtss_peer;

extern uint8_t bleConnectOk;
extern uint8_t bleSendEn;
extern uint8_t bleAdvStat;
extern volatile uint32_t voiceUptimeMs;

extern uint8_t intFlagMs10;
extern uint8_t secTmo;
extern uint32_t dayTmo;

void initialGmem(void); // 初始化全局变量，设置初始状态和计数器值

#endif /* _GMEM_H_ */
