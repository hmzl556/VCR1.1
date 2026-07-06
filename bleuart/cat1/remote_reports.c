/**
 * @file remote_reports.c
 * @brief UDP 上报：STB TLV 透传 + RC 本地直报 Tag0x01/0x10。
 */
#include "remote_reports.h"
#include "aes_transport.h"
#include "ble_rc_svc.h"
#include "rc_protocol.h"
#include "udp_ota.h"
#include "br1601x.h"
#include "co_printf.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#ifndef REMOTE_REPORT_FIXED_TIME_BCD
#define REMOTE_REPORT_FIXED_TIME_BCD  { 0x26u, 0x06u, 0x09u, 0x10u, 0x08u, 0x00u }
#endif

#ifndef REMOTE_REPORT_QUEUE_DEPTH // 发送队列深度，避免队列溢出
#define REMOTE_REPORT_QUEUE_DEPTH 2u
#endif

typedef struct
{
    uint8_t buf[CAT1_UDP_MAX_UDP_PDU];
    uint16_t len;
    uint8_t rc_trans_notify;
    uint32_t pack_index;
} remote_frame_t;

static remote_frame_t s_queue[REMOTE_REPORT_QUEUE_DEPTH];
static uint8_t s_q_head;
static uint8_t s_q_tail;
static uint8_t s_q_count;
static uint16_t s_next_mid = 1u;
static uint8_t s_key_capture_enable = REMOTE_KEY_CAPTURE_DEFAULT;
/** BLE/RC 会话本地开启采集后，忽略云端 Tag11 0x01=2 关闭指令 */
static uint8_t s_key_capture_hold;
static const uint8_t s_default_time_bcd[6] = REMOTE_REPORT_FIXED_TIME_BCD;
static uint8_t s_tlv_frame[CAT1_UDP_MAX_UDP_PDU];

uint8_t remote_report_queue_count(void);

/**
 * @brief  将 RC-READ 中连续 ASCII 数字串转为 BCD（每 2 字符 1 字节）
 */
static int rc_ascii_digits_to_bcd(const uint8_t *ascii, uint8_t ascii_len, uint8_t *bcd_out, uint8_t bcd_len)
{
    uint8_t i;

    if (ascii == NULL || bcd_out == NULL || (uint8_t)(ascii_len / 2u) < bcd_len)
    {
        return -1;
    }
    for (i = 0u; i < bcd_len; i++)
    {
        uint8_t hi = ascii[(uint8_t)(i * 2u)];
        uint8_t lo = ascii[(uint8_t)(i * 2u + 1u)];

        if (hi < (uint8_t)'0' || hi > (uint8_t)'9' || lo < (uint8_t)'0' || lo > (uint8_t)'9')
        {
            return -1;
        }
        bcd_out[i] = (uint8_t)(((hi - (uint8_t)'0') << 4) | (lo - (uint8_t)'0'));
    }
    return 0;
}

/**
 * @brief  将 RC-READ 的 YYYYMMDDhhmmss ASCII（14 字符）转为 Tag0x01 的 6 字节 BCD（YYMMDDhhmmss）
 */
static int rc_read_active_time_to_bcd6(const uint8_t *rc_read, uint8_t out[6])
{
    if (rc_read == NULL || out == NULL)
    {
        return -1;
    }
    return rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_ACTIVE_TIME + 2u,
                                  12u, out, 6u);
}

/**
 * @brief  读取芯片 MAC 填入 0x6868 帧终端编码字段
 */
static void fill_default_mac(uint8_t out[6])
{
    const uint8_t *mac = SystemGetMacAddr();

    if (mac != NULL)
    {
        memcpy(out, mac, 6u);
        return;
    }
    memset(out, 0, 6u);
}

/**
 * @brief  Value 区内追加 DataID + Value（PDF 内嵌字段，无 per-field 长度）
 * @return 0 成功；-1 缓冲区不足或参数非法
 */
