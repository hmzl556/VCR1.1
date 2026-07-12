#include <string.h>
#include "ble_rc_svc.h"
#include "ble_gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "rc_trans_assembly.h"
#include "rc_cat1_bridge.h"
#include "remote_reports.h"

static gatt_svc_t s_rc_svc;
static gatt_chr_t s_chr_status;
static gatt_chr_t s_chr_key_notify;

static uint8_t     s_rc_read[RC_READ_LEN];
static rc_status_t s_rc_status;
static rc_conf_t   s_rc_conf;
static peer_t      s_rc_peer;

/**
 * @brief  从大端字节流读取 Uint16
 */
static uint16_t rc_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/**
 * @brief  将 RC-STATUS.State 转为可读字符串（串口调试用）
 */
static const char *rc_state_name(uint8_t state)
{
    switch (state)
    {
    case RC_STATE_IDLE:  return "IDLE";
    case RC_STATE_READY: return "READY";
    case RC_STATE_BUSY:  return "BUSY";
    case RC_STATE_DONE:  return "DONE";
    case RC_STATE_ERROR: return "ERROR";
    default:             return "?";
    }
}

/**
 * @brief  将 RC-STATUS.Result 转为可读字符串（串口调试用）
 */
static const char *rc_result_name(uint8_t result)
{
    switch (result)
    {
    case RC_RESULT_NONE:         return "NONE";
    case RC_RESULT_OK:           return "OK";
    case RC_RESULT_CRC_ERROR:    return "CRC_ERROR";
    case RC_RESULT_LEN_ERROR:    return "LEN_ERROR";
    case RC_RESULT_PARAM_ERROR:  return "PARAM_ERROR";
    case RC_RESULT_DUPLICATE:    return "DUPLICATE";
    case RC_RESULT_BUSY_REJECT:  return "BUSY_REJECT";
    case RC_RESULT_NET_ERROR:    return "NET_ERROR";
    default:                     return "?";
    }
}

/**
 * @brief  RC-STATUS Notify 前打印各字段及 14 字节原始载荷（MD §3.4）
 */
static void rc_status_log_notify(const rc_status_t *st, const uint8_t buf[RC_STATUS_LEN])
{
    co_printf("\r\n[RC   ] STATUS Notify: State = %s(0x%02X) Result = %s(0x%02X) Retry = %u Rsv = 0x%02X PackIndex = 0x%08X CloudPolicy = %02X %02X %02X %02X %02X %02X\r\n",
              rc_state_name(st->state), (unsigned)st->state,
              rc_result_name(st->result), (unsigned)st->result,
              (unsigned)st->retry_count, (unsigned)st->reserved1,
              (unsigned)st->pack_index,
              (unsigned)st->cloud_policy[0], (unsigned)st->cloud_policy[1],
              (unsigned)st->cloud_policy[2], (unsigned)st->cloud_policy[3],
              (unsigned)st->cloud_policy[4], (unsigned)st->cloud_policy[5]);
    co_printf("[RC   ] STATUS raw: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
              (unsigned)buf[0], (unsigned)buf[1], (unsigned)buf[2], (unsigned)buf[3],
              (unsigned)buf[4], (unsigned)buf[5], (unsigned)buf[6], (unsigned)buf[7],
              (unsigned)buf[8], (unsigned)buf[9], (unsigned)buf[10], (unsigned)buf[11],
              (unsigned)buf[12], (unsigned)buf[13]);
}

/**
 * @brief  将 RC-STATUS 结构体序列化为 14 字节大端格式
 */
static void rc_status_to_bytes(const rc_status_t *st, uint8_t out[RC_STATUS_LEN])
{
    out[0] = st->state;
    out[1] = st->result;
    out[2] = st->retry_count;
    out[3] = st->reserved1;
    out[4] = (uint8_t)(st->pack_index >> 24);
    out[5] = (uint8_t)(st->pack_index >> 16);
    out[6] = (uint8_t)(st->pack_index >> 8);
    out[7] = (uint8_t)(st->pack_index);
    memcpy(out + 8, st->cloud_policy, 6u);
}

/**
 * @brief  填充 RC-READ 61 字节静态设备信息（MD §3.1 字段表）
 */
