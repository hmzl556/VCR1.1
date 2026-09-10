/**
 * @file app_image_service.h
 * @brief Application integration for the label image-transfer service.
 */
#ifndef APP_IMAGE_SERVICE_H
#define APP_IMAGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_image_protocol.h"

void app_image_service_init(void);
bool app_image_service_set_adapters(const image_protocol_ops_t *operations);
void app_image_service_set_notifications(uint16_t conhdl, bool enabled);
void app_image_service_receive(uint16_t conhdl, const uint8_t *data, uint16_t length);
void app_image_service_notification_complete(uint16_t conhdl);
void app_image_service_disconnect(uint16_t conhdl);

#endif
