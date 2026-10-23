#include "mock_merge.h"
#include <windows.h>
#include "cat1_skt_udp.c"
static uint32_t uart_baud=115200,modem_baud,start_time,ready_delay,rx_due;
static char rx[128];static unsigned rx_pos,rx_len,probes,ipr,normal,mode,garbage;
static void response(const char *s){strcpy(rx,s);rx_pos=0;rx_len=(unsigned)strlen(s);rx_due=now_ms+5;}
volatile uint16_t USART1_CAT1_RX_HEAD,USART1_CAT1_RX_TAIL;
uint8_t USART1_CAT1_RxPop(uint8_t *b){if((int32_t)(now_ms-rx_due)<0||rx_pos==rx_len)return 0;*b=(uint8_t)rx[rx_pos++];return 1;}
void USART1_CAT1_RxFlush(void){rx_pos=rx_len=0;}
void USART1_CAT1_RxSync(void){}
uint32_t USART1_CAT1_RxDropCount(void){return 0;}
int USART1_CAT1_SetBaudrate(uint32_t b){assert(!s_at_active);assert(b==9600||b==115200);uart_baud=b;USART1_CAT1_RxFlush();return 0;}
void usart1_at_send(char *s,unsigned n){
 (void)n;
 if(!strcmp(s,"AT"))++probes;
 else if(!strcmp(s,"AT+IPR=115200")){++ipr;assert(uart_baud==9600&&modem_baud==9600);}
 else {++normal;assert(!strcmp(s,"AT+CSCON=1")&&uart_baud==115200);assert(s_baud_phase==BAUD_SYNC_DONE||s_baud_phase==BAUD_SYNC_INIT);}
 if(now_ms-start_time<ready_delay||uart_baud!=modem_baud)return;
 if(garbage){response("NOTOK\r\n");return;}
 if(!strcmp(s,"AT+CSCON=1")&&mode==4){response("ERROR\r\n");return;}
 if(!strcmp(s,"AT+CSCON=1")&&mode==5){response("+CME ERROR: 4\r\n");return;}
 if(!strcmp(s,"AT+IPR=115200")){
  if(mode==2){response("ERROR\r\n");return;}
  if(mode==3){response("OK\r\n");return;}
  modem_baud=115200;if(mode==0)response("OK\r\n");
 }else response("AT\r\n+CSCON: 1\r\nOK\r\n");
}
int usart1_at_send_pump(char *s,unsigned n,void (*fn)(void)){(void)fn;usart1_at_send(s,n);return 0;}
int cat1_time_sync_from_cclk_rsp(const char *s){(void)s;return 0;}
static void init(uint32_t baud,unsigned delay,unsigned ipr_mode,uint32_t begin){
 now_ms=begin;start_time=begin;ready_delay=delay;mode=ipr_mode;garbage=0;
 uart_baud=115200;modem_baud=baud;probes=ipr=normal=0;rx_pos=rx_len=0;
 s_at_active=0;s_urc_line_len=s_urc_spill_len=0;cat1_skt_udp_setup_reset();
}
static int run(unsigned max_ms){
 unsigned t;int rc=CAT1_SETUP_BUSY;
 for(t=0;t<max_ms;t++){
  rc=cat1_skt_udp_setup_step();
  if(rc==CAT1_SETUP_FAIL||s_setup_phase==SETUP_PHASE_CEREG)return rc;
  ++now_ms;
 }
 assert(!"sync did not finish within test bound");return rc;
}
int main(void){unsigned saved;
 SetErrorMode(SEM_NOGPFAULTERRORBOX);
 init(115200,0,0,0);assert(run(1000)==CAT1_SETUP_BUSY&&s_baud_phase==BAUD_SYNC_DONE);
 assert(now_ms==5&&!probes&&!ipr&&normal==1);
 /* Completed CSCON: re-run setup as on modem power-up, no extra probe. */
 saved=probes;
 cat1_skt_udp_setup_reset();assert(run(1000)==CAT1_SETUP_BUSY&&probes==saved&&!ipr&&normal==2);
 init(9600,0,0,0);assert(run(8000)==CAT1_SETUP_BUSY&&s_baud_phase==BAUD_SYNC_DONE&&ipr==1&&modem_baud==115200);
 assert(now_ms>=3000&&now_ms<4000&&normal==2);
 init(9600,0,1,0);assert(run(8000)==CAT1_SETUP_BUSY&&ipr==1&&s_baud_phase==BAUD_SYNC_DONE); /* IPR switches before OK */
 init(9600,0,2,0);assert(run(15000)==CAT1_SETUP_FAIL&&normal==1&&ipr==3&&uart_baud==115200);
 init(9600,0,3,0);assert(run(15000)==CAT1_SETUP_FAIL&&normal==1&&ipr==3); /* IPR OK without baud change is not success */
 init(115200,2500,0,0);assert(run(10000)==CAT1_SETUP_BUSY&&s_baud_phase==BAUD_SYNC_DONE&&!ipr&&normal==2);
 init(38400,0,0,0);assert(run(10000)==CAT1_SETUP_FAIL&&now_ms<=7100&&normal==1&&!ipr&&uart_baud==115200);
 init(115200,0,0,0);garbage=1;assert(run(10000)==CAT1_SETUP_FAIL&&!ipr&&normal==1);
 init(9600,0,1,0xffffff00U);assert(run(8000)==CAT1_SETUP_BUSY&&s_baud_phase==BAUD_SYNC_DONE&&ipr==1);
 init(115200,0,4,0);assert(run(1000)==CAT1_SETUP_BUSY&&now_ms==5&&!probes&&!ipr);
 init(115200,0,5,0);assert(run(1000)==CAT1_SETUP_BUSY&&now_ms==5&&!probes&&!ipr);
 puts("baud sync: normal CSCON first/no added wait or probe, ERROR replies preserve baud, 9600 conversion, old/new IPR reply baud, mandatory verification, slow boot, bounded no-response/garbage, reset and clock wrap PASS");return 0;
}
