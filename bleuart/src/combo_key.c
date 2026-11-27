/**
 * @file    combo_key.c
 * @brief   组合键管理器实现
 * @note    STB pairing: volume- + TV business; unpairing: back + TV business.
 *          Volume+ & HOME switches STB user code, Volume+ & MENU switches TV user code.
 *          Trigger once until release, with slow-2 / fast-3 LED feedback.
 *          Factory reset and learning are mutually exclusive in task_keyscan.
 */

#include "combo_key.h"
#include "app_keyscan.h"      /* keyscan_is_key_pressed, KEYSCAN_INFO_CHECK, KEY_NUM */
#include "task_keyscan.h"     /* TASK_KEYSCAN_KEY4/5/6/9/14/15 */
#include "ble_gap.h"          /* ble_gap_get_peer, ble_gap_clear_bonding_info */
#include "ble_adv.h"          /* ble_adv_start_pairing */
#include "pm_state.h"         /* pm_state_transition, PM_STATE_PAIRING, PM_STATE_SLEEP */
#include "udp_ota.h"
#include "upg_session.h"      /* upg_session_busy — 键值下载会话期间不执行配对副作用 */
#include "userhard.h"         /* SETLED1, CLRLED1 */
#include "co_printf.h"
#include "ir_key_map.h"       /* ir_send_pair_code, ir_user_switch_stb/tv */

/*===========================================================================
 * 一、类型定义
 *===========================================================================*/

typedef enum {
    COMBO_IDLE = 0,
    COMBO_HOLDING,       /* 双键按住中，计时 3s                     */
    COMBO_TRIGGERED,     /* 已触发，执行后续动作（仅解配使用此状态） */
    COMBO_FLASHING,      /* 用户码切换后的指示灯反馈（慢闪2下/快闪3下） */
    COMBO_WAIT_RELEASE,
} combo_state_t;

/** @brief 组合键动作类型 */
typedef enum {
    COMBO_ACT_PAIR = 0,  /* STB Bluetooth pairing + Gehua RC6 request */
    COMBO_ACT_UNPAIR,    /* Clear STB bond + wait for disconnect */
    COMBO_ACT_USER_STB,  /* GY/T 407 §6.1.3 音量+ & 主页 -> 切机顶盒用户码 0x3A/0x3C */
    COMBO_ACT_USER_TV,   /* GY/T 407 §6.1.3 音量+ & 菜单 -> 切电视机用户码 0x51/0x53 */
} combo_action_t;

typedef struct {
    uint32_t       state_ts_ms;   /* 进入当前状态的时间戳            */
    uint32_t       release_ts_ms; /* 首次读到组合键不完整的时刻；TRIGGERED 态复用为 LED 闪烁时间戳 */
    uint8_t        key1;          /* 按键编号 1                      */
    uint8_t        key2;          /* 按键编号 2                      */
    uint8_t        action;        /* combo_action_t                  */
    uint8_t        state;         /* combo_state_t                   */
    uint8_t        flags;         /* bit0 release_pending, bit1 led_on */
} combo_key_def_t;

/* flags 位分配（不新增结构体成员，避免 RAM 增长）：
 *   bit0     release_pending
 *   bit1     led_on
 *   bit2..4  flash_done（已闪半周期次数 0~7）
 *   bit5     flash_fast（1=快闪 200ms×6，0=慢闪 500ms×4） */
#define COMBO_FLAG_RELEASE_PENDING 0x01u
#define COMBO_FLAG_LED_ON          0x02u
#define COMBO_FLAG_FAST            0x20u
#define COMBO_FLASH_DONE_SHIFT     2u
#define COMBO_FLASH_DONE_MASK      0x1Cu

/** @brief 用户码切换指示灯：默认码慢闪 2 下（4 个半周期） */
#define COMBO_USER_SLOW_FLASH_MS   500U
/** @brief 用户码切换指示灯：备用码快闪 3 下（6 个半周期） */
#define COMBO_USER_FAST_FLASH_MS   200U

/*===========================================================================
 * 二、静态变量
 *===========================================================================*/

/* 组合键注册表（编译时确定，不动态分配） */
#define COMBO_IDX_PAIR_STB   0   /* KEY14 + KEY3 */
#define COMBO_IDX_UNPAIR_STB 1   /* KEY9 + KEY3 */
#define COMBO_IDX_USER_STB   2   /* KEY12 + KEY8  音量+ & 主页 → 切机顶盒用户码 */
#define COMBO_IDX_USER_TV    3   /* KEY12 + KEY10 音量+ & 菜单 → 切电视机用户码 */
#define COMBO_COUNT         4

/* Automatic KEYSCAN uses 32ms scan interval plus 10ms debounce.
 * Do not treat one incomplete snapshot as an actual release. */
#define COMBO_RELEASE_CONFIRM_MS 100U

static combo_key_def_t g_combos[COMBO_COUNT];

/*===========================================================================
 * 三、内部辅助函数
 *===========================================================================*/

