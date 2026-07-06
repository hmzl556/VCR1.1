#ifndef _GMEM_H_
#define _GMEM_H_

#include "gap.h"
#include "gatt.h"
#include <stdint.h>

extern gatt_chr_t dtss_chr;
extern peer_t dtss_peer;

extern uint8_t bleConnectOk;
extern uint8_t bleSendEn;
extern uint8_t bleAdvStat;

/** 全系统统一毫秒时基（sdk_timer，单调递增） */
uint32_t app_now_ms(void);

void initialGmem(void);

#endif /* _GMEM_H_ */
