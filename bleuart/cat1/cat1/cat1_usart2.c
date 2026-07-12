/**
 * @file cat1_usart2.c
 * @brief USART2 PB4/PB5 (AF3)：RX 中断环形缓冲，TX 阻塞发送，不使用 DMA。
 */
#include "cat1_usart2.h"
#include "usart.h"
#include "misc.h"
#include "core_cmFunc.h"
#include "co_printf.h"
#include <string.h>

#define USART2_CFG_GPIO_PORT     GPIOB
#define USART2_CFG_GPIO_CLK      RCC_APB2_PERIPH_GPIOB
#define USART2_CFG_TX_PIN        GPIO_PIN_4
#define USART2_CFG_RX_PIN        GPIO_PIN_5
#define USART2_CFG_TX_AF         GPIO_AF3_USART2
#define USART2_CFG_RX_AF         GPIO_AF3_USART2

#define USART2_CAT1_BAUDRATE     115200u

volatile uint8_t USART2_CAT1_RX_RING[USART2_CAT1_RX_RING_CAP];
volatile uint16_t USART2_CAT1_RX_HEAD;
volatile uint16_t USART2_CAT1_RX_TAIL;
static volatile uint32_t s_usart2_rx_drop;

/**
 * @brief 使能 USART2 时钟与 PB4/PB5 复用 AF3。
 */
static void usart2_rcc_gpio_init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAPB2PeriphClk(USART2_CFG_GPIO_CLK | RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);

    GPIO_InitStruct(&gpio);
    gpio.Pin            = USART2_CFG_TX_PIN;
    gpio.GPIO_Mode      = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull      = GPIO_NO_PULL;
    gpio.GPIO_Alternate = USART2_CFG_TX_AF;
    GPIO_InitPeripheral(USART2_CFG_GPIO_PORT, &gpio);

    gpio.Pin            = USART2_CFG_RX_PIN;
    gpio.GPIO_Pull      = GPIO_PULL_UP;
    gpio.GPIO_Alternate = USART2_CFG_RX_AF;
    GPIO_InitPeripheral(USART2_CFG_GPIO_PORT, &gpio);
}

/**
 * @brief 配置 USART2 参数并使能 RX 非空中断。
 */
static void usart2_periph_init(void)
{
    USART_InitType us;

    USART_DeInit(USART2);
    USART_StructInit(&us);
    us.BaudRate            = USART2_CAT1_BAUDRATE;
    us.WordLength          = USART_WL_8B;
    us.StopBits            = USART_STPB_1;
    us.Parity              = USART_PE_NO;
    us.HardwareFlowControl = USART_HFCTRL_NONE;
    us.Mode                = USART_MODE_RX | USART_MODE_TX;
    USART_Init(USART2, &us);

    USART_EnableDMA(USART2, USART_DMAREQ_RX | USART_DMAREQ_TX, DISABLE);

    USART_Enable(USART2, ENABLE);
    USART_ConfigInt(USART2, USART_INT_RXDNE, ENABLE);

    {
        NVIC_InitType nvic;
        nvic.NVIC_IRQChannel                   = USART2_IRQn;
        nvic.NVIC_IRQChannelPriority           = 1;
        nvic.NVIC_IRQChannelCmd                = ENABLE;
        NVIC_Init(&nvic);
    }
}

void USART2_CAT1_RxFlush(void)
{
    __disable_irq();
    USART2_CAT1_RX_HEAD = 0U;
    USART2_CAT1_RX_TAIL = 0U;
    __enable_irq();
    while (USART_GetFlagStatus(USART2, USART_FLAG_RXDNE) != RESET)
    {
        (void)USART_ReceiveData(USART2);
    }
}

