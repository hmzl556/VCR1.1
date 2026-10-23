#include "mock_merge.h"
#include "heartbeat.c"
static unsigned powered,ready,queues,queue_fail,power_calls;
static pm_state_t state=PM_STATE_WORKING;
pm_state_t pm_state_get(void){return state;}
uint8_t rc_cat1_udp_is_ready(void){return ready;}
uint8_t rc_cat1_is_powered(void){return powered;}
void rc_cat1_power_up(void){powered=1;++power_calls;}
int remote_queue_tag0A_heartbeat(void){++queues;return queue_fail?-2:0;}
uint8_t cat1_modem_is_busy(void){return modem_busy;}
int main(void){uint32_t due;unsigned n;
 heartbeat_init();assert(heartbeat_enabled());due=s_next_due_ms;now_ms=100;
 heartbeat_set_period_s(1800);assert(s_next_due_ms==due);
 heartbeat_set_period_s(2);due=s_next_due_ms;now_ms=due;state=PM_STATE_STANDBY;
 heartbeat_poll();assert(heartbeat_pending()&&!powered&&!queues);
 state=PM_STATE_WORKING;modem_busy=1;heartbeat_poll();assert(!powered);modem_busy=0;
 ota_busy=1;heartbeat_poll();assert(!powered);ota_busy=0;
 heartbeat_poll();assert(powered&&s_pending==2&&heartbeat_needs_cat1_poll());
 heartbeat_set_period_s(3);assert(s_pending==2);n=power_calls;heartbeat_poll();assert(power_calls==n);
 ready=1;queue_fail=1;heartbeat_on_udp_ready();assert(s_pending==1);
 heartbeat_poll();assert(s_pending==1);queue_fail=0;heartbeat_poll();assert(!heartbeat_pending());
 now_ms=s_next_due_ms;heartbeat_poll();assert(!heartbeat_pending()&&queues==4);
 heartbeat_set_period_s(0);now_ms+=999999;heartbeat_poll();assert(!heartbeat_due()&&!heartbeat_pending());
 now_ms=0xFFFFFF00u;heartbeat_set_period_s(1);now_ms+=999;assert(!heartbeat_due());now_ms++;assert(heartbeat_due());
 powered=0;ready=0;heartbeat_set_period_s(1);now_ms=s_next_due_ms;heartbeat_poll();n=power_calls;
 for(unsigned j=0;j<10;j++){now_ms=s_next_due_ms;heartbeat_poll();assert(s_pending==2&&power_calls==n);}
 powered=0;heartbeat_poll();assert(!heartbeat_pending());
 puts("heartbeat: unchanged period, standby deferral, OTA/CAT1 exclusion, setup protection, queue-full retry, disable, clock wrap PASS");return 0;}
