/**
 * @file    combo_key.c
 * @brief   组合键管理器实现
 * @note    统一管理两组简单组合键：
 *          1. KEY14 + KEY5 → 进入配对广播（LED2 200ms 闪，30s 超时）
 *          2. KEY5  + KEY9 → 解除配对（LED1 200ms 闪，断连+擦除绑定→SLEEP）
 *
 *          每周期由 task_keyscan_process() 调用，使用外部传入的硬件快照。
 *          与工厂复位（KEY6+KEY14）互斥：调用方在 factory_rst_state != IDLE 时
 *          跳过 combo_key_process()；本模块通过 combo_key_is_idle() 通知调用方
 *          是否允许工厂复位运行。
 */

#include "combo_key.h"
#include "app_keyscan.h"      /* keyscan_is_key_pressed, KEYSCAN_INFO_CHECK, KEY_NUM */
#include "task_keyscan.h"     /* TASK_KEYSCAN_KEY4/5/6/9/14/15 */
#include "ble_gap.h"          /* ble_gap_get_peer, ble_gap_clear_bonding_info */
#include "ble_adv.h"          /* ble_adv_start_pairing */
#include "pm_state.h"         /* pm_state_transition, PM_STATE_PAIRING, PM_STATE_SLEEP */
#include "userhard.h"         /* SETLED1, CLRLED1 */
#include "co_printf.h"

/*===========================================================================
 * 一、类型定义
 *===========================================================================*/

typedef enum {
    COMBO_IDLE = 0,
    COMBO_HOLDING,       /* 双键按住中，计时 3s                     */
    COMBO_TRIGGERED,     /* 已触发，执行后续动作（仅解配使用此状态） */
} combo_state_t;

typedef struct {
    uint8_t        key1;          /* 按键编号 1                      */
    uint8_t        key2;          /* 按键编号 2                      */
    combo_state_t  state;         /* 当前状态                        */
    uint32_t       state_ts_ms;   /* 进入当前状态的时间戳            */
    uint32_t       led_ts_ms;     /* LED 闪烁时间戳                  */
    bool           led_on;        /* LED 当前亮灭                    */
} combo_key_def_t;

/*===========================================================================
 * 二、静态变量
 *===========================================================================*/

/* 组合键注册表（编译时确定，不动态分配） */
#define COMBO_IDX_PAIR   0
#define COMBO_IDX_UNPAIR 1
#define COMBO_COUNT      2

static combo_key_def_t g_combos[COMBO_COUNT];

/*===========================================================================
 * 三、内部辅助函数
 *===========================================================================*/

/** @brief 检测组合键的两个按键是否同时按下 */
static bool combo_both_keys_down(const combo_key_def_t *c, const uint32_t hw[2])
{
    return keyscan_is_key_pressed(hw, c->key1)
        && keyscan_is_key_pressed(hw, c->key2);
}

/** @brief 检测组合键是否有任一按键松开 */
static bool combo_any_key_up(const combo_key_def_t *c, const uint32_t hw[2])
{
    return !keyscan_is_key_pressed(hw, c->key1)
        || !keyscan_is_key_pressed(hw, c->key2);
}

/*===========================================================================
 * 四、公共 API
 *===========================================================================*/

void combo_key_init(void)
{
    /* Combo #1: KEY14 + KEY5 → 进入配对 */
    g_combos[COMBO_IDX_PAIR].key1      = TASK_KEYSCAN_KEY14;
    g_combos[COMBO_IDX_PAIR].key2      = TASK_KEYSCAN_KEY4;
    g_combos[COMBO_IDX_PAIR].state     = COMBO_IDLE;
    g_combos[COMBO_IDX_PAIR].state_ts_ms = 0U;
    g_combos[COMBO_IDX_PAIR].led_ts_ms   = 0U;
    g_combos[COMBO_IDX_PAIR].led_on      = false;

    /* Combo #2: KEY5 + KEY9 → 解除配对 */
    g_combos[COMBO_IDX_UNPAIR].key1      = TASK_KEYSCAN_KEY4;
    g_combos[COMBO_IDX_UNPAIR].key2      = TASK_KEYSCAN_KEY9;
    g_combos[COMBO_IDX_UNPAIR].state     = COMBO_IDLE;
    g_combos[COMBO_IDX_UNPAIR].state_ts_ms = 0U;
    g_combos[COMBO_IDX_UNPAIR].led_ts_ms   = 0U;
    g_combos[COMBO_IDX_UNPAIR].led_on      = false;
}

bool combo_key_is_idle(void)
{
    uint8_t i;
    for (i = 0; i < COMBO_COUNT; i++) {
        if (g_combos[i].state != COMBO_IDLE) {
            return false;
        }
    }
    return true;
}

bool combo_key_unpair_in_progress(void)
{
    return (g_combos[COMBO_IDX_UNPAIR].state == COMBO_TRIGGERED);
}

