#ifndef __APP_VOICE_CTRL_H__
#define __APP_VOICE_CTRL_H__

#include <stdint.h>
#include "FreeRTOS.h"

#define VOICE_CMD_START   (0U)
#define VOICE_CMD_STOP    (1U)

void voice_ctrl_init(void);
BaseType_t voice_ctrl_post_start(void);
BaseType_t voice_ctrl_post_stop(void);
void voice_ctrl_process_pending(void);

#endif /* __APP_VOICE_CTRL_H__ */
