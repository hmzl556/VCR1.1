/**
 * @file app_image_at.h
 * @brief UART1 AT access to committed label images.
 */
#ifndef APP_IMAGE_AT_H
#define APP_IMAGE_AT_H

#include <stdbool.h>
#include <stdint.h>

#define APP_IMAGE_AT_MAX_READ       112U
#define APP_IMAGE_AT_RESPONSE_SIZE  256U

typedef enum
{
    APP_IMAGE_AT_OK = 0,
    APP_IMAGE_AT_INVALID_FORMAT = 1,
    APP_IMAGE_AT_INVALID_PARAM = 2,
    APP_IMAGE_AT_READ_FAILED = 3,
    APP_IMAGE_AT_UNAVAILABLE = 4,
    APP_IMAGE_AT_UNMATCH = 5
} app_image_at_result_t;

typedef struct
{
    void *context;
    bool (*ready_send)(void *context, uint32_t length, uint32_t checksum);
    bool (*retry_schedule)(void *context, void (*task)(void *),
                           void *argument);
} app_image_at_ops_t;

void app_image_at_init(const app_image_at_ops_t *operations);
void app_image_at_ready(uint32_t length, uint32_t checksum);
bool app_image_at_event_pending(void);
void app_image_at_retry_pending(void);

app_image_at_result_t app_image_at_execute(bool uart_source,
                                           const char *command,
                                           const char *parameters,
                                           char *response,
                                           uint16_t response_size);

#endif
