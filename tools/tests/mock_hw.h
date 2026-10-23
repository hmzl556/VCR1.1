#ifndef MOCK_HW_H
#define MOCK_HW_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define ENABLE 1
#define DISABLE 0
#define TIM3_IRQn 16
#define TIM6_IRQn 20
#define RCC_APB1_PERIPH_TIM6 2
#define TIM_INT_UPDATE 1u
#define TIM_FLAG_UPDATE 1u
#define TIM_OCMODE_PWM1 0x60u
#define TIM_OUTPUT_STATE_ENABLE 1u
#define TIM_OC_POLARITY_HIGH 0u
#define TIM_OC_PRE_LOAD_DISABLE 0u
#define IRC_IRQn 31
#define RCC_APB1_PERIPH_TIM3 1
#define RCC_APB2_PERIPH_GPIOB 1
#define RCC_APB2_PERIPH_AFIO 2
#define RCC_AHBPCLKEN_IRCEN 1
#define RCC_AHBRST_IRCRST 1
#define GPIO_PIN_4 16u
#define GPIO_PIN_5 32u
#define GPIO_MODE_AF_PP 2
#define GPIO_PULL_UP 1
#define GPIO_NO_PULL 0
#define GPIO_AF2 2
#define GPIO_AF2_TIM3 2
#define TIM_CNT_MODE_UP 0
#define TIM_CH_2 2
#define TIM_IC_POLARITY_BOTHEDGE 0
#define TIM_IC_POLARITY_FALLING 2
#define TIM_IC_POLARITY_RISING 1
#define TIM_IC_PSC_DIV4 4
#define TIM_IC_SELECTION_DIRECTTI 0
#define TIM_IC_PSC_DIV1 0
#define TIM_INT_CC1 2u
#define TIM_INT_CC2 4u
#define TIM_FLAG_CC1 2u
#define TIM_FLAG_CC2 4u
#define TIM_FLAG_CC2OF 1024u
#define IR_CTRL_ENABLE_Msk 4u
#define IR_CTRL_TX_START_Msk 8u
#define IR_CTRL_CODE_FIFO_RESET_Msk 1u
#define IR_CTRL_REP_FIFO_RESET_Msk 2u
#define IR_STATUS_BUSY 1024u
#define IR_IRQ_FLAG 2048u
#define FlashOperationSuccess 0u
#define co_printf(...) ((void)0)
typedef struct {uint32_t DINTEN, STS, CCDAT1, CCDAT2, CNT, CCEN, CCMOD1, CTRL1, PSC, AR;} TIM_Module;
typedef struct {uint32_t PID;} GPIO_Module;
typedef struct {uint32_t AHBPCLKEN, AHBPRST, APB1PCLKEN, APB2PCLKEN;} RCC_Module;
typedef struct {uint32_t FREQ_CARRIER_ON,FREQ_CARRIER_OFF,LOGIC_ZERO_TIME,LOGIC_ONE_TIME,CTRL,STATUS,CODE_FIFO;} IRC_Module;
typedef struct {unsigned Pin, GPIO_Mode, GPIO_Pull, GPIO_Alternate;} GPIO_InitType;
typedef struct {unsigned Prescaler,Period,CntMode;} TIM_TimeBaseInitType;
typedef struct {unsigned Channel,IcPolarity,IcSelection,IcPrescaler,IcFilter;} TIM_ICInitType;
typedef struct {unsigned OcMode,OutputState,OcPolarity,Pulse;} OCInitType;
static TIM_Module timer3, timer6;
static GPIO_Module portb;
static RCC_Module rcc;
static IRC_Module irc;
#define TIM3 (&timer3)
#define TIM6 (&timer6)
#define GPIOB (&portb)
#define RCC (&rcc)
#define IRC (&irc)
static uint32_t now_ms, ota_busy;
static uint32_t app_now_ms(void){return now_ms;}
static uint32_t udp_ota_session_busy(void){return ota_busy;}
static void RCC_EnableAPB1PeriphClk(unsigned a,unsigned b){(void)a;(void)b;}
static void RCC_EnableAPB2PeriphClk(unsigned a,unsigned b){(void)a;(void)b;}
static void GPIO_InitStruct(GPIO_InitType *p){memset(p,0,sizeof(*p));}
static unsigned ir_mock_pin_mode[16],ir_mock_pin_af[16];
static void GPIO_InitPeripheral(GPIO_Module *p,GPIO_InitType *v){unsigned i;(void)p;
 for(i=0;i<16;i++)if(v->Pin&(1u<<i)){ir_mock_pin_mode[i]=v->GPIO_Mode;ir_mock_pin_af[i]=v->GPIO_Alternate;}}
static void GPIO_ResetBits(GPIO_Module *p,unsigned v){(void)p;(void)v;}
static void NVIC_DisableIRQ(unsigned a){(void)a;}
static void NVIC_EnableIRQ(unsigned a){(void)a;}
static void NVIC_ClearPendingIRQ(unsigned a){(void)a;}
static void NVIC_SetPriority(unsigned a,unsigned b){(void)a;(void)b;}
static void TIM_Enable(TIM_Module *p,unsigned a){p->CTRL1=a;}
static void TIM_InitTimBaseStruct(TIM_TimeBaseInitType *p){memset(p,0,sizeof(*p));}
static void TIM_InitTimeBase(TIM_Module *p,TIM_TimeBaseInitType *v){p->PSC=v->Prescaler;p->AR=v->Period;}
static void TIM_InitIcStruct(TIM_ICInitType *p){memset(p,0,sizeof(*p));}
static void TIM_ICInit(TIM_Module *p,TIM_ICInitType *v){(void)v;p->CCMOD1=0x0100u;p->CCEN=0x10u;}
static void TIM_SetCnt(TIM_Module *p,uint16_t v){p->CNT=v;}
static uint16_t TIM_GetCnt(TIM_Module *p){return p->CNT;}
static void TIM_InitOcStruct(OCInitType *p){memset(p,0,sizeof(*p));}
static void TIM_InitOc1(TIM_Module *p,OCInitType *o){p->CCDAT1=o->Pulse;p->CCMOD1=o->OcMode;p->CCEN=1u;}
static void TIM_EnableOc1Preload(TIM_Module *p,unsigned a){(void)p;(void)a;}
static void TIM_ConfigArPreload(TIM_Module *p,unsigned a){(void)p;(void)a;}
#endif
