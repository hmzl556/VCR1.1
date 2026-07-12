/**
 * @file aes_transport.h
 * @brief UDP 0x6868 数据域 AES-128-ECB + PKCS5（tiny-AES-c）。
 */
#ifndef BSP_AES_TRANSPORT_H
#define BSP_AES_TRANSPORT_H

#include <stdint.h>

// 初始化 AES 上下文（RC_UDP_AES_ENABLE=0 时为空操作）
int rc_udp_aes_init(void);

// 加密明文 data field（含 TLV数据长度 2B）；输出 PKCS5 对齐密文
int rc_udp_aes_encrypt_data_field(const uint8_t *plain, uint16_t plain_len,
                                  uint8_t *cipher, uint16_t cipher_cap, uint16_t *cipher_len);

// 解密密文 data field，输出明文（含 TLV数据长度 2B）
int rc_udp_aes_decrypt_data_field(const uint8_t *cipher, uint16_t cipher_len,
                                  uint8_t *plain, uint16_t plain_cap, uint16_t *plain_len);

#endif /* BSP_AES_TRANSPORT_H */
