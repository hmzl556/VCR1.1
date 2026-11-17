#ifndef _TASK_KEYSCAN_H_
#define _TASK_KEYSCAN_H_

#include <stdint.h>

#define KEY_DEBOUNCE_MS				20U
#define KEY_LONG_PRESS_MS			50U
#define KEY_RELEASE_TIMEOUT_MS		20U

/** @brief 红外重复码发送间隔（GY/T 407-2024 §6.1.4） */
#define IR_REPEAT_INTERVAL_MS		108U

// 按键编号定义（SDK 44 键编号 KEY_n = keyscan_map 下标, 与新版原理图线对一致;
// 枚举结果 2026-11-17 飞线实测确认, 见 doc/新板按键映射工作流程.md）
#define TASK_KEYSCAN_KEY1            1U   /* 电视机电源(右) PA1-PA0     */
#define TASK_KEYSCAN_KEY2            2U   /* 机顶盒电源(左) PA0-PA2     */
#define TASK_KEYSCAN_KEY3           23U   /* 电视机业务 PA1-PB9         */
#define TASK_KEYSCAN_KEY4            3U   /* 确定 PA1-PA2               */
#define TASK_KEYSCAN_KEY5            8U   /* 左 PA1-PA6                 */
#define TASK_KEYSCAN_KEY6            9U   /* 右 PA2-PA6                 */
#define TASK_KEYSCAN_KEY7            6U   /* 下 PA2-PA3                 */
#define TASK_KEYSCAN_KEY8           13U   /* 主页 PA2-PB10              */
#define TASK_KEYSCAN_KEY9           19U   /* 返回 PA3-PB8               */
#define TASK_KEYSCAN_KEY10          18U   /* 菜单 PA2-PB8               */
#define TASK_KEYSCAN_KEY11          17U   /* 频道+ PA1-PB8              */
#define TASK_KEYSCAN_KEY12          10U   /* 音量+ PA3-PA6              */
#define TASK_KEYSCAN_KEY13           4U   /* 频道- PA0-PA3              */
#define TASK_KEYSCAN_KEY14           7U   /* 音量- PA0-PA6              */
#define TASK_KEYSCAN_KEY15          16U   /* 语音 PA0-PB8               */
#define TASK_KEYSCAN_KEY16           5U   /* 上 PA1-PA3                 */
#define TASK_KEYSCAN_KEY17          11U   /* 回看 PA0-PB10              */
#define TASK_KEYSCAN_KEY18           2U   /* 机顶盒电源(左) PA0-PA2 (与KEY2同键) */
#define TASK_KEYSCAN_TV_POWER       TASK_KEYSCAN_KEY1  /* right */
#define TASK_KEYSCAN_STB_POWER      TASK_KEYSCAN_KEY18 /* left */

/* 新增按键 (SDK 键号, 飞线枚举确认) */
#define TASK_KEYSCAN_LIVE           12U   /* 直播业务 PA1-PB10          */
#define TASK_KEYSCAN_VOD            14U   /* 点播(暂借设置码) PA3-PB10  */
#define TASK_KEYSCAN_DIGIT1         15U   /* 数字1 PA6-PB10             */
#define TASK_KEYSCAN_DIGIT2         20U   /* 数字2 PA6-PB8              */
#define TASK_KEYSCAN_DIGIT3         21U   /* 数字3 PB8-PB10             */
#define TASK_KEYSCAN_DIGIT4         22U   /* 数字4 PA0-PB9              */
#define TASK_KEYSCAN_DIGIT5         24U   /* 数字5 PA2-PB9              */
#define TASK_KEYSCAN_DIGIT6         25U   /* 数字6 PA3-PB9              */
#define TASK_KEYSCAN_DIGIT7         26U   /* 数字7 PA6-PB9              */
#define TASK_KEYSCAN_DIGIT8         27U   /* 数字8 PB10-PB9             */
#define TASK_KEYSCAN_DIGIT9         28U   /* 数字9 PB8-PB9              */
#define TASK_KEYSCAN_DIGIT0         29U   /* 数字0 独立键 PA0-GND       */

// 恢复出厂设置相关时间常量（ms）
#define FACTORY_RST_COMBO_HOLD_MS   3000U   // 主页 + 音量- 同时长按 3s
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

#include "ir_learn.h"
void task_keyscan_ir_learn_result(ir_learn_evt_t evt, uint8_t user, uint8_t cmd);
bool task_keyscan_ir_busy(void);
#endif /* _TASK_KEYSCAN_H_ */
