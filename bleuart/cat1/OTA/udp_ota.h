/**
 * @file udp_ota.h
 * @brief UDP OTA §3.1 协议时序（终端拉片、写 APP2、整包校验、复位）
 *
 * 时序（与服务器 ota_session.py 一致）：
 *   NEED_SEND → 发 req_seq=N → WAIT_CHUNK → 收 chunk N → 写 Flash
 *   → NEED_SEND(req N+1) … → 发 req_seq=total+1（SKTSEND 发出即 VERIFY，不等服务器/模组 OK）→ 复位
 */
#ifndef UDP_OTA_H
#define UDP_OTA_H

#include <stdint.h>

#define OTA_LED_BLINK_PERIOD_MS      100u
#define OTA_LED_BLINK_DURATION_MS    3000u

void udp_ota_init(void);
void udp_ota_on_udp_ready(void);
void udp_ota_on_tag11_task(uint16_t sw_version_bcd, uint16_t total_packets);

/**
 * @param  pdu      UDP 载荷
 * @param  pdu_len  载荷长度
 * @return 1 本轮回合代码片已收（可结束 SKTSEND 等待）；0 已消费；-1 非 0x41
 */
int udp_ota_on_rx(const uint8_t *pdu, uint16_t pdu_len);

/** 主循环：写 Flash（须在 rc_cat1_poll 内、收包与发 req 之间调用） */
void udp_ota_poll_work(void);

/**
 * @return 1 有上行帧；*frame 指向内部缓冲（勿与 s_cat1_tlv 混用）；-2 无
 */
int udp_ota_poll_tx_get(const uint8_t **frame, uint16_t *len);

/** OTA 下载中 LED1 慢闪（表示未卡死）；与 chunk1 后常亮可叠加 */
void udp_ota_download_led_poll(void);

/** WAIT_CHUNK 超时兜底（主循环被饿死时强制重试）；@return 1 已触发重试 */
uint8_t udp_ota_poll_watchdog(void);

/** SKTSEND 已发出（modem BUSY）；拉片 req 立即进入 WAIT_CHUNK */
void udp_ota_on_uplink_issued(void);

/** SKTSEND 事务结束（modem OK：代码片已到或握手 total+1 成功） */
void udp_ota_on_uplink_sent(void);

/** 等 downlink 代码片超时（modem 层；上行 SKTSEND 可能已成功，重试同一 req_seq） */
void udp_ota_on_uplink_timeout(void);

/** 当前 uplink 是否须等 downlink 代码片（req_seq≤total 为 1，握手 total+1 为 0） */
uint8_t udp_ota_uplink_waits_chunk(void);

/** 兼容旧接口；协议层请用 on_uplink_sent/timeout */
void udp_ota_confirm_tx(void);

uint8_t udp_ota_is_active(void);
/** @brief 整个 OTA 会话是否占用（含 PAUSE 退避；fail/复位前均为 1） */
uint8_t udp_ota_session_busy(void);
/** @brief 进入 OTA 会话：关广播、停语音；幂等 */
void udp_ota_session_enter(void);
/** @brief 退出 OTA 会话（udp_ota_fail 等）；幂等 */
void udp_ota_session_exit(void);
uint8_t udp_ota_has_pending_uplink(void);
void udp_ota_boot_indicate_poll(void);

/** 下载无进展超时则 PAUSE；@return 1 本周期刚 PAUSE */
uint8_t udp_ota_poll_stall_pause(void);
/** 取走一次 PAUSE 事件（bridge 释放 modem/tx 状态） */
uint8_t udp_ota_take_pause_event(void);
/** PAUSE 退避到期后恢复 NEED_SEND 并直接发 0x41；@return 1 本周期刚 RESUME */
uint8_t udp_ota_poll_auto_resume(void);

#endif /* UDP_OTA_H */
