/**
 * @file cat1_udp_config.h
 * @brief CAT1 模组 UDP ECSO* 远端参数（NT26 方案 B，按需修改与联调环境一致）。
 *
 * 《5G遥控器数据采集接口文档》中的业务 Tag 由 STB 经 RC-TRANS 下发；
 * remote_reports 将 RC-TRANS Payload 透传封装为 0x6868 帧。
 */
#ifndef BSP_CAT1_UDP_CONFIG_H
#define BSP_CAT1_UDP_CONFIG_H

#if 0   /* 0=自己 106.53.179.53，1=赛科 120.48.54.123 */
#define CAT1_UDP_REMOTE_HOST   "120.48.54.123"     // 服务器地址 赛科
#else
#define CAT1_UDP_REMOTE_HOST   "106.53.179.53"     // 服务器地址 自己
#endif

#define CAT1_UDP_REMOTE_PORT   8888u               // 数据采集 0x6868 + Tag11
#define CAT1_OTA_REMOTE_PORT   8889u               // OTA 0x41 拉片

#define CAT1_ECSO_SOCKET_ID     0u                 // ECSOCR/ECSOCL/ECSOST 使用的 socket id
#define CAT1_ECSORF_READ_MAX    1024u              // AT+ECSORF 单次读取上限（手册 1–1400）
#define CAT1_ECSONMI_LINE_MAX   32u                // +ECSONMI URC 拼行（与 AT/ECSORF 缓冲分离）

/** ECSORF 仅回 OK 无数据行时，短延时重读次数（+ECSONMI 仍有效，不清 pending） */
#define CAT1_ECSORF_EMPTY_RETRY_MAX  5u
/** 解析 mismatch（半包/脏行）后最多再试次数；再空则放弃，勿再走 empty 满次 */
#define CAT1_ECSORF_MISMATCH_RETRY_MAX  1u
/** 收到 +ECSONMI 后首次 AT+ECSORF 前等待（毫秒）；0=立即读 */
#define CAT1_ECSORF_FIRST_DELAY_MS   0u
/** 每次 ECSORF 空 OK 重读前等待（毫秒），给模组 socket buffer 就绪时间 */
#define CAT1_ECSORF_RETRY_DELAY_MS   80u
/** ECSORF 解析失败时打印应答行前缀长度 */
#define CAT1_ECSORF_PARSE_DIAG_HEAD  64u

#define CAT1_UDP_MAX_UDP_PDU    608u /* 560B plain + 16B PKCS padding + 21B framing = 597B. */

#define RC_UDP_AES_ENABLE       1u                 // 0=明文联调, 1=AES-128-ECB 加密 data field

#define RC_UDP_AES_PLAIN_DATA_MAX  560u            // 与 RC_TRANS_ASSEMBLY_MAX 一致（含 TLV数据长度 2B）

#if CAT1_UDP_MAX_UDP_PDU < ((RC_UDP_AES_PLAIN_DATA_MAX / 16u + 1u) * 16u + 21u)
#error "UDP PDU must include the complete PKCS padding and 6868 frame header"
#endif

// AES 密钥（按需修改与联调环境一致）
//#define RC_UDP_AES_KEY_BYTES    { 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, \
//                                  0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu, 0x10u }
#define RC_UDP_AES_KEY_BYTES    { 0x67u, 0x41u, 0x50u, 0x57u, 0x71u, 0x4Eu, 0x56u, 0x69u, \
                                  0x48u, 0x4Cu, 0x52u, 0x78u, 0x32u, 0x73u, 0x6Bu, 0x77u }

// ECSOST 返回 OK 后继续等待 +ECSONMI/ECSORF 的时间（毫秒）
#define CAT1_UDP_DOWNLINK_WAIT_MS  1000u

// OTA 0x41 上行 ECSOST 不等待（代码片异步 +ECSONMI 到达）
#define CAT1_OTA_UPLINK_WAIT_MS    0u

// OTA 下行最大 547B（22B 头 + 512B 代码）
#define CAT1_OTA_MAX_PDU          547u
#define CAT1_OTA_ECSORF_LINE_MAX  CAT1_OTA_MAX_PDU
								  
#define CAT1_OTA_AT_TOTAL_WAIT_MS  3000u
#define CAT1_OTA_CHUNK_RETRY_MS    CAT1_OTA_AT_TOTAL_WAIT_MS

