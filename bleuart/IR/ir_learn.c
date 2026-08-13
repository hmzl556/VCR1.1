/**
 * @file    ir_learn.c
 * @brief   红外学习模块实现 — TIM3 输入捕获解码 NEC 码
 * @note    硬件方案：
 *            PB5 = TIM3_CH2（AF2），双边沿捕获（TIM_IC_POLARITY_BOTHEDGE）。
 *            TIM3 时钟 64MHz 分频 1MHz（1 tick = 1μs），16 位计数器
 *            （周期约 65.5ms），回绕检测累加 32 位绝对微秒时间。
 *
 *          捕获流程：
 *            中断只记录脉宽序列；收满 66 个脉宽（1 引导低 + 1 引导高 +
 *            32 位 × 2）后置 s_frame_ready；主循环 ir_learn_process()
 *            完成解码与结果上报。15 秒超时（GY/T 407-2024 §6.2）。
 */

#include "ir_learn.h"
#include "br1601x.h"
#include "rcc.h"
#include "gpio.h"
#include "tim.h"
#include "misc.h"
#include "gmem.h"       /* app_now_ms() */
#include "co_printf.h"

/*===========================================================================
 * 一、时序与容量参数（单位：μs，1MHz 计数）
 *===========================================================================*/

/** @brief 引导码低电平 9ms 容差范围 */
#define IR_LEARN_LEAD_LOW_MIN     7000u
#define IR_LEARN_LEAD_LOW_MAX     11000u

/** @brief 引导码高电平 4.5ms 容差范围 */
#define IR_LEARN_LEAD_HIGH_MIN    3500u
#define IR_LEARN_LEAD_HIGH_MAX    5500u

/** @brief 逻辑位判定阈值：高电平 >1100μs 判 1（560 vs 1680 中点） */
#define IR_LEARN_BIT_THRESHOLD    1100u

/** @brief 脉宽缓冲容量 */
#define IR_LEARN_MAX_WIDTHS       70u

/** @brief 完整 NEC 帧脉宽数：引导低+引导高+32 位×2 = 66 */
#define IR_LEARN_FRAME_WIDTHS     66u

/** @brief 学习超时 15 秒 */
#define IR_LEARN_TIMEOUT_MS       15000u

/*===========================================================================
 * 二、内部状态
 *===========================================================================*/

/** @brief 学习状态机：空闲 / 学习中 */
typedef enum
{
    IR_LEARN_ST_IDLE = 0,
    IR_LEARN_ST_LEARNING,
} ir_learn_state_t;

/** @brief 当前学习状态 */
static ir_learn_state_t s_state = IR_LEARN_ST_IDLE;

/** @brief 学习结果回调 */
static ir_learn_cb_t   s_cb    = NULL;

/** @brief 脉宽缓冲（中断写、主循环读） */
static volatile uint32_t s_widths[IR_LEARN_MAX_WIDTHS];

/** @brief 已捕获脉宽个数 */
static volatile uint8_t  s_width_count;

/** @brief 收满一帧标志（中断置位、主循环消费） */
static volatile uint8_t  s_frame_ready;

/** @brief 上次捕获计数值（回绕检测用） */
static volatile uint16_t s_last_cap;

/** @brief 32 位绝对微秒时间的溢出累加部分 */
static volatile uint32_t s_overflow_us;

/** @brief 上次边沿的绝对微秒时间 */
static volatile uint32_t s_last_us;

/** @brief 学习开始时间戳（毫秒，超时判断用） */
static uint32_t s_start_ms;

/*===========================================================================
 * 三、内部辅助函数
 *===========================================================================*/

/**
 * @brief  清空捕获缓冲与计时状态
 */
static void ir_learn_reset_capture(void)
{
    uint8_t i;

    for (i = 0u; i < IR_LEARN_MAX_WIDTHS; i++) {
        s_widths[i] = 0u;
    }
    s_width_count  = 0u;
    s_frame_ready  = 0u;
    s_last_cap     = 0u;
    s_overflow_us  = 0u;
    s_last_us      = 0u;
}