static int remote_value_append(uint8_t *buf, uint16_t *len, uint16_t max_len,
                               uint8_t data_id, const uint8_t *value, uint8_t value_len)
{
    if (buf == NULL || len == NULL || value == NULL)
    {
        return -1;
    }
    if ((uint16_t)(*len + 1u + (uint16_t)value_len) > max_len)
    {
        return -1;
    }
    buf[*len] = data_id;
    *len = (uint16_t)(*len + 1u);
    memcpy(buf + *len, value, value_len);
    *len = (uint16_t)(*len + value_len);
    return 0;
}

/**
 * @brief  从 RC-READ 组装 Tag0x01 Value（与 BLE RC-READ 复用，见 PDF↔MD 字段映射）
 * @note   DataID0←DeviceRegionId(6B ASCII→3B BCD)；1←DeviceModel；
 *         2←DeviceSWVersion(前 4 位 ASCII→2B BCD)；3←DeviceStatus；
 *         4←DeviceActiveTime(14B ASCII→6B BCD)；5←DeviceInactiveTime；
 *         6←DeviceTransmissionNetwork。默认值改 rc_protocol.h 的 RC_READ_VAL_*。
 * @return 0 成功；-1 失败
 */
static int remote_build_tag01_value(uint8_t *value, uint16_t *value_len)
{
    uint16_t len = 0u;
    const uint8_t *rc_read = ble_rc_svc_get_read();
    uint8_t region[3];
    uint8_t sw[2];
    uint8_t active[6];
    uint8_t inactive[6];
    uint8_t model;
    uint8_t status;
    uint8_t network;

    if (value == NULL || value_len == NULL || rc_read == NULL)
    {
        return -1;
    }
    if (rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_REGION_ID,
                               RC_READ_LEN_DEVICE_REGION_ID, region, 3u) != 0)
    {
        return -1;
    }
    if (rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_SW_VERSION, 4u, sw, 2u) != 0)
    {
        return -1;
    }
    if (rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_ACTIVE_TIME + 2u, 12u, active, 6u) != 0)
    {
        return -1;
    }
    if (rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_INACTIVE_TIME + 2u, 12u, inactive, 6u) != 0)
    {
        return -1;
    }
    model = rc_read[RC_READ_OFF_DEVICE_MODEL];
    status = rc_read[RC_READ_OFF_DEVICE_STATUS];
    network = rc_read[RC_READ_OFF_DEVICE_TRANSMISSION_NETWORK];

    /* Value 固定 27B（7×DataID+Value） */
    if (remote_value_append(value, &len, 27u, 0x00u, region, 3u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x01u, &model, 1u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x02u, sw, 2u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x03u, &status, 1u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x04u, active, 6u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x05u, inactive, 6u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 27u, 0x06u, &network, 1u) != 0)
    {
        return -1;
    }
    if (len != 27u)
    {
        return -1;
    }
    *value_len = len;
    return 0;
}

/**
 * @brief  组装 Tag0x10 按键 Value（操作时间取自 RC-READ DeviceActiveTime）
 * @return 0 成功；-1 失败
 */
static int remote_build_tag10_value(uint8_t *value, uint16_t *value_len, uint8_t key_code)
{
    uint16_t len = 0u;
    uint8_t active[6];

    if (value == NULL || value_len == NULL || key_code == 0u)
    {
        return -1;
    }
    if (rc_read_active_time_to_bcd6(ble_rc_svc_get_read(), active) != 0)
    {
        memcpy(active, s_default_time_bcd, 6u);
    }
    /* Value 固定 9B（DataID0+时间6B + DataID1+键码1B） */
    if (remote_value_append(value, &len, 9u, 0x00u, active, 6u) != 0)
    {
        return -1;
    }
    if (remote_value_append(value, &len, 9u, 0x01u, &key_code, 1u) != 0)
    {
        return -1;
    }
    if (len != 9u)
    {
        return -1;
    }
    *value_len = len;
    return 0;
}

/**
 * @brief  本地直报：按 RC-TRANS Payload 格式拼明文 data field 并入队
 * @note   TLV数据长度 2B 由 STB 在 RC-TRANS 中提供；本地路径按同格式写入
 */
