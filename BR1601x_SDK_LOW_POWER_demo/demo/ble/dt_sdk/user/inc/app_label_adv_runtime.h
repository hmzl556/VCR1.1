/**
 * @file app_label_adv_runtime.h
 * @brief Runtime Bluetooth label advertisement integration.
 */
#ifndef APP_LABEL_ADV_RUNTIME_H
#define APP_LABEL_ADV_RUNTIME_H

#include <stdint.h>

uint8_t app_label_adv_data_get(uint8_t *output, uint8_t output_size);
void app_label_adv_refresh(void);

#endif
