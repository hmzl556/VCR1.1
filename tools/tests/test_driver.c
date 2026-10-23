#include "mock_hw.h"
#include "irc.c"
static bool rc6_active;
bool ir_rc6_is_busy(void){return rc6_active;}
int main(void){unsigned i;
 assert(!irc_send(0x3A,0xDC));irc_init();
 assert(IRC->FREQ_CARRIER_ON==IRC_CARR_ON&&IRC->FREQ_CARRIER_OFF==IRC_CARR_OFF);
 assert(IRC_CARR_ON<2048u&&IRC_CARR_OFF<2048u);
 assert(64000000u/(IRC_CARR_ON+IRC_CARR_OFF)>=37240u&&64000000u/(IRC_CARR_ON+IRC_CARR_OFF)<=38760u);
 rc6_active=true;assert(!irc_send(0x3A,0xDC));rc6_active=false;
 assert(irc_send(0x3A,0xDC));assert(IRC->CTRL&IR_CTRL_TX_START_Msk);assert(s_transmitting);
 IRC->STATUS=IR_STATUS_BUSY;assert(!irc_send(0x51,0));
 IRC->STATUS=IR_IRQ_FLAG;assert(!irc_is_busy());assert(!(IRC->CTRL&IR_CTRL_TX_START_Msk));assert(!s_transmitting);
 irc_send_repeat();assert(s_transmitting);IRC->STATUS=IR_STATUS_BUSY;
 now_ms=121;irc_is_busy();assert(!s_transmitting&&!(IRC->CTRL&IR_CTRL_TX_START_Msk));
 IRC->STATUS=0;assert(irc_send(0x51,0));irc_stop();assert(!s_transmitting);
 /* Real wake restore must reconfigure parked PB4 despite s_ok remaining true.
  * Verify repeated cycles, peripheral register loss and PB5 isolation. */
 for(i=0;i<32;i++){
  ir_mock_pin_mode[4]=3;ir_mock_pin_af[4]=0;ir_mock_pin_mode[5]=7;
  RCC->AHBPCLKEN&=~RCC_AHBPCLKEN_IRCEN;
  IRC->FREQ_CARRIER_ON=IRC->FREQ_CARRIER_OFF=0;IRC->CTRL=0;
  irc_init();
  assert(ir_mock_pin_mode[4]==GPIO_MODE_AF_PP&&ir_mock_pin_af[4]==GPIO_AF2);
  assert(ir_mock_pin_mode[5]==7&&(RCC->AHBPCLKEN&RCC_AHBPCLKEN_IRCEN));
  assert(IRC->FREQ_CARRIER_ON==IRC_CARR_ON&&IRC->CTRL==IR_CTRL_ENABLE_Msk);
  assert(!s_transmitting&&irc_send(0x51,0xDC));irc_stop();
 }
 puts("driver: 32 wake restorations of PB4/clocks/registers with PB5 preserved, 11-bit carrier limits, frequency, busy rejection, completion, stuck TX bound, zero command PASS");return 0;}