static int remote_queue_tag_tlv(uint8_t tag, const uint8_t *value, uint16_t value_len)
{
    uint8_t plain[RC_UDP_AES_PLAIN_DATA_MAX];
    uint16_t plain_len;

    if (value == NULL || value_len == 0u)
    {
        return -1;
    }
    plain_len = (uint16_t)(value_len + 5u);
    if (plain_len > RC_UDP_AES_PLAIN_DATA_MAX)
    {
        return -1;
    }
    plain[0] = (uint8_t)((plain_len >> 8) & 0xFFu);
    plain[1] = (uint8_t)(plain_len & 0xFFu);
    plain[2] = tag;
    plain[3] = (uint8_t)((value_len >> 8) & 0xFFu);
    plain[4] = (uint8_t)(value_len & 0xFFu);
    memcpy(plain + 5u, value, value_len);
    return remote_queue_tlv_blob(plain, plain_len, RC_PACK_INDEX_NONE);
}

/**
 * @brief  读取本机软件版本 BCD（RC-READ 前 4 位 ASCII，与 Tag0x01 DataID 0x02 一致）
 * @return 0 成功；-1 失败
 */
int remote_get_local_sw_version_bcd(uint16_t *out_ver)
{
    const uint8_t *rc_read;
    uint8_t sw[2];

    if (out_ver == NULL)
    {
        return -1;
    }
    rc_read = ble_rc_svc_get_read();
    if (rc_read == NULL)
    {
        return -1;
    }
    if (rc_ascii_digits_to_bcd(rc_read + RC_READ_OFF_DEVICE_SW_VERSION, 4u, sw, 2u) != 0)
    {
        return -1;
    }
    *out_ver = (uint16_t)(((uint16_t)sw[0] << 8) | sw[1]);
    return 0;
}

/**
 * @brief  RC 本地直报 Tag0x01 终端状态并入 UDP 发送队列
 * @return 0 成功；-1 失败
 */
int remote_queue_tag01_status(void)
{
    uint8_t value[27u];
    uint16_t value_len;

    if (remote_build_tag01_value(value, &value_len) != 0)
    {
        return -1;
    }
    return remote_queue_tag_tlv(0x01u, value, value_len);
}

/**
 * @brief  RC 本地直报 Tag0x10 按键并入 UDP 发送队列
 * @param  key_code  HID usage 低字节，0 无效
 * @return 0 成功；-1 失败；-2 队列满
 */
int remote_queue_tag10_key(uint8_t key_code)
{
    uint8_t value[9u];
    uint16_t value_len;
    int build_rc;
    int push_rc;

    build_rc = remote_build_tag10_value(value, &value_len, key_code);
    if (build_rc != 0)
    {
        co_printf("[RC   ] Tag10 build fail, key=%u\r\n", (unsigned)key_code);
        return -1;
    }
    push_rc = remote_queue_tag_tlv(0x10u, value, value_len);
    if (push_rc != 0)
    {
        return -2;
    }
    return 0;
}

#define REMOTE_FRAME_FIXED_TAIL  21u   // 0x6868 头至 data field 前 19B + 校验 1B + 0x16 1B 中的「头+尾固定」

/**
 * @brief  从 0x6868 帧取出明文 data field（加密时先解密）
 * @return 0 成功；-1 失败
 */
static int remote_extract_plain_data_field(const uint8_t *frame, uint16_t frame_len,
                                           uint8_t *plain, uint16_t plain_cap, uint16_t *plain_len)
{
    uint16_t wire_len;
    if (frame == NULL || plain == NULL || plain_len == NULL || frame_len < REMOTE_FRAME_FIXED_TAIL)
    {
        return -1;
    }
    wire_len = (uint16_t)(((uint16_t)frame[17] << 8) | frame[18]);
    if ((uint16_t)(19u + wire_len + 2u) > frame_len || wire_len == 0u)
    {
        return -1;
    }
#if RC_UDP_AES_ENABLE
    return rc_udp_aes_decrypt_data_field(frame + 19u, wire_len, plain, plain_cap, plain_len);
#else
    if (wire_len > plain_cap)
    {
        return -1;
    }
    memcpy(plain, frame + 19u, wire_len);
    *plain_len = wire_len;
    return 0;
#endif
}

