/**
 * @file upg_md5.h
 * @brief MD5（GY/T 407-2024 附录 J.3），用于三模通用遥控器升级认证摘要
 */
#ifndef _UPG_MD5_H_
#define _UPG_MD5_H_

#include <stdint.h>

typedef struct {
    uint32_t count[2];      /* 已处理 bit 数，低 32 位在前 */
    uint32_t state[4];      /* A/B/C/D */
    uint8_t  buffer[64];
} upg_md5_ctx_t;

void upg_md5_init(upg_md5_ctx_t *ctx);
void upg_md5_update(upg_md5_ctx_t *ctx, const uint8_t *data, uint32_t len);
void upg_md5_final(upg_md5_ctx_t *ctx, uint8_t digest[16]);

/** @brief 一次性计算：data 连续内存 */
void upg_md5(const uint8_t *data, uint32_t len, uint8_t digest[16]);

#endif /* _UPG_MD5_H_ */
