"""Exercise actual battery state machine/filter with deterministic ADC/time mocks."""
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/"bleuart/src/bat_adc.c").read_text(encoding="utf-8")
constants=s[s.index("#define BAT_DIV_NUM"):s.index("/**")]
functions=s[s.index("/* Preserve the existing"):]
mock=r'''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <windows.h>
static unsigned lte,prepared,released,reads,fail_at,busy,ir,ota;
static uint32_t now;
static uint16_t sample[9]={700,700,700,700,700,700,700,700,700};
static uint8_t g_audio_pcm_enabled;
#define LTE_EN_IS_ON() (lte)
#define ADC_CTRL_CH_5 5
#define co_printf(...) ((void)0)
static uint32_t app_now_ms(void){return now;}
static unsigned cat1_modem_is_busy(void){return busy;}
static unsigned task_keyscan_ir_busy(void){return ir;}
static unsigned udp_ota_session_busy(void){return ota;}
static void bat_adc_hw_prepare(void){prepared++;}
static void bat_adc_hw_release(void){released++;}
static int bat_adc_read_once_raw(uint16_t *v){*v=sample[reads%9];reads++;return fail_at&&reads==fail_at?-1:0;}
static uint32_t ADC_ConverValueToVoltage(uint16_t v,unsigned ch){(void)ch;return v;}
'''
main=r'''
static void reset(void){
 unsigned i;
 lte=prepared=released=reads=fail_at=busy=ir=ota=0;now=0;g_audio_pcm_enabled=0;
 s_pb6_analog_ready=0;s_bat_level_display=100;s_bat_ema_q8=0;s_bat_valid=0;s_last_update_ms=0;
 bat_adc_restart_window();for(i=0;i<9;i++)sample[i]=700;
}
static void tick(unsigned ms){now+=ms;bat_adc_poll();}
static void initial_set(void){lte=1;bat_adc_poll();tick(200);tick(20);tick(20);assert(s_bat_valid);}
int main(void){
 SetErrorMode(SEM_NOGPFAULTERRORBOX);
 unsigned i,old;uint8_t valid;uint16_t mv;
 reset();assert(bat_get_level_percent()==100&&bat_get_cached_level()==100);
 for(i=0;i<1000;i++)bat_adc_poll();assert(!reads);
 lte=1;bat_adc_poll();tick(199);assert(!reads);
 tick(1);assert(reads==9&&!s_bat_valid&&bat_get_level_percent()==100);
 tick(19);assert(reads==9);tick(1);assert(reads==18&&!s_bat_valid);
 tick(20);assert(reads==27&&s_bat_valid&&prepared==3&&released==3);
 valid=bat_get_cached_level();assert(valid>0&&valid<100);
 for(i=0;i<10000;i++){assert(bat_get_level_percent()==valid);assert(bat_get_cached_level()==valid);bat_adc_poll();}
 assert(reads==27);
 tick(59999);assert(reads==27);tick(1);assert(reads==36);tick(20);tick(20);assert(reads==54);
 lte=0;for(i=0;i<1000;i++)bat_adc_poll();assert(reads==54&&bat_get_cached_level()==valid);
 /* Power/reset/wake hook preserves cache and rate limit, but re-arms settling. */
 lte=1;bat_adc_restart_window();tick(60000);assert(reads==54);tick(199);assert(reads==54);tick(1);assert(reads==63);
 /* Each busy condition cancels partial groups, retaining the valid cache. */
 old=reads;g_audio_pcm_enabled=1;tick(1000);assert(reads==old&&!s_group_count);
 g_audio_pcm_enabled=0;busy=1;tick(1000);assert(reads==old);
 busy=0;ir=1;tick(1000);assert(reads==old);ir=0;ota=1;tick(1000);assert(reads==old);
 ota=0;bat_adc_poll();tick(99);assert(reads==old);tick(1);assert(reads==old+9&&s_group_count==1);
 assert(bat_get_cached_level()==valid);
 /* Failed conversions cannot publish partial data; retry bounded to 1s. */
 reset();lte=1;bat_adc_poll();fail_at=5;tick(200);
 assert(reads==5&&prepared==1&&released==1&&!s_bat_valid&&!s_group_count);
 assert(bat_get_cached_level()==100);tick(999);assert(reads==5);
 tick(1);assert(reads==14&&!s_bat_valid);tick(20);tick(20);assert(s_bat_valid&&prepared==released);
 /* Trim spikes within each group; discard startup conversion. */
 reset();sample[0]=1000;sample[1]=0;sample[2]=1000;
 assert(bat_sample_group(&mv)==0&&mv==(700U*2470U)/470U);
 /* Median of three independent groups rejects one entire high group. */
 reset();lte=1;bat_adc_poll();tick(200);
 for(i=0;i<9;i++)sample[i]=900;tick(20);
 for(i=0;i<9;i++)sample[i]=701;tick(20);
 assert(s_bat_valid&&s_bat_ema_q8==bat_mv_to_q8((701U*2470U)/470U));
 /* Q8 EMA follows a small persistent change that integer EMA lost. */
 reset();bat_publish(3750,0);assert(bat_get_cached_level()==50);
 for(i=1;i<=160;i++)bat_publish(3687,i*60000U);
 assert(bat_get_cached_level()>=43&&bat_get_cached_level()<=44&&s_bat_ema_q8<44U*BAT_Q);
 reset();bat_publish(3750,0);for(i=1;i<=160;i++)bat_publish(3741,i*60000U);
 assert(s_bat_ema_q8<50U*BAT_Q&&bat_get_cached_level()==50); /* intentional 2% hysteresis */
 reset();bat_publish(3750,0);bat_publish(3687,30U*60000U);
 assert(bat_get_cached_level()<50); /* elapsed-time catch-up, not read-count based */
 for(i=31;i<=200;i++)bat_publish(3300,i*60000U);assert(bat_get_cached_level()==0);
 for(i=201;i<=400;i++)bat_publish(4200,i*60000U);assert(bat_get_cached_level()==100);
 assert(bat_mv_to_q8(0)==0&&bat_mv_to_q8(5000)==100U*BAT_Q);
 /* Unsigned elapsed time is safe across the 32-bit millisecond wrap. */
 reset();now=0xffffff80U;initial_set();assert(reads==27&&s_bat_valid);
 tick(60000);assert(reads==36);
 puts("battery: cache-only readers, 60s bound, power/quiet settling, trimmed groups/median, Q8 slow drift/endpoints, elapsed EMA, ADC failures/retry, voice/IR/AT/OTA guards, power/wake retention and clock wrap PASS");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="vcr_battery_") as d:
 p=Path(d);c=p/"battery.c";exe=p/"battery.exe"
 c.write_text(mock+constants+functions+main,encoding="utf-8")
 subprocess.run(["gcc","-std=c99","-O2","-Wall","-Wextra","-Werror","-Wno-misleading-indentation",str(c),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
# Keep sampling out of readers and before modem transmit work in the BLE loop.
ble=(r/"bleuart/src/task_ble.c").read_text(encoding="utf-8")
assert ble.index("task_voice_process_once();") < ble.index("bat_adc_poll();") < ble.index("rc_cat1_poll();")
bridge=(r/"bleuart/cat1/cat1/rc_cat1_bridge.c").read_text(encoding="utf-8")
assert "LTE_EN_OFF();\n    bat_adc_restart_window();" in bridge
assert "void rc_cat1_init(void)\n{\n    bat_adc_restart_window();" in bridge
