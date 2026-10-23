/**
 * @file    irc.c
 * @brief   IRC 红外控制器驱动 — NEC 协议发送实现
 * @note    CODE_FIFO 消息格式（手册 §20.3 表 20-1/20-2）：
 *            Digital [20]=1, [19:16]=len-1, [15:0]=bits（IRC 自动套用 MARK/SPACE）
 *            Paint   [20]=0, [14]=Mark/Space, [13:0]=时长（载波周期数）
 *
 *          NEC 帧：Paint(9ms) → Paint(4.5ms) → Digital(16bit) → Digital(16bit) → Paint(600μs)
 *          重复码：Paint(9ms) → Paint(2.25ms) → Paint(600μs)
 *
 *          时序基准：AHB = HSI = 64MHz。
 */

#include "irc.h"
#include "ir_rc6.h"
#include "br1601x.h"
#include "rcc.h"
#include "gpio.h"
#include "misc.h"
#include "gmem.h"

/*===========================================================================
 * 一、CODE_FIFO 消息编码宏
 *===========================================================================*/

/** @brief Digital message：位序列，IRC 自动套用 LOGIC_*_TIME */
#define DIGITAL(len, data)  ((1u<<20)|(((len)-1u)<<16)|((uint32_t)(data)&0xFFFFu))
/** @brief Paint Mark message：原始载波导通时长 */
#define PAINT_MARK(t)       ((1u<<14)|((uint32_t)(t)&0x3FFFu))
/** @brief Paint Space message：原始载波关断时长 */
#define PAINT_SPACE(t)      ((uint32_t)(t)&0x3FFFu)

/*===========================================================================
 * 二、内部静态变量
 *===========================================================================*/

/** @brief IRC 初始化完成标志 */
static bool    s_ok;
static uint32_t s_start_ms;
static bool s_transmitting;

/* IRQ delivery stays disabled; reap completion before submitting a new frame.
 * Manual 20.5.6 requires software to clear TX_START at completion.
 */
static void irc_poll_complete(void)
{
    if (s_ok && (IRC->STATUS & IR_IRQ_FLAG)) {
        IRC->CTRL &= ~IR_CTRL_TX_START_Msk;
        IRC->STATUS &= ~IR_IRQ_FLAG;
        s_transmitting = false;
    }
    if (s_ok && s_transmitting && app_now_ms() - s_start_ms > 120u) {
        /* A normal NEC frame is <70ms. Bound recovery without blocking BLE. */
        irc_stop();
    }
}

/*===========================================================================
 * 三、公共 API
 *===========================================================================*/

/**
 * @brief  初始化 IRC 外设
 * @note   开时钟 → 复位 → 配载波/位时序 → 使能，发送不使用中断。
 */
void irc_init(void)
{
    GPIO_InitType gpio;

    /* Called at cold boot and after idle-only standby/sleep. The software
     * flag survives low power, but PB4 has been parked as GPIO output and
     * IRC clocks/registers may need restoration. Always restore hardware. */

    /* Manual table 5-12: PB4 AF2 is shared by TIM3_CH1 and IRC_TX.
     * IR_ENABLE selects IRC within AF2; it does not configure the GPIO mux.
     * The board's NPN emitter driver is active high.
     */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);
    GPIO_ResetBits(GPIOB, GPIO_PIN_4);
    GPIO_InitStruct(&gpio);
    gpio.Pin = GPIO_PIN_4;
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull = GPIO_NO_PULL;
    gpio.GPIO_Alternate = GPIO_AF2;
    GPIO_InitPeripheral(GPIOB, &gpio);

    RCC->AHBPCLKEN |= RCC_AHBPCLKEN_IRCEN;
    RCC->AHBPRST   |= RCC_AHBRST_IRCRST;
    RCC->AHBPRST   &= ~RCC_AHBRST_IRCRST;

    IRC->FREQ_CARRIER_ON  = IRC_CARR_ON;
    IRC->FREQ_CARRIER_OFF = IRC_CARR_OFF;
    IRC->LOGIC_ZERO_TIME  = (IRC_LO_MARK << 8)  | IRC_LO_SPACE;
    IRC->LOGIC_ONE_TIME   = (IRC_HI_MARK << 8)  | IRC_HI_SPACE;
    /* cs.lib copies only 31 external vectors (IRQ0..30) into its RAM
     * dispatch table. IRQ31 (IRC) is omitted and would branch to NULL.
     * A complete frame fits in CODE_FIFO; hardware transmits it without
     * interrupt service, and callers already poll IR_STATUS_BUSY.
     */
    NVIC_DisableIRQ(IRC_IRQn);
    NVIC_ClearPendingIRQ(IRC_IRQn);
    IRC->CTRL = IR_CTRL_ENABLE_Msk;

    s_transmitting = false;
    s_ok = true;
}

