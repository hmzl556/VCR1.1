/**
 * @file app_label_adv_runtime.c
 * @brief Runtime Bluetooth label advertisement integration.
 */
#include "app_label_adv_runtime.h"

#include <string.h>

#include "app_adc.h"
#include "app_label_adv.h"
#include "app_label_info.h"
#include "app_label_product.h"
#include "ble_adv.h"
#include "cfg.h"

uint8_t app_label_adv_data_get(uint8_t *output, uint8_t output_size)
{
    app_label_info_t stored_info;
    label_adv_info_t info;
    uint8_t address[6] = {0};
    uint8_t tx_power = 0;
    uint8_t auth = 0;
    uint8_t index;

    memset(&info, 0, sizeof(info));
    app_label_info_get(&stored_info);
    cfg_get(CFG_TYPE_ADDR, address, sizeof(address));
    cfg_get(CFG_TYPE_TXPOWER, &tx_power, sizeof(tx_power));
    cfg_reserver_get(AUTH_OFF, &auth, AUTH_LEN);

    info.report_type = 0xFE;
    info.hardware_version = stored_info.hardware_version;
    info.software_version = APP_LABEL_SOFTWARE_VERSION;
    info.device_code = stored_info.device_code;
    info.battery_percent = adc_voltage_to_percentage(adc_read_battery_voltage());
    info.tx_power = (uint8_t)(0x10 | (tx_power & 0x0F));
    info.security_level = auth ? 3 : 1;
    info.wake_period = stored_info.wake_period;
    info.group = stored_info.group;
    info.temperature = stored_info.temperature;
    info.custom = stored_info.custom;
    for (index = 0; index < sizeof(info.mac); index++)
    {
        info.mac[index] = address[sizeof(address) - 1 - index];
    }

    return label_adv_build(&info, output, output_size);
}

void app_label_adv_refresh(void)
{
    ble_adv_data_t data;

    data.adv_data_len = app_label_adv_data_get(data.adv_data,
                                               sizeof(data.adv_data));
    if (data.adv_data_len != 0)
    {
        ble_adv_data_set(&data);
    }
}
