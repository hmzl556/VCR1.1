/**
 * @file cat1_usart2.c
 * @brief USART2 PB4/PB5 (AF3)：RX 用 DMA_CH2 循环写入环形缓冲，TX 阻塞发送。
 *
 * 根治 ECSORF 长 hex 行在 IRQ+软件环下偶发丢 1～2 字节：硬件 DMA 收字节，
 * 任务侧只推进读指针。ADC 语音占用 DMA_CH1，本模块固定 DMA_CH2。
 */
#include "cat1_usart2.h"
#include "usart.h"
#include "dma.h"
#include "rcc.h"
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

/** ADC 语音用 CH1；USART2 RX 固定 CH2，避免互相 DeInit */
#define USART2_RX_DMA_CH         DMA_CH2

volatile uint8_t USART2_CAT1_RX_RING[USART2_CAT1_RX_RING_CAP];
volatile uint16_t USART2_CAT1_RX_HEAD;
volatile uint16_t USART2_CAT1_RX_TAIL;
static volatile uint32_t s_usart2_rx_drop;
/** 上次 sync 时的 DMA 写指针，用于检测环被覆盖 */
static uint16_t s_dma_last_head;
static uint8_t  s_rx_dma_ready;

/**
 * @brief 使能 USART2 时钟与 PB4/PB5 复用 AF3。
 */
static void usart2_rcc_gpio_init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAPB2PeriphClk(USART2_CFG_GPIO_CLK | RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);

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

/** DMA 写指针：已写入位置 = CAP - 剩余计数 */
static uint16_t usart2_dma_write_pos(void)
{
    uint16_t rem = DMA_GetCurrDataCounter(USART2_RX_DMA_CH);

    if (rem > USART2_CAT1_RX_RING_CAP) {
        rem = USART2_CAT1_RX_RING_CAP;
    }
    return (uint16_t)(USART2_CAT1_RX_RING_CAP - rem);
}

/**
 * @brief 从 DMA NDTR 刷新 HEAD；若本段写入超过当时空闲空间则记 drop 并丢弃未读。
 */
void USART2_CAT1_RxSync(void)
{
    uint16_t head;
    uint16_t last;
    uint16_t advanced;
    uint16_t used;
    uint16_t free_space;

    if (s_rx_dma_ready == 0u) {
        return;
    }

    head = usart2_dma_write_pos();
    last = s_dma_last_head;

    if (head >= last) {
        advanced = (uint16_t)(head - last);
    } else {
        advanced = (uint16_t)(USART2_CAT1_RX_RING_CAP - last + head);
    }

    if (USART2_CAT1_RX_HEAD >= USART2_CAT1_RX_TAIL) {
        used = (uint16_t)(USART2_CAT1_RX_HEAD - USART2_CAT1_RX_TAIL);
    } else {
        used = (uint16_t)(USART2_CAT1_RX_RING_CAP - USART2_CAT1_RX_TAIL
                          + USART2_CAT1_RX_HEAD);
    }

    free_space = (uint16_t)(USART2_CAT1_RX_RING_CAP - 1u - used);
    if (advanced > free_space) {
        s_usart2_rx_drop += (uint32_t)(advanced - free_space);
        USART2_CAT1_RX_TAIL = head;
    }

    s_dma_last_head = head;
    USART2_CAT1_RX_HEAD = head;
}

static void usart2_rx_dma_start(void)
{
    DMA_InitType dma;

    DMA_EnableChannel(USART2_RX_DMA_CH, DISABLE);
    DMA_DeInit(USART2_RX_DMA_CH);

    dma.PeriphAddr     = (uint32_t)&USART2->DAT;
    dma.MemAddr        = (uint32_t)USART2_CAT1_RX_RING;
    dma.Direction      = DMA_DIR_PERIPH_SRC;
    dma.BufSize        = USART2_CAT1_RX_RING_CAP;
    dma.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    dma.DMA_MemoryInc  = DMA_MEM_INC_ENABLE;
    dma.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
    dma.MemDataSize    = DMA_MemoryDataSize_Byte;
    dma.CircularMode   = DMA_MODE_CIRCULAR;
    dma.Priority       = DMA_PRIORITY_VERY_HIGH;
    dma.Mem2Mem        = DMA_M2M_DISABLE;
    DMA_Init(USART2_RX_DMA_CH, &dma);
    DMA_RequestRemap(DMA_REMAP_USART2_RX, DMA, USART2_RX_DMA_CH, ENABLE);

    USART2_CAT1_RX_HEAD = 0u;
    USART2_CAT1_RX_TAIL = 0u;
    s_dma_last_head = 0u;

    DMA_EnableChannel(USART2_RX_DMA_CH, ENABLE);
    USART_EnableDMA(USART2, USART_DMAREQ_RX, ENABLE);
    s_rx_dma_ready = 1u;
}

