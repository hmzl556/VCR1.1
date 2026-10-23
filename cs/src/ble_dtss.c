#include "app_gatt_safe.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
#include "ble_dtss.h"
#include "hogp.h"
#include "gmem.h"
#include "userhard.h"
#include "app_voice_ctrl.h"

/*
 * Voice service protocol mapping:
 * chr0 = voice data channel (read/notify)
 * chr1 = voice control request channel (write_req)
 * chr2 = voice control response / capability response channel (read/indication)
 */
#define VOICE_CAP_REQ_CMD                (0x02U)//能力查询命令
#define VOICE_CTRL_REQ_CMD               (0x01U)//控制命令
#define VOICE_CTRL_REQ_START             (0x01U)//控制命令-开始语音采集
#define VOICE_CTRL_REQ_STOP              (0x02U)//控制命令-停止语音采集
#define VOICE_CAP_RESP_CMD               (0x02U)//能力响应命令
#define VOICE_CTRL_RESP_CMD              (0x01U)//控制响应命令
#define VOICE_CTRL_STATE_SPEAKING        (0x01U)//语音状态-正在说话
#define VOICE_CTRL_STATE_IDLE            (0x02U)//语音状态-空闲
#define VOICE_CODEC_SBC                  (0x01U)//语音编码格式-SBC
#define VOICE_CODEC_ADPCM                (0x02U)//语音编码格式-ADPCM
#define VOICE_CODEC_OPUS                 (0x03U)//语音编码格式-OPUS
#define VOICE_CODEC_L2HC                 (0x04U)//语音编码格式-L2HC
#define VOICE_TX_HID                     (0x01U)//语音数据通过HID协议发送
#define VOICE_TX_GATT                    (0x02U)//语音数据通过GATT协议发送
#define VOICE_CAP_VERSION                (0x01U)//语音能力版本号
#define DEVICE_CODEC_MASK                (0x01U)//本机仅支持 SBC
#define DEVICE_TX_MASK                   (VOICE_TX_GATT)//本机语音仅经 GATT 传输

gatt_svc_t dtss_svc = NULL;
gatt_chr_t dtss_chr0 = NULL;
gatt_chr_t dtss_chr1 = NULL;
gatt_chr_t dtss_chr2 = NULL;

static uint8_t voice_capture_enabled = 0U;
static uint8_t voice_selected_codec = VOICE_CODEC_SBC;
static uint8_t voice_control_state = VOICE_CTRL_STATE_IDLE;
static uint8_t voice_chr2_value[8] = { VOICE_CTRL_RESP_CMD, VOICE_CTRL_STATE_IDLE };
static uint8_t voice_chr2_len = 2U;

static gatt_chr_t voice_indication_chr = NULL;	//特征句柄
static peer_t voice_indication_peer = NULL;		//当前连接的对端设备句柄

//将一个4字节数组（按照小端字节序）转换为一个32位无符号整数
static uint32_t voice_read_u32_le(const uint8_t *buf)
{
	return ((uint32_t)buf[0]) |
		   ((uint32_t)buf[1] << 8U) |
		   ((uint32_t)buf[2] << 16U) |
		   ((uint32_t)buf[3] << 24U);
}

//将一个32位无符号整数（按照小端字节序）写入一个4字节数组
static void voice_write_u32_le(uint8_t *buf, uint32_t value)
{
	buf[0] = (uint8_t)(value);
	buf[1] = (uint8_t)(value >> 8U);
	buf[2] = (uint8_t)(value >> 16U);
	buf[3] = (uint8_t)(value >> 24U);
}

//更新chr2的值
static void voice_update_chr2_value(const uint8_t *value, uint8_t len)
{
	if (len > (uint8_t)sizeof(voice_chr2_value)) {
		len = (uint8_t)sizeof(voice_chr2_value);
	}
	//更新本地缓存的chr2值
	memcpy(voice_chr2_value, value, len);
	voice_chr2_len = len;
}

