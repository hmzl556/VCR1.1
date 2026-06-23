#ifndef _BLECOMM_H_
#define _BLECOMM_H_

#include <stdint.h>

void bleSendVoiceSbcFrame(const uint8_t *buf, uint16_t len);
void bleResetVoiceSbcSeq(void);
void bleSendVoiceStartFrame(void);
void bleSendVoiceEndFrame(void);

#endif /* _BLECOMM_H_ */
