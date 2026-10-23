"""Host regression for newly merged APP modules, against actual C sources."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];t=r/'tools/tests'
with tempfile.TemporaryDirectory(prefix='vcr_base_merge_tests_') as temp:
 p=Path(temp)
 common='''#ifndef MOCK_MERGE_H
#define MOCK_MERGE_H
#include "mock_hw.h"
#include "real_pm_state.h"
static uint8_t modem_busy;
static uint32_t sdk_timer_get_current_ms(void){return now_ms;}
#undef co_printf
static int mock_log(const char *fmt,...){(void)fmt;return 0;}
#define co_printf mock_log
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define GPIO_PIN_2 4u
#define GPIO_PIN_3 8u
#define GPIO_PIN_6 64u
#define GPIO_PIN_7 128u
#define GPIO_PIN_8 256u
#define GPIO_PIN_9 512u
#define GPIO_PIN_10 1024u
#define GPIO_PIN_11 2048u
#define GPIO_PIN_12 4096u
#define GPIO_PIN_13 8192u
#define GPIO_MODE_OUTPUT_PP 3u
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_ANALOG 4u
#define GPIO_PULL_DOWN 2u
#define RCC_APB2_PERIPH_SPI1 1u
#define RCC_APB2_PERIPH_SPI2 2u
#define RCC_APB2_PERIPH_TIM1 4u
#define RCC_APB2_PERIPH_USART1 8u
#define RCC_APB1_PERIPH_WWDG 1u
#define RCC_AHB_PERIPH_CRC 1u
#define USART_CTRL1_TXEN 8u
#define USART_CTRL1_RXEN 4u
static GPIO_Module porta;
#define GPIOA (&porta)
static struct {uint32_t CTRL1;} uart1;
#define USART1 (&uart1)
typedef void *peer_t;
typedef struct{peer_t peer;uint16_t intv_min,intv_max,latency_max,timeout,ce_len_min,ce_len_max;} le_conn_update_t;
peer_t ble_gap_get_peer(void);
void le_conn_update(const le_conn_update_t *p);
uint8_t peer_bonded_count(void);
uint8_t br_sleep_lock_release(void);
void br_sleep_lock_acquire(void);
void ble_adv_start_by_bonding(void);
static void RCC_EnableAHBPeriphClk(unsigned mask,unsigned en){(void)mask;(void)en;}
static void RCC_EnableLpuartClk(unsigned en){(void)en;}
static unsigned red,blue;
#define SETLED1 do{red=1;}while(0)
#define CLRLED1 do{red=0;}while(0)
#define CLRLED2 do{blue=0;}while(0)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
void udp_ota_on_tag11_task(uint16_t sw,uint16_t packets);
#define EXTI_LINE13 8192u
#define SET 1u
#define RESET 0u
#define KEYSCAN_IRQn 5u
#define USART1_IRQn 8u
#define DMA_Channel1_2_3_4_IRQn 9u
#define DMA_INT_GLB2 0x10u
#define DMA_INT_HTX2 0x40u
#define DMA_INT_TXC2 0x20u
#define DMA_INT_ERR2 0x80u
#define DMA ((void*)1)
#define BR_TIMER_INVALID_HANDLER 0xffu
#define PWR_SLEEPENTRY_WFI 1u
static uint8_t bleAdvStat=0xa6;
unsigned EXTI_GetITStatus(unsigned line);
void EXTI_ClrITPendBit(unsigned line);
bool app_keyscan_any_pressed(void);
void KEYSCAN_InfoClear(void);
void KEYSCAN_ClearInterrupt(void);
void DMA_ClrIntPendingBit(unsigned flags,void *dma);
void IWDG_ReloadKey(void);
void ble_adv_stop(void);
void ble_gap_process_disconnect_pending(void);
void le_conn_destory(peer_t p);
void br_sleep(void);
void cs_schedule(void);
uint32_t sdk_timer_add(uint32_t ms,uint8_t ev,void (*cb)(uint8_t,void*),void *arg);
int sdk_timer_del(uint32_t tid);
void PWR_EnterSLEEPMode(uint8_t how);
void Audio_Pcm_HardwareShutdown(void);
static void delay_n_10us(unsigned n){(void)n;}
#endif
'''
 (p/'real_pm_state.h').write_text((r/'bleuart/pmu/inc/pm_state.h').read_text(encoding='utf-8-sig'), encoding='utf-8')
 (p/'mock_merge.h').write_text(common)
 for name in ['br1601x.h','gmem.h','co_printf.h','br_timer.h','udp_ota.h','pm_state.h','gap.h','ble_gap.h','userhard.h','rcc.h','ble_adv.h','FreeRTOS.h','task.h','exti.h','pwr.h','cs_core.h','iwdg.h','keyscan.h','misc.h','dma.h','rtc.h','app_audio_pcm.h','app_keyscan.h','ble_hid_kbd.h','app_voice_ctrl.h']:
  (p/name).write_text('#include "mock_merge.h"\n')
 # Test the timer functions actually compiled from main.c, without replacing
 # their implementation with a handwritten host equivalent.
 main=(r/'bleuart/src/main.c').read_text(encoding='utf-8-sig')
 start=main.index('/* One owned SDK timer,')
 end=main.index('// 初始化 BLE 栈与 GATT/GAP', start)
 (p/'actual_sdk_tick.c').write_text(main[start:end], encoding='utf-8')
 (p/'qflash.h').write_text('#include "mock_merge.h"\n')
 gap=(r/'bleuart/src/ble_gap.c').read_text(encoding='utf-8-sig')
 one=gap[gap.index('static uint32_t s_cccd_retry_tid'):gap.index('// 连接参数更新完成回调')]
 two=gap[gap.index('void ble_gap_connected('):gap.index('static void ble_conn_read_rssi(')]
 (p/'actual_ble_lifecycle.c').write_text(one+two,encoding='utf-8')
 voice=(r/'bleuart/src/app_voice_ctrl.c').read_text(encoding='utf-8-sig')
 (p/'actual_voice_control.c').write_text(voice[voice.index('static QueueHandle_t'):],encoding='utf-8')

 for name in ['heartbeat','at_urc','baud_sync','standby','pm_state','reports','sleep','sdk_tick','ota_safety','ble_lifecycle','voice_control','aes','ota_flow']:
  exe=p/(name+'.exe')
  subprocess.run(['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-unused-parameter','-Wno-pointer-to-int-cast','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(p),'-I'+str(t),'-I'+str(r/'bleuart/cat1/cat1'),'-I'+str(r/'bleuart/pmu/inc'),'-I'+str(r/'bleuart/pmu/src'),'-I'+str(r/'bleuart/IR'),'-I'+str(r/'bleuart/cat1/cat1/OTA'),'-I'+str(r/'bleuart/inc'),str(t/('test_'+name+'.c')),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
