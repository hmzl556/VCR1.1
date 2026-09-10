/**
 * @file app_label_info.c
 * @brief Persistent Bluetooth label information.
 */
#include "app_label_info.h"

#include <string.h>

#include "app_label_product.h"
#include "cfg.h"

#define LABEL_INFO_MAGIC_0                    'L'
#define LABEL_INFO_MAGIC_1                    'B'
#define LABEL_INFO_LEGACY_VERSION             1U
#define LABEL_INFO_VERSION                    2U
#define LABEL_INFO_RECORD_SIZE                13U

static uint8_t app_label_info_checksum(const uint8_t *record)
{
    uint8_t index;
    uint8_t checksum = 0;

    for (index = 0; index < LABEL_INFO_RECORD_SIZE - 1; index++)
    {
        checksum = (uint8_t)(checksum + record[index]);
    }
    return checksum;
}

bool app_label_info_get(app_label_info_t *info)
{
    uint8_t record[LABEL_INFO_RECORD_SIZE];

    if (info == NULL)
    {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->hardware_version = APP_LABEL_HARDWARE_VERSION;
    info->device_code = APP_LABEL_DEVICE_CODE;
    if (cfg_reserver_get(LABEL_INFO_OFF, record, sizeof(record)) != sizeof(record))
    {
        return false;
    }
    if ((record[0] != LABEL_INFO_MAGIC_0) ||
        (record[1] != LABEL_INFO_MAGIC_1) ||
        ((record[2] != LABEL_INFO_VERSION) &&
         (record[2] != LABEL_INFO_LEGACY_VERSION)) ||
        (record[3] != LABEL_INFO_RECORD_SIZE) ||
        (record[12] != app_label_info_checksum(record)))
    {
        return false;
    }

    info->hardware_version = record[4];
    info->device_code = ((uint16_t)record[5] << 8) | record[6];
    info->wake_period = record[7];
    info->group = ((uint16_t)record[8] << 8) | record[9];
    info->temperature = (int8_t)record[10];
    info->custom = record[11];
    if (record[2] == LABEL_INFO_LEGACY_VERSION)
    {
        info->hardware_version = APP_LABEL_HARDWARE_VERSION;
        info->device_code = APP_LABEL_DEVICE_CODE;
    }
    return true;
}

bool app_label_info_set(const app_label_info_t *info)
{
    uint8_t record[LABEL_INFO_RECORD_SIZE];

    if (info == NULL)
    {
        return false;
    }

    record[0] = LABEL_INFO_MAGIC_0;
    record[1] = LABEL_INFO_MAGIC_1;
    record[2] = LABEL_INFO_VERSION;
    record[3] = LABEL_INFO_RECORD_SIZE;
    record[4] = info->hardware_version;
    record[5] = (uint8_t)(info->device_code >> 8);
    record[6] = (uint8_t)info->device_code;
    record[7] = info->wake_period;
    record[8] = (uint8_t)(info->group >> 8);
    record[9] = (uint8_t)info->group;
    record[10] = (uint8_t)info->temperature;
    record[11] = info->custom;
    record[12] = app_label_info_checksum(record);
    return cfg_reserver_set(LABEL_INFO_OFF, record, sizeof(record)) == sizeof(record);
}
