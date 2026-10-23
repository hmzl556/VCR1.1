#ifndef __APP_ENCODER_SBC_H__
#define __APP_ENCODER_SBC_H__

#include <stdint.h>
#include "sys/types.h"
#include "br1601x.h"

void Encoder_Sbc_Init(void);
void Encoder_Sbc_Enable(void);
int Encoder_Sbc_IsReady(void);
void Encoder_Sbc_Disable(void);
void Encoder_Sbc_ProcessPcmBlock(const __IO int16_t* data, uint16_t count);

#endif /* __APP_ENCODER_SBC_H__ */
