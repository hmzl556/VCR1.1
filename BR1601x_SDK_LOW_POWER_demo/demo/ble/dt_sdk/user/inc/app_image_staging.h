/**
 * @file app_image_staging.h
 * @brief Persistent image-staging interface for the application cache.
 */
#ifndef APP_IMAGE_STAGING_H
#define APP_IMAGE_STAGING_H

#include <stdbool.h>
#include <stdint.h>

#define APP_IMAGE_STAGING_SECTOR_SIZE       4096U
#define APP_IMAGE_STAGING_TOTAL_SIZE        (240U * 1024U)
#define APP_IMAGE_STAGING_DATA_SIZE         (236U * 1024U)
#define APP_IMAGE_STAGING_METADATA_OFFSET   APP_IMAGE_STAGING_DATA_SIZE

typedef bool (*app_image_staging_ready_t)(void *context, uint32_t size, bool success);

typedef struct
{
    void *context;
    bool (*erase)(void *context, uint32_t offset);
    bool (*write)(void *context, uint32_t offset, const uint8_t *data, uint16_t length);
    bool (*read)(void *context, uint32_t offset, uint8_t *data, uint16_t length);
    bool (*schedule)(void *context, void (*task)(void *), void *argument);
} app_image_staging_ops_t;

typedef struct
{
    bool valid;
    uint32_t length;
    uint32_t xtek_checksum;
} app_image_staging_info_t;

void app_image_staging_init(const app_image_staging_ops_t *operations);
bool app_image_staging_begin(uint32_t size, app_image_staging_ready_t callback,
                             void *context);
bool app_image_staging_write(uint32_t offset, const uint8_t *data, uint16_t length);
bool app_image_staging_read(uint32_t offset, uint8_t *data, uint16_t length);
bool app_image_staging_commit(void);
void app_image_staging_cancel(void);
void app_image_staging_release(void);
bool app_image_staging_invalidate(void);
bool app_image_staging_info_get(app_image_staging_info_t *info);
bool app_image_staging_busy(void);

#endif
