/**
 * @file cat1_udp_config.h
 * @brief CAT1 模组 UDP SKT\* 远端与本地端口（按需修改与联调环境一致）。
 *
 * 《5G遥控器数据采集接口文档》中的业务 Tag 由 STB 经 RC-TRANS 下发；
 * remote_reports 将 RC-TRANS Payload 透传封装为 0x6868 帧。
 */
#ifndef BSP_CAT1_UDP_CONFIG_H
#define BSP_CAT1_UDP_CONFIG_H

#define CAT1_UDP_REMOTE_HOST   "106.53.179.53"     // 服务器地址 自己：106.53.179.53
//#define CAT1_UDP_REMOTE_HOST   "120.48.54.123"     // 服务器地址 赛科：120.48.54.123

#define CAT1_UDP_REMOTE_PORT   8888u               // UDP 服务端端口

#define CAT1_UDP_BIND_ADDR     "\"0.0.0.0\""       // SKTBIND：带引号的本地监听地址字面量

#define CAT1_UDP_LOCAL_PORT    5000u               // 本地 UDP 端口，与先期联调 SKTBIND 一致

#define CAT1_SKT_CREATE_P1      1u                 // SKTCREATE 第 1 参（利尔达实测）
#define CAT1_SKT_CREATE_P2      2u                 // 第 2 参
#define CAT1_SKT_CREATE_P3      17u                // 第 3 参

#define CAT1_UDP_MAX_UDP_PDU    400u               // 单包 PDU 上限（加密 PKCS5 最多 +15B）；须 ≤SKTSEND 能力

#define RC_UDP_AES_ENABLE       1u                 // 0=明文联调, 1=AES-128-ECB 加密 data field

#define RC_UDP_AES_PLAIN_DATA_MAX  347u            // 明文 data field 上限（含 TLV数据长度 2B）

// AES 密钥（按需修改与联调环境一致）
//#define RC_UDP_AES_KEY_BYTES    { 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, \
//                                  0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu, 0x10u }
#define RC_UDP_AES_KEY_BYTES    { 0x67u, 0x41u, 0x50u, 0x57u, 0x71u, 0x4Eu, 0x56u, 0x69u, \
                                  0x48u, 0x4Cu, 0x52u, 0x78u, 0x32u, 0x73u, 0x6Bu, 0x77u }

// SKTSEND 返回 OK 后继续等待 +SKTRECV 的时间（毫秒）
#define CAT1_UDP_DOWNLINK_WAIT_MS  3000u

// SKTSEND 整行 AT 长度：前缀 + Hex(2×PDU)；供 usart2 / skt_udp 共用
#define CAT1_AT_LINE_MAX        (32u + (CAT1_UDP_MAX_UDP_PDU * 2u))

// 模组 AT 应答拼装缓冲（+SKTCREATE/OK 等，512 足够）
#define CAT1_MODEM_RSP_CAP      512u

#define CAT1_REMOTE_IP_MAX      20u                // 点分 IPv4 + 预留

/** Tag0x10 按键采集默认开关（Tag11 DataID 0x01：1=开启，2=关闭） */
#define REMOTE_KEY_CAPTURE_DEFAULT  2u

#endif /* BSP_CAT1_UDP_CONFIG_H */
