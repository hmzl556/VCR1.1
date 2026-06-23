/**
 * @file cat1_usart2.h
 * @brief USART2 (PB4 TX / PB5 RX, AF3) 与 CAT1 模组通信：115200 8N1，RX 中断环形缓冲，TX 轮询。
 */
#ifndef BSP_CAT1_USART2_H
#define BSP_CAT1_USART2_H

#include "br1601x.h"
#include "cat1_udp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USART2_CAT1_RX_RING_CAP 384u

/** AT 单行发送缓冲（= CAT1_AT_LINE_MAX，容纳 SKTSEND Hex 行） */
#define USART2_CAT1_TX_MAX    CAT1_AT_LINE_MAX

/** 中断环形 RX 缓冲（cat1_usart2.c 定义） */
extern volatile uint8_t USART2_CAT1_RX_RING[USART2_CAT1_RX_RING_CAP];
extern volatile uint16_t USART2_CAT1_RX_HEAD;
extern volatile uint16_t USART2_CAT1_RX_TAIL;

void USART2_CAT1_Init(void);
void USART2_CAT1_IRQHandler(void);

void USART2_CAT1_PollSend(const uint8_t *data, uint16_t len);
/** AT 单行发送：在 line 末尾追加 \\r\\n 后直接发送（与 skt_udp 共用缓冲，无本地拷贝） */
void usart2_at_send(char *line, unsigned line_cap);

/** 丢弃尚未读取的 RX 数据 */
void USART2_CAT1_RxFlush(void);

/** 从环形缓冲非阻塞读出 1 字节（成功返回 1，空返回 0）。 */
uint8_t USART2_CAT1_RxPop(uint8_t *out_byte);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAT1_USART2_H */
