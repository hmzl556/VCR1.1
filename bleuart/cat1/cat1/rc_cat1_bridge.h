#ifndef RC_CAT1_BRIDGE_H
#define RC_CAT1_BRIDGE_H

#include <stdint.h>
#include "rc_protocol.h"

void rc_cat1_init(void);
/* Includes setup/boot/retry gaps and queued work, not only an active AT. */
uint8_t rc_cat1_has_work(void);
void rc_cat1_poll(void);
void rc_cat1_poll_ota_exclusive(void);
int rc_cat1_on_tlv_ready(uint16_t tlv_len, uint32_t pack_index);
void rc_cat1_on_udp_done(uint8_t result, uint32_t pack_index);
void rc_cat1_get_cloud_policy(uint8_t policy[6]);
void rc_cat1_cancel_pending(void);

/**
 * @brief CAT1 模组断电：关 LTE_EN，重置 socket/UDP 状态
 * @note  将在 pm_standby_enter() 中调用，替代仅关 TX 的做法
 */
void rc_cat1_power_down(void);

/** UDP socket 是否已就绪 (setup 完成, 可安全入队上行帧) */
uint8_t rc_cat1_udp_is_ready(void);

/**
 * @brief CAT1 模组上电：开 LTE_EN，完整初始化（boot 延时 + UDP setup）
 * @note  将在 pm_standby_exit() 中调用，替代仅开 TX 的做法
 */
void rc_cat1_power_up(void);

/**
 * @brief 查询 CAT1 模组当前是否已上电
 * @return 1=已上电，0=已断电
 */
uint8_t rc_cat1_is_powered(void);

#endif /* RC_CAT1_BRIDGE_H */
