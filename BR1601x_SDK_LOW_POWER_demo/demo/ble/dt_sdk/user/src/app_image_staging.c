/**
 * @file app_image_staging.c
 * @brief Asynchronous image erase, storage, and metadata persistence.
 */
#include "app_image_staging.h"
#include "app_cache_owner.h"

#include <string.h>

#define APP_IMAGE_STAGING_RECORD_SIZE       20U
#define APP_IMAGE_STAGING_RECORD_VERSION    1U
#define APP_IMAGE_STAGING_RECORD_VALID      0xA5U

static const uint8_t staging_magic[4] = {'I', 'M', 'G', 'R'};
static app_image_staging_ops_t staging_ops;
static app_image_staging_ready_t staging_ready_callback;
static void *staging_ready_context;
static uint32_t staging_size;
static uint32_t staging_erase_offset;
static uint32_t staging_erase_end;
static bool staging_preparing;
static bool staging_ready;
static app_image_staging_info_t staging_info;

static uint32_t app_image_staging_read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void app_image_staging_write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint32_t app_image_staging_crc32(const uint8_t *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint16_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static void app_image_staging_load_info(void)
{
    uint8_t record[APP_IMAGE_STAGING_RECORD_SIZE];
    uint32_t stored_crc;

    memset(&staging_info, 0, sizeof(staging_info));
    if (staging_ops.read == NULL ||
        !staging_ops.read(staging_ops.context, APP_IMAGE_STAGING_METADATA_OFFSET,
                          record, sizeof(record)))
    {
        return;
    }
    stored_crc = app_image_staging_read_le32(&record[16]);
    if (memcmp(record, staging_magic, sizeof(staging_magic)) != 0 ||
        record[4] != APP_IMAGE_STAGING_RECORD_VERSION ||
        record[5] != APP_IMAGE_STAGING_RECORD_VALID ||
        stored_crc != app_image_staging_crc32(record, 16U))
    {
        return;
    }
    staging_info.length = app_image_staging_read_le32(&record[8]);
    staging_info.xtek_checksum = app_image_staging_read_le32(&record[12]);
    staging_info.valid = staging_info.length != 0U &&
                         staging_info.length <= APP_IMAGE_STAGING_DATA_SIZE;
}

static void app_image_staging_finish(bool success)
{
    app_image_staging_ready_t callback = staging_ready_callback;
    void *context = staging_ready_context;
    uint32_t size = staging_size;

    staging_preparing = false;
    staging_ready = success;
    if (!success)
    {
        app_cache_release(APP_CACHE_OWNER_IMAGE);
    }
    if (callback != NULL)
    {
        callback(context, size, success);
    }
}

static void app_image_staging_step(void *argument)
{
    uint32_t offset;

    (void)argument;
    if (!staging_preparing)
    {
        return;
    }
    offset = staging_erase_offset;
    if (staging_ops.erase == NULL ||
        !staging_ops.erase(staging_ops.context, offset))
    {
        app_image_staging_finish(false);
        return;
    }
    if (offset == APP_IMAGE_STAGING_METADATA_OFFSET)
    {
        staging_erase_offset = 0U;
        memset(&staging_info, 0, sizeof(staging_info));
    }
    else
    {
        staging_erase_offset += APP_IMAGE_STAGING_SECTOR_SIZE;
    }
    if (staging_erase_offset >= staging_erase_end &&
        staging_erase_offset != APP_IMAGE_STAGING_METADATA_OFFSET)
    {
        app_image_staging_finish(true);
        return;
    }
    if (staging_ops.schedule == NULL ||
        !staging_ops.schedule(staging_ops.context, app_image_staging_step, NULL))
    {
        app_image_staging_finish(false);
    }
}

void app_image_staging_init(const app_image_staging_ops_t *operations)
{
    memset(&staging_ops, 0, sizeof(staging_ops));
    if (operations != NULL)
    {
        staging_ops = *operations;
    }
    staging_preparing = false;
    staging_ready = false;
    staging_size = 0U;
    app_image_staging_load_info();
}

bool app_image_staging_begin(uint32_t size, app_image_staging_ready_t callback,
                             void *context)
{
    bool task_pending = staging_preparing;

    if (size == 0U || size > APP_IMAGE_STAGING_DATA_SIZE ||
        !app_cache_acquire(APP_CACHE_OWNER_IMAGE))
    {
        return false;
    }
    if (staging_preparing && staging_size == size)
    {
        return true;
    }
    staging_size = size;
    staging_erase_end = (size + APP_IMAGE_STAGING_SECTOR_SIZE - 1U) &
                        ~(APP_IMAGE_STAGING_SECTOR_SIZE - 1U);
    staging_erase_offset = APP_IMAGE_STAGING_METADATA_OFFSET;
    staging_ready_callback = callback;
    staging_ready_context = context;
    staging_preparing = true;
    staging_ready = false;
    if (task_pending)
    {
        return true;
    }
    if (staging_ops.schedule == NULL ||
        !staging_ops.schedule(staging_ops.context, app_image_staging_step, NULL))
    {
        app_image_staging_finish(false);
        return false;
    }
    return true;
}

bool app_image_staging_write(uint32_t offset, const uint8_t *data, uint16_t length)
{
    return staging_ready && data != NULL && length != 0U &&
           offset + length <= staging_size && staging_ops.write != NULL &&
           staging_ops.write(staging_ops.context, offset, data, length);
}

bool app_image_staging_read(uint32_t offset, uint8_t *data, uint16_t length)
{
    uint32_t readable_size = staging_ready ? staging_size : staging_info.length;

    return !staging_preparing && (staging_ready || staging_info.valid) &&
           data != NULL && length != 0U &&
           offset <= readable_size && length <= readable_size - offset &&
           staging_ops.read != NULL &&
           staging_ops.read(staging_ops.context, offset, data, length);
}

bool app_image_staging_commit(void)
{
    uint8_t record[APP_IMAGE_STAGING_RECORD_SIZE];
    uint8_t checksum[4];

    if (!staging_ready || staging_ops.read == NULL || staging_ops.write == NULL ||
        !staging_ops.read(staging_ops.context, 4U, checksum, sizeof(checksum)))
    {
        return false;
    }
    memset(record, 0, sizeof(record));
    memcpy(record, staging_magic, sizeof(staging_magic));
    record[4] = APP_IMAGE_STAGING_RECORD_VERSION;
    record[5] = APP_IMAGE_STAGING_RECORD_VALID;
    app_image_staging_write_le32(&record[8], staging_size);
    staging_info.xtek_checksum = ((uint32_t)checksum[0] << 24U) |
                                 ((uint32_t)checksum[1] << 16U) |
                                 ((uint32_t)checksum[2] << 8U) | checksum[3];
    app_image_staging_write_le32(&record[12], staging_info.xtek_checksum);
    app_image_staging_write_le32(&record[16], app_image_staging_crc32(record, 16U));
    if (!staging_ops.write(staging_ops.context, APP_IMAGE_STAGING_METADATA_OFFSET,
                           record, sizeof(record)))
    {
        return false;
    }
    staging_info.valid = true;
    staging_info.length = staging_size;
    return true;
}

void app_image_staging_cancel(void)
{
    staging_preparing = false;
    staging_ready = false;
    staging_size = 0U;
    app_cache_release(APP_CACHE_OWNER_IMAGE);
}

void app_image_staging_release(void)
{
    staging_ready = false;
    app_cache_release(APP_CACHE_OWNER_IMAGE);
}

bool app_image_staging_invalidate(void)
{
    uint8_t invalid = 0U;

    if (!staging_info.valid)
    {
        return true;
    }
    if (staging_ops.write == NULL ||
        !staging_ops.write(staging_ops.context,
                           APP_IMAGE_STAGING_METADATA_OFFSET + 5U,
                           &invalid, sizeof(invalid)))
    {
        return false;
    }
    memset(&staging_info, 0, sizeof(staging_info));
    return true;
}

bool app_image_staging_info_get(app_image_staging_info_t *info)
{
    if (info == NULL)
    {
        return false;
    }
    *info = staging_info;
    return true;
}

bool app_image_staging_busy(void)
{
    return app_cache_owner_get() != APP_CACHE_OWNER_NONE;
}
