#ifndef RC_PROTOCOL_H
#define RC_PROTOCOL_H

#include <stdint.h>
#include "gap.h"

// RC-STATUS State（MD §3.4）
#define RC_STATE_IDLE   0x00u
#define RC_STATE_READY  0x01u
#define RC_STATE_BUSY   0x02u
#define RC_STATE_DONE   0x03u
#define RC_STATE_ERROR  0x04u

// RC-STATUS Result
#define RC_RESULT_NONE         0x00u
#define RC_RESULT_OK           0x01u
#define RC_RESULT_CRC_ERROR    0x02u
#define RC_RESULT_LEN_ERROR    0x03u
#define RC_RESULT_PARAM_ERROR  0x04u
#define RC_RESULT_DUPLICATE    0x05u
#define RC_RESULT_BUSY_REJECT  0x06u
#define RC_RESULT_NET_ERROR    0x07u

#define RC_STATUS_LEN          14u
#define RC_READ_LEN            61u
#define RC_CONF_LEN            16u

// RC-READ（MD §3.1）字段偏移
#define RC_READ_OFF_DEVICE_ID                    0u
#define RC_READ_OFF_DEVICE_REGION_ID             2u
#define RC_READ_OFF_USER_ID                      8u
#define RC_READ_OFF_DEVICE_MODEL                 12u
#define RC_READ_OFF_DEVICE_SW_VERSION            13u
#define RC_READ_OFF_DEVICE_STATUS                21u
#define RC_READ_OFF_DEVICE_ACTIVE_TIME           22u
#define RC_READ_OFF_DEVICE_INACTIVE_TIME         36u
#define RC_READ_OFF_DEVICE_TRANSMISSION_NETWORK  50u
#define RC_READ_OFF_DEVICE_MAC                   51u
#define RC_READ_OFF_RESERVED                     57u

// RC-READ（MD §3.1）字段长度
#define RC_READ_LEN_DEVICE_ID                    2u
#define RC_READ_LEN_DEVICE_REGION_ID             6u
#define RC_READ_LEN_USER_ID                      4u
#define RC_READ_LEN_DEVICE_MODEL                 1u
#define RC_READ_LEN_DEVICE_SW_VERSION            8u
#define RC_READ_LEN_DEVICE_STATUS                1u
#define RC_READ_LEN_DEVICE_ACTIVE_TIME           14u
#define RC_READ_LEN_DEVICE_INACTIVE_TIME         14u
#define RC_READ_LEN_DEVICE_TRANSMISSION_NETWORK  1u
#define RC_READ_LEN_DEVICE_MAC                   6u
#define RC_READ_LEN_RESERVED                     4u

// RC-READ 默认字段值
#define RC_READ_VAL_DEVICE_ID                      "RC"               //设备编码
#define RC_READ_VAL_DEVICE_REGION_ID               "110108"           //行政区划码，符合 GB/T 2260 北京海淀
#define RC_READ_VAL_USER_ID                        "0001"             //用户编码
#define RC_READ_VAL_DEVICE_MODEL                   0x01u              //0x01=语音遥控器, 0x02=4G 模块
#define RC_READ_VAL_DEVICE_SW_VERSION              "02010000"         //软件版本，如 `01020304` 版本 v1.2.3.4
#define RC_READ_VAL_DEVICE_STATUS                  0x00u              //0x00=正常, 0x01=失效, 0x02=未知
#define RC_READ_VAL_DEVICE_ACTIVE_TIME             "20260101100800"   // YYYYMMDDhhmmss，无值填全 0
#define RC_READ_VAL_DEVICE_INACTIVE_TIME           "20360101100800"   // YYYYMMDDhhmmss，无值填全 0
#define RC_READ_VAL_DEVICE_TRANSMISSION_NETWORK    0x00u              //0x00=广电, 0x01=电信, 0x02=联通, 0x03=移动, 0x04=其他

/* Tag0x01（PDF）与 RC-READ（MD §3.1）复用字段：改 RC_READ_VAL_* / rc_read_build 即可同步 BLE 与 UDP */

#define RC_KEY_NOTIFY_LEN      4u

#define RC_TRANS_MAGIC         0x55AAu
#define RC_TRANS_FRAME_MAX     128u
#define RC_TRANS_PAYLOAD_MAX   112u
// 单大包最大子包数：ASSEMBLY_MAX/PAYLOAD_MAX
#define RC_TRANS_ASSEMBLY_MAX  560u
#define RC_TRANS_MAX_SUBPACK   5u
// 半包汇聚超时（ms）：无新子包到达则 reset，避免拖死后续大包
#define RC_TRANS_ASM_TIMEOUT_MS  20000u

#define RC_PACK_INDEX_NONE     0xFFFFFFFFu

typedef struct {
    uint8_t  state;
    uint8_t  result;
    uint8_t  retry_count;
    uint8_t  reserved1;
    uint32_t pack_index;
    uint8_t  cloud_policy[6];
} rc_status_t;

typedef struct {
    uint8_t  max_retry;
    uint16_t retry_delay_ms;
    uint16_t sleep_delay_sec;
    uint8_t  max_packet_size;
} rc_conf_t;

#endif /* RC_PROTOCOL_H */