static void rc_read_build(void)
{
    memset(s_rc_read, 0, sizeof(s_rc_read));
    memcpy(s_rc_read + RC_READ_OFF_DEVICE_ID,                    RC_READ_VAL_DEVICE_ID,                    RC_READ_LEN_DEVICE_ID);
    memcpy(s_rc_read + RC_READ_OFF_DEVICE_REGION_ID,             RC_READ_VAL_DEVICE_REGION_ID,             RC_READ_LEN_DEVICE_REGION_ID);
    memcpy(s_rc_read + RC_READ_OFF_USER_ID,                      RC_READ_VAL_USER_ID,                      RC_READ_LEN_USER_ID);
    s_rc_read[RC_READ_OFF_DEVICE_MODEL]                = RC_READ_VAL_DEVICE_MODEL;               // DeviceModel:               0x01=语音遥控器, 0x02=4G 模块
    memcpy(s_rc_read + RC_READ_OFF_DEVICE_SW_VERSION,            RC_READ_VAL_DEVICE_SW_VERSION,            RC_READ_LEN_DEVICE_SW_VERSION);
    s_rc_read[RC_READ_OFF_DEVICE_STATUS]               = RC_READ_VAL_DEVICE_STATUS;              // DeviceStatus:              0x00=正常, 0x01=失效, 0x02=未知
    memcpy(s_rc_read + RC_READ_OFF_DEVICE_ACTIVE_TIME,           RC_READ_VAL_DEVICE_ACTIVE_TIME,           RC_READ_LEN_DEVICE_ACTIVE_TIME);
    memcpy(s_rc_read + RC_READ_OFF_DEVICE_INACTIVE_TIME,         RC_READ_VAL_DEVICE_INACTIVE_TIME,         RC_READ_LEN_DEVICE_INACTIVE_TIME);
    s_rc_read[RC_READ_OFF_DEVICE_TRANSMISSION_NETWORK] = RC_READ_VAL_DEVICE_TRANSMISSION_NETWORK;  // DeviceTransmissionNetwork: 0x00=广电, 0x01=电信, 0x02=联通, 0x03=移动, 0x04=其他
    for (uint8_t i = 0u; i < RC_READ_LEN_DEVICE_MAC; i++)
    {
        s_rc_read[RC_READ_OFF_DEVICE_MAC + i] = local_bd[RC_READ_LEN_DEVICE_MAC - 1u - i];  // DeviceMac: MSB 在前，与扫描界面一致
    }
    /* Reserved[RC_READ_OFF_RESERVED..RC_READ_LEN-1] 已由 memset 置 0 */
}

/**
 * @brief  收到 RC-CONF Write 时打印各字段及 16 字节原始载荷（MD §3.2）
 */
static void rc_conf_log_rx(const uint8_t *val, uint16_t len)
{
    uint8_t i;
    uint16_t dump_len;

    if (val == NULL)
    {
        return;
    }

    if (len >= RC_CONF_LEN)
    {
        co_printf("[RC   ] CONF rx: ConfigTS = %.4s TransProto = 0x%02X MaxPkt = %u MaxRetry = %u RetryDelayMs = %u SleepDelaySec = %u\r\n",
                  (const char *)val,
                  (unsigned)val[4],
                  (unsigned)rc_be16(val + 5),
                  (unsigned)val[7],
                  (unsigned)rc_be16(val + 8),
                  (unsigned)rc_be16(val + 10));
    }
    else
    {
        co_printf("[RC   ] CONF rx: len = %u (expect %u)\r\n", (unsigned)len, (unsigned)RC_CONF_LEN);
    }

    dump_len = (len > RC_CONF_LEN) ? RC_CONF_LEN : len;
    co_printf("[RC   ] CONF raw:");
    for (i = 0u; i < dump_len; i++)
    {
        co_printf(" %02X", (unsigned)val[i]);
    }
    co_printf("\r\n");
}

/**
 * @brief  解析 RC-CONF 并更新本地配置
 */
static int rc_conf_apply(const uint8_t *val, uint16_t len, peer_t peer)
{
    rc_status_t st;
    uint16_t max_pkt;
    uint16_t retry_delay;

    if (val == NULL || len != RC_CONF_LEN)
    {
        co_printf("[RC   ] CONF reject: bad len\r\n");
        return -1;
    }
    if (val[4] != 0x01u)
    {
        co_printf("[RC   ] CONF reject: TransProto=0x%02X\r\n", (unsigned)val[4]);
        return -1;
    }
    max_pkt = rc_be16(val + 5);
    if (max_pkt != 128u)
    {
        co_printf("[RC   ] CONF reject: MaxPkt=%u (expect 128)\r\n", (unsigned)max_pkt);
        return -1;
    }
    if (val[7] > 3u)
    {
        co_printf("[RC   ] CONF reject: MaxRetry=%u\r\n", (unsigned)val[7]);
        return -1;
    }
    retry_delay = rc_be16(val + 8);
    if (retry_delay < 100u || retry_delay > 5000u)
    {
        co_printf("[RC   ] CONF reject: RetryDelayMs=%u\r\n", (unsigned)retry_delay);
        return -1;
    }

    s_rc_conf.max_retry = val[7];
    s_rc_conf.retry_delay_ms = retry_delay;
    s_rc_conf.sleep_delay_sec = rc_be16(val + 10);
    s_rc_conf.max_packet_size = 128u;
    s_rc_peer = peer;

    rc_trans_session_reset();
    rc_cat1_get_cloud_policy(s_rc_status.cloud_policy);

    s_rc_status.state = RC_STATE_READY;
    s_rc_status.result = RC_RESULT_OK;
    s_rc_status.retry_count = 0u;
    s_rc_status.reserved1 = 0u;

    memcpy(&st, &s_rc_status, sizeof(st));
    ble_rc_svc_status_notify(peer, &st);
    co_printf("\r\n[RC   ] CONF applied, READY\r\n\r\n");
    return 0;
}

