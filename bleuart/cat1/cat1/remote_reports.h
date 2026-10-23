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

/** 发送队列返回值（remote_queue_tlv_blob / remote_queue_tag10_key） */
#define REMOTE_QUEUE_OK           0
#define REMOTE_QUEUE_ERR         -1   /**< 参数/组帧错误 */
#define REMOTE_QUEUE_PARAM       -3   /**< Malformed TLV/date; report PARAM_ERROR to STB */
#define REMOTE_QUEUE_FULL        -2   /**< 队列满，设备忙 */

/* Normalize known RC-READ date truncation in owned RC-TRANS data; update length. */
int remote_normalize_tlv_blob(uint8_t *plain, uint16_t *length);

/** 初始化远程报告 */
void remote_reports_init(void);
void remote_report_clear_queue(void);

/** 从发送队列取出一帧 0x6868 报文；返回 1 表示队列空 */
int remote_report_poll(uint8_t *out, size_t buflen, uint16_t *outlen,
                       uint8_t *out_rc_trans_notify, uint32_t *out_pack_index);

/**
 * @brief  将 RC-TRANS Payload（明文 data field）封装为 0x6868 帧并入队
 * @param  plain      TLV数据长度[2 BE] + Tag[1] + ValueLen[2 BE] + Value[n]
 * @param  plain_len  Payload 总长度（由 STB 提供，含 TLV数据长度 2B）
 * @param  rc_trans_pack_index  RC-TRANS 汇聚包序号；非 RC_PACK_INDEX_NONE 时 UDP 完成后通知 STB
 * @note   有 pack_index 的 RC-TRANS 优先：队列满时丢弃排队中的 Tag0x10 再入队
 * @return REMOTE_QUEUE_OK 成功；REMOTE_QUEUE_ERR 参数/组帧失败；REMOTE_QUEUE_FULL 队列满
 */
int remote_queue_tlv_blob(const uint8_t *plain, uint16_t plain_len,
                          uint32_t rc_trans_pack_index);

/** RC 本地直报 Tag0x01 终端状态（开机/UDP 就绪后调用，不经 STB） */
int remote_queue_tag01_status(void);

/** RC 本地直报 Tag0x10 按键（不经 STB/RC-TRANS）；返回值见 REMOTE_QUEUE_* */
int remote_queue_tag10_key(uint8_t key_code);

/**
 * @brief  心跳直报 Tag0x0A（时间+SIM卡号+电量, heartbeat 模块到期触发）
 * @return REMOTE_QUEUE_OK 成功；REMOTE_QUEUE_ERR 组帧失败；REMOTE_QUEUE_FULL 队列满
 */
int remote_queue_tag0A_heartbeat(void);

/** UDP 软重建期间暂停/恢复 Tag10 入队（1=暂停，0=恢复） */
void remote_tag10_enqueue_set_paused(uint8_t paused);

/** @return 1 正在暂停 Tag10 入队 */
uint8_t remote_tag10_enqueue_is_paused(void);

/** 丢弃发送队列中尚未发出的 Tag0x10；返回丢弃帧数 */
uint8_t remote_report_drop_all_tag10(void);

/**
 * @brief  解析云端 UDP 下行 0x6868 帧中的 Tag0x11，更新 CloudUploadPolicy
 * @param  cloud_policy  入参为当前策略；出参写入 UploadIntervalSec(4B BE) + UploadItemCount(2B BE)
 * @return 0 Tag0x11 解析成功；-1 失败
 */
int remote_handle_downlink_frame(const uint8_t *frame, uint16_t frame_len, uint8_t cloud_policy[6]);

/** Tag11 DataID 0x01：1=开启 Tag0x10 按键采集，2=关闭 */
int remote_is_key_capture_enabled(void);

/** 设置按键采集开关（1=开启，2=关闭，与 Tag11 DataID 0x01 一致） */
void remote_set_key_capture_enable(uint8_t enable);

/** BLE 断连钩子：保留 Tag11/Flash 下发的按键采集状态，不断连清零 */
void remote_key_capture_on_ble_disconnect(void);

/** 当前 UDP 发送队列占用深度 */
uint8_t remote_report_queue_count(void);

/**
 * @brief  从发送队列剔除所有 RC-TRANS 帧（rc_trans_notify!=0），保留 Tag01/10 等
 * @param  packs_out  被剔除的 PackIndex 列表
 * @param  packs_cap  packs_out 容量
 * @return 剔除帧数（写入 packs_out 的个数，不超过 packs_cap）
 */
uint8_t remote_report_abort_rc_trans(uint32_t *packs_out, uint8_t packs_cap);

/** 读取本机软件版本 BCD（与 Tag0x01 DataID 0x02 一致）；0 成功，-1 失败 */
int remote_get_local_sw_version_bcd(uint16_t *out_ver);

#endif /* BSP_REMOTE_REPORTS_H */
