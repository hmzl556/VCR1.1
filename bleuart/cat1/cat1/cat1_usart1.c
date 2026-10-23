/**
 * @file cat1_usart1.c
 * @brief USART1 PA4/PA5 (AF3)：RX 用 DMA_CH2 循环写入环形缓冲，TX 阻塞发送。
 *
 * 根治 ECSORF 长 hex 行在 IRQ+软件环下偶发丢 1～2 字节：硬件 DMA 收字节，
 * 任务侧只推进读指针。ADC 语音占用 DMA_CH1，本模块固定 DMA_CH2。
 */
#include "cat1_usart1.h"
#include "usart.h"
#include "dma.h"
#include "rcc.h"
#include "misc.h"
#include "core_cmFunc.h"
#include "co_printf.h"
#include <string.h>

#define USART1_CFG_GPIO_PORT     GPIOA
#define USART1_CFG_GPIO_CLK      RCC_APB2_PERIPH_GPIOA
#define USART1_CFG_TX_PIN        GPIO_PIN_4
#define USART1_CFG_RX_PIN        GPIO_PIN_5
#define USART1_CFG_TX_AF         GPIO_AF3_USART1
#define USART1_CFG_RX_AF         GPIO_AF3_USART1

#define USART1_CAT1_BAUDRATE     115200u

/** ADC 语音用 CH1；USART1 RX 固定 CH2，避免互相 DeInit */
#define USART1_RX_DMA_CH         DMA_CH2

volatile uint8_t USART1_CAT1_RX_RING[USART1_CAT1_RX_RING_CAP];
volatile uint16_t USART1_CAT1_RX_HEAD;
volatile uint16_t USART1_CAT1_RX_TAIL;
static volatile uint32_t s_usart1_rx_drop;
/** 上次 sync 时的 DMA 写指针，用于检测环被覆盖 */
static uint16_t s_dma_last_head;
static uint8_t  s_rx_dma_ready;
static uint32_t s_uart_baud;

/**
 * @brief 使能 USART1 时钟与 PA4/PA5 复用 AF3。
 */
static void usart1_rcc_gpio_init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAPB2PeriphClk(USART1_CFG_GPIO_CLK | RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_USART1, ENABLE);
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);

    GPIO_InitStruct(&gpio);
    gpio.Pin            = USART1_CFG_TX_PIN;
    gpio.GPIO_Mode      = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull      = GPIO_NO_PULL;
    gpio.GPIO_Alternate = USART1_CFG_TX_AF;
    GPIO_InitPeripheral(USART1_CFG_GPIO_PORT, &gpio);

    gpio.Pin            = USART1_CFG_RX_PIN;
    gpio.GPIO_Pull      = GPIO_PULL_UP;
    gpio.GPIO_Alternate = USART1_CFG_RX_AF;
    GPIO_InitPeripheral(USART1_CFG_GPIO_PORT, &gpio);
}

/** DMA 写指针：已写入位置 = CAP - 剩余计数 */
static uint16_t usart1_dma_write_pos(void)
{
    uint16_t rem = DMA_GetCurrDataCounter(USART1_RX_DMA_CH);

    if (rem > USART1_CAT1_RX_RING_CAP) {
        rem = USART1_CAT1_RX_RING_CAP;
    }
    return (uint16_t)(USART1_CAT1_RX_RING_CAP - rem);
}

/**
 * @brief 从 DMA NDTR 刷新 HEAD；若本段写入超过当时空闲空间则记 drop 并丢弃未读。
 */
void USART1_CAT1_RxSync(void)
{
    uint16_t head;
    uint16_t last;
    uint16_t advanced;
    uint16_t used;
    uint16_t free_space;

    if (s_rx_dma_ready == 0u) {
        return;
    }

    head = usart1_dma_write_pos();
    last = s_dma_last_head;

    if (head >= last) {
        advanced = (uint16_t)(head - last);
    } else {
        advanced = (uint16_t)(USART1_CAT1_RX_RING_CAP - last + head);
    }

    if (USART1_CAT1_RX_HEAD >= USART1_CAT1_RX_TAIL) {
        used = (uint16_t)(USART1_CAT1_RX_HEAD - USART1_CAT1_RX_TAIL);
    } else {
        used = (uint16_t)(USART1_CAT1_RX_RING_CAP - USART1_CAT1_RX_TAIL
                          + USART1_CAT1_RX_HEAD);
    }

    free_space = (uint16_t)(USART1_CAT1_RX_RING_CAP - 1u - used);
    if (advanced > free_space) {
        s_usart1_rx_drop += (uint32_t)(advanced - free_space);
        USART1_CAT1_RX_TAIL = head;
    }

    s_dma_last_head = head;
    USART1_CAT1_RX_HEAD = head;
}

