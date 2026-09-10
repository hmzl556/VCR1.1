/**
 * @file app_label_info.h
 * @brief Persistent Bluetooth label information.
 */
#ifndef APP_LABEL_INFO_H
#define APP_LABEL_INFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t hardware_version;
    uint16_t device_code;
    uint8_t wake_period;
    uint16_t group;
    int8_t temperature;
    uint8_t custom;
} app_label_info_t;

bool app_label_info_get(app_label_info_t *info);
bool app_label_info_set(const app_label_info_t *info);

#endif
