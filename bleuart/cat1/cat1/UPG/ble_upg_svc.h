/**
 * @file ble_upg_svc.h
 * @brief 三模通用遥控器升级服务（GY/T 407-2024 表 A.5）
 *
 * 服务 UUID：10111213-1415-1617-1819-1a1b1c1d1931
 * 升级数据特征：10111213-1415-1617-1819-1a1b1c1d2b31
 * 特征属性：read / write_cmd / notify（MTU 最小 139）
 */
#ifndef _BLE_UPG_SVC_H_
#define _BLE_UPG_SVC_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  注册升级服务
 * @note   必须在 ble_gap_init() 之前调用，使绑定恢复时协议栈能追踪 GATT 变更
 */
int ble_upg_svc_init(void);

/**
 * @brief  通过升级数据特征发送通知
 * @return true 已提交给协议栈
 */
bool ble_upg_notify(const uint8_t *buf, uint16_t len);

/**
 * @brief  断链处理：清订阅缓存
 * @note   订阅状态不能放在借来的会话工作区（会话重建会清零），故由本模块持有
 */
void ble_upg_on_link_lost(void);

#endif /* _BLE_UPG_SVC_H_ */
