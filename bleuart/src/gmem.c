#include "gap.h"
#include "gatt.h"
#include "gmem.h"

gatt_chr_t dtss_chr;
peer_t dtss_peer;

uint8_t bleConnectOk;
uint8_t bleSendEn;
uint8_t bleAdvStat;
volatile uint32_t voiceUptimeMs;

uint8_t intFlagMs10;
uint8_t secTmo;
uint32_t dayTmo;

void initialGmem(void)
{
	bleConnectOk = 0;	// 蓝牙连接状态标志，0 表示未连接，1 表示已连接
	bleSendEn = 0;		// 蓝牙发送使能标志，0 表示不能发送数据，1 表示可以发送数据
	bleAdvStat = 0;		// 蓝牙广播状态标志，0 表示未开始广播，0xa6U 表示正在广播

	voiceUptimeMs = 0;	// 语音系统的运行时间，单位为毫秒，初始值为 0，随着系统运行逐渐增加

	intFlagMs10 = 0;	// 10ms 定时器的计数器，每 8ms 回调一次，达到 125 次即 1 秒钟，初始值为 0
	secTmo = 0;			// 1 秒钟的定时器标志，供主循环或其他任务检查使用，0 表示未到达 1 秒钟，1 表示已到达 1 秒钟
	dayTmo = 0;			// 以秒为单位的定时器计数器，每当 secTmo 被设置为 1 时增加一次，达到 86400 秒（24 小时）后可以执行特定操作，初始值为 0
}
