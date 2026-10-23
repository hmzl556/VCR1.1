#include "mock_merge.h"
static unsigned adds, dels, ticks, live, max_live, fail_add;
static uint8_t event;
static uint32_t next_handle;
#define taskSCHEDULER_RUNNING 2
static int xTaskGetSchedulerState(void){return taskSCHEDULER_RUNNING;}
static int xTaskIncrementTick(void){++ticks;return 0;}
void setSdkTimer(uint16_t ms);
void callBakSdkTimer(uint8_t ev,void *arg);
uint32_t sdk_timer_add(uint32_t ms,uint8_t ev,void (*cb)(uint8_t,void*),void *arg){
 (void)ms;(void)cb;(void)arg;++adds;event=ev;
 if(fail_add)return BR_TIMER_INVALID_HANDLER;
 ++live;if(live>max_live)max_live=live;return next_handle++%32;
}
int sdk_timer_del(uint32_t h){assert(h<32&&live);--live;++dels;return 0;}
#include "actual_sdk_tick.c"
int main(void){unsigned i,n;uint8_t old;
 setSdkTimer(8);assert(s_sdk_hdl==0&&live==1);
 for(i=0;i<100000;i++)setSdkTimer(8);
 assert(adds==1&&max_live==1);old=event;sdk_timer_pause();assert(dels==1&&!live);
 callBakSdkTimer(old,0);assert(!ticks&&!live);sdk_timer_resume();assert(live==1);
 callBakSdkTimer(old,0);assert(!ticks&&live==1);
 for(i=0;i<10000;i++){old=event;--live;callBakSdkTimer(old,0);assert(live==1);}
 assert(ticks==10000&&max_live==1);
 sdk_timer_pause();n=adds;setSdkTimer(8);assert(adds==n);
 fail_add=1;sdk_timer_resume();assert(s_sdk_hdl==BR_TIMER_INVALID_HANDLER&&!live);
 fail_add=0;setSdkTimer(8);assert(live==1);
 puts("SDK tick: slot zero, 100000 OTA arms, single timer, pause/stale callbacks, 10000 ticks, allocation retry PASS");return 0;
}