uint8_t USART2_CAT1_RxPop(uint8_t *out_byte)
{
    uint8_t b;

    if (out_byte == NULL)
    {
        return 0U;
    }
    __disable_irq();
    if (USART2_CAT1_RX_HEAD == USART2_CAT1_RX_TAIL)
    {
        __enable_irq();
        return 0U;
    }
    b = USART2_CAT1_RX_RING[USART2_CAT1_RX_TAIL];
    USART2_CAT1_RX_TAIL = (uint16_t)((USART2_CAT1_RX_TAIL + 1U) % USART2_CAT1_RX_RING_CAP);
    __enable_irq();
    *out_byte = b;
    return 1U;
}

void USART2_CAT1_Init(void)
{
    USART2_CAT1_RxFlush();
    usart2_rcc_gpio_init();
    usart2_periph_init();
}

/**
 * @brief USART2 RX 中断：字节写入环形缓冲。
 */
void USART2_CAT1_IRQHandler(void)
{
    if (USART_GetIntStatus(USART2, USART_INT_RXDNE) != RESET)
    {
        uint8_t b = (uint8_t)USART_ReceiveData(USART2);
        uint16_t next = (uint16_t)((USART2_CAT1_RX_HEAD + 1U) % USART2_CAT1_RX_RING_CAP);

        if (next != USART2_CAT1_RX_TAIL)
        {
            USART2_CAT1_RX_RING[USART2_CAT1_RX_HEAD] = b;
            USART2_CAT1_RX_HEAD = next;
        }
        else
        {
            s_usart2_rx_drop++;
        }
    }
}

void USART2_IRQHandler(void)
{
    USART2_CAT1_IRQHandler();
}

/**
 * @brief 轮询发送；等待 TX 时空转并 pump，避免 OTA 期间丢 +SKTRECV。
 * @return 0 成功；-1 TX 超时
 */
static int usart2_poll_send_pump(const uint8_t *data, uint16_t len, void (*pump_fn)(void))
{
    uint16_t i;
    uint32_t spin;

    if (data == NULL || len == 0U)
    {
        return 0;
    }
    for (i = 0U; i < len; i++)
    {
        spin = 0U;
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXDE) == RESET)
        {
            if (pump_fn != NULL && (spin & 0x3Fu) == 0U)
            {
                pump_fn();
            }
            if (++spin >= CAT1_UART_TX_SPIN_MAX)
            {
                co_printf("[CAT1] USART2 TX timeout\r\n");
                return -1;
            }
        }
        USART_SendData(USART2, data[i]);
    }

    spin = 0U;
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXC) == RESET)
    {
        if (pump_fn != NULL && (spin & 0x3Fu) == 0U)
        {
            pump_fn();
        }
        if (++spin >= CAT1_UART_TX_SPIN_MAX)
        {
            co_printf("[CAT1] USART2 TXC timeout\r\n");
            return -1;
        }
    }
    USART_ClrFlag(USART2, USART_FLAG_TXC);
    return 0;
}

void USART2_CAT1_PollSend(const uint8_t *data, uint16_t len)
{
    (void)usart2_poll_send_pump(data, len, NULL);
}

uint32_t USART2_CAT1_RxDropCount(void)
{
    return s_usart2_rx_drop;
}

int usart2_at_send_pump(char *line, unsigned line_cap, void (*pump_fn)(void))
{
    size_t n;
    size_t i;
    const size_t chunk = 32u;

    if (line == NULL || line_cap < 3u)
    {
        return -1;
    }
    n = strlen(line);
    while (n > 0U && (line[n - 1U] == '\n' || line[n - 1U] == '\r'))
    {
        n--;
    }
    if (n + 2u > (size_t)line_cap)
    {
        n = (size_t)line_cap - 2u;
    }
    line[n++] = '\r';
    line[n++] = '\n';

    for (i = 0U; i < n; i += chunk)
    {
        size_t left = n - i;
        size_t send_len = (left > chunk) ? chunk : left;

        if (usart2_poll_send_pump((const uint8_t *)(line + i), (uint16_t)send_len, pump_fn) != 0)
        {
            return -1;
        }
    }
    line[n - 2u] = '\0';
    return 0;
}

void usart2_at_send(char *line, unsigned line_cap)
{
    (void)usart2_at_send_pump(line, line_cap, NULL);
}