/** 同一 req_seq 连续重试超过此次数则 PAUSE 写断点，随后直接发 0x41 续传 */
#define CAT1_OTA_CHUNK_RETRY_MAX    5u

// ECSOST 整行 AT 长度：前缀 + Hex(2×PDU)；供 usart2 / skt_udp 共用
#define CAT1_AT_LINE_MAX        (48u + (CAT1_UDP_MAX_UDP_PDU * 2u))

/** USART2 TX 等 TXDE 最大空转次数（≈数 ms）；超时返回失败避免整 task 卡死 */
#define CAT1_UART_TX_SPIN_MAX   60000u

/** USART2 RX DMA 环：1 条 AT 行长 + 256B 余量（省 1KB；须 ATE0 关回显） */
#define CAT1_UART_RX_RING_CAP   (CAT1_AT_LINE_MAX + 256u)

// 模组 AT 应答拼装缓冲（setup 诊断/ECSOCR；不含 ECSTATUS=PHY）
#define CAT1_MODEM_RSP_CAP      128u

#define CAT1_REMOTE_IP_MAX      20u                // 点分 IPv4 + 预留


/* ====== Tag0x10 按键采集上报编译开关 ====== */
/**
 * 1 = 开启遥控器自身 Tag10 按键采集上报功能，受服务器 Tag11 (DataID 0x01) 控制
 * 0 = 关闭 Tag10 按键采集上报功能，忽略服务器 Tag11 控制，整条上报链路关闭
 */
#define RC_TAG10_SELF_REPORT_ENABLE  0u   /* 按需改为 0 关闭 */

/** Tag0x10 按键采集默认开关（Tag11 DataID 0x01：1=开启，2=关闭） */
#define REMOTE_KEY_CAPTURE_DEFAULT  2u

/** 连续业务上行 ECSOST 失败（含 Tag10/Tag01/RC-TRANS）达到此次数后触发 UDP 软重建 */
#define CAT1_UDP_REBUILD_FAIL_THRESH  3u

/** 业务上行后连续下行空等（无 Tag11）达到此次数后触发 UDP 软重建 */
#define CAT1_UDP_SILENT_REBUILD_THRESH  3u

/** setup：CGATT=0 时两次 AT+CGATT? 的间隔（毫秒） */
#define CAT1_CGATT_POLL_INTERVAL_MS   2000u
/** setup：单次流程内等待附着的最长总时间（毫秒） */
#define CAT1_CGATT_WAIT_TOTAL_MS     60000u

/**
 * 业务 ECSOST 整笔 AT 事务总超时（含等 OK + 等下行窗口）。
 * 须明显大于 CAT1_UDP_DOWNLINK_WAIT_MS，避免 WAIT_OK 阶段过早超时。
 */
#define CAT1_AT_TOTAL_WAIT_MS         4000u

/** OTA 下载无片进度超过此时间则强制 PAUSE，随后本地直接发 0x41 续传 */
#define UDP_OTA_STALL_PAUSE_MS        10000u    //10s

/** PAUSE 后等待此时间再自动 RESUME 发 0x41（避免秒级 PAUSE/RESUME 死循环） */
#define UDP_OTA_RESUME_BACKOFF_MS     10000u    //10s

/**
 * 业务收发热路径日志（ECSOST/ECSONMI/ECSORF 细节、UDP6868、send/recv OK）。
 * 0=关闭以减轻 debug 串口占用（利于 ECSORF 收包）；1=联调打开。
 * CME/timeout/rebuild 等错误日志不受此开关影响。
 */
#define CAT1_UDP_TXRX_LOG_ENABLE      1u

#if CAT1_UDP_TXRX_LOG_ENABLE
    #include "co_printf.h"
    #define CAT1_UDP_LOG(fmt, ...)   co_printf(fmt, ##__VA_ARGS__)
#else
    #define CAT1_UDP_LOG(fmt, ...)   ((void)0)
#endif

/** 1=0x41 下行 parse fail 时打印 hex（联调完改 0） */
#define OTA_DEBUG_DUMP_RX             1u
/** 1=首次 uplink req_seq=1 发送前打印 hex（联调完改 0） */
#define OTA_DEBUG_DUMP_TX             1u
/** 1=打印整帧；0=仅 head(34B)+tail(CRC+0x16) */
#define OTA_DEBUG_DUMP_FULL           1u
#define OTA_DEBUG_DUMP_HEAD_BYTES     34u

#endif /* BSP_CAT1_UDP_CONFIG_H */
