"""键值下载读流程闭环回归：编译真实 upg_*.c，桩替代协议栈与外设。
用法: python tools/test_upg_flow.py [--cc path/to/gcc]
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()

root = Path(__file__).resolve().parents[1]
tests = root / 'tools/tests'
upg = root / 'bleuart/cat1/cat1/UPG'
ota = root / 'bleuart/cat1/cat1/OTA'

# 只声明被测单元需要的外部符号，实现放在 test_upg_flow.c 里
STUBS = {
    'gmem.h': '#ifndef GMEM_H\n#define GMEM_H\n#include <stdint.h>\nuint32_t app_now_ms(void);\n#endif\n',
    'co_printf.h': '#ifndef CO_PRINTF_H\n#define CO_PRINTF_H\n#define co_printf(...) ((void)0)\n#endif\n',
    'ble_gap.h': ('#ifndef BLE_GAP_H\n#define BLE_GAP_H\n#include <stdint.h>\n'
                  'typedef void *peer_t;\npeer_t ble_gap_get_peer(void);\n'
                  'uint16_t ble_gap_get_mtu(void);\n#endif\n'),
    'udp_bootload.h': ('#ifndef UDP_BOOTLOAD_H\n#define UDP_BOOTLOAD_H\n#include <stdint.h>\n'
                       'uint8_t *udp_bootload_borrow_buf(uint16_t *len);\n#endif\n'),
    'udp_ota.h': ('#ifndef UDP_OTA_H\n#define UDP_OTA_H\n#include <stdint.h>\n'
                  'uint8_t udp_ota_session_busy(void);\n#endif\n'),
    'ble_adv.h': ('#ifndef BLE_ADV_H\n#define BLE_ADV_H\n'
                  'int ble_adv_stop(void);\nvoid ble_adv_start_by_bonding(void);\n#endif\n'),
    'ir_key_map.h': '#ifndef IR_KEY_MAP_H\n#define IR_KEY_MAP_H\nvoid ir_tx_cancel(void);\n#endif\n',
    'ir_learn.h': '#ifndef IR_LEARN_H\n#define IR_LEARN_H\nvoid ir_learn_stop(void);\n#endif\n',
    'app_voice_ctrl.h': ('#ifndef APP_VOICE_CTRL_H\n#define APP_VOICE_CTRL_H\n'
                         'int voice_ctrl_post_stop(void);\n#endif\n'),
    'app_audio_pcm.h': ('#ifndef APP_AUDIO_PCM_H\n#define APP_AUDIO_PCM_H\n#include <stdint.h>\n'
                        'extern volatile uint8_t g_audio_pcm_enabled;\nvoid Audio_Pcm_Stop(void);\n#endif\n'),
    'qflash.h': ('#ifndef QFLASH_H\n#define QFLASH_H\n#include <stdint.h>\n'
                 '#define FlashOperationSuccess 0u\n#define FLASH_PAGE_SIZE 0x100\n#define FLASH_SECTOR_SIZE 0x1000\n'
                 'uint32_t Qflash_Read(uint32_t addr, uint8_t *buf, uint32_t len);\n'
                 'uint32_t Qflash_Write(uint32_t addr, uint8_t *buf, uint32_t len);\n'
                 'uint32_t Qflash_Erase_Sector(uint32_t addr);\n#endif\n'),
    'rc_protocol.h': ('#ifndef RC_PROTOCOL_H\n#define RC_PROTOCOL_H\n'
                      '#define RC_READ_VAL_DEVICE_SW_VERSION "02020000"\n#endif\n'),
}

with tempfile.TemporaryDirectory(prefix='vcr_upg_flow_') as temp:
    tmp = Path(temp)
    for name, body in STUBS.items():
        (tmp / name).write_text(body, encoding='utf-8')

    exe = tmp / 'upg_flow.exe'
    subprocess.run([
        args.cc, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
        '-I' + str(tmp),
        '-I' + str(upg),
        '-I' + str(ota),
        '-I' + str(tests),
        str(tests / 'test_upg_flow.c'),
        '-o', str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)
