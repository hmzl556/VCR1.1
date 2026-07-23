/**
 * @file cat1_usart2.h
 * @brief USART2 (PB4 TX / PB5 RX, AF3) 与 CAT1 模组通信：115200 8N1，
 *        RX=DMA_CH2 循环环，TX 轮询。
 */
#ifndef BSP_CAT1_USART2_H
#define BSP_CAT1_USART2_H

#include "br1601x.h"
#include "cat1_udp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** RX 环容量见 cat1_udp_config.h（DMA 写 / 软件读） */
#define USART2_CAT1_RX_RING_CAP CAT1_UART_RX_RING_CAP

/** AT 单行发送缓冲（= CAT1_AT_LINE_MAX，容纳 SKTSEND Hex 行） */
#define USART2_CAT1_TX_MAX    CAT1_AT_LINE_MAX

/** DMA 循环 RX 缓冲；HEAD 由 RxSync/RxPop 自 DMA NDTR 刷新 */
extern volatile uint8_t USART2_CAT1_RX_RING[USART2_CAT1_RX_RING_CAP];
extern volatile uint16_t USART2_CAT1_RX_HEAD;
extern volatile uint16_t USART2_CAT1_RX_TAIL;

void USART2_CAT1_Init(void);
void USART2_CAT1_IRQHandler(void);

void USART2_CAT1_PollSend(const uint8_t *data, uint16_t len);
/** 分段发送并在段间/等 TX 时调用 pump，避免阻塞 TX 期间丢 URC */
void usart2_at_send(char *line, unsigned line_cap);
/** @return 0 成功；-1 TX 超时 */
int usart2_at_send_pump(char *line, unsigned line_cap, void (*pump_fn)(void));
/** RX 环满/ORE 丢弃字节累计（诊断） */
uint32_t USART2_CAT1_RxDropCount(void);

/** 从 DMA 写指针刷新 HEAD（读 HEAD/TAIL 诊断前可先调用） */
void USART2_CAT1_RxSync(void);

/** 丢弃尚未读取的 RX 数据并重启 DMA 写指针 */
void USART2_CAT1_RxFlush(void);

/** 从环形缓冲非阻塞读出 1 字节（成功返回 1，空返回 0）。 */
uint8_t USART2_CAT1_RxPop(uint8_t *out_byte);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAT1_USART2_H */
