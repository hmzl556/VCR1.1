#include "mock_merge.h"
#include "cat1_skt_udp.c"
static void route(const char *in,char *out)
{
 unsigned n=0;while(*in){char c=*in++;unsigned taken=cat1_skt_udp_feed_char(c);
  if(s_urc_spill_len){memcpy(out+n,s_urc_spill,s_urc_spill_len);n+=s_urc_spill_len;s_urc_spill_len=0;}
  if(!taken)out[n++]=c;
 }out[n]=0;
}
int main(void){char out[2048];uint8_t sim[20],len;const char *data="+ECICCID: 898604F41525D1986801\r\n";
 route(data,out);assert(!strcmp(out,data));cat1_iccid_capture_from_rsp(out);
 assert(!cat1_iccid_get_raw(sim,&len)&&len==20&&!memcmp(sim,"898604F41525D1986801",20));
 route("+CSCON: 1\r\n+CCLK: \"2026/10/21,11:47:21+32\"\r\n",out);
 assert(!strstr(out,"CSCON")&&strstr(out,"+CCLK:"));
 route("+CME ERROR: 406\r\n",out);assert(!strcmp(out,"+CME ERROR: 406\r\n"));
 route("+ECSONMI: 0,53\r\n",out);assert(!out[0]&&s_ecsonmi_pending&&s_ecsonmi_len==53);
 route("+ECSORF: 0,106.53.179.53,8888,20,00112233445566778899AABBCCDDEEFF00112233,0\r\n",out);
 assert(strstr(out,"+ECSORF:")&&strstr(out,"CCDDEEFF"));
 cat1_iccid_capture_from_rsp("+ECICCID: 1234\r\n+CSCON: 1\r\nOK\r\n");
 assert(!cat1_iccid_get_raw(sim,&len)&&!memcmp(sim,"898604F41525D1986801",20));
 puts("CAT1 RX: interleaved CSCON/ECSONMI, full ICCID/CCLK/ECSORF/CME lines, incomplete ICCID retention PASS");return 0;}

volatile uint16_t USART1_CAT1_RX_HEAD,USART1_CAT1_RX_TAIL;
uint8_t USART1_CAT1_RxPop(uint8_t *b){(void)b;return 0;}
void USART1_CAT1_RxFlush(void){}
void USART1_CAT1_RxSync(void){}
uint32_t USART1_CAT1_RxDropCount(void){return 0;}
void usart1_at_send(char *s,unsigned n){(void)s;(void)n;}
int usart1_at_send_pump(char *s,unsigned n,void (*fn)(void)){(void)s;(void)n;(void)fn;return 0;}
int cat1_time_sync_from_cclk_rsp(const char *rsp){(void)rsp;return 0;}

int USART1_CAT1_SetBaudrate(uint32_t baud){(void)baud;return 0;}
