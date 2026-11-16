/**
 * @file upg_auth.h
 * @brief 三模通用遥控器升级认证（GY/T 407-2024 E.1.2，双向 MD5）
 *
 * 摘要算法：hash = MD5( key[16] || rand[16] || idcode[32] )
 */
#ifndef _UPG_AUTH_H_
#define _UPG_AUTH_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UPG_AUTH_IDLE = 0,         /**< 未开始 */
    UPG_AUTH_WAIT_HOST_HASH,   /**< 已发 rand1，等主机回 hash */
    UPG_AUTH_WAIT_RC_REQ,      /**< 主机已通过，等主机发起遥控器认证 */
    UPG_AUTH_DONE,             /**< 双向认证通过 */
    UPG_AUTH_FAILED            /**< 认证失败 */
} upg_auth_state_t;

void upg_auth_reset(void);

/**
 * @brief  发起认证：生成 rand1，输出「启动升级主机认证请求」帧（FA 20 状态 0x00）
 * @return 帧长度；0 表示失败
 */
uint16_t upg_auth_start(uint8_t *out, uint16_t cap);

/**
 * @brief  处理主机回传的摘要（F9 20）
 * @param  hash 16 字节摘要
 * @param  out  输出「升级主机认证响应」帧（FA 20 状态 0x01/0x02）
 * @return 帧长度
 */
uint16_t upg_auth_on_host_hash(const uint8_t *hash, uint16_t len, uint8_t *out, uint16_t cap);

/**
 * @brief  处理主机发起的遥控器认证（F9 21）
 * @param  out 状态码 0x00 时输出 FA 21 摘要帧；其余状态输出长度 0
 * @return 帧长度
 */
uint16_t upg_auth_on_rc_request(const uint8_t *frame, uint16_t len, uint8_t *out, uint16_t cap);

upg_auth_state_t upg_auth_state(void);
bool upg_auth_is_done(void);

/** @brief 填充随机数（当前为软件 PRNG，量产建议改用芯片硬件随机源） */
void upg_auth_rand(uint8_t *out, uint16_t len);

#endif /* _UPG_AUTH_H_ */