void combo_key_process(uint32_t now_ms, const uint32_t hw_snapshot[5])
{
    uint8_t i;

    // /* 诊断: 每500次打印按键快照值，确认combo_key_process被调用且读到按键 */
    // {
    //     static uint16_t diag_cnt = 0;
    //     static uint16_t init_done = 0;
    //     diag_cnt++;
    //     if (init_done == 0) {
    //         co_printf("CK init: k1=%d k2=%d\r\n",
    //                   g_combos[COMBO_IDX_PAIR].key1,
    //                   g_combos[COMBO_IDX_PAIR].key2);
    //         init_done = 1;
    //     }
    //     if ((diag_cnt % 500) == 0) {
    //         bool k14 = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY14);
    //         bool k5  = keyscan_is_key_pressed(hw_snapshot, TASK_KEYSCAN_KEY5);
    //         co_printf("CK %08lX %08lX k14=%d k5=%d\r\n",
    //                   hw_snapshot[0], hw_snapshot[1], k14, k5);
    //     }
    // }

    for (i = 0; i < COMBO_COUNT; i++) {
        combo_key_def_t *c = &g_combos[i];

        switch (c->state) {

        case COMBO_IDLE:
            if (combo_both_keys_down(c, hw_snapshot)) {
                c->state = COMBO_HOLDING;
                c->state_ts_ms = now_ms;
                //SETLED2;
                co_printf("[COMBO] key%d+key%d pressed, hold 3s...\r\n",
                          c->key1, c->key2);
            }
            break;

        /* ---------------------------------------------------------- */
        /*  HOLDING：计时 3s，松手取消                                  */
        /* ---------------------------------------------------------- */
        case COMBO_HOLDING:
            if (combo_any_key_up(c, hw_snapshot)) {
                /* 提前松开 → 取消 */
                c->state = COMBO_IDLE;
                co_printf("[COMBO] key%d+key%d released early, cancel.\r\n",
                          c->key1, c->key2);
            } else if ((now_ms - c->state_ts_ms) >= COMBO_KEY_HOLD_MS) {
                /* 3s 到 → 触发动作 */
                if (i == COMBO_IDX_PAIR)    //KEY14+KEY5
                {  
                    /* ---- 配对组合键 ---- */
                    co_printf("[COMBO] KEY14+KEY5 triggered! Enter pairing...\r\n");

                    /*
                     * 先清除旧绑定以确保设备可被发现。
                     * 再设置配对广播活跃标志，使 ble_gap_led_indicate() 开始闪烁 LED2。
                     * 注意：clear_bonding_info() 会重置 g_pairing_active，所以必须在它之后设置。
                     */
                    ble_gap_clear_bonding_info();
                    g_pairing_active = true;

                    /* 进入配对态并开始可发现广播 */
                    pm_state_transition(PM_STATE_PAIRING);
                    ble_adv_start_pairing();

                    /*
                     * LED2 闪烁由 ble_gap_led_indicate() 自动处理：
                     *   ble_gap_get_peer()==NULL && peer_bonded_count()==0
                     * 30s 超时由 PM 状态机处理：PAIRING → SLEEP
                     * 连接成功时 PM 自动切到 WORKING，LED2 灭
                     */
                    c->state = COMBO_IDLE;

                } else {
                    /* ---- 解配组合键 ---- */
                    co_printf("[COMBO] KEY5+KEY9 triggered! Unpairing...\r\n");
                    c->state = COMBO_TRIGGERED;
                    c->state_ts_ms = now_ms;
                    c->led_ts_ms   = now_ms;
                    c->led_on      = true;
                    SETLED1;

                    /*
                     * 清除绑定信息（内部先断连再擦除 Flash）。
                     * 断连是异步的，ble_gap_disconnected() 回调中：
                     *   pm_notify_disconnected() 无绑定 → IDLE
                     *   ble_gap_process_disconnect_pending() 检测到
                     *   combo_key_unpair_in_progress()==true → 跳过广播重启
                     */
                    ble_gap_clear_bonding_info();
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /*  TRIGGERED：仅解配使用，等断连或超时，LED1 闪烁              */
        /* ---------------------------------------------------------- */
        case COMBO_TRIGGERED:
        {
            /* LED1 200ms 周期闪烁 */
            if ((now_ms - c->led_ts_ms) >= COMBO_LED_FLASH_MS) {
                c->led_ts_ms = now_ms;
                c->led_on = !c->led_on;
                if (c->led_on) SETLED1; else CLRLED1;
            }

            uint32_t elapsed      = now_ms - c->state_ts_ms;
            bool     disconnected = (ble_gap_get_peer() == NULL);
            bool     min_led_met  = (elapsed >= COMBO_UNPAIR_LED_MIN_MS); //确保 LED 至少亮 2s 给用户视觉反馈
            bool     timed_out    = (elapsed >= COMBO_UNPAIR_TIMEOUT_MS); //安全网，防止意外卡死

            /*
             * 退出条件：
             *   1. 已断连 且 LED 至少亮了 2s（给用户视觉反馈）
             *   2. 30s 超时（安全网，防止意外卡死）
             */
            if ((disconnected && min_led_met) || timed_out) {
                CLRLED1;
                c->state = COMBO_IDLE;
                co_printf("[COMBO] Unpair done (elapsed=%lums). Enter SLEEP.\r\n",
                          (unsigned long)elapsed);
                pm_state_transition(PM_STATE_SLEEP);
            }
            break;
        }

        default:
            c->state = COMBO_IDLE;
            break;
        }
    }
}