static void usart1_rx_dma_start(void)
{
    DMA_InitType dma;

    DMA_EnableChannel(USART1_RX_DMA_CH, DISABLE);
    DMA_DeInit(USART1_RX_DMA_CH);

    dma.PeriphAddr     = (uint32_t)&USART1->DAT;
    dma.MemAddr        = (uint32_t)USART1_CAT1_RX_RING;
    dma.Direction      = DMA_DIR_PERIPH_SRC;
    dma.BufSize        = USART1_CAT1_RX_RING_CAP;
    dma.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    dma.DMA_MemoryInc  = DMA_MEM_INC_ENABLE;
    dma.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
    dma.MemDataSize    = DMA_MemoryDataSize_Byte;
    dma.CircularMode   = DMA_MODE_CIRCULAR;
    dma.Priority       = DMA_PRIORITY_VERY_HIGH;
    dma.Mem2Mem        = DMA_M2M_DISABLE;
    DMA_Init(USART1_RX_DMA_CH, &dma);
    DMA_RequestRemap(DMA_REMAP_USART1_RX, DMA, USART1_RX_DMA_CH, ENABLE);

    USART1_CAT1_RX_HEAD = 0u;
    USART1_CAT1_RX_TAIL = 0u;
    s_dma_last_head = 0u;

    DMA_EnableChannel(USART1_RX_DMA_CH, ENABLE);
    USART_EnableDMA(USART1, USART_DMAREQ_RX, ENABLE);
    s_rx_dma_ready = 1u;
}

/**
 * @brief 配置 USART1；RX 走 DMA，仅保留错误/溢出中断诊断。
 */
static void usart1_periph_init(void)
{
    USART_InitType us;
    NVIC_InitType nvic;

    USART_DeInit(USART1);
    USART_StructInit(&us);
    us.BaudRate            = USART1_CAT1_BAUDRATE;
    us.WordLength          = USART_WL_8B;
    us.StopBits            = USART_STPB_1;
    us.Parity              = USART_PE_NO;
    us.HardwareFlowControl = USART_HFCTRL_NONE;
    us.Mode                = USART_MODE_RX | USART_MODE_TX;
    USART_Init(USART1, &us);

    USART_EnableDMA(USART1, USART_DMAREQ_RX | USART_DMAREQ_TX, DISABLE);
    USART_ConfigInt(USART1, USART_INT_RXDNE, DISABLE);

    USART_Enable(USART1, ENABLE);
    usart1_rx_dma_start();
    s_uart_baud = USART1_CAT1_BAUDRATE;

    /* ORE 等仍走 USART IRQ（DMA 忙碌时偶发）；优先级保持最高 */
    nvic.NVIC_IRQChannel                   = USART1_IRQn;
    nvic.NVIC_IRQChannelPriority           = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

int USART1_CAT1_SetBaudrate(uint32_t baud)
{
    USART_InitType us;
    uint32_t irq_mask;
    if ((baud != 9600U && baud != 115200U) || !s_rx_dma_ready) return -1;
    if (baud == s_uart_baud) return 0;
    irq_mask = __get_PRIMASK();
    __disable_irq();
    USART_EnableDMA(USART1, USART_DMAREQ_RX | USART_DMAREQ_TX, DISABLE);
    DMA_EnableChannel(USART1_RX_DMA_CH, DISABLE);
    USART_Enable(USART1, DISABLE);
    USART_StructInit(&us);
    us.BaudRate = baud;
    us.WordLength = USART_WL_8B;
    us.StopBits = USART_STPB_1;
    us.Parity = USART_PE_NO;
    us.HardwareFlowControl = USART_HFCTRL_NONE;
    us.Mode = USART_MODE_RX | USART_MODE_TX;
    USART_Init(USART1, &us);
    USART_ConfigInt(USART1, USART_INT_RXDNE, DISABLE);
    USART_Enable(USART1, ENABLE);
    (void)USART_ReceiveData(USART1);
    USART_ClrFlag(USART1, USART_FLAG_OREF);
    USART1_CAT1_RX_HEAD = USART1_CAT1_RX_TAIL = 0U;
    s_dma_last_head = 0U;
    DMA_SetCurrDataCounter(USART1_RX_DMA_CH, USART1_CAT1_RX_RING_CAP);
    DMA_EnableChannel(USART1_RX_DMA_CH, ENABLE);
    USART_EnableDMA(USART1, USART_DMAREQ_RX, ENABLE);
    s_uart_baud = baud;
    __set_PRIMASK(irq_mask);
    return 0;
}

void USART1_CAT1_RxFlush(void)
{
    __disable_irq();
    if (s_rx_dma_ready != 0u) {
        USART_EnableDMA(USART1, USART_DMAREQ_RX, DISABLE);
        DMA_EnableChannel(USART1_RX_DMA_CH, DISABLE);
    }
    USART1_CAT1_RX_HEAD = 0U;
    USART1_CAT1_RX_TAIL = 0U;
    s_dma_last_head = 0U;
    __enable_irq();

    while (USART_GetFlagStatus(USART1, USART_FLAG_RXDNE) != RESET) {
        (void)USART_ReceiveData(USART1);
    }
    if (USART_GetFlagStatus(USART1, USART_FLAG_OREF) != RESET) {
        USART_ClrFlag(USART1, USART_FLAG_OREF);
        (void)USART_ReceiveData(USART1);
    }

    if (s_rx_dma_ready != 0u) {
        DMA_SetCurrDataCounter(USART1_RX_DMA_CH, USART1_CAT1_RX_RING_CAP);
        DMA_EnableChannel(USART1_RX_DMA_CH, ENABLE);
        USART_EnableDMA(USART1, USART_DMAREQ_RX, ENABLE);
    }
}

uint8_t USART1_CAT1_RxPop(uint8_t *out_byte)
{
    uint8_t b;

    if (out_byte == NULL) {
        return 0U;
    }

    __disable_irq();
    USART1_CAT1_RxSync();
    if (USART1_CAT1_RX_HEAD == USART1_CAT1_RX_TAIL) {
        __enable_irq();
        return 0U;
    }
    b = USART1_CAT1_RX_RING[USART1_CAT1_RX_TAIL];
    USART1_CAT1_RX_TAIL =
        (uint16_t)((USART1_CAT1_RX_TAIL + 1U) % USART1_CAT1_RX_RING_CAP);
    __enable_irq();
    *out_byte = b;
    return 1U;
}

void USART1_CAT1_Init(void)
{
    co_printf("[RC   ] CAT1 rcc_gpio_init...\r\n");
    usart1_rcc_gpio_init();
    co_printf("[RC   ] CAT1 periph_init (RX DMA)...\r\n");
    usart1_periph_init();
    USART1_CAT1_RxFlush();
    co_printf("[RC   ] CAT1 periph_init done\r\n");
}

/**
 * @brief USART1 错误路径：清 OREF；数据由 DMA 搬运，勿再读 RXDNE 抢字节。
 */
void USART1_CAT1_IRQHandler(void)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_OREF) != RESET) {
        USART_ClrFlag(USART1, USART_FLAG_OREF);
        (void)USART_ReceiveData(USART1);
        s_usart1_rx_drop++;
    }
}