/**
 * @brief  从脉宽序列解码 NEC 帧
 * @param  user 输出 NEC 用户码
 * @param  cmd  输出 NEC 数据码
 * @return true = 解码成功，false = 引导/反码校验失败
 * @note   脉宽序列：w[0]=引导低，w[1]=引导高，w[2+2i]=位 i 低，w[3+2i]=位 i 高。
 *         NEC 32 位 = 用户码 + 反码 + 数据码 + 反码，LSB first。
 */
static bool ir_learn_decode(uint8_t *user, uint8_t *cmd)
{
    uint32_t w0 = s_widths[0];
    uint32_t w1 = s_widths[1];
    uint32_t data = 0u;
    uint8_t  u, ui, d, di;
    uint8_t  i;

    if (s_width_count < IR_LEARN_FRAME_WIDTHS) {
        return false;
    }
    if (w0 < IR_LEARN_LEAD_LOW_MIN || w0 > IR_LEARN_LEAD_LOW_MAX) {
        return false;
    }
    if (w1 < IR_LEARN_LEAD_HIGH_MIN || w1 > IR_LEARN_LEAD_HIGH_MAX) {
        return false;
    }

    for (i = 0u; i < 32u; i++) {
        if (s_widths[3u + 2u * i] > IR_LEARN_BIT_THRESHOLD) {
            data |= (1u << i);
        }
    }

    u  = (uint8_t)(data & 0xFFu);
    ui = (uint8_t)((data >> 8)  & 0xFFu);
    d  = (uint8_t)((data >> 16) & 0xFFu);
    di = (uint8_t)((data >> 24) & 0xFFu);

    if ((uint8_t)(u ^ ui) != 0xFFu) {
        return false;
    }
    if ((uint8_t)(d ^ di) != 0xFFu) {
        return false;
    }

    *user = u;
    *cmd  = d;
    return true;
}

/*===========================================================================
 * 四、公共 API
 *===========================================================================*/

/**
 * @brief  初始化学习模块（PB5 → TIM3_CH2 + NVIC）
 * @param  cb 学习结果回调
 */
void ir_learn_init(ir_learn_cb_t cb)
{
    GPIO_InitType        gpio;
    TIM_TimeBaseInitType tb;
    TIM_ICInitType       ic;
    NVIC_InitType        nv;

    s_cb    = cb;
    s_state = IR_LEARN_ST_IDLE;
    ir_learn_reset_capture();

    /* 1. 时钟：TIM3(APB1) + GPIOB/AFIO(APB2) */
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);

    /* 2. PB5 = TIM3_CH2（AF2） */
    GPIO_InitStruct(&gpio);
    gpio.Pin           = GPIO_PIN_5;
    gpio.GPIO_Mode     = GPIO_MODE_AF_PP;
    gpio.GPIO_Current  = GPIO_DC_LOW;
    gpio.GPIO_Alternate = GPIO_AF2_TIM3;
    GPIO_InitPeripheral(GPIOB, &gpio);

    /* 3. TIM3 时基 1MHz（64MHz / 64） */
    TIM_InitTimBaseStruct(&tb);
    tb.Prescaler = 63u;
    tb.CntMode   = TIM_CNT_MODE_UP;
    tb.Period    = 0xFFFFu;
    tb.ClkDiv    = TIM_CLK_DIV1;
    tb.RepetCnt  = 0u;
    TIM_InitTimeBase(TIM3, &tb);

    /* 4. CH2 双边沿捕获 */
    TIM_InitIcStruct(&ic);
    ic.Channel     = TIM_CH_2;
    ic.IcPolarity  = TIM_IC_POLARITY_BOTHEDGE;
    ic.IcSelection = TIM_IC_SELECTION_DIRECTTI;
    ic.IcPrescaler = TIM_IC_PSC_DIV1;
    ic.IcFilter    = 0u;
    TIM_ICInit(TIM3, &ic);

    /* 5. 使能 CH2 捕获中断 */
    TIM_ConfigInt(TIM3, TIM_INT_CC2, ENABLE);

    /* 6. NVIC */
    nv.NVIC_IRQChannel           = TIM3_IRQn;
    nv.NVIC_IRQChannelPriority   = 1;
    nv.NVIC_IRQChannelCmd        = ENABLE;
    NVIC_Init(&nv);

    /* 7. 启动计数器 */
    TIM_Enable(TIM3, ENABLE);
}

