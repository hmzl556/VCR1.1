#ifndef RC_KEY_INPUT_H
#define RC_KEY_INPUT_H

#include <stdint.h>

void rc_key_input_init(void);

/* 在 rc_cat1_poll 中调用，将按键 Tag0x10 延后入队（避免 HID 路径栈溢出） */
void rc_key_input_poll_tag10(void);

/*
 * notify_value: RC-KEY-NOTIFY 特征值（HID usage，与原先一致）
 * key_id:       硬件按键编号，经 rc_key_id_to_tag10 映射为 Tag0x10 DataID1（1~18）
 */
void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t notify_value,
                        uint16_t key_id);

#endif /* RC_KEY_INPUT_H */
