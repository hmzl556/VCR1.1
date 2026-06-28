/**
 * @file    pm_standby.h
 * @brief   待机态管理 API — 进入/退出待机态
 */

#ifndef _PM_STANDBY_H_
#define _PM_STANDBY_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  进入待机态
 * @note   切换 BLE 连接参数（50ms/lat100）、关闭非必要外设（CAT1 TX、LED）
 */
void pm_standby_enter(void);

/**
 * @brief  退出待机态
 * @note   恢复 BLE 连接参数（10ms/lat40）、恢复外设
 */
void pm_standby_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _PM_STANDBY_H_ */
