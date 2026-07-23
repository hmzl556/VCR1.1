#ifndef _BLE_BAS_H_
#define _BLE_BAS_H_

/**
 * @brief  初始化 BAS 1.0（Battery Service 0x180F / Battery Level 0x2A19）
 */
int ble_bas_init(void);

/**
 * @brief  断开连接时仅清 peer，保留 Notify 订阅标志
 */
void ble_bas_on_disconnect(void);

/**
 * @brief  周期轮询：CCC 已订阅时每 60 秒采样并 Notify 一次
 */
void ble_bas_poll(void);

#endif /* _BLE_BAS_H_ */
