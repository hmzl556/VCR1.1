/**
 * @file app_image_at.c
 * @brief Text AT formatting for persistent label-image access.
 */
#include "app_image_at.h"

#include <stdio.h>
#include <string.h>

#include "app_image_staging.h"

static app_image_at_ops_t image_at_ops;
static uint32_t image_at_ready_length;
static uint32_t image_at_ready_checksum;
static bool image_at_ready_pending;
static bool image_at_retry_scheduled;

static void app_image_at_retry(void *argument);

static void app_image_at_try_ready(void)
{
    if (!image_at_ready_pending)
    {
        return;
    }
    if (image_at_ops.ready_send != NULL &&
        image_at_ops.ready_send(image_at_ops.context, image_at_ready_length,
                                image_at_ready_checksum))
    {
        image_at_ready_pending = false;
        return;
    }
    if (!image_at_retry_scheduled &&
        image_at_ops.retry_schedule != NULL)
    {
        image_at_retry_scheduled = image_at_ops.retry_schedule(
            image_at_ops.context, app_image_at_retry, NULL);
    }
}

static void app_image_at_retry(void *argument)
{
    (void)argument;
    image_at_retry_scheduled = false;
    app_image_at_try_ready();
}

void app_image_at_init(const app_image_at_ops_t *operations)
{
    memset(&image_at_ops, 0, sizeof(image_at_ops));
    if (operations != NULL)
    {
        image_at_ops = *operations;
    }
    image_at_ready_length = 0U;
    image_at_ready_checksum = 0U;
    image_at_ready_pending = false;
    image_at_retry_scheduled = false;
}

void app_image_at_ready(uint32_t length, uint32_t checksum)
{
    image_at_ready_length = length;
    image_at_ready_checksum = checksum;
    image_at_ready_pending = true;
    app_image_at_try_ready();
}

bool app_image_at_event_pending(void)
{
    return image_at_ready_pending;
}

void app_image_at_retry_pending(void)
{
    app_image_at_try_ready();
}

app_image_at_result_t app_image_at_execute(bool uart_source,
                                           const char *command,
                                           const char *parameters,
                                           char *response,
                                           uint16_t response_size)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t data[APP_IMAGE_AT_MAX_READ];
    unsigned long offset;
    unsigned long requested;
    uint32_t actual;
    int consumed;
    uint16_t index;
    app_image_staging_info_t info;
    int length;

    if (!uart_source)
    {
        return APP_IMAGE_AT_UNMATCH;
    }
    if (command == NULL || parameters == NULL || response == NULL ||
        response_size == 0U)
    {
        return APP_IMAGE_AT_INVALID_FORMAT;
    }
    if (strcmp(command, "IMGREAD") == 0)
    {
        consumed = 0;
        if (parameters[0] != '=' ||
            sscanf(&parameters[1], "%lu,%lu%n", &offset, &requested,
                   &consumed) != 2 ||
            parameters[1 + consumed] != 0 || requested == 0U ||
            requested > APP_IMAGE_AT_MAX_READ)
        {
            return APP_IMAGE_AT_INVALID_PARAM;
        }
        if (app_image_staging_busy())
        {
            return APP_IMAGE_AT_UNAVAILABLE;
        }
        if (!app_image_staging_info_get(&info) || !info.valid)
        {
            return APP_IMAGE_AT_UNAVAILABLE;
        }
        if (offset >= info.length)
        {
            return APP_IMAGE_AT_INVALID_PARAM;
        }
        actual = info.length - (uint32_t)offset;
        if (actual > requested)
        {
            actual = (uint32_t)requested;
        }
        if (!app_image_staging_read((uint32_t)offset, data,
                                    (uint16_t)actual))
        {
            return APP_IMAGE_AT_READ_FAILED;
        }
        length = snprintf(response, response_size, "+IMGREAD:%lu,%lu,",
                          offset, (unsigned long)actual);
        if (length < 0 || (uint32_t)length + actual * 2U >= response_size)
        {
            return APP_IMAGE_AT_READ_FAILED;
        }
        for (index = 0U; index < actual; index++)
        {
            response[length++] = hex[data[index] >> 4U];
            response[length++] = hex[data[index] & 0x0FU];
        }
        response[length] = 0;
        return APP_IMAGE_AT_OK;
    }
    if (strcmp(command, "IMGINFO") != 0 ||
        strcmp(parameters, "?") != 0)
    {
        return APP_IMAGE_AT_UNMATCH;
    }
    if (app_image_staging_busy())
    {
        return APP_IMAGE_AT_UNAVAILABLE;
    }
    if (!app_image_staging_info_get(&info) || !info.valid)
    {
        return APP_IMAGE_AT_UNAVAILABLE;
    }
    length = snprintf(response, response_size, "+IMGINFO:%lu,%08lX",
                      (unsigned long)info.length,
                      (unsigned long)info.xtek_checksum);
    if (length < 0 || (uint16_t)length >= response_size)
    {
        return APP_IMAGE_AT_READ_FAILED;
    }
    return APP_IMAGE_AT_OK;
}
