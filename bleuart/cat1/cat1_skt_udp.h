/**
 * @file cat1_skt_udp.h
 * @brief 模组 UDP：`SKTDELETE`→`CREATE`→`BIND`→`CONNECT`→`SKTSEND`（Hex），协作式非阻塞。
 */
#ifndef BSP_CAT1_SKT_UDP_H
#define BSP_CAT1_SKT_UDP_H

#include "br1601x.h"
#include <stdint.h>

typedef enum {
	CAT1_MODEM_IDLE = 0,
	CAT1_MODEM_BUSY,
	CAT1_MODEM_OK,
	CAT1_MODEM_FAIL,
} cat1_modem_result_t;

/** setup_step：1=进行中，0=成功，-1=失败 */
#define CAT1_SETUP_BUSY  1
#define CAT1_SETUP_OK    0
#define CAT1_SETUP_FAIL  (-1)

/**
 * @brief 推进进行中的 AT 事务（仅消费 ring 已有字节，不阻塞）。
 */
cat1_modem_result_t cat1_modem_step(void);

/** @brief 是否有 AT 事务正在进行 */
uint8_t cat1_modem_is_busy(void);

/**
 * @brief 分步建立 UDP Socket（DELETE→CREATE→BIND→CONNECT）。
 * @return CAT1_SETUP_BUSY / CAT1_SETUP_OK / CAT1_SETUP_FAIL
 */
int cat1_skt_udp_setup_step(void);

/**
 * @brief 发起 SKTSEND（非阻塞）；须先 cat1_modem_step 直至返回 OK/FAIL。
 */
cat1_modem_result_t cat1_skt_udp_send_begin(const uint8_t *payload, uint16_t len);

/**
 * @brief  非阻塞处理 RX 中 +SKTRECV URC，取出一条 UDP 下行 PDU
 * @return 1 收到一条；0 无数据；-1 参数/解析错误
 */
int cat1_skt_udp_poll_recv(uint8_t *pdu, uint16_t pdu_cap, uint16_t *pdu_len);

/**
 * @brief  绑定 UDP 下行 PDU 解析目标（与 rc_cat1_bridge.s_cat1_tlv 复用）
 */
void cat1_skt_udp_bind_recv_buf(uint8_t *buf, uint16_t cap);

/** @brief  setup 失败后重置，以便重试 */
void cat1_skt_udp_setup_reset(void);

#endif /* BSP_CAT1_SKT_UDP_H */
