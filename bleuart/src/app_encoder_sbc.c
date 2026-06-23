#include "app_encoder_sbc.h"
#include "sbc.h"
#include "co_printf.h"
#include <string.h>
#include <stdbool.h>

/* Output raw buffer helper from BSP */
extern void STDOUT(uint8_t *_buf, uint32_t _dlen);
#include "blecomm.h"
#include "gmem.h"

static sbc_t g_sbc;					// SBC编码器实例
static bool g_sbc_inited = false;
static uint8_t g_pcm_acc[512];		// PCM数据累积缓冲区，大小应至少为sbc_get_codesize()的两倍以确保有足够空间存储未编码的PCM数据
static uint16_t g_pcm_acc_len = 0;	// 当前累积的PCM数据长度
static uint8_t g_sbc_out[128];		// SBC编码输出缓冲区，大小应至少为sbc_get_frame_length()以确保有足够空间存储一帧SBC数据

static void PrintHexLine(const uint8_t *data, uint32_t len)
{
	/* 热路径不使用 co_printf：串口打印会拖慢 taskVoice，导致 SBC 突发发送、语速变快 */
	if (bleConnectOk == 0xa6 && bleSendEn) {
		//co_printf("SBC frame encoded, len=%u\n", (unsigned)len);
		bleSendVoiceSbcFrame(data, (uint16_t)len);
	} else {
		//STDOUT((uint8_t *)data, len);
		// co_printf("Voice is OK, but ble not connected! ok=0x%02X en=0x%02X peer=%p chr=%p\n",
		// 	bleConnectOk, bleSendEn, (void *)dtss_peer, (void *)dtss_chr);
	}
}

static void Encoder_Sbc_ApplyParams(void)
{
	g_sbc.frequency = SBC_FREQ_16000;
	g_sbc.blocks    = SBC_BLK_16;
	g_sbc.mode      = SBC_MODE_MONO;
	g_sbc.allocation = SBC_AM_LOUDNESS;
	g_sbc.subbands  = SBC_SB_8;
	g_sbc.bitpool   = 30;
}

void Encoder_Sbc_Init(void)
{
	int sbc_status = sbc_init(&g_sbc, 0);
	if (sbc_status == 0) {
		Encoder_Sbc_ApplyParams();
		g_sbc_inited = true;
/* 		co_printf("SBC init ok, codesize=%u, framelen=%u\n",
		         (unsigned)sbc_get_codesize(&g_sbc),
		         (unsigned)sbc_get_frame_length(&g_sbc)); */
	} else {
		co_printf("SBC init failed, error code: %d\n", sbc_status);
	}
}

void Encoder_Sbc_Enable(void)
{
	if (!g_sbc_inited) {	//如果SBC未初始化，则初始化SBC
		Encoder_Sbc_Init();
	}
	g_pcm_acc_len = 0U;		//清空PCM数据累积缓冲区
	memset(g_pcm_acc, 0, sizeof(g_pcm_acc));	//清空PCM数据累积缓冲区
}

void Encoder_Sbc_Disable(void)
{
	g_pcm_acc_len = 0U;		//清空PCM数据累积缓冲区
	memset(g_pcm_acc, 0, sizeof(g_pcm_acc));	//清空PCM数据累积缓冲区
	if (g_sbc_inited) {
		sbc_finish(&g_sbc);	//释放SBC资源
		g_sbc_inited = false;		//设置SBC未初始化
	}
}

void Encoder_Sbc_ProcessPcmBlock(const __IO int16_t* data, uint16_t count)
{
	uint16_t i;
	uint16_t bytes_to_add = (uint16_t)(count * 2U);
	uint32_t codesize;
	uint32_t framelen;
	ssize_t out_written;

	if (!g_sbc_inited) {
		return;
	}

	// 获取SBC编码器的codesize和framelen信息
	codesize = (uint32_t)sbc_get_codesize(&g_sbc);
	framelen = (uint32_t)sbc_get_frame_length(&g_sbc);
	if (codesize == 0U || framelen == 0U || framelen > sizeof(g_sbc_out)) {
		return;
	}

	// 如果累积缓冲区即将溢出，重置累积长度以丢弃过多的PCM数据，避免内存越界
	if ((uint32_t)g_pcm_acc_len + (uint32_t)bytes_to_add > sizeof(g_pcm_acc)) {
		g_pcm_acc_len = 0;
		}

	// 将新的PCM数据添加到累积缓冲区
	for (i = 0; i < count; i++) {
		int16_t s = data[i];
		g_pcm_acc[g_pcm_acc_len++] = (uint8_t)(s & 0xFF);
		g_pcm_acc[g_pcm_acc_len++] = (uint8_t)((s >> 8) & 0xFF);
	}

	// 当累积的PCM数据长度足够编码一帧SBC时，进行编码处理
	while ((uint32_t)g_pcm_acc_len >= codesize) {
		out_written = 0;
		if (sbc_encode(&g_sbc,
		               (const void *)g_pcm_acc, (size_t)codesize,
		               (void *)g_sbc_out, (size_t)framelen,
		               &out_written) < 0) {
			memmove(g_pcm_acc, g_pcm_acc + codesize, (size_t)(g_pcm_acc_len - codesize));
			g_pcm_acc_len = (uint16_t)(g_pcm_acc_len - (uint16_t)codesize);
			continue;
		}
		if (out_written > 0) {
			PrintHexLine(g_sbc_out, (uint32_t)out_written);
		}
		memmove(g_pcm_acc, g_pcm_acc + codesize, (size_t)(g_pcm_acc_len - codesize));
		g_pcm_acc_len = (uint16_t)(g_pcm_acc_len - (uint16_t)codesize);
	}
}