// 主机掩码与本机能力掩码取交集后选择编解码；无交集返回 0
static uint8_t voice_select_codec(uint32_t codec_mask)
{
	uint32_t common = codec_mask & DEVICE_CODEC_MASK;

	if (common & 0x01U) {
		return VOICE_CODEC_SBC;
	}
	return 0U;
}

//在有订阅的情况下更新chr2的值，发送通知/指示
static void voice_send_chr2_value(const uint8_t *value, uint8_t len)
{
	voice_update_chr2_value(value, len);

	if (voice_indication_chr && voice_indication_peer) {
		app_gatt_indicate(voice_indication_peer, voice_indication_chr,
							   voice_chr2_value, voice_chr2_len);
	}
}

//发送能力协商响应；本机不支持则串口打印且不回复 chr2
void ble_dtss_send_capability_response(uint32_t codec_mask, uint8_t tx_mask)
{
	uint8_t payload[6];
	uint8_t selected;

	if ((tx_mask & DEVICE_TX_MASK) == 0U) {
		co_printf("[VOICE] Cap unsupported: NO GATT\r\n");
		return;
	}

	if ((codec_mask & DEVICE_CODEC_MASK) == 0U) {
		co_printf("[VOICE] Cap unsupported: NO SBC\r\n");
		return;
	}

	selected = voice_select_codec(codec_mask);
	if (selected == 0U) {
		co_printf("[VOICE] Cap unsupported: SBC\r\n");
		return;
	}

	voice_selected_codec = selected;

	payload[0] = VOICE_CAP_RESP_CMD;
	voice_write_u32_le(&payload[1], (uint32_t)voice_selected_codec);
	payload[5] = VOICE_CAP_VERSION;

	voice_send_chr2_value(payload, sizeof(payload));
	co_printf("[VOICE] Cap ok: SBC, GATT\r\n");
}

//发送控制响应，内容为一个字节的命令码和一个字节的状态码
void ble_dtss_send_control_response(uint8_t voice_state)
{
	uint8_t payload[2];

	voice_control_state = (voice_state == VOICE_CTRL_STATE_SPEAKING) ?
						  VOICE_CTRL_STATE_SPEAKING : VOICE_CTRL_STATE_IDLE;

	payload[0] = VOICE_CTRL_RESP_CMD;
	payload[1] = voice_control_state;

	voice_capture_enabled = (voice_control_state == VOICE_CTRL_STATE_SPEAKING) ? 1U : 0U;
	voice_send_chr2_value(payload, sizeof(payload));
}

//语音服务GATT事件回调函数，处理服务和特征的读写请求等事件
void dtss_svc_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;
	(void)param;

	switch (event) {
		case GATTS_EV_SERVICE_START: {
			gatt_svc_start_t *in = (gatt_svc_start_t *)param;
			co_printf("[VOICE] svc start = %X\r\n", in->result);
			break;
		}
		default:
			break;
	}
}

//chr0事件回调函数，处理主机对chr0的读请求和发送开始事件
void dtss_chr0_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;

	switch (event) {
		case GATTS_EV_CHR_VALUE_READ_REQ: {
			gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
			
			//给手机返回的值，第一个字节表示语音采集状态，第二个字节表示是否允许发送数据
			uint8_t value[] = { voice_capture_enabled, bleSendEn };
			gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK, value, sizeof(value));
			break;
		}

		case GATTS_EV_CHR_VALUE_SEND_START: { //手机订阅了chr0的通知，准备接收语音数据
			gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            if (in != NULL && in->peer != ble_gap_get_peer()) break;
            if (!in || in->type != GATT_SEND_VAL_NOTIFICATION) {
                bleSendEn=0;dtss_chr=NULL;dtss_peer=NULL;
                (void)voice_ctrl_post_stop();
                co_printf("[VOICE] chr0 disabled\r\n");
                break;
            }
            co_printf("[VOICE] chr0 send start event!\r\n");
			bleSendEn = 0xA6U;
			dtss_chr = in->chr;
			dtss_peer = in->peer;
			break;
		}

		default:
			break;
	}
}