/**
 * @brief  开始学习（进入学习态，等待红外帧）
 * @return true = 已进入学习，false = 已在学习中
 */
bool ir_learn_start(void)
{
    if (s_state == IR_LEARN_ST_LEARNING) {
        return false;
    }

    ir_learn_reset_capture();
    s_start_ms = app_now_ms();
    s_state    = IR_LEARN_ST_LEARNING;
    co_printf("[IR   ] learn started\r\n");
    return true;
}

/**
 * @brief  停止学习（主动取消）
 */
void ir_learn_stop(void)
{
    if (s_state == IR_LEARN_ST_IDLE) {
        return;
    }
    s_state       = IR_LEARN_ST_IDLE;
    s_frame_ready = 0u;
    if (s_cb != NULL) {
        s_cb(IR_LEARN_EVT_CANCELED, 0u, 0u);
    }
}

/**
 * @brief  查询是否处于学习态
 * @return true = 学习中，false = 空闲
 */
bool ir_learn_is_active(void)
{
    return (s_state == IR_LEARN_ST_LEARNING);
}

/**
 * @brief  学习主循环轮询（须周期性调用）
 * @note   负责 15 秒超时检测与解码结果上报。
 */
void ir_learn_process(void)
{
    if (s_state != IR_LEARN_ST_LEARNING) {
        return;
    }

    /* 收满一帧 → 解码 */
    if (s_frame_ready != 0u) {
        uint8_t user = 0u;
        uint8_t cmd  = 0u;

        s_frame_ready = 0u;
        s_state       = IR_LEARN_ST_IDLE;

        if (ir_learn_decode(&user, &cmd)) {
            co_printf("[IR   ] learn done user=0x%02X cmd=0x%02X\r\n",
                      (unsigned)user, (unsigned)cmd);
            if (s_cb != NULL) {
                s_cb(IR_LEARN_EVT_DONE, user, cmd);
            }
        } else {
            co_printf("[IR   ] learn decode fail\r\n");
            if (s_cb != NULL) {
                s_cb(IR_LEARN_EVT_FAILED, 0u, 0u);
            }
        }
        return;
    }

    /* 15 秒超时 */
    if ((app_now_ms() - s_start_ms) >= IR_LEARN_TIMEOUT_MS) {
        s_state = IR_LEARN_ST_IDLE;
        co_printf("[IR   ] learn timeout\r\n");
        if (s_cb != NULL) {
            s_cb(IR_LEARN_EVT_TIMEOUT, 0u, 0u);
        }
    }
}

/*===========================================================================
 * 五、中断处理
 *===========================================================================*/

/**
 * @brief  TIM3 全局中断：CH2 双边沿捕获记录脉宽
 * @note   仅在学习态记录；首脉宽须为引导低否则视为噪声丢弃。
 */
void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM3, TIM_INT_CC2) == SET) {
        uint16_t cap;
        uint32_t cur_us;
        uint32_t width;

        TIM_ClrIntPendingBit(TIM3, TIM_INT_CC2);
        cap = TIM_GetCap2(TIM3);

        /* 回绕检测：两次边沿间隔 < 65.5ms，最多回绕一次 */
        if (cap < s_last_cap) {
            s_overflow_us += 0x10000u;
        }
        s_last_cap = cap;
        cur_us     = s_overflow_us + (uint32_t)cap;
        width      = cur_us - s_last_us;
        s_last_us  = cur_us;

        if (s_state != IR_LEARN_ST_LEARNING) {
            return;
        }
        if (s_frame_ready != 0u || s_width_count >= IR_LEARN_MAX_WIDTHS) {
            return;
        }

        if (s_width_count == 0u) {
            /* 首脉宽必须是引导低，否则为噪声丢弃 */
            if (width >= IR_LEARN_LEAD_LOW_MIN && width <= IR_LEARN_LEAD_LOW_MAX) {
                s_widths[s_width_count++] = width;
            }
        } else {
            s_widths[s_width_count++] = width;
            if (s_width_count >= IR_LEARN_FRAME_WIDTHS) {
                s_frame_ready = 1u;
            }
        }
    }
}
