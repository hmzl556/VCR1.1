#include "mock_merge.h"
static unsigned modes[2][16],restored_ir,restored_uart,stopped,paused,cleared_dma,alarm_id,watches;
static unsigned hb_enabled=1,hb_due,key_wake=1,physical_keys,sleep_calls;
bool app_keyscan_any_pressed(void){return physical_keys!=0;}
volatile uint16_t key_receive;
static void gpio_record(GPIO_Module *p,GPIO_InitType *v){unsigned i;for(i=0;i<16;++i)if(v->Pin&(1u<<i))modes[p==GPIOA?0:1][i]=v->GPIO_Mode;}
#define GPIO_InitPeripheral gpio_record
#include "pm_sleep.c"
unsigned EXTI_GetITStatus(unsigned x){(void)x;return 0;}
void EXTI_ClrITPendBit(unsigned x){(void)x;}
void KEYSCAN_InfoClear(void){}
void KEYSCAN_ClearInterrupt(void){}
void DMA_ClrIntPendingBit(unsigned flags,void *dma){(void)dma;cleared_dma=flags;}
void IWDG_ReloadKey(void){++watches;}
void ble_adv_stop(void){++stopped;}
peer_t ble_gap_get_peer(void){return (peer_t)1;}
void le_conn_destory(peer_t p){(void)p;}
void br_sleep(void){++sleep_calls;if(key_wake)key_receive=1;else hb_due=1;}
void cs_schedule(void){}
void ble_gap_process_disconnect_pending(void){}
uint32_t sdk_timer_add(uint32_t ms,uint8_t ev,void (*cb)(uint8_t,void*),void *arg){(void)ms;(void)ev;(void)cb;(void)arg;return alarm_id++%32;}
int sdk_timer_del(uint32_t id){(void)id;return 0;}
void PWR_EnterSLEEPMode(uint8_t how){(void)how;key_receive=1;}
void sdk_timer_pause(void){++paused;}
void Audio_Pcm_HardwareShutdown(void){}
void cat1_skt_udp_force_fail_active(void){}
void USART1_CAT1_Init(void){++restored_uart;modes[0][4]=modes[0][5]=2;}
void irc_init(void){++restored_ir;modes[1][4]=2;}
uint8_t heartbeat_enabled(void){return hb_enabled;}
uint8_t heartbeat_due(void){return hb_due;}
uint8_t br_sleep_lock_release(void){return 0;}
int main(void){unsigned a,i;for(a=0;a<2;++a)for(i=0;i<16;++i)modes[a][i]=5;
 assert(pm_sleep_enter()==PM_WAKE_REASON_KEYSCAN);assert(bleAdvStat==0xa6&&!g_pm_sleeping&&restored_uart==1&&restored_ir==1&&stopped==1&&paused==1&&watches==2);
 assert(cleared_dma==(DMA_INT_GLB2|DMA_INT_HTX2|DMA_INT_TXC2|DMA_INT_ERR2));
 for(i=0;i<=6;++i)if(i!=4&&i!=5)assert(modes[0][i]==5);for(i=8;i<=10;++i)assert(modes[1][i]==5);
 key_receive=0;key_wake=0;assert(pm_sleep_enter()==PM_WAKE_REASON_NONE);assert(restored_uart==2&&restored_ir==2&&hb_due);
 {unsigned n=sleep_calls;key_receive=0;hb_due=0;physical_keys=3;assert(pm_sleep_enter()==PM_WAKE_REASON_KEYSCAN&&sleep_calls==n);physical_keys=0;}
 {uint8_t old;alarm_id=0;s_sleep_wake_tid=BR_TIMER_INVALID_HANDLER;pm_sleep_wake_timer_rearm();assert(s_sleep_wake_tid==0);old=s_sleep_alarm_generation;pm_sleep_wake_timer_rearm();pm_sleep_wake_timer_cb(old,0);assert(s_sleep_wake_tid!=BR_TIMER_INVALID_HANDLER);pm_sleep_wake_timer_cb(s_sleep_alarm_generation,0);assert(s_sleep_wake_tid==BR_TIMER_INVALID_HANDLER);}
 puts("sleep: key/heartbeat wake, BLE stack retained, V1.2 UART/IR restore, KEYSCAN preserved, DMA_CH2 pending cleanup PASS");return 0;}
