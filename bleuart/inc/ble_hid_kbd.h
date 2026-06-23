#ifndef BLE_HID_KBD_H
#define BLE_HID_KBD_H

#include <stdint.h>
#include <stdbool.h>
#include "hogp.h"

// HID 报告 ID（对应 Report Map 中的 1/2/3）。
#define HID_KBD_RPT_ID_KEYBOARD  (1u)
#define HID_KBD_RPT_ID_CONSUMER  (2u)
#define HID_KBD_RPT_ID_VENDOR    (3u)

// 常用报告长度定义，便于上层构造缓冲区。
#define HID_KBD_REPORT_LEN        (8u)
#define HID_CONSUMER_REPORT_LEN   (1u)

// 键盘修饰键位（报告第 0 字节）。
#define HID_MOD_LCTRL  (0x01u)
#define HID_MOD_LSHIFT (0x02u)
#define HID_MOD_LALT   (0x04u)
#define HID_MOD_LGUI   (0x08u)
#define HID_MOD_RCTRL  (0x10u)
#define HID_MOD_RSHIFT (0x20u)
#define HID_MOD_RALT   (0x40u)
#define HID_MOD_RGUI   (0x80u)

// 常用键盘 Usage ID（Keyboard Page，供测试/映射使用）。
#define HID_KEY_A      (0x04u)
#define HID_KEY_B      (0x05u)
#define HID_KEY_C      (0x06u)
#define HID_KEY_ENTER  (0x28u)
#define HID_KEY_ESC    (0x29u)
#define HID_KEY_SPACE  (0x2Cu)
#define HID_KEY_LEFT   (0x50u)
#define HID_KEY_RIGHT  (0x4Fu)
#define HID_KEY_UP     (0x52u)
#define HID_KEY_DOWN   (0x51u)

// 常用 Consumer Usage ID（Consumer Page，16-bit）。
#define HID_CONSUMER_VOL_UP    (0x00E9u)
#define HID_CONSUMER_VOL_DOWN  (0x00EAu)
#define HID_CONSUMER_MUTE      (0x00E2u)
#define HID_CONSUMER_PLAY_PAUSE (0x00CDu)


// HID 键盘业务层接口：只负责服务启动与报告发送，不包含任何硬件或按键扫描逻辑。
void hid_kbd_init(void);
void hid_kbd_on_disconnect(void);
bool hid_kbd_can_send_current(void);
void hid_kbd_send_report(peer_t peer, uint8_t rpt_id, uint8_t rpt_type, const uint8_t *data, uint16_t len);
// 便捷接口：使用当前已连接的对端句柄发送 HID 报告。
void hid_kbd_send_report_current(uint8_t rpt_id, uint8_t rpt_type, const uint8_t *data, uint16_t len);
// 测试接口：发送一次固定键值（按下+抬起），用于验证 HID 通路。
void hid_kbd_send_test_key_current(void);
// 业务接口：发送单个键值（按下+抬起），用于后续按键映射对接。
void hid_kbd_send_keycode_current(uint8_t usage_id);
// 业务接口：发送带修饰键的单键（按下+抬起），如 Ctrl/Shift/Alt 等。
void hid_kbd_send_keycode_with_mod_current(uint8_t modifier, uint8_t usage_id);
// 业务接口：发送 Consumer 控制键值（按下+抬起）。
void hid_kbd_send_consumer_current(uint16_t usage_id);
// 业务接口：发送 Consumer 控制键释放（全 0）。
void hid_kbd_send_consumer_release_current(void);
// 业务接口：发送键盘释放报告（全 0）。
void hid_kbd_send_release_current(void);
// 映射接口：将本地按键编码映射为 HID Usage（后续按键对接用）。
// 返回 true 表示命中映射；false 表示未定义。
bool hid_kbd_map_key_to_usage(uint16_t key_id, uint16_t *usage_id, bool *is_consumer);
// 业务接口：按本地按键编码触发 HID 发送（内部调用映射 + 发送）。
void hid_kbd_send_key_id_current(uint16_t key_id);

// 强制使能所有 HID CCCD 标志（回连已绑定设备且 STB 不写 CCCD 时使用）
void hid_kbd_force_cccd_enable(peer_t peer);
// 检查 STB 是否已主动使能了任一个 HID CCCD
bool hid_kbd_is_any_cccd_enabled(void);

#endif
