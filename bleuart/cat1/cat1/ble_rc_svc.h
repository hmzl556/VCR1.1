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

// RC-KEY-NOTIFY 4 字节镜像上报
void ble_rc_svc_key_notify(peer_t peer, uint8_t key_type, uint8_t action, uint16_t key_value);

void ble_rc_svc_on_connect(peer_t peer);

void ble_rc_svc_on_disconnect(void);

#endif /* BLE_RC_SVC_H */
