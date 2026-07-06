#ifndef _TASK_BLE_H_
#define _TASK_BLE_H_

#include <stdint.h>

void task_ble_entry(void *pvParameters);

/** 初始化 BLE 栈/GATT/GAP，不启动广播；OTA 会话期间并行 HID 时使用 */
uint8_t ble_stack_init_only(void);

#endif /* _TASK_BLE_H_ */
