/**
 * @file cat1_skt_udp.h
 * @brief 模组 UDP（NT26 方案 B）：`ECSOCL`→`ECSOCR`→`ECSONMI`→`ECSOST`（Hex），
 *        下行 `+ECSONMI` + `ECSORF` 主动读，协作式非阻塞。
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

/** @brief OTA 已收到代码片时提前结束 ECSOST 等待（不必等 AT 超时） */
void cat1_skt_udp_finish_active_ok(void);

/**
 * @brief 分步建立 UDP Socket（诊断 AT → CGATT/CGPADDR → ECSOCL→ECSOCR→ECSONMI）。
 * @return CAT1_SETUP_BUSY / CAT1_SETUP_OK / CAT1_SETUP_FAIL
 */
int cat1_skt_udp_setup_step(void);

/**
 * @brief 发起 ECSOST（非阻塞）；须先 cat1_modem_step 直至返回 OK/FAIL。
 * @note  wait_after_ok_ms：OK 后等待 +ECSONMI/ECSORF 毫秒数；OTA 0x41 上行应传 0（代码片异步到达）。
 */
cat1_modem_result_t cat1_skt_udp_send_begin(const uint8_t *payload, uint16_t len);

cat1_modem_result_t cat1_skt_udp_send_begin_wait(const uint8_t *payload, uint16_t len,
                                                 uint32_t wait_after_ok_ms);

cat1_modem_result_t cat1_skt_udp_send_begin_wait_ex(const uint8_t *payload, uint16_t len,
                                                    uint32_t wait_after_ok_ms,
                                                    uint8_t wait_downlink_chunk,
                                                    uint16_t remote_port);

/** @brief 将 USART2 已有字节喂入 +ECSONMI 解析（不取 PDU，OTA 期间可频繁调用） */
void cat1_skt_udp_pump_urc(void);

/** 尽量抽干 RX 环并解析 +ECSONMI（Flash 写后、发下一 req 前调用） */
void cat1_skt_udp_drain_urc(void);

/** @brief 是否已有未取走的 ECSORF PDU */
uint8_t cat1_skt_udp_recv_pending(void);

/** @brief 强制结束进行中的 AT 事务为 FAIL（OTA 看门狗用） */
void cat1_skt_udp_force_fail_active(void);

/**
 * @brief  非阻塞处理 RX 中 +ECSONMI，经 ECSORF 取出一条 UDP 下行 PDU
 * @return 1 收到一条；0 无数据；-1 参数/解析错误
 */
int cat1_skt_udp_poll_recv(uint8_t *pdu, uint16_t pdu_cap, uint16_t *pdu_len);

/**
 * @brief  绑定 UDP 下行 PDU 解析目标（与 rc_cat1_bridge.s_cat1_tlv 复用）
 */
void cat1_skt_udp_bind_recv_buf(uint8_t *buf, uint16_t cap);

/** @brief  0=丢弃 ECSORF 写入 bind 目标（入队/发送窗口保护） */
void cat1_skt_udp_set_recv_allow(uint8_t allow);

/** @brief  setup 失败后重置，以便重试 */
void cat1_skt_udp_setup_reset(void);

/** OTA 等 downlink 片超时时打印 UART/+ECSONMI 诊断（定因 rx_drop / 拼行 / 无数据） */
void cat1_skt_udp_log_recv_diag(uint16_t req_seq);

/*===========================================================================
 * 休眠态 CAT1 管理
 *===========================================================================*/

/**
 * @brief  休眠前准备：发送 AT+QSCLK=1 让模组进入 DRX 睡眠
 * @note   阻塞调用，等待 OK 或超时 3s。模组在 DRX 睡眠期间仍可接收寻呼和数据。
 */
void cat1_sleep_prepare(void);

/**
 * @brief  休眠唤醒后恢复：发送 AT+QSCLK=0 让模组退出睡眠
 * @note   阻塞调用，先发 AT 唤醒模组 UART，再发 AT+QSCLK=0。
 */
void cat1_sleep_wakeup(void);

#endif /* BSP_CAT1_SKT_UDP_H */
