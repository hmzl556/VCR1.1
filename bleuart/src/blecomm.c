#include "app_gatt_safe.h"
#include <string.h>
#include "gmem.h"
#include "cs_core.h"
#include "co_printf.h"

#define VOICE_SBC_TS_STEP_MS  (8U)

static uint32_t g_voice_sbc_seq = 0;
static uint32_t g_voice_sbc_ts = 0;
static uint8_t s_voice_sbc_packet[247];
static uint32_t s_voice_last_ms,s_voice_submitted,s_voice_dropped,s_voice_no_link;
static uint8_t s_voice_sent;
static int s_voice_last_error;

static void bleSendVoiceStateFrame(uint8_t cmd)
{
	const uint8_t *payload = NULL;
	uint8_t len = 0;

	if (bleConnectOk != 0xa6 || !bleSendEn) return;
	if (!dtss_peer || !dtss_chr) return;

	if (cmd == 0x01) {
		payload = (const uint8_t *)"START";
		len = 5;
	} else if (cmd == 0x02) {
		payload = (const uint8_t *)"AUDIOEND";
		len = 8;
	} else {
		return;
	}

	app_gatt_notify(dtss_peer, dtss_chr, (uint8_t *)payload, len);
}

void bleSendVoiceSbcFrame(const uint8_t *buf, uint16_t len)
{
	uint32_t seq;
	uint32_t timestamp;
	uint16_t total_len;

	if (!buf || len == 0) return;
	if (bleConnectOk != 0xa6 || !bleSendEn || !dtss_peer || !dtss_chr) {++s_voice_no_link;return;}
    if(s_voice_sent && app_now_ms()-s_voice_last_ms<VOICE_SBC_TS_STEP_MS){++g_voice_sbc_seq;g_voice_sbc_ts+=VOICE_SBC_TS_STEP_MS;++s_voice_dropped;return;}

	if (len > (uint16_t)(sizeof(s_voice_sbc_packet) - 8U)) {
		len = (uint16_t)(sizeof(s_voice_sbc_packet) - 8U);
	}

	seq = g_voice_sbc_seq++;
	timestamp = g_voice_sbc_ts;
	g_voice_sbc_ts += VOICE_SBC_TS_STEP_MS;
	total_len = (uint16_t)(8U + len);

	s_voice_sbc_packet[0] = (uint8_t)(seq >> 24);
	s_voice_sbc_packet[1] = (uint8_t)(seq >> 16);
	s_voice_sbc_packet[2] = (uint8_t)(seq >> 8);
	s_voice_sbc_packet[3] = (uint8_t)(seq);
	s_voice_sbc_packet[4] = (uint8_t)(timestamp >> 24);
	s_voice_sbc_packet[5] = (uint8_t)(timestamp >> 16);
	s_voice_sbc_packet[6] = (uint8_t)(timestamp >> 8);
	s_voice_sbc_packet[7] = (uint8_t)(timestamp);
	memcpy(&s_voice_sbc_packet[8], buf, len);
    s_voice_last_error=app_gatt_notify(dtss_peer,dtss_chr,s_voice_sbc_packet,(uint8_t)total_len);
    s_voice_last_ms=app_now_ms();s_voice_sent=1;
    if(s_voice_last_error==GATT_OK)++s_voice_submitted;else ++s_voice_dropped;
}

void bleResetVoiceSbcSeq(void)
{
	g_voice_sbc_seq = 0;
	g_voice_sbc_ts = 0;
    s_voice_last_ms=s_voice_submitted=s_voice_dropped=s_voice_no_link=0;
    s_voice_sent=0;s_voice_last_error=0;
}

void bleSendVoiceStartFrame(void)
{
	co_printf("[VOICE] start subscribed=%u\r\n",(unsigned)(bleSendEn && dtss_peer && dtss_chr));
	bleSendVoiceStateFrame(0x01);
}

void bleSendVoiceEndFrame(void)
{
	bleSendVoiceStateFrame(0x02);
    co_printf("[VOICE] end submit=%u drop=%u blocked=%u ret=%d\r\n",
              (unsigned)s_voice_submitted,(unsigned)s_voice_dropped,(unsigned)s_voice_no_link,s_voice_last_error);
}
