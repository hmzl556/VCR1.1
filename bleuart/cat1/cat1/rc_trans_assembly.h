#ifndef RC_TRANS_ASSEMBLY_H
#define RC_TRANS_ASSEMBLY_H

#include <stdint.h>
#include "gap.h"
#include "rc_protocol.h"

// 重置 RC-TRANS 汇聚状态（配置生效或断线时调用）
void rc_trans_reset(void);

// 重置汇聚 + 断线续传 PackIndex 会话（CONF / BLE 断线）
void rc_trans_session_reset(void);

// 处理 RC-TRANS Write Request 载荷
int rc_trans_on_write(peer_t peer, const uint8_t *val, uint16_t len);

// UDP 发送成功后记录 PackIndex（断线续传用）
void rc_trans_note_upload_ok(uint32_t pack_index);

// 主循环独占已完成且未release的缓冲；允许校验/修正后入队，不可保留指针
uint8_t *rc_trans_get_assembled(uint16_t *len);

// 主循环入队完成后释放汇聚缓冲
void rc_trans_release(void);

// 半包汇聚超时检查（在 rc_cat1_poll / 主循环中周期调用）
void rc_trans_poll(void);

#endif /* RC_TRANS_ASSEMBLY_H */
