/**
 * @file app_label_adv.h
 * @brief Bluetooth label advertisement data builder.
 */
#ifndef APP_LABEL_ADV_H
#define APP_LABEL_ADV_H

#include <stdint.h>

#define LABEL_ADV_DATA_SIZE                    27U

typedef struct
{
    uint8_t report_type;
    uint8_t hardware_version;
    uint16_t software_version;
    uint16_t device_code;
    uint8_t battery_percent;
    uint8_t tx_power;
    uint8_t security_level;
    uint8_t wake_period;
    uint16_t group;
    int8_t temperature;
    uint8_t custom;
    uint8_t mac[6];
} label_adv_info_t;

uint8_t label_adv_build(const label_adv_info_t *info, uint8_t *output,
                        uint8_t output_size);

#endif