/**
 * @brief  将 RC-TRANS Payload（明文 data field）套入 0x6868 外层帧
 * @note   plain 含 TLV数据长度 2B，由 STB 提供；本函数不再计算或改写
 * @return 0 成功；-1 失败
 */
static int remote_frame_build(const uint8_t *plain, uint16_t plain_len,
                              uint8_t *out, size_t buflen, uint16_t *outlen)
{
    uint16_t wire_len;
    uint8_t *p;
    uint8_t checksum;
    uint16_t mid;
    uint16_t i;
    if (out == NULL || outlen == NULL || plain == NULL)
    {
        return -1;
    }
    if (plain_len < 5u || plain_len > RC_UDP_AES_PLAIN_DATA_MAX)
    {
        return -1;
    }
#if RC_UDP_AES_ENABLE
    {
        uint16_t cipher_len;
        const size_t hdr_len = 19u;

        if ((size_t)(hdr_len + 2u) > buflen)
        {
            return -1;
        }
        if (rc_udp_aes_encrypt_data_field(plain, plain_len, out + hdr_len,
                                          (uint16_t)(buflen - hdr_len), &cipher_len) != 0)
        {
            return -1;
        }
        wire_len = cipher_len;
        if ((size_t)(hdr_len + wire_len + 2u) > buflen)
        {
            return -1;
        }
        p = out;
        *p++ = 0x68u;
        *p++ = 0x68u;
        *p++ = 0x10u;
        memcpy(p, s_default_time_bcd, 6u);
        p += 6u;
        mid = s_next_mid;
        s_next_mid = (s_next_mid == 0xFFFFu) ? 0u : (uint16_t)(s_next_mid + 1u);
        *p++ = (uint8_t)((mid >> 8) & 0xFFu);
        *p++ = (uint8_t)(mid & 0xFFu);
        fill_default_mac(p);
        p += 6u;
        *p++ = (uint8_t)((wire_len >> 8) & 0xFFu);
        *p++ = (uint8_t)(wire_len & 0xFFu);
        p += wire_len;
    }
#else
    wire_len = plain_len;
    if ((size_t)(19u + wire_len + 2u) > buflen)
    {
        return -1;
    }
    p = out;
    *p++ = 0x68u;
    *p++ = 0x68u;
    *p++ = 0x10u;
    memcpy(p, s_default_time_bcd, 6u);
    p += 6u;
    mid = s_next_mid;
    s_next_mid = (s_next_mid == 0xFFFFu) ? 0u : (uint16_t)(s_next_mid + 1u);
    *p++ = (uint8_t)((mid >> 8) & 0xFFu);
    *p++ = (uint8_t)(mid & 0xFFu);
    fill_default_mac(p);
    p += 6u;
    *p++ = (uint8_t)((wire_len >> 8) & 0xFFu);
    *p++ = (uint8_t)(wire_len & 0xFFu);
    memcpy(p, plain, wire_len);
    p += wire_len;
#endif
    checksum = 0u;
    for (i = 2u; i < (uint16_t)(p - out); i++)
    {
        checksum = (uint8_t)(checksum + out[i]);
    }
    *p++ = checksum;
    *p++ = 0x16u;
    *outlen = (uint16_t)(p - out);
    return 0;
}

/**
 * @brief  将完整 0x6868 帧写入环形发送队列
 * @return 0 成功；-1 队列满或参数非法
 */