/** @brief 检测组合键的两个按键是否同时按下 */
static bool combo_both_keys_down(const combo_key_def_t *c, const uint32_t hw[2])
{
    uint8_t k;
    if (!keyscan_is_key_pressed(hw, c->key1) || !keyscan_is_key_pressed(hw, c->key2)) return false;
    for (k = 1u; k <= IR_KEY_COUNT; ++k)
        if (k != c->key1 && k != c->key2 && keyscan_is_key_pressed(hw, k)) return false;
    return true;
}

/*===========================================================================
 * 四、公共 API
 *===========================================================================*/

/** @brief 注册一组组合键（清零运行时状态） */
static void combo_register(uint8_t idx, uint8_t key1, uint8_t key2, combo_action_t action)
{
    g_combos[idx].key1          = key1;
    g_combos[idx].key2          = key2;
    g_combos[idx].action        = action;
    g_combos[idx].state         = COMBO_IDLE;
    g_combos[idx].state_ts_ms   = 0U;
    g_combos[idx].release_ts_ms = 0U;
    g_combos[idx].flags         = 0u;
}

void combo_key_init(void)
{
    /* One BLE target: these pairs replace the previous TV actions and
     * the left-key aliases. No user-code switching combo is registered. */
    combo_register(COMBO_IDX_PAIR_STB,   TASK_KEYSCAN_KEY14, TASK_KEYSCAN_KEY3, COMBO_ACT_PAIR);
    combo_register(COMBO_IDX_UNPAIR_STB, TASK_KEYSCAN_KEY9,  TASK_KEYSCAN_KEY3, COMBO_ACT_UNPAIR);
    /* 红外用户码切换（GY/T 407-2024 §6.1.3）：长按 3s，切换默认/备用用户码 */
    combo_register(COMBO_IDX_USER_STB,   TASK_KEYSCAN_KEY12, TASK_KEYSCAN_KEY8,  COMBO_ACT_USER_STB);
    combo_register(COMBO_IDX_USER_TV,    TASK_KEYSCAN_KEY12, TASK_KEYSCAN_KEY10, COMBO_ACT_USER_TV);
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
    return g_combos[COMBO_IDX_UNPAIR_STB].state == COMBO_TRIGGERED;
}

/*===========================================================================
 * 五、动作实现（内部）
 *===========================================================================*/

/**
 * @brief  机顶盒清绑定、配对广播并发 RC6 0xAD
 */
static void combo_do_pair(combo_key_def_t *c)
{
    co_printf("[COMBO] key%d+key%d pair triggered\r\n", c->key1, c->key2);

    /*
     * 清除旧绑定以确保设备可被发现，再设置配对广播活跃标志。
     * 注意：clear_bonding_info() 会重置 g_pairing_active，所以必须在它之后设置。
     */
    ble_gap_clear_bonding_info();
    g_pairing_active = true;
    pm_state_transition(PM_STATE_PAIRING);
    ble_adv_start_pairing();

    /* Start the timed RC6 envelope only AFTER the synchronous bonding Flash
     * write: that operation masks interrupts and would corrupt a live frame. */
    ir_send_pair_code_target(false);

    /* LED2 闪烁由 ble_gap_led_indicate() 自动处理 */
    c->state = COMBO_WAIT_RELEASE;
}

/**
 * @brief  机顶盒清绑定并进入等断连状态
 */
static void combo_do_unpair(combo_key_def_t *c, uint32_t now_ms)
{
    co_printf("[COMBO] key%d+key%d unpair triggered\r\n", c->key1, c->key2);

    c->state       = COMBO_TRIGGERED;
    c->state_ts_ms = now_ms;
    c->release_ts_ms = now_ms;
    c->flags |= COMBO_FLAG_LED_ON;
    SETLED1;

    /* 清绑定（内部先断连再擦 Flash），断连异步完成 */
    ble_gap_clear_bonding_info();
}

/**
 * @brief  切换红外用户码并给出指示灯反馈（GY/T 407-2024 §6.1.3）
 * @note   默认码慢闪 2 下（500ms 半周期×4），备用码快闪 3 下（200ms×6）。
 *         闪烁期间组合键保持非空闲，正常按键处理被跳过，LED 不会被抢占。
 */
static void combo_do_user_switch(combo_key_def_t *c, uint32_t now_ms)
{
    bool is_default;

    if (c->action == COMBO_ACT_USER_STB) {
        is_default = ir_user_switch_stb();
    } else {
        is_default = ir_user_switch_tv();
    }

    co_printf("[COMBO] key%d+key%d %s user code -> %s\r\n", c->key1, c->key2,
              (c->action == COMBO_ACT_USER_STB) ? "STB" : "TV",
              is_default ? "default" : "backup");

    c->flags = 0u;
    if (!is_default) c->flags |= COMBO_FLAG_FAST;
    c->release_ts_ms = now_ms;      /* 闪烁半周期计时 */
    c->state = COMBO_FLASHING;
    SETLED1; SETLED2;               /* 先点亮，之后按半周期翻转 */
    c->flags |= COMBO_FLAG_LED_ON;
}