/**
 * @brief 配置 USART2；RX 走 DMA，仅保留错误/溢出中断诊断。
 */
static void usart2_periph_init(void)
{
    USART_InitType us;
    NVIC_InitType nvic;

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
    USART_ConfigInt(USART2, USART_INT_RXDNE, DISABLE);

    USART_Enable(USART2, ENABLE);
    usart2_rx_dma_start();

    /* ORE 等仍走 USART IRQ（DMA 忙碌时偶发）；优先级保持最高 */
    nvic.NVIC_IRQChannel                   = USART2_IRQn;
    nvic.NVIC_IRQChannelPriority           = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

void USART2_CAT1_RxFlush(void)
{
    __disable_irq();
    if (s_rx_dma_ready != 0u) {
        USART_EnableDMA(USART2, USART_DMAREQ_RX, DISABLE);
        DMA_EnableChannel(USART2_RX_DMA_CH, DISABLE);
    }
    USART2_CAT1_RX_HEAD = 0U;
    USART2_CAT1_RX_TAIL = 0U;
    s_dma_last_head = 0U;
    __enable_irq();

    while (USART_GetFlagStatus(USART2, USART_FLAG_RXDNE) != RESET) {
        (void)USART_ReceiveData(USART2);
    }
    if (USART_GetFlagStatus(USART2, USART_FLAG_OREF) != RESET) {
        USART_ClrFlag(USART2, USART_FLAG_OREF);
        (void)USART_ReceiveData(USART2);
    }

    if (s_rx_dma_ready != 0u) {
        DMA_SetCurrDataCounter(USART2_RX_DMA_CH, USART2_CAT1_RX_RING_CAP);
        DMA_EnableChannel(USART2_RX_DMA_CH, ENABLE);
        USART_EnableDMA(USART2, USART_DMAREQ_RX, ENABLE);
    }
}

uint8_t USART2_CAT1_RxPop(uint8_t *out_byte)
{
    uint8_t b;

    if (out_byte == NULL) {
        return 0U;
    }

    __disable_irq();
    USART2_CAT1_RxSync();
    if (USART2_CAT1_RX_HEAD == USART2_CAT1_RX_TAIL) {
        __enable_irq();
        return 0U;
    }
    b = USART2_CAT1_RX_RING[USART2_CAT1_RX_TAIL];
    USART2_CAT1_RX_TAIL =
        (uint16_t)((USART2_CAT1_RX_TAIL + 1U) % USART2_CAT1_RX_RING_CAP);
    __enable_irq();
    *out_byte = b;
    return 1U;
}

void USART2_CAT1_Init(void)
{
    /* 必须先开时钟再操作 USART2 寄存器, 否则总线卡死 */
    co_printf("[RC   ] CAT1 rcc_gpio_init...\r\n");
    usart2_rcc_gpio_init();
    co_printf("[RC   ] CAT1 periph_init (RX DMA)...\r\n");
    usart2_periph_init();
    USART2_CAT1_RxFlush();
    co_printf("[RC   ] CAT1 periph_init done\r\n");
}

/**
 * @brief USART2 错误路径：清 OREF；数据由 DMA 搬运，勿再读 RXDNE 抢字节。
 */
void USART2_CAT1_IRQHandler(void)
{
    if (USART_GetFlagStatus(USART2, USART_FLAG_OREF) != RESET) {
        USART_ClrFlag(USART2, USART_FLAG_OREF);
        (void)USART_ReceiveData(USART2);
        s_usart2_rx_drop++;
    }
}

void USART2_IRQHandler(void)
{
    USART2_CAT1_IRQHandler();
}

/**
 * @brief 轮询发送；等待 TX 时空转并 pump，避免 OTA 期间丢 +ECSONMI。
 * @return 0 成功；-1 TX 超时
 */
static int usart2_poll_send_pump(const uint8_t *data, uint16_t len, void (*pump_fn)(void))
{
    uint16_t i;
    uint32_t spin;

    if (data == NULL || len == 0U) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        spin = 0U;
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXDE) == RESET) {
            if (pump_fn != NULL && (spin & 0x0Fu) == 0U) {
                pump_fn();
            }
            if (++spin >= CAT1_UART_TX_SPIN_MAX) {
                co_printf("[CAT1] USART2 TX timeout\r\n");
                return -1;
            }
        }
        USART_SendData(USART2, data[i]);
    }

    spin = 0U;
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXC) == RESET) {
        if (pump_fn != NULL && (spin & 0x0Fu) == 0U) {
            pump_fn();
        }
        if (++spin >= CAT1_UART_TX_SPIN_MAX) {
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

        if (usart2_poll_send_pump((const uint8_t *)(line + i), (uint16_t)send_len, pump_fn) != 0) {
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