static int remote_queue_push(const uint8_t *frame, uint16_t len,
                             uint8_t rc_trans_notify, uint32_t pack_index)
{
    int rc = -1;

    if (frame == NULL || len == 0u || len > CAT1_UDP_MAX_UDP_PDU)
    {
        return -1;
    }

    taskENTER_CRITICAL();
    if (s_q_count < REMOTE_REPORT_QUEUE_DEPTH)
    {
        memcpy(s_queue[s_q_tail].buf, frame, len);
        s_queue[s_q_tail].len = len;
        s_queue[s_q_tail].rc_trans_notify = rc_trans_notify;
        s_queue[s_q_tail].pack_index = pack_index;
        s_q_tail = (uint8_t)((s_q_tail + 1u) % REMOTE_REPORT_QUEUE_DEPTH);
        s_q_count++;
        rc = 0;
    }
    taskEXIT_CRITICAL();
    return rc;
}

/**
 * @brief  将 RC-TRANS Payload（明文 data field）封装为 0x6868 帧并入队
 * @param  plain      TLV数据长度[2 BE] + Tag[1] + ValueLen[2 BE] + Value[n]
 * @param  plain_len  Payload 总长度（由 STB 提供，含 TLV数据长度 2B）
 * @return 0 成功；-1 失败
 */
int remote_queue_tlv_blob(const uint8_t *plain, uint16_t plain_len,
                          uint32_t rc_trans_pack_index)
{
    uint16_t frame_len;
    uint8_t notify;

    if (plain == NULL || plain_len < 5u || plain_len > RC_UDP_AES_PLAIN_DATA_MAX)
    {
        return -1;
    }
    if (remote_frame_build(plain, plain_len, s_tlv_frame, sizeof(s_tlv_frame),
                           &frame_len) != 0)
    {
        return -1;
    }
    notify = (rc_trans_pack_index != RC_PACK_INDEX_NONE) ? 1u : 0u;
    return remote_queue_push(s_tlv_frame, frame_len, notify, rc_trans_pack_index);
}

/**
 * @brief  初始化发送队列与 MID 计数器
 */
void remote_reports_init(void)
{
    s_q_head = 0u;
    s_q_tail = 0u;
    s_q_count = 0u;
    s_next_mid = 1u;
    s_key_capture_enable = REMOTE_KEY_CAPTURE_DEFAULT;
    s_key_capture_hold = 0u;
    (void)rc_udp_aes_init();
    co_printf("[RC   ] Tag10 key capture default=%s (Tag11 0x01=1 to enable)\r\n",
              (s_key_capture_enable == 1u) ? "ON" : "OFF");
}

/**
 * @brief  获取当前按键捕获使能状态
 * @return 1 使能；0 禁止
 */
int remote_is_key_capture_enabled(void)
{
    int enabled;

    taskENTER_CRITICAL();
    enabled = (s_key_capture_enable == 1u) ? 1 : 0;
    taskEXIT_CRITICAL();
    return enabled;
}

void remote_set_key_capture_enable(uint8_t enable)
{
    if (enable != 1u && enable != 2u)
    {
        return;
    }
    taskENTER_CRITICAL();
    s_key_capture_enable = enable;
    if (enable == 1u)
    {
        s_key_capture_hold = 1u;
    }
    else
    {
        s_key_capture_hold = 0u;
    }
    taskEXIT_CRITICAL();
    co_printf("[RC   ] Tag10 key capture %s (Tag11 0x01=%u)\r\n",
              (enable == 1u) ? "ON" : "OFF", (unsigned)enable);
}

void remote_key_capture_on_ble_disconnect(void)
{
    taskENTER_CRITICAL();
    s_key_capture_hold = 0u;
    s_key_capture_enable = REMOTE_KEY_CAPTURE_DEFAULT;
    taskEXIT_CRITICAL();
    co_printf("[RC   ] Tag10 key capture reset on BLE disconnect\r\n");
}

/**
 * @brief  获取当前发送队列中待发送帧的数量
 * @return 待发送帧的数量
 */
uint8_t remote_report_queue_count(void)
{
    uint8_t count;

    taskENTER_CRITICAL();
    count = s_q_count;
    taskEXIT_CRITICAL();
    return count;
}

/**
 * @brief  从发送队列取出一帧待 UDP 发送的 0x6868 报文
 * @return 0 成功；1 队列空；-1 失败
 */
