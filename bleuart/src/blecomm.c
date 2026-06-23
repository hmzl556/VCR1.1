#include <string.h>
#include "gmem.h"
#include "cs_core.h"

#define VOICE_SBC_TS_STEP_MS  (8U)

static uint32_t g_voice_sbc_seq = 0;
static uint32_t g_voice_sbc_ts = 0;
static uint8_t s_voice_sbc_packet[247];

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

	gatts_chr_val_notif(dtss_peer, dtss_chr, (uint8_t *)payload, len);
}

void bleSendVoiceSbcFrame(const uint8_t *buf, uint16_t len)
{
	uint32_t seq;
	uint32_t timestamp;
	uint16_t total_len;

	if (len == 0) return;
	if (bleConnectOk != 0xa6 || !bleSendEn) return;
	if (!dtss_peer || !dtss_chr) return;

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
	gatts_chr_val_notif(dtss_peer, dtss_chr, s_voice_sbc_packet, (uint8_t)total_len);
}

void bleResetVoiceSbcSeq(void)
{
	g_voice_sbc_seq = 0;
	g_voice_sbc_ts = 0;
}

void bleSendVoiceStartFrame(void)
{
	bleSendVoiceStateFrame(0x01);
}

void bleSendVoiceEndFrame(void)
{
	bleSendVoiceStateFrame(0x02);
}
