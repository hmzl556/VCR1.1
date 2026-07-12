#ifndef _TASK_KEYSCAN_H_
#define _TASK_KEYSCAN_H_

#include <stdint.h>

#define TASK_KEYSCAN_KEY15			15U
#define KEY_DEBOUNCE_MS				20U
#define KEY_LONG_PRESS_MS			50U
#define KEY_RELEASE_TIMEOUT_MS		20U

// 按键编号定义（与 KEYSCAN 中断上报一致）
#define TASK_KEYSCAN_KEY1            1U
#define TASK_KEYSCAN_KEY3            3U
#define TASK_KEYSCAN_KEY4            4U
#define TASK_KEYSCAN_KEY6            6U
#define TASK_KEYSCAN_KEY8            8U
#define TASK_KEYSCAN_KEY9            9U
#define TASK_KEYSCAN_KEY14          14U

// 恢复出厂设置相关时间常量（ms）
#define FACTORY_RST_COMBO_HOLD_MS   3000U   // KEY6 + KEY14 同时长按 3s
#define FACTORY_RST_PENDING_MS      10000U  // 待恢复出厂模式 10s 窗口
#define FACTORY_RST_LED_ON_MS       2000U   // 确认/取消指示灯亮 2s
#define LED_FAST_FLASH_MS           200U    // LED2 快闪周期 200ms
#define KEY_LED_ON_MS               100U   // 按键LED亮2s


extern volatile uint16_t key_receive;

void task_keyscan_process(void);
/** 在 task_voice_process_once 之后调用，发送 KEY_15 语音 defer 的 RC-KEY notify */
void task_keyscan_flush_deferred_voice_rc(void);
void task_keyscan_reset_on_ble_disconnect(void);

#endif /* _TASK_KEYSCAN_H_ */
