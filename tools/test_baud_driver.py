"""Compile actual UART baud setter against register/DMA ownership mocks."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
s=(r/'bleuart/cat1/cat1/cat1_usart1.c').read_text(encoding='utf-8')
a=s.index('int USART1_CAT1_SetBaudrate(');b=s.index('void USART1_CAT1_RxFlush',a)
mock=r"""#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef struct{uint32_t BaudRate,WordLength,StopBits,Parity,HardwareFlowControl,Mode;}USART_InitType;
#define USART1 1
#define USART1_RX_DMA_CH 2
#define USART1_CAT1_RX_RING_CAP 4096
#define USART_DMAREQ_RX 1
#define USART_DMAREQ_TX 2
#define DISABLE 0
#define ENABLE 1
#define USART_WL_8B 8
#define USART_STPB_1 1
#define USART_PE_NO 0
#define USART_HFCTRL_NONE 0
#define USART_MODE_RX 1
#define USART_MODE_TX 2
#define USART_INT_RXDNE 3
#define USART_FLAG_OREF 4
static uint32_t mask,s_uart_baud,changes,ndtr,dma_on,rx_on;
static uint8_t s_rx_dma_ready=1;
static uint16_t USART1_CAT1_RX_HEAD=11,USART1_CAT1_RX_TAIL=9,s_dma_last_head=11;
static uint32_t __get_PRIMASK(void){return mask;}
static void __disable_irq(void){mask=1;}
static void __set_PRIMASK(uint32_t m){mask=m;}
static void USART_EnableDMA(unsigned u,unsigned bits,unsigned on){assert(mask==1&&u==1);if(bits&1)rx_on=on;}
static void DMA_EnableChannel(unsigned ch,unsigned on){assert(mask==1&&ch==2);dma_on=on;}
static void USART_Enable(unsigned u,unsigned on){(void)on;assert(mask==1&&u==1);}
static void USART_StructInit(USART_InitType *p){*p=(USART_InitType){0};}
static void USART_Init(unsigned u,USART_InitType *p){assert(mask==1&&u==1&&p->WordLength==8&&p->Mode==3&&p->StopBits==1&&!p->Parity&&!p->HardwareFlowControl);++changes;}
static void USART_ConfigInt(unsigned u,unsigned intr,unsigned on){assert(u==1&&intr==3&&!on);}
static uint32_t USART_ReceiveData(unsigned u){assert(u==1);return 0;}
static void USART_ClrFlag(unsigned u,unsigned f){assert(u==1&&f==4);}
static void DMA_SetCurrDataCounter(unsigned ch,unsigned n){assert(ch==2&&mask==1);ndtr=n;}
"""
main=r"""
int main(void){
 s_uart_baud=115200;
 assert(USART1_CAT1_SetBaudrate(38400)==-1&&!changes);
 assert(!USART1_CAT1_SetBaudrate(115200)&&!changes&&!mask);
 assert(!USART1_CAT1_SetBaudrate(9600)&&changes==1&&!mask&&dma_on&&rx_on&&ndtr==4096);
 assert(!USART1_CAT1_RX_HEAD&&!USART1_CAT1_RX_TAIL&&!s_dma_last_head&&s_uart_baud==9600);
 mask=1;assert(!USART1_CAT1_SetBaudrate(115200)&&mask==1&&changes==2&&dma_on&&rx_on);
 s_rx_dma_ready=0;assert(USART1_CAT1_SetBaudrate(9600)==-1&&changes==2&&mask==1);
 puts("baud driver: invalid/same rate, 8N1, RX DMA_CH2 only, ring reset, DMA restored and nested IRQ mask preserved PASS");return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='vcr_baud_driver_') as d:
 p=Path(d);(p/'driver.c').write_text(mock+s[a:b]+main,encoding='utf-8');exe=p/'driver.exe'
 subprocess.run(['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror',str(p/'driver.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
