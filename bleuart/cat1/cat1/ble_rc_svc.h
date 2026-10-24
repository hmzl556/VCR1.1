#ifndef BLE_RC_SVC_H
#define BLE_RC_SVC_H

#include "gap.h"
#include "rc_protocol.h"

// 初始化 RC GATT 服务（19ee51a4）及五个特征
int ble_rc_svc_init(void);

// 获取当前连接对端（供 RC-STATUS Notify 使用）
peer_t ble_rc_svc_get_peer(void);

// 读取 RC-STATUS 缓存
const rc_status_t *ble_rc_svc_get_status(void);

/** 读取 RC-READ 61 字节缓存（Tag0x01 等 UDP 字段与之间复用） */
const uint8_t *ble_rc_svc_get_read(void);

// 更新 RC-STATUS 并通过 Notify 推送（peer 可为 NULL 仅更新缓存）
void ble_rc_svc_status_notify(peer_t peer, const rc_status_t *st);

// 更新 RC-STATUS 并通过 Indicate 推送（规范 §2: 结果应使用 Indicate）
void ble_rc_svc_status_indicate(peer_t peer, const rc_status_t *st);

// RC-KEY-NOTIFY 4 字节镜像上报
void ble_rc_svc_key_notify(peer_t peer, uint8_t key_type, uint8_t action, uint16_t key_value);

void ble_rc_svc_on_connect(peer_t peer);

void ble_rc_svc_on_disconnect(void);

/** 本连接会话内 STB 是否已写 RC CCCD 订阅 */
uint8_t ble_rc_svc_is_subscribed(void);

/** 是否需要会话恢复 (曾有效 CONF 但本会话未收到 CONF) */
uint8_t ble_rc_svc_needs_restore(void);

/** STB 未写 CCCD 时强制打开 RC 发送通道（与 HID force 一致） */
void ble_rc_svc_force_cccd_enable(peer_t peer);

#endif /* BLE_RC_SVC_H */