void USART1_IRQHandler(void)
{
    USART1_CAT1_IRQHandler();
}

static int usart1_poll_send_pump(const uint8_t *data, uint16_t len, void (*pump_fn)(void))
{
    uint16_t i;
    uint32_t spin;

    if (data == NULL || len == 0U) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        spin = 0U;
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXDE) == RESET) {
            if (pump_fn != NULL && (spin & 0x0Fu) == 0U) {
                pump_fn();
            }
            if (++spin >= CAT1_UART_TX_SPIN_MAX) {
                co_printf("[CAT1] USART1 TX timeout\r\n");
                return -1;
            }
        }
        USART_SendData(USART1, data[i]);
    }

    spin = 0U;
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXC) == RESET) {
        if (pump_fn != NULL && (spin & 0x0Fu) == 0U) {
            pump_fn();
        }
        if (++spin >= CAT1_UART_TX_SPIN_MAX) {
            co_printf("[CAT1] USART1 TXC timeout\r\n");
            return -1;
        }
    }
    USART_ClrFlag(USART1, USART_FLAG_TXC);
    return 0;
}

void USART1_CAT1_PollSend(const uint8_t *data, uint16_t len)
{
    (void)usart1_poll_send_pump(data, len, NULL);
}

uint32_t USART1_CAT1_RxDropCount(void)
{
    return s_usart1_rx_drop;
}

int usart1_at_send_pump(char *line, unsigned line_cap, void (*pump_fn)(void))
{
    size_t n;
    size_t i;
    const size_t chunk = 32u;

    if (line == NULL || line_cap < 3u) {
        return -1;
    }
    n = strlen(line);
    while (n > 0U && (line[n - 1U] == '\n' || line[n - 1U] == '\r')) {
        n--;
    }
    if (n + 2u > (size_t)line_cap) {
        n = (size_t)line_cap - 2u;
    }
    line[n++] = '\r';
    line[n++] = '\n';

    for (i = 0U; i < n; i += chunk) {
        size_t left = n - i;
        size_t send_len = (left > chunk) ? chunk : left;

        if (usart1_poll_send_pump((const uint8_t *)(line + i), (uint16_t)send_len, pump_fn) != 0) {
            return -1;
        }
    }
    line[n - 2u] = '\0';
    return 0;
}

void usart1_at_send(char *line, unsigned line_cap)
{
    (void)usart1_at_send_pump(line, line_cap, NULL);
}