// RC GATT 服务回调
static void rc_svc_cbk(gatt_event_t event, void *context, void *param)
{
    if (event == GATTS_EV_SERVICE_START)
    {
        gatt_svc_start_t *in = (gatt_svc_start_t *)param;
        co_printf("[RC   ] svc start = %X\r\n", in->result);
    }
}

// RC-READ 回调
static void rc_read_cbk(gatt_event_t event, void *context, void *param)
{
    if (event == GATTS_EV_CHR_VALUE_READ_REQ)
    {
        gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
        gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, s_rc_read, RC_READ_LEN);
    }
}

// RC-CONF 回调
static void rc_conf_cbk(gatt_event_t event, void *context, void *param)
{
    if (event == GATTS_EV_CHR_VALUE_WRITE_REQ)
    {
        gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
        rc_status_t st;

        gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
        rc_conf_log_rx((const uint8_t *)in->val, in->val_len);
        if (in->val_len != RC_CONF_LEN)
        {
            memcpy(&st, &s_rc_status, sizeof(st));
            st.state = RC_STATE_ERROR;
            st.result = RC_RESULT_LEN_ERROR;
            ble_rc_svc_status_notify(in->peer, &st);
        }
        else if (rc_conf_apply(in->val, in->val_len, in->peer) != 0)
        {
            memcpy(&st, &s_rc_status, sizeof(st));
            st.state = RC_STATE_ERROR;
            st.result = RC_RESULT_PARAM_ERROR;
            ble_rc_svc_status_notify(in->peer, &st);
        }
    }
}

// RC-TRANS 回调：先拷贝并处理，再 write_rsp，避免过早 ACK 导致连发子包丢失
static void rc_trans_cbk(gatt_event_t event, void *context, void *param)
{
    gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
    static uint8_t rx_buf[RC_TRANS_FRAME_MAX];
    uint16_t len;

    (void)context;

    if (event != GATTS_EV_CHR_VALUE_WRITE_REQ && event != GATTS_EV_CHR_VALUE_WRITE)
    {
        return;
    }

    len = in->val_len;
    if (len == 0u || len > RC_TRANS_FRAME_MAX)
    {
        if (event == GATTS_EV_CHR_VALUE_WRITE_REQ)
        {
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH);
        }
        return;
    }

    memcpy(rx_buf, in->val, len);
    s_rc_peer = in->peer;
    rc_trans_on_write(in->peer, rx_buf, len);
    if (event == GATTS_EV_CHR_VALUE_WRITE_REQ)
    {
        gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
    }
}

// RC-STATUS 回调
static void rc_status_cbk(gatt_event_t event, void *context, void *param)
{
    uint8_t buf[RC_STATUS_LEN];

    if (event == GATTS_EV_CHR_VALUE_READ_REQ)
    {
        gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;

        rc_cat1_get_cloud_policy(s_rc_status.cloud_policy);//获取云端配置
        rc_status_to_bytes(&s_rc_status, buf);
        gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, buf, RC_STATUS_LEN);
    }
    else if (event == GATTS_EV_CHR_VALUE_SEND_START)
    {
        co_printf("[RC   ] RC-STATUS send start event!\r\n");
        gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
        s_rc_peer = in->peer;
        s_chr_status = in->chr;
    }
}

// RC-KEY-NOTIFY 回调
static void rc_key_notify_cbk(gatt_event_t event, void *context, void *param)
{
    if (event == GATTS_EV_CHR_VALUE_SEND_START)
    {
        co_printf("[RC   ] RC-KEY-NOTIFY send start event!\r\n");
        gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
        s_rc_peer = in->peer;
        s_chr_key_notify = in->chr;
    }
}

// 获取当前连接对端
peer_t ble_rc_svc_get_peer(void)
{
    if (s_rc_peer != NULL)
    {
        return s_rc_peer;
    }
    return ble_gap_get_peer();
}

// 获取 RC-STATUS 缓存
const rc_status_t *ble_rc_svc_get_status(void)
{
    return &s_rc_status;
}

const uint8_t *ble_rc_svc_get_read(void)
{
    return s_rc_read;
}

/**
 * @brief  更新 RC-STATUS 并通过 Notify 推送
 */
