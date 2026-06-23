#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "FreeRTOS.h"
#include <stdint.h>

typedef struct {
    uint8_t  result;
    uint32_t pack_index;
} cat1_ble_evt_t;

void app_rtos_init(void);
void app_rtos_notify_dma_from_isr(BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t app_rtos_take_dma_notify(void);

BaseType_t cat1_post_udp_done(uint8_t result, uint32_t pack_index);
void cat1_ble_process_pending(void);

#endif /* APP_RTOS_H */
