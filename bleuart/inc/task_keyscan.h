#ifndef _TASK_KEYSCAN_H_
#define _TASK_KEYSCAN_H_

#include <stdint.h>

#define TASK_KEYSCAN_KEY15			15U
#define KEY_DEBOUNCE_MS				20U
#define KEY_LONG_PRESS_MS			50U
#define KEY_RELEASE_TIMEOUT_MS		20U

/** @brief 红外重复码发送间隔（GY/T 407-2024 §6.1.4） */
#define IR_REPEAT_INTERVAL_MS		108U

// 按键编号定义（与 KEYSCAN 中断上报一致）
#define TASK_KEYSCAN_KEY1            1U
#define TASK_KEYSCAN_KEY2            2U
#define TASK_KEYSCAN_KEY3            3U
#define TASK_KEYSCAN_KEY4            4U
#define TASK_KEYSCAN_KEY5            5U  /* Left; matrix-safe combo alias */
#define TASK_KEYSCAN_KEY6            6U
#define TASK_KEYSCAN_KEY7            7U
#define TASK_KEYSCAN_KEY8            8U
#define TASK_KEYSCAN_KEY9            9U
#define TASK_KEYSCAN_KEY10          10U
#define TASK_KEYSCAN_KEY11          11U
#define TASK_KEYSCAN_KEY12          12U
#define TASK_KEYSCAN_KEY13          13U
#define TASK_KEYSCAN_KEY14          14U
#define TASK_KEYSCAN_KEY16          16U
#define TASK_KEYSCAN_KEY17          17U
#define TASK_KEYSCAN_KEY18          18U /* 机顶盒电源（左）PA0-PA2 */
#define TASK_KEYSCAN_KEY19          19U /* 点播 PA3-PB10 */
#define TASK_KEYSCAN_KEY20          20U /* 数字1 PA6-PB10 */
#define TASK_KEYSCAN_KEY21          21U /* 数字2 PA6-PB8  */
#define TASK_KEYSCAN_KEY22          22U /* 数字3 PB8-PB10 */
#define TASK_KEYSCAN_KEY23          23U /* 数字4 PA0-PB9  */
#define TASK_KEYSCAN_KEY24          24U /* 数字5 PA2-PB9  */
#define TASK_KEYSCAN_KEY25          25U /* 数字6 PA3-PB9  */
#define TASK_KEYSCAN_KEY26          26U /* 数字7 PA6-PB9  */
#define TASK_KEYSCAN_KEY27          27U /* 数字8 PB10-PB9 */
#define TASK_KEYSCAN_KEY28          28U /* 数字9 PB8-PB9  */
#define TASK_KEYSCAN_KEY29          29U /* 数字0 独立键 PA0-GND */
#define TASK_KEYSCAN_TV_POWER       TASK_KEYSCAN_KEY1  /* right */
#define TASK_KEYSCAN_STB_POWER      TASK_KEYSCAN_KEY18 /* left */

/* ---- 表 B.1 语义别名（GY/T 407-2024 附录 B）----
 * 29 键目标形态的编号与本版不同，路由逻辑一律用这些别名，
 * 换键位时只改这里的定义，不改业务代码。
 * 注：目标形态**取消静音键**，KEY2 改为「直播业务」。 */
#define TASK_KEYSCAN_LIVE           TASK_KEYSCAN_KEY2   /* 序号2  直播业务（原静音） */
#define TASK_KEYSCAN_TV_BUSINESS    TASK_KEYSCAN_KEY3   /* 序号3  电视机业务 */
#define TASK_KEYSCAN_CH_UP          TASK_KEYSCAN_KEY11  /* 序号18 频道+（位号见下） */
#define TASK_KEYSCAN_CH_DOWN        TASK_KEYSCAN_KEY13  /* 序号19 频道- */
#define TASK_KEYSCAN_REPLAY         TASK_KEYSCAN_KEY17  /* 序号20 回看 */
#define TASK_KEYSCAN_VOICE          TASK_KEYSCAN_KEY15  /* 序号13 语音（BLE-only） */
#define TASK_KEYSCAN_MUTE           TASK_KEYSCAN_KEY19  /* 静音（原理图丝印误写为点播） */
#define TASK_KEYSCAN_DIGIT1         TASK_KEYSCAN_KEY20  /* 序号21 数字1 */
#define TASK_KEYSCAN_DIGIT2         TASK_KEYSCAN_KEY21  /* 序号22 数字2 */
#define TASK_KEYSCAN_DIGIT3         TASK_KEYSCAN_KEY22  /* 序号23 数字3 */
#define TASK_KEYSCAN_DIGIT4         TASK_KEYSCAN_KEY23  /* 序号24 数字4 */
#define TASK_KEYSCAN_DIGIT5         TASK_KEYSCAN_KEY24  /* 序号25 数字5 */
#define TASK_KEYSCAN_DIGIT6         TASK_KEYSCAN_KEY25  /* 序号26 数字6 */
#define TASK_KEYSCAN_DIGIT7         TASK_KEYSCAN_KEY26  /* 序号27 数字7 */
#define TASK_KEYSCAN_DIGIT8         TASK_KEYSCAN_KEY27  /* 序号28 数字8 */
#define TASK_KEYSCAN_DIGIT9         TASK_KEYSCAN_KEY28  /* 序号29 数字9 */
#define TASK_KEYSCAN_DIGIT0         TASK_KEYSCAN_KEY29  /* 序号30 数字0 */

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
/** CAT1 AT 结束后补发延后的 HID（与 AT 错峰） */
void task_keyscan_flush_deferred_hid(void);
void task_keyscan_reset_on_ble_disconnect(void);
/** @brief 当前双控模式：0=机顶盒 1=电视机 */
uint8_t task_keyscan_control_mode(void);
/** @brief 切换双控模式（组合键与业务键使用） */
void task_keyscan_set_control_mode(uint8_t mode);
#define TASK_KEYSCAN_MODE_STB 0U
#define TASK_KEYSCAN_MODE_TV  1U

#include "ir_learn.h"
void task_keyscan_ir_learn_result(ir_learn_evt_t evt, uint8_t user, uint8_t cmd);
bool task_keyscan_ir_busy(void);
#endif /* _TASK_KEYSCAN_H_ */
