#include "gap.h"
#include "gatt.h"
#include "gmem.h"
#include "br_timer.h"

gatt_chr_t dtss_chr;
peer_t dtss_peer;

uint8_t bleConnectOk;
uint8_t bleSendEn;
uint8_t bleAdvStat;

void initialGmem(void)
{
	bleConnectOk = 0;
	bleSendEn = 0;
	bleAdvStat = 0;
}

uint32_t app_now_ms(void)
{
	return sdk_timer_get_current_ms();
}