void ble_rc_svc_status_notify(peer_t peer, const rc_status_t *st)
{
    uint8_t buf[RC_STATUS_LEN];

    if (st != NULL)
    {
        memcpy(&s_rc_status, st, sizeof(s_rc_status));
    }
    rc_cat1_get_cloud_policy(s_rc_status.cloud_policy);
    rc_status_to_bytes(&s_rc_status, buf);
    rc_status_log_notify(&s_rc_status, buf);// 打印 RC-STATUS 各字段及 14 字节原始载荷

    if (peer == NULL)
    {
        peer = ble_rc_svc_get_peer();
    }
    if (peer != NULL && s_chr_status != NULL)
    {
        gatts_chr_val_notif(peer, s_chr_status, buf, RC_STATUS_LEN);
    }
}

/**
 * @brief  RC-KEY-NOTIFY 4 字节镜像上报
 */
void ble_rc_svc_key_notify(peer_t peer, uint8_t key_type, uint8_t action, uint16_t key_value)
{
    uint8_t buf[RC_KEY_NOTIFY_LEN];

    buf[0] = key_type;
    buf[1] = action;
    buf[2] = (uint8_t)(key_value >> 8);
    buf[3] = (uint8_t)(key_value);

    if (peer == NULL)
    {
        peer = ble_rc_svc_get_peer();
    }
    if (peer != NULL && s_chr_key_notify != NULL)
    {
        gatts_chr_val_notif(peer, s_chr_key_notify, buf, RC_KEY_NOTIFY_LEN);
    }
}

void ble_rc_svc_on_connect(peer_t peer)
{
    if (peer != NULL)
    {
        s_rc_peer = peer;
    }
    co_printf("[RC   ] on_connect peer=%08X state=%s(0x%02X)\r\n",
              (unsigned)(uint32_t)peer,
              rc_state_name(s_rc_status.state),
              (unsigned)s_rc_status.state);
}

void ble_rc_svc_on_disconnect(void)
{
    s_rc_peer = NULL;
    s_rc_status.state = RC_STATE_IDLE;
    s_rc_status.result = RC_RESULT_NONE;
    s_rc_status.retry_count = 0u;
    s_rc_status.pack_index = RC_PACK_INDEX_NONE;
    remote_key_capture_on_ble_disconnect();
}

/**
 * @brief  注册 RC GATT 服务及五个特征
 */
int ble_rc_svc_init(void)
{
    memset(&s_rc_conf, 0, sizeof(s_rc_conf));
    memset(&s_rc_status, 0, sizeof(s_rc_status));
    s_rc_status.state = RC_STATE_IDLE;
    s_rc_status.pack_index = RC_PACK_INDEX_NONE;
    s_rc_peer = NULL;
    rc_cat1_get_cloud_policy(s_rc_status.cloud_policy);
    rc_read_build();

    uuid_t svc_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x00, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    s_rc_svc = gatts_svc_create(&svc_uuid, rc_svc_cbk, NULL);

    uuid_t chr_read_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x10, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    (void)gatts_chr_add(s_rc_svc, &chr_read_uuid,
                        GATT_CHR_PROP_READ,
                        ATT_PERM_READ,
                        rc_read_cbk, NULL);

    uuid_t chr_conf_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x12, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    (void)gatts_chr_add(s_rc_svc, &chr_conf_uuid,
                        GATT_CHR_PROP_WRITE,
                        ATT_PERM_WRITE,
                        rc_conf_cbk, NULL);

    uuid_t chr_trans_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x13, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    (void)gatts_chr_add(s_rc_svc, &chr_trans_uuid,
                        GATT_CHR_PROP_WRITE,
                        ATT_PERM_WRITE,
                        rc_trans_cbk, NULL);

    uuid_t chr_status_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x14, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    s_chr_status = gatts_chr_add(s_rc_svc, &chr_status_uuid,
                                 GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY | GATT_CHR_PROP_INDICATE,
                                 ATT_PERM_READ,
                                 rc_status_cbk, NULL);
    gatts_dsc_ccc_add(s_chr_status,
                      GATT_CHR_PROP_NOTIFY | GATT_CHR_PROP_INDICATE,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    uuid_t chr_key_notify_uuid = {
        UUID_LEN_128,
        { .uuid128 = {
            0x15, 0xa0, 0x46, 0x3f, 0xc3, 0xeb, 0xe7, 0xe4,
            0x6f, 0x7e, 0xd3, 0x9a, 0xa4, 0x51, 0xee, 0x19
        }}
    };
    s_chr_key_notify = gatts_chr_add(s_rc_svc, &chr_key_notify_uuid,
                                     GATT_CHR_PROP_NOTIFY,
                                     ATT_PERM_READ,
                                     rc_key_notify_cbk, NULL);
    gatts_dsc_ccc_add(s_chr_key_notify,
                      GATT_CHR_PROP_NOTIFY,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_svc_start(s_rc_svc);
    return 0;
}