int remote_report_poll(uint8_t *out, size_t buflen, uint16_t *outlen,
                       uint8_t *out_rc_trans_notify, uint32_t *out_pack_index)
{
    int rc = 1;

    if (out == NULL || outlen == NULL)
    {
        return -1;
    }

    taskENTER_CRITICAL();
    if (s_q_count == 0u)
    {
        *outlen = 0u;
    }
    else if (s_queue[s_q_head].len > buflen)
    {
        rc = -1;
    }
    else
    {
        memcpy(out, s_queue[s_q_head].buf, s_queue[s_q_head].len);
        *outlen = s_queue[s_q_head].len;
        if (out_rc_trans_notify != NULL)
        {
            *out_rc_trans_notify = s_queue[s_q_head].rc_trans_notify;
        }
        if (out_pack_index != NULL)
        {
            *out_pack_index = s_queue[s_q_head].pack_index;
        }
        s_q_head = (uint8_t)((s_q_head + 1u) % REMOTE_REPORT_QUEUE_DEPTH);
        s_q_count--;
        rc = 0;
    }
    taskEXIT_CRITICAL();
    return rc;
}

#define REMOTE_TAG_11  0x11u

/**
 * @brief  将 UploadIntervalSec 写入 CloudUploadPolicy 前 4 字节（大端）
 */
static void remote_cloud_policy_set_interval(uint8_t policy[6], uint32_t interval_sec)
{
    policy[0] = (uint8_t)((interval_sec >> 24) & 0xFFu);
    policy[1] = (uint8_t)((interval_sec >> 16) & 0xFFu);
    policy[2] = (uint8_t)((interval_sec >> 8) & 0xFFu);
    policy[3] = (uint8_t)(interval_sec & 0xFFu);
}

/**
 * @brief  将 UploadItemCount 写入 CloudUploadPolicy 后 2 字节（大端）
 */
static void remote_cloud_policy_set_item_count(uint8_t policy[6], uint16_t item_count)
{
    policy[4] = (uint8_t)((item_count >> 8) & 0xFFu);
    policy[5] = (uint8_t)(item_count & 0xFFu);
}

/**
 * @brief  解析 Tag0x11 Value 内嵌 DataID，更新 cloud_policy
 * @note   DataID 0x04/0x05 为 OTA 软件版本与总包数；非升级场景云端填 0，仍须解析
 * @return 0 成功；-1 失败
 */