//chr1的写请求处理，解析控制命令并执行相应操作
static void dtss_chr1_handle_write(const gatt_chr_val_write_req_t *in)
{
    const uint8_t *val;
    uint16_t len;
    if (in == NULL || in->val == NULL || in->val_len == 0u) return;
    val = in->val;
    len = in->val_len;

	bleConnectOk = 0xA6U;

	/*
	 * chr1 receives two kinds of host requests:
	 * - Table D.3 capability negotiation request:
	 *   byte0 = 0x02
	 *   byte1..4 = supported codec bitmap (SBC/ADPCM/Opus/L2HC)
	 *   byte5 = supported transport bitmap (HID/GATT)
	 *   byte6 = protocol version
	 * - Table D.5 voice control request:
	 *   byte0 = 0x01
	 *   byte1 = 0x01 start capture, 0x02 stop capture
	 */

	if (val[0] == VOICE_CAP_REQ_CMD) {
		if (len < 7U) {
			co_printf("[VOICE] Cap unsupported: Length Error\r\n");
		} else if (val[6] != VOICE_CAP_VERSION) {
			co_printf("[VOICE] Cap unsupported: Version\r\n");
		} else {
			uint32_t codec_mask = voice_read_u32_le(&val[1]);
			uint8_t tx_mask = val[5];

			ble_dtss_send_capability_response(codec_mask, tx_mask);
		}
	} else if ((len >= 2U) && (val[0] == VOICE_CTRL_REQ_CMD)) {
		if (val[1] == VOICE_CTRL_REQ_START) {
			(void)voice_ctrl_post_start();
		} else if (val[1] == VOICE_CTRL_REQ_STOP) {
			(void)voice_ctrl_post_stop();
		} else {
			co_printf("[VOICE] Ctrl unsupported: SubCmd\r\n");
		}
	} else if (len > 0U) {
		co_printf("[VOICE] Chr1 unsupported: Cmd\r\n");
	}
}

//chr1事件回调函数，处理主机对chr1的写请求
void dtss_chr1_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;

	switch (event) {
		case GATTS_EV_CHR_VALUE_WRITE_REQ:
		case GATTS_EV_CHR_VALUE_WRITE:
			dtss_chr1_handle_write((gatt_chr_val_write_req_t *)param);
			gatts_chr_val_write_rsp(((gatt_chr_val_write_req_t *)param)->peer,
									((gatt_chr_val_write_req_t *)param)->chr,
									GATT_OK);
			break;

		default:
			break;
	}
}

//chr2事件回调函数，处理主机对chr2的读请求和发送开始事件
void dtss_chr2_cbk(gatt_event_t event, void *context, void *param)
{
	(void)context;

	switch (event) {
		case GATTS_EV_CHR_VALUE_READ_REQ: {
			gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;

			gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
								   voice_chr2_value, voice_chr2_len);
			break;
		}

		case GATTS_EV_CHR_VALUE_SEND_START: {
			gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            if (in != NULL && in->peer != ble_gap_get_peer()) break;
            if (!in || in->type != GATT_SEND_VAL_INDICATION) {
                voice_indication_chr=NULL;voice_indication_peer=NULL;
                (void)voice_ctrl_post_stop();
                co_printf("[VOICE] chr2 disabled\r\n");
                break;
            }
            co_printf("[VOICE] chr2 send start event!\r\n");

			/*
			 * chr2 uses indication for Table D.4 / D.6 responses.
			 * Indication is used instead of notify so the host must confirm
			 * delivery at the GATT layer before we treat the response as sent.
			 */
			voice_indication_chr = in->chr;
			voice_indication_peer = in->peer;
			if (voice_chr2_len > 0U) {
				app_gatt_indicate(voice_indication_peer, voice_indication_chr,
									   voice_chr2_value, voice_chr2_len);
			}
			break;
		}

		default:
			break;
	}
}

