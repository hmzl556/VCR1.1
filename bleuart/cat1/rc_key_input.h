#ifndef RC_KEY_INPUT_H
#define RC_KEY_INPUT_H

#include <stdint.h>

void rc_key_input_init(void);

/* 在 rc_cat1_poll 中调用，将按键 Tag0x10 延后入队（避免 HID 路径栈溢出） */
void rc_key_input_poll_tag10(void);

/* key_value: RC-KEY-NOTIFY 与 Tag0x10 DataID1（HID usage 低字节，KEY_15=0x3E） */
void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t key_value);

#endif /* RC_KEY_INPUT_H */
