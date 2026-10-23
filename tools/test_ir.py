"""Compile and execute actual firmware modules against deterministic hardware mocks.
Usage: python tools/test_ir.py [--cc path/to/gcc]
"""
from pathlib import Path
import argparse, subprocess, tempfile
parser=argparse.ArgumentParser();parser.add_argument('--cc',default='gcc');args=parser.parse_args()
root=Path(__file__).resolve().parents[1]; tests=root/'tools/tests'
with tempfile.TemporaryDirectory(prefix='vcr_ir_tests_') as temp:
    tmp=Path(temp)
    for name in ['br1601x.h','keyscan.h','rcc.h','gpio.h','tim.h','misc.h','gmem.h','udp_ota.h','qflash.h']:
        (tmp/name).write_text('#include "mock_hw.h"\n'+('uint32_t Qflash_Read(uint32_t,uint8_t*,uint32_t);\nuint32_t Qflash_Write(uint32_t,uint8_t*,uint32_t);\nuint32_t Qflash_Erase_Sector(uint32_t);\n' if name=='qflash.h' else ''))
    app_headers = ['app_voice_ctrl.h','app_keyscan.h','ble_hid_kbd.h','ble_gap.h','rc_key_input.h','pm_state.h','combo_key.h','ble_adv.h','cs_core.h','userhard.h','cat1_skt_udp.h','co_printf.h']
    for name in app_headers:
        extra = 'void combo_key_process(uint32_t,const uint32_t[5]);\nbool combo_key_is_idle(void);\n' if name=='combo_key.h' else ''
        (tmp/name).write_text('#include "mock_app.h"\n'+extra)
    # Test the actual OTA blink routine interleaved with actual key scanning.
    ota=(root/'bleuart/cat1/cat1/OTA/udp_ota.c').read_text(encoding='utf-8')
    start=ota.index('void udp_ota_download_led_poll(void)')
    end=ota.index('uint8_t udp_ota_poll_watchdog',start)
    (tmp/'actual_ota_led.h').write_text(ota[start:end],encoding='utf-8')
    for name in ['nec','raw_capture','receive','wave_store','tx','keys','combo','matrix','driver','rc6_encode','rc6_driver']:
        exe=tmp/(name+'.exe')
        subprocess.run([args.cc,'-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation','-I'+str(tmp),'-I'+str(tests),'-I'+str(root/'bleuart/IR'),'-I'+str(root/'bleuart/src'),'-I'+str(root/'bleuart/inc'),'-I'+str(root/'bleuart/cat1/cat1/OTA'),str(tests/('test_'+name+'.c')),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