static int remote_parse_tag11_payload(const uint8_t *value_bytes, uint16_t value_len, uint8_t cloud_policy[6])
{
    uint16_t pos;
    uint8_t got_interval;
    uint8_t got_count;
    uint8_t got_key_capture;
    uint8_t got_ota_sw;
    uint8_t got_ota_pkts;
    uint16_t ota_sw_version;
    uint16_t ota_total_packets;

    if (value_bytes == NULL || value_len == 0u || cloud_policy == NULL)
    {
        return -1;
    }

    got_interval = 0u;
    got_count = 0u;
    got_key_capture = 0u;
    got_ota_sw = 0u;
    got_ota_pkts = 0u;
    ota_sw_version = 0u;
    ota_total_packets = 0u;
    pos = 0u;
    while (pos < value_len)
    {
        uint8_t data_id = value_bytes[pos++];
        uint16_t remain = (uint16_t)(value_len - pos);

        switch (data_id)
        {
            case 0x00u:
                if (remain < 1u)
                {
                    return -1;
                }
                pos += 1u;
                break;
            case 0x01u:
                if (remain < 1u)
                {
                    return -1;
                }
                if (value_bytes[pos] != 1u && value_bytes[pos] != 2u)
                {
                    return -1;
                }
                {
                    uint8_t cap_val = value_bytes[pos];
                    uint8_t hold;

                    pos += 1u;
                    got_key_capture = 1u;
                    if (cap_val == 2u)
                    {
                        taskENTER_CRITICAL();
                        hold = s_key_capture_hold;
                        taskEXIT_CRITICAL();
                        if (hold != 0u)
                        {
                            co_printf("[RC   ] Tag11 key capture 0x01=2 ignored (RC/BLE hold)\r\n");
                            break;
                        }
                    }
                    remote_set_key_capture_enable(cap_val);
                }
                break;
            case 0x02u:
                if (remain < 2u)
                {
                    return -1;
                }
                remote_cloud_policy_set_interval(cloud_policy,
                    (uint32_t)(((uint32_t)value_bytes[pos] << 8) | value_bytes[pos + 1u]));
                got_interval = 1u;
                pos += 2u;
                break;
            case 0x03u:
                if (remain < 2u)
                {
                    return -1;
                }
                remote_cloud_policy_set_item_count(cloud_policy,
                    (uint16_t)(((uint16_t)value_bytes[pos] << 8) | value_bytes[pos + 1u]));
                got_count = 1u;
                pos += 2u;
                break;
            case 0x04u:
                if (remain < 2u)
                {
                    return -1;
                }
                ota_sw_version = (uint16_t)(((uint16_t)value_bytes[pos] << 8)
                    | value_bytes[pos + 1u]);
                got_ota_sw = 1u;
                pos += 2u;
                break;
            case 0x05u:
                if (remain < 2u)
                {
                    return -1;
                }
                ota_total_packets = (uint16_t)(((uint16_t)value_bytes[pos] << 8)
                    | value_bytes[pos + 1u]);
                got_ota_pkts = 1u;
                pos += 2u;
                break;
            default:
                return -1;
        }
    }

    if (got_interval == 0u && got_count == 0u && got_key_capture == 0u)
    {
        return -1;
    }

    /* 仅当 Tag11 含 DataID 0x04/0x05 时才触发 OTA 任务（纯策略应答不启动升级） */
    if (got_ota_sw != 0u || got_ota_pkts != 0u) {
        udp_ota_on_tag11_task(ota_sw_version, ota_total_packets);
    }
    return 0;
}

/**
 * @brief  从 0x6868 外层帧定位 Tag0x11 并解析 Value
 * @return 0 成功；-1 失败
 */
static int remote_parse_frame_for_tag11(const uint8_t *frame, uint16_t frame_len, uint8_t cloud_policy[6])
{
    uint8_t plain[RC_UDP_AES_PLAIN_DATA_MAX];
    uint16_t plain_len;
    uint16_t tlv_len;
    uint16_t value_len;
    if (frame == NULL || frame_len < 23u || cloud_policy == NULL)
    {
        return -1;
    }
    if (frame[0] != 0x68u || frame[1] != 0x68u || frame[frame_len - 1u] != 0x16u)
    {
        return -1;
    }
    if (remote_extract_plain_data_field(frame, frame_len, plain, (uint16_t)sizeof(plain), &plain_len) != 0)
    {
        return -1;
    }
    if (plain_len < 5u)
    {
        return -1;
    }
    tlv_len = (uint16_t)(((uint16_t)plain[0] << 8) | plain[1]);
    if (tlv_len != plain_len)
    {
        return -1;
    }
    if (plain[2] != REMOTE_TAG_11)
    {
        return -1;
    }
    value_len = (uint16_t)(((uint16_t)plain[3] << 8) | plain[4]);
    if ((uint16_t)(value_len + 5u) != plain_len)
    {
        return -1;
    }
    return remote_parse_tag11_payload(plain + 5u, value_len, cloud_policy);
}

/**
 * @brief  处理云端 UDP 下行 0x6868 帧，解析 Tag0x11 并更新 CloudUploadPolicy
 * @param  cloud_policy  入参为当前策略；出参写入 interval(4B) + count(2B)
 * @return 0 成功；-1 失败
 */
int remote_handle_downlink_frame(const uint8_t *frame, uint16_t frame_len, uint8_t cloud_policy[6])
{
    uint8_t policy[6];

    if (frame == NULL || cloud_policy == NULL)
    {
        return -1;
    }
    memcpy(policy, cloud_policy, 6u);
    if (remote_parse_frame_for_tag11(frame, frame_len, policy) != 0)
    {
        return -1;
    }
    memcpy(cloud_policy, policy, 6u);
    return 0;
}