//语音服务初始化函数，创建服务和特征，并启动服务
int ble_dtss_init(void)
{
	uuid_t svc_uuid = {
		UUID_LEN_128,
		{ .uuid128 = {
			0x01, 0x79, 0x8b, 0x33, 0xb4, 0x23, 0xcf, 0x0d,
			0x0c, 0x4e, 0xbc, 0xa0, 0x61, 0x4e, 0x76, 0xc1
		}}
	};
	dtss_svc = gatts_svc_create(&svc_uuid, dtss_svc_cbk, NULL);

	uuid_t chr0_uuid = {
		UUID_LEN_128,
		{ .uuid128 = {
			0x03, 0x79, 0x8b, 0x33, 0xb4, 0x23, 0xcf, 0x0d,
			0x0c, 0x4e, 0xbc, 0xa0, 0x61, 0x4e, 0x76, 0xc1
		}}
	};
	dtss_chr0 = gatts_chr_add(dtss_svc, &chr0_uuid,
							  GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY,
							  ATT_PERM_READ,
							  dtss_chr0_cbk, NULL);
	gatts_dsc_ccc_add(dtss_chr0,
					  GATT_CHR_PROP_NOTIFY,
					  ATT_PERM_READ | ATT_PERM_WRITE);

	uuid_t chr1_uuid = {
		UUID_LEN_128,
		{ .uuid128 = {
			0x04, 0x79, 0x8b, 0x33, 0xb4, 0x23, 0xcf, 0x0d,
			0x0c, 0x4e, 0xbc, 0xa0, 0x61, 0x4e, 0x76, 0xc1
		}}
	};
	dtss_chr1 = gatts_chr_add(dtss_svc, &chr1_uuid,
							  GATT_CHR_PROP_WRITE,
							  ATT_PERM_WRITE,
							  dtss_chr1_cbk, NULL);

	uuid_t chr2_uuid = {
		UUID_LEN_128,
		{ .uuid128 = {
			0x05, 0x79, 0x8b, 0x33, 0xb4, 0x23, 0xcf, 0x0d,
			0x0c, 0x4e, 0xbc, 0xa0, 0x61, 0x4e, 0x76, 0xc1
		}}
	};
	dtss_chr2 = gatts_chr_add(dtss_svc, &chr2_uuid,
							  GATT_CHR_PROP_READ | GATT_CHR_PROP_INDICATE,
							  ATT_PERM_READ,
							  dtss_chr2_cbk, NULL);
	gatts_dsc_ccc_add(dtss_chr2,
					  GATT_CHR_PROP_INDICATE,
					  ATT_PERM_READ | ATT_PERM_WRITE);

	gatts_svc_start(dtss_svc);

	return 0;
}

void ble_dtss_on_disconnect(void)
{
    bleSendEn = 0U;
    dtss_peer = NULL;
    dtss_chr = NULL;
    voice_selected_codec = VOICE_CODEC_SBC;
	voice_indication_chr = NULL;
	voice_indication_peer = NULL;
	voice_capture_enabled = 0U;
	voice_control_state = VOICE_CTRL_STATE_IDLE;
	voice_chr2_len = 2U;
	voice_chr2_value[0] = VOICE_CTRL_RESP_CMD;
	voice_chr2_value[1] = VOICE_CTRL_STATE_IDLE;
}

void ble_dtss_force_cccd_enable(peer_t peer)
{
	if (peer == NULL)
	{
		return;
	}

	bleSendEn = 0xA6U;
	dtss_chr = dtss_chr0;
	dtss_peer = peer;
	voice_indication_chr = dtss_chr2;
	voice_indication_peer = peer;
	co_printf("[VOICE] chr0 send start event! (forced)\r\n");
	co_printf("[VOICE] chr2 send start event! (forced)\r\n");
}

int ble_dtss_is_subscribed(void)
{
	return (bleSendEn == 0xA6U && dtss_peer != NULL && dtss_chr != NULL) ? 1 : 0;
}



uint8_t ble_dtss_ready_channels(void)
{
    peer_t peer = ble_gap_get_peer();
    uint8_t ready = 0U;
    if (peer == NULL || bleConnectOk != 0xA6U) return 0U;
    if (bleSendEn == 0xA6U && dtss_peer == peer && dtss_chr != NULL)
        ready |= BLE_DTSS_DATA_READY;
    if (voice_indication_peer == peer && voice_indication_chr != NULL)
        ready |= BLE_DTSS_CTRL_READY;
    return ready;
}
