/**
 * @file app_label_adv.c
 * @brief Bluetooth label advertisement data builder.
 */
#include "app_label_adv.h"

#include <stddef.h>

static void label_adv_write_u16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8);
    output[1] = (uint8_t)value;
}

uint8_t label_adv_build(const label_adv_info_t *info, uint8_t *output,
                        uint8_t output_size)
{
    uint8_t index;

    if ((info == NULL) || (output == NULL) ||
        (output_size < LABEL_ADV_DATA_SIZE))
    {
        return 0;
    }

    output[0] = 0x02;
    output[1] = 0x01;
    output[2] = 0x06;
    output[3] = 0x17;
    output[4] = 0xFF;
    output[5] = 'X';
    output[6] = 'R';
    output[7] = info->report_type;
    output[8] = info->hardware_version;
    label_adv_write_u16(&output[9], info->software_version);
    label_adv_write_u16(&output[11], info->device_code);
    output[13] = info->battery_percent;
    output[14] = info->tx_power;
    output[15] = info->security_level;
    output[16] = info->wake_period;
    label_adv_write_u16(&output[17], info->group);
    output[19] = (uint8_t)info->temperature;
    output[20] = info->custom;
    for (index = 0; index < 6; index++)
    {
        output[21 + index] = info->mac[index];
    }

    return LABEL_ADV_DATA_SIZE;
}