void combo_key_process(uint32_t now_ms, const uint32_t hw_snapshot[5])
{
    uint8_t i;

    for (i = 0; i < COMBO_COUNT; i++) {
        combo_key_def_t *c = &g_combos[i];

        switch (c->state) {

        case COMBO_IDLE:
            if (combo_both_keys_down(c, hw_snapshot)) {
                c->state = COMBO_HOLDING;
                c->flags &= (uint8_t)~COMBO_FLAG_RELEASE_PENDING;
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
            if (!combo_both_keys_down(c, hw_snapshot)) {
                /* Confirm missing keys over several scan intervals. Never
                 * execute an action while the current snapshot is invalid. */
                if (!(c->flags & COMBO_FLAG_RELEASE_PENDING)) {
                    c->flags |= COMBO_FLAG_RELEASE_PENDING;
                    c->release_ts_ms = now_ms;
                }
                if (now_ms - c->release_ts_ms >= COMBO_RELEASE_CONFIRM_MS) {
                    c->state = COMBO_IDLE;
                    c->flags &= (uint8_t)~COMBO_FLAG_RELEASE_PENDING;
                    co_printf("[COMBO] key%d+key%d cancel raw=%08lX/%08lX\r\n",
                              c->key1, c->key2,
                              (unsigned long)hw_snapshot[0], (unsigned long)hw_snapshot[1]);
                }
                break;
            }
            /* A gap already lasting the release threshold cannot be joined
             * into the old hold even if this is the first recovered sample. */
            if ((c->flags & COMBO_FLAG_RELEASE_PENDING) &&
                now_ms - c->release_ts_ms >= COMBO_RELEASE_CONFIRM_MS) {
                c->state_ts_ms = now_ms;
            }
            c->flags &= (uint8_t)~COMBO_FLAG_RELEASE_PENDING;
            if ((now_ms - c->state_ts_ms) >= COMBO_KEY_HOLD_MS) {
                /* OTA/键值下载会话期间保留组合键检测，但不执行配对/解配副作用 */
                if (udp_ota_session_busy() != 0u || upg_session_busy()) {
                    co_printf("[COMBO] ignored: session busy\r\n");
                    c->state = COMBO_WAIT_RELEASE;
                    break;
                }
                /* 3s 到 → 按动作类型分发 */
                switch (c->action) {
                case COMBO_ACT_PAIR:
                    combo_do_pair(c);
                    break;
                case COMBO_ACT_UNPAIR:
                    combo_do_unpair(c, now_ms);
                    break;
                case COMBO_ACT_USER_STB:
                case COMBO_ACT_USER_TV:
                    combo_do_user_switch(c, now_ms);
                    break;
                default:
                    c->state = COMBO_IDLE;
                    break;
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /*  FLASHING：用户码切换后的指示灯反馈                          */
        /* ---------------------------------------------------------- */
        case COMBO_FLASHING:
        {
            uint32_t period = (c->flags & COMBO_FLAG_FAST)
                              ? COMBO_USER_FAST_FLASH_MS : COMBO_USER_SLOW_FLASH_MS;
            uint8_t  total  = (c->flags & COMBO_FLAG_FAST) ? 6u : 4u;
            uint8_t  done   = (uint8_t)((c->flags & COMBO_FLASH_DONE_MASK) >> COMBO_FLASH_DONE_SHIFT);

            if ((now_ms - c->release_ts_ms) >= period) {
                c->release_ts_ms = now_ms;
                c->flags ^= COMBO_FLAG_LED_ON;
                if (c->flags & COMBO_FLAG_LED_ON) { SETLED1; SETLED2; }
                else { CLRLED1; CLRLED2; }

                done++;
                c->flags = (uint8_t)((c->flags & (uint8_t)~COMBO_FLASH_DONE_MASK) |
                                     (uint8_t)(done << COMBO_FLASH_DONE_SHIFT));
                if (done >= total) {
                    CLRLED1; CLRLED2;
                    c->flags &= (uint8_t)~COMBO_FLAG_LED_ON;
                    c->state = COMBO_WAIT_RELEASE;
                }
            }
            break;
        }

        /* ---------------------------------------------------------- */
        /*  TRIGGERED：仅解配使用，等断连或超时，LED1 闪烁              */
        /* ---------------------------------------------------------- */
        case COMBO_TRIGGERED:
        {
            /* LED1 200ms 周期闪烁 */
            if ((now_ms - c->release_ts_ms) >= COMBO_LED_FLASH_MS) {
                c->release_ts_ms = now_ms;
                c->flags ^= COMBO_FLAG_LED_ON;
                if (c->flags & COMBO_FLAG_LED_ON) SETLED1; else CLRLED1;
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
                c->state = COMBO_WAIT_RELEASE;
                co_printf("[COMBO] Unpair done (elapsed=%lums). Enter SLEEP.\r\n",
                          (unsigned long)elapsed);
                pm_state_transition(PM_STATE_SLEEP);
            }
            break;
        }

        case COMBO_WAIT_RELEASE:
            if (!keyscan_is_key_pressed(hw_snapshot, c->key1) &&
                !keyscan_is_key_pressed(hw_snapshot, c->key2)) c->state = COMBO_IDLE;
            break;

        default:
            c->state = COMBO_IDLE;
            break;
        }
    }
}
