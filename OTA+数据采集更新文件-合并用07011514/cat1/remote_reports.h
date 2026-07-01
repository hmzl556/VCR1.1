/**
 * @file remote_reports.h
 * @brief UDP 上报：STB TLV 透传 + RC 本地直报 Tag0x01/0x10。
 */
#ifndef BSP_REMOTE_REPORTS_H
#define BSP_REMOTE_REPORTS_H

#include "cat1_udp_config.h"
#include "rc_protocol.h"
#include <stddef.h>
#include <stdint.h>

/** 初始化远程报告 */
void remote_reports_init(void);

/** 从发送队列取出一帧 0x6868 报文；返回 1 表示队列空 */
int remote_report_poll(uint8_t *out, size_t buflen, uint16_t *outlen,
                       uint8_t *out_rc_trans_notify, uint32_t *out_pack_index);

/**
 * @brief  将 RC-TRANS Payload（明文 data field）封装为 0x6868 帧并入队
 * @param  plain      TLV数据长度[2 BE] + Tag[1] + ValueLen[2 BE] + Value[n]
 * @param  plain_len  Payload 总长度（由 STB 提供，含 TLV数据长度 2B）
 * @return 0 成功；-1 失败
 */
/**
 * @param  rc_trans_pack_index  RC-TRANS 汇聚包序号；非 RC_PACK_INDEX_NONE 时 UDP 完成后通知 STB
 */
int remote_queue_tlv_blob(const uint8_t *plain, uint16_t plain_len,
                          uint32_t rc_trans_pack_index);

/** RC 本地直报 Tag0x01 终端状态（开机/UDP 就绪后调用，不经 STB） */
int remote_queue_tag01_status(void);

/** RC 本地直报 Tag0x10 按键（不经 STB/RC-TRANS） */
int remote_queue_tag10_key(uint8_t key_code);

/**
 * @brief  解析云端 UDP 下行 0x6868 帧中的 Tag0x11，更新 CloudUploadPolicy
 * @param  cloud_policy  入参为当前策略；出参写入 UploadIntervalSec(4B BE) + UploadItemCount(2B BE)
 * @return 0 Tag0x11 解析成功；-1 失败
 */
int remote_handle_downlink_frame(const uint8_t *frame, uint16_t frame_len, uint8_t cloud_policy[6]);

/** Tag11 DataID 0x01：1=开启 Tag0x10 按键采集，2=关闭 */
int remote_is_key_capture_enabled(void);

/** 当前 UDP 发送队列占用深度 */
uint8_t remote_report_queue_count(void);

#endif /* BSP_REMOTE_REPORTS_H */
