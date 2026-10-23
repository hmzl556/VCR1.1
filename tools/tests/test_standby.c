#include "mock_merge.h"
static unsigned modes[2][16],uart_restores,ir_restores,pauses,resumes,power_offs,locks=1;
static le_conn_update_t params;
static void gpio_record(GPIO_Module *p,GPIO_InitType *v){unsigned i;for(i=0;i<16;++i)if(v->Pin&(1u<<i))modes[p==GPIOA?0:1][i]=v->GPIO_Mode;}
#define GPIO_InitPeripheral gpio_record
#include "pm_standby.c"
peer_t ble_gap_get_peer(void){return (peer_t)1;}
void le_conn_update(const le_conn_update_t *p){params=*p;}
uint8_t rc_cat1_is_powered(void){return 1;}
void rc_cat1_power_down(void){++power_offs;}
void cat1_skt_udp_force_fail_active(void){}
void Audio_Pcm_HardwareShutdown(void){}
void sdk_timer_pause(void){++pauses;}
void sdk_timer_resume(void){++resumes;}
void USART1_CAT1_Init(void){++uart_restores;modes[0][4]=modes[0][5]=2;}
void irc_init(void){++ir_restores;modes[1][4]=2;}
uint8_t br_sleep_lock_release(void){if(locks)--locks;return locks;}
void br_sleep_lock_acquire(void){++locks;}
int main(void){unsigned a,i;for(a=0;a<2;++a)for(i=0;i<16;++i)modes[a][i]=5;
 RCC->APB1PCLKEN=0x1234;RCC->APB2PCLKEN=0x5678;RCC->AHBPCLKEN=0x9abc;
 pm_standby_enter();assert(power_offs==1&&pauses==1&&!locks&&params.intv_min==40);
 for(i=0;i<=6;++i)if(i!=4&&i!=5)assert(modes[0][i]==5);
 for(i=8;i<=10;++i)assert(modes[1][i]==5);
 assert(modes[0][4]==3&&modes[0][5]==0&&modes[1][4]==3&&modes[1][5]==0);
 pm_standby_exit();assert(resumes==1&&locks==1&&uart_restores==1&&ir_restores==1&&params.intv_min==8);
 assert(modes[0][4]==2&&modes[0][5]==2&&modes[1][4]==2);
 assert(RCC->APB1PCLKEN==0x1234&&RCC->APB2PCLKEN==0x5678&&RCC->AHBPCLKEN==0x9abc);
 puts("standby: KEYSCAN pins retained, V1.2 UART/IR independent park/restore, clocks, SDK timer and sleep locks PASS");return 0;}