/**
 * @brief  发送 NEC 红外帧
 * @param  user_code  NEC 用户码
 * @param  cmd_code   NEC 数据码
 * @return true 发送已启动，false IRC 忙或未初始化
 * @note   组装 5 条消息写入 CODE_FIFO 后置 TX_START。
 */
bool irc_send(uint8_t user_code, uint8_t cmd_code)
{
    irc_poll_complete();
    if (!s_ok || ir_rc6_is_busy() || (IRC->STATUS & IR_STATUS_BUSY)) return false;

    uint16_t u = (uint16_t)user_code | ((uint16_t)(uint8_t)~user_code << 8);
    uint16_t d = (uint16_t)cmd_code  | ((uint16_t)(uint8_t)~cmd_code  << 8);

    IRC->CODE_FIFO = PAINT_MARK(IRC_T9MS);
    IRC->CODE_FIFO = PAINT_SPACE(IRC_T4P5MS);
    IRC->CODE_FIFO = DIGITAL(16u, u);
    IRC->CODE_FIFO = DIGITAL(16u, d);
    IRC->CODE_FIFO = PAINT_MARK(IRC_T600US);
    s_start_ms = app_now_ms();
    s_transmitting = true;
    IRC->CTRL |= IR_CTRL_TX_START_Msk;
    return true;
}

/**
 * @brief  发送 NEC 重复码
 * @note   波形 = 9ms + 2.25ms + 600μs，写入 CODE_FIFO 后置 TX_START。
 */
void irc_send_repeat(void)
{
    irc_poll_complete();
    if (!s_ok || ir_rc6_is_busy() || (IRC->STATUS & IR_STATUS_BUSY)) return;

    IRC->CODE_FIFO = PAINT_MARK(IRC_T9MS);
    IRC->CODE_FIFO = PAINT_SPACE(IRC_T2P25MS);
    IRC->CODE_FIFO = PAINT_MARK(IRC_T600US);
    s_start_ms = app_now_ms();
    s_transmitting = true;
    IRC->CTRL |= IR_CTRL_TX_START_Msk;
}

/**
 * @brief  停止 IRC 发送
 * @note   清 TX_START，硬件自动清空 FIFO。
 */
void irc_stop(void)
{
    if (s_ok) {
        IRC->CTRL &= ~IR_CTRL_TX_START_Msk;
        IRC->CTRL |= IR_CTRL_CODE_FIFO_RESET_Msk | IR_CTRL_REP_FIFO_RESET_Msk;
        IRC->CTRL &= ~(IR_CTRL_CODE_FIFO_RESET_Msk | IR_CTRL_REP_FIFO_RESET_Msk);
        IRC->STATUS &= ~IR_IRQ_FLAG;
        s_transmitting = false;
    }
}

/**
 * @brief  查询 IRC 是否正在发送
 * @return true = 发送中，false = 空闲
 */
bool irc_is_busy(void)
{
    irc_poll_complete();
    return s_ok && (IRC->STATUS & IR_STATUS_BUSY);
}

/*===========================================================================
 * 四、中断处理
 *===========================================================================*/

/**
 * @brief  IRC 中断服务函数
 * @note   传输完成时硬件置 IR_IRQ_FLG，软件清 0 即可。
 */
void IRC_IRQHandler(void)
{
    if (IRC->STATUS & IR_IRQ_FLAG)
        IRC->STATUS &= ~IR_IRQ_FLAG;
}
