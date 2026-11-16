/**
 * @file upg_proto.h
 * @brief 三模通用遥控器升级协议状态机（GY/T 407-2024 E.1.1 / E.3）
 *
 * P1 阶段实现：双向认证、读取硬件信息与版本信息、升级标志、写指令配置。
 * 键值写入（0x12/0x13 实际落盘）留待 P2。
 */
#ifndef _UPG_PROTO_H_
#define _UPG_PROTO_H_

#include <stdint.h>

/**
 * @brief  是否强制先认证再允许读取/写入
 * @note   P1 联调阶段置 0，便于用手机手工验证 GATT 与读取流程；
 *         接入正式 APP 并确认认证链路后应置 1。
 */
#ifndef UPG_REQUIRE_AUTH
#define UPG_REQUIRE_AUTH   0
#endif

void upg_proto_init(void);

/** @brief 对端已订阅通知：进入会话并主动发起认证 */
void upg_proto_on_link_ready(void);

/** @brief 收到一帧 write_cmd 数据（拷贝已在服务层完成） */
void upg_proto_on_frame(const uint8_t *data, uint16_t len);

/** @brief 周期任务：推进读数据分包、检查会话超时与断连 */
void upg_proto_poll(void);

#endif /* _UPG_PROTO_H_ */
