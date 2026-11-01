#ifndef RC_KEY_INPUT_H
#define RC_KEY_INPUT_H

#include <stdint.h>

void rc_key_input_init(void);

/* 在 rc_cat1_poll 中调用，将按键 Tag0x10 延后入队（避免 HID 路径栈溢出） */
void rc_key_input_poll_tag10(void);

/*
 * notify_value: RC-KEY-NOTIFY 特征值（HID usage，与原先一致）
 * key_id:       硬件按键编号，经 rc_key_id_to_tag10 映射为 Tag0x10 DataID1（1~18）
 * ir_routed:    1=BLE 断开（红外模式），按键主动上报 Tag0x10（与红外是否真正发出无关）；
 *               0=BLE 连接，走机顶盒路径，不重复直报
 */
void rc_key_input_event(uint8_t key_type, uint8_t action, uint16_t notify_value,
                        uint16_t key_id, uint8_t ir_routed);

/* 纯红外键（电视机电源）专用：只上报 Tag0x10，不发 BLE RC-KEY-NOTIFY */
void rc_key_input_report_tag10(uint16_t key_id, uint8_t action);

/** @return 1=聚合缓冲中还有尚未上报的 Tag0x10 按键（CAT1 需保持上电直到 flush） */
uint8_t rc_key_input_has_pending_tag10(void);

#endif /* RC_KEY_INPUT_H */
