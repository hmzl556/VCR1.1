/**
 * @file app_image_service.c
 * @brief Application integration for the label image-transfer service.
 */
#include "app_image_service.h"

#include <string.h>

#include "app_cache_owner.h"
#include "app_image_staging.h"
#include "app_image_at.h"
#include "flash_manage.h"
#include "hal_flash.h"
#include "gap.h"
#include "app_ble.h"
#include "gatt.h"
#include "log.h"
#include "prf_image.h"
#include "rbuf_data.h"
#include "task.h"

#define IMAGE_RESPONSE_RETRY_INITIAL_MS  10U
#define IMAGE_RESPONSE_RETRY_MAX_MS      1000U
#define IMAGE_RETRY_FAILURE_REASON       0x13U
#define IMAGE_AT_RETRY_MS                50U

static image_protocol_t image_protocol;
static image_protocol_ops_t image_operations;
static uint16_t image_conhdl = GAP_INVALID_CONHDL;
static tid_t image_retry_id;
static uint32_t image_retry_delay_ms;
static bool image_notifications_enabled;
static bool image_release_after_notify;
static void (*image_staging_task)(void *argument);
static void *image_staging_task_argument;
static tid_t image_at_retry_id;
static void (*image_at_retry_task)(void *argument);
static void *image_at_retry_argument;
static task_handler_t *image_at_retry_fallback_task;
static bool image_committed_for_notify;
static bool image_at_busy_logged;


static void app_image_staging_task_bridge(uint8_t event, void *argument);

static void app_image_retry_task(uint8_t event, void *argument);

static void app_image_staging_task_bridge(uint8_t event, void *argument)
{
    void (*task)(void *argument) = image_staging_task;
    void *task_argument = image_staging_task_argument;

    (void)event;
    (void)argument;
    image_staging_task = NULL;
    image_staging_task_argument = NULL;
    if (task != NULL)
    {
        task(task_argument);
    }
}

static bool app_image_staging_schedule(void *context, void (*task)(void *),
                                       void *argument)
{
    (void)context;
    if (image_staging_task != NULL)
    {
        image_staging_task = task;
        image_staging_task_argument = argument;
        return true;
    }
    image_staging_task = task;
    image_staging_task_argument = argument;
    if (TASK_CREATE_ONESHOT(app_image_staging_task_bridge, NULL, 0U) == NULL)
    {
        image_staging_task = NULL;
        image_staging_task_argument = NULL;
        return false;
    }
    return true;
}

static void app_image_at_retry_bridge(uint8_t event, void *argument)
{
    void (*task)(void *) = image_at_retry_task;
    void *task_argument = image_at_retry_argument;

    (void)event;
    (void)argument;
    image_at_retry_id = 0U;
    image_at_retry_task = NULL;
    image_at_retry_argument = NULL;
    if (task != NULL)
    {
        task(task_argument);
    }
}

static bool app_image_at_retry_schedule(void *context, void (*task)(void *),
                                        void *argument)
{
    (void)context;
    if (image_at_retry_id != 0U)
    {
        return true;
    }
    image_at_retry_task = task;
    image_at_retry_argument = argument;
    if (!TIMER_CREATE_ONESHOT(&image_at_retry_id, IMAGE_AT_RETRY_MS,
                              NULL, app_image_at_retry_bridge))
    {
        image_at_retry_id = 0U;
        if (image_at_retry_fallback_task != NULL &&
            task_set_active(image_at_retry_fallback_task))
        {
            return true;
        }
        image_at_retry_task = NULL;
        image_at_retry_argument = NULL;
        return false;
    }
    return true;
}

static bool app_image_at_ready_send(void *context, uint32_t length,
                                    uint32_t checksum)
{
    uint32_t sent;

    (void)context;
    sent = trans_uart_event_ind("+IMGREADY:%lu,%08lX",
                                (unsigned long)length,
                                (unsigned long)checksum);
    if (sent == 0U)
    {
        if (!image_at_busy_logged)
        {
            BR_LOG_WARNING("[IMG][AT] ready event busy, retrying\r\n");
            image_at_busy_logged = true;
        }
        return false;
    }
    image_at_busy_logged = false;
    BR_LOG_INFO("[IMG][AT] ready length:%lu checksum:%08lx\r\n",
                (unsigned long)length, (unsigned long)checksum);
    return true;
}

static bool app_image_staging_erase(void *context, uint32_t offset)
{
    (void)context;
    return flash_app_cache_erase(offset) == HAL_FLASH_ERR_NONE;
}

static bool app_image_staging_write_adapter(void *context, uint32_t offset,
                                            const uint8_t *data, uint16_t length)
{
    (void)context;
    return flash_app_cache_write(offset, (uint8_t *)data, length) ==
           HAL_FLASH_ERR_NONE;
}

static bool app_image_staging_read_adapter(void *context, uint32_t offset,
                                           uint8_t *data, uint16_t length)
{
    (void)context;
    return flash_app_cache_read(offset, data, length) == HAL_FLASH_ERR_NONE;
}

static bool app_image_staging_ready(void *context, uint32_t size, bool success)
{
    (void)context;
    BR_LOG_INFO("[IMG] staging ready size:%lu success:%d\r\n",
                (unsigned long)size, success);
    image_protocol_storage_ready(&image_protocol, size, success);
    return true;
}

static bool app_image_storage_begin_async(void *context, uint32_t size)
{
    (void)context;
    if (image_release_after_notify || app_image_at_event_pending())
    {
        BR_LOG_WARNING("[IMG] staging blocked by pending completion\r\n");
        return false;
    }
    image_committed_for_notify = false;
    BR_LOG_INFO("[IMG] staging prepare size:%lu\r\n",
                (unsigned long)size);
    return app_image_staging_begin(size, app_image_staging_ready, NULL);
}

static bool app_image_storage_write(void *context, uint32_t offset,
                                    const uint8_t *data, uint16_t length)
{
    (void)context;
    return app_image_staging_write(offset, data, length);
}

static bool app_image_storage_read(void *context, uint32_t offset,
                                   uint8_t *data, uint16_t length)
{
    (void)context;
    return app_image_staging_read(offset, data, length);
}

static bool app_image_storage_prepare(void *context, const image_action_t *action)
{
    (void)context;
    if (action->slot != 1U ||
        (action->kind != IMAGE_ACTION_SAVE_REFRESH &&
         action->kind != IMAGE_ACTION_SAVE_ONLY))
    {
        BR_LOG_WARNING("[IMG] unsupported action:%d slot:%d\r\n",
                       action->kind, action->slot);
        return false;
    }
    return app_image_staging_commit();
}

static void app_image_storage_apply(void *context)
{
    (void)context;
    image_committed_for_notify = true;
}

static void app_image_storage_abort(void *context)
{
    (void)context;
    image_committed_for_notify = false;
    app_image_staging_cancel();
}

static bool app_image_display_prepare(void *context,
                                      const image_metadata_t *metadata,
                                      const image_action_t *action)
{
    (void)context;
    (void)metadata;
    (void)action;
    return true;
}

static bool app_image_display_write(void *context, const uint8_t *data,
                                    uint16_t length)
{
    (void)context;
    (void)data;
    (void)length;
    return true;
}

static bool app_image_display_finish(void *context)
{
    (void)context;
    return true;
}

static void app_image_display_apply(void *context)
{
    (void)context;
    BR_LOG_INFO("[IMG] display skipped (driver unavailable)\r\n");
}

static void app_image_display_abort(void *context)
{
    (void)context;
}

static bool app_image_error_retryable(uint32_t result)
{
    return result == GATT_RES_INSUFFICIENT_RESOURCES ||
           result == (uint32_t)GATT_RES_PENDING;
}

static void app_image_stop_retry(void)
{
    if (image_retry_id != 0U)
    {
        timer_delete(&image_retry_id);
        image_retry_id = 0U;
    }
}

static void app_image_fail_connection(void)
{
    BR_LOG_ERROR("[IMG] force disconnect conhdl:%04x\r\n", image_conhdl);
    app_image_stop_retry();
    image_notifications_enabled = false;
    image_protocol_disconnect(&image_protocol);
    if (app_cache_owner_get() == APP_CACHE_OWNER_IMAGE)
    {
        app_image_staging_cancel();
    }
    image_release_after_notify = false;
    image_committed_for_notify = false;
    prf_image_disconnect();
    if (image_conhdl != GAP_INVALID_CONHDL)
    {
        ble_gap_disconn(image_conhdl, IMAGE_RETRY_FAILURE_REASON);
    }
}

static void app_image_schedule_retry(void)
{
    if (!image_notifications_enabled || image_retry_id != 0U)
    {
        return;
    }
    if (!TIMER_CREATE_ONESHOT(&image_retry_id, image_retry_delay_ms,
                              NULL, app_image_retry_task))
    {
        BR_LOG_ERROR("[IMG] retry timer create failed delay:%dms\r\n",
                     image_retry_delay_ms);
        app_image_fail_connection();
        return;
    }
    if (image_retry_delay_ms < IMAGE_RESPONSE_RETRY_MAX_MS)
    {
        image_retry_delay_ms <<= 1U;
        if (image_retry_delay_ms > IMAGE_RESPONSE_RETRY_MAX_MS)
        {
            image_retry_delay_ms = IMAGE_RESPONSE_RETRY_MAX_MS;
        }
    }
}

static void app_image_retry_task(uint8_t event, void *argument)
{
    (void)event;
    (void)argument;

    image_retry_id = 0U;
    BR_LOG_INFO("[IMG] retry fired enabled:%d\r\n",
                image_notifications_enabled);
    if (image_notifications_enabled)
    {
        image_protocol_retry_response(&image_protocol);
    }
}

static bool app_image_notify(void *context, const uint8_t *data, uint16_t length)
{
    uint32_t result;
    bool terminal_push_response;

    (void)context;

    if (image_conhdl == GAP_INVALID_CONHDL || !image_notifications_enabled)
    {
        BR_LOG_WARNING("[IMG] notify blocked conhdl:%04x cccd:%d\r\n",
                       image_conhdl, image_notifications_enabled);
        return false;
    }
    terminal_push_response = length == 8U &&
                             data[6] == IMAGE_PROTOCOL_COMMAND_PUSH;
    if (length >= 8U)
    {
        BR_LOG_INFO("[IMG] rsp cmd:%02x result:%02x len:%d\r\n",
                    data[6], data[7], length);
    }
    if (terminal_push_response)
    {
        image_release_after_notify = true;
    }
    result = prf_image_send(image_conhdl, data, length);
    if (result == GATT_OK)
    {
        image_retry_delay_ms = IMAGE_RESPONSE_RETRY_INITIAL_MS;
        return true;
    }
    if (terminal_push_response)
    {
        image_release_after_notify = false;
    }
    if (app_image_error_retryable(result))
    {
        BR_LOG_WARNING("[IMG] notify busy result:%08x retry:%dms\r\n",
                       result, image_retry_delay_ms);
        app_image_schedule_retry();
    }
    else
    {
        BR_LOG_ERROR("[IMG] fatal notify result:%08x\r\n", result);
        app_image_fail_connection();
    }
    return false;
}

static void app_image_trace(void *context, image_protocol_trace_event_t event,
                            uint16_t value1, uint16_t value2)
{
    (void)context;

    if (event == IMAGE_PROTOCOL_TRACE_COMMAND)
    {
        BR_LOG_INFO("[IMG] frame command:%02x type:%02x\r\n", value1, value2);
    }
    else if (event == IMAGE_PROTOCOL_TRACE_DATA)
    {
        if (image_protocol_staging_preparing(&image_protocol))
        {
            BR_LOG_WARNING("[IMG] data dropped while staging packet:%d/%d\r\n",
                           value1, value2);
        }
        else if ((value1 == 1U) || (value1 == value2) ||
                 ((value1 & 0x0FU) == 0U))
        {
            BR_LOG_INFO("[IMG] frame data packet:%d/%d\r\n", value1, value2);
        }
    }
    else
    {
        BR_LOG_WARNING("[IMG] invalid frame type:%02x len:%d\r\n", value1, value2);
    }
}

void app_image_service_init(void)
{
    app_image_staging_ops_t staging_operations;
    app_image_staging_info_t staging_info;
    app_image_at_ops_t at_operations;

    app_cache_owner_init();
    memset(&staging_operations, 0, sizeof(staging_operations));
    staging_operations.erase = app_image_staging_erase;
    staging_operations.write = app_image_staging_write_adapter;
    staging_operations.read = app_image_staging_read_adapter;
    staging_operations.schedule = app_image_staging_schedule;
    image_staging_task = NULL;
    image_staging_task_argument = NULL;
    app_image_staging_init(&staging_operations);
    memset(&at_operations, 0, sizeof(at_operations));
    at_operations.ready_send = app_image_at_ready_send;
    at_operations.retry_schedule = app_image_at_retry_schedule;
    image_at_retry_id = 0U;
    image_at_retry_task = NULL;
    image_at_retry_argument = NULL;
    image_at_retry_fallback_task = task_create(app_image_at_retry_bridge,
                                               NULL, 0U, false, true);
    image_committed_for_notify = false;
    image_at_busy_logged = false;
    app_image_at_init(&at_operations);


    memset(&image_operations, 0, sizeof(image_operations));
    image_operations.storage_begin_async = app_image_storage_begin_async;
    image_operations.storage_write = app_image_storage_write;
    image_operations.storage_read = app_image_storage_read;
    image_operations.storage_prepare = app_image_storage_prepare;
    image_operations.storage_apply = app_image_storage_apply;
    image_operations.storage_abort = app_image_storage_abort;
    image_operations.display_prepare = app_image_display_prepare;
    image_operations.display_write = app_image_display_write;
    image_operations.display_finish = app_image_display_finish;
    image_operations.display_apply = app_image_display_apply;
    image_operations.display_abort = app_image_display_abort;
    image_operations.notify = app_image_notify;
    image_operations.trace = app_image_trace;
    image_conhdl = GAP_INVALID_CONHDL;
    image_retry_id = 0U;
    image_retry_delay_ms = IMAGE_RESPONSE_RETRY_INITIAL_MS;
    image_notifications_enabled = false;
    image_release_after_notify = false;
    image_protocol_init(&image_protocol, &image_operations);
    app_image_staging_info_get(&staging_info);
    BR_LOG_INFO("[IMG] service initialized adapters:1\r\n");
    BR_LOG_INFO("[IMG] staged valid:%d length:%lu checksum:%08lx\r\n",
                staging_info.valid, (unsigned long)staging_info.length,
                (unsigned long)staging_info.xtek_checksum);
}

bool app_image_service_set_adapters(const image_protocol_ops_t *operations)
{
    if ((operations == NULL) || (image_conhdl != GAP_INVALID_CONHDL) ||
        image_protocol_session_active(&image_protocol))
    {
        return false;
    }

    image_operations = *operations;
    image_operations.notify = app_image_notify;
    image_operations.trace = app_image_trace;
    image_protocol_init(&image_protocol, &image_operations);
    BR_LOG_INFO("[IMG] hardware adapters registered\r\n");
    return true;
}

void app_image_service_set_notifications(uint16_t conhdl, bool enabled)
{
    image_conhdl = conhdl;
    image_notifications_enabled = enabled;
    BR_LOG_INFO("[IMG] notifications conhdl:%04x enabled:%d pending:%d\r\n",
                conhdl, enabled,
                image_protocol_response_pending(&image_protocol));
    image_protocol_set_notifications(&image_protocol, enabled);
    if (!enabled)
    {
        app_image_stop_retry();
        image_retry_delay_ms = IMAGE_RESPONSE_RETRY_INITIAL_MS;
    }
    else if (image_protocol_response_pending(&image_protocol))
    {
        app_image_schedule_retry();
    }
}

void app_image_service_receive(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    if ((image_conhdl == conhdl) && (data != NULL))
    {
        image_protocol_feed(&image_protocol, data, length);
    }
    else
    {
        BR_LOG_WARNING("[IMG] rx ignored conhdl:%04x active:%04x len:%d\r\n",
                       conhdl, image_conhdl, length);
    }
}

void app_image_service_notification_complete(uint16_t conhdl)
{
    app_image_staging_info_t info;
    bool report_ready;

    if (image_conhdl == conhdl && image_release_after_notify)
    {
        BR_LOG_INFO("[IMG] terminal notify complete conhdl:%04x\r\n", conhdl);
        report_ready = image_committed_for_notify;
        app_image_staging_info_get(&info);
        image_release_after_notify = false;
        app_image_staging_release();
        image_committed_for_notify = false;
        if (report_ready && info.valid)
        {
            app_image_at_ready(info.length, info.xtek_checksum);
        }
    }
}

void app_image_service_disconnect(uint16_t conhdl)
{
    BR_LOG_INFO("[IMG] service disconnect conhdl:%04x active:%04x session:%d pending:%d\r\n",
                conhdl, image_conhdl,
                image_protocol_session_active(&image_protocol),
                image_protocol_response_pending(&image_protocol));
    if (image_conhdl == conhdl)
    {
        app_image_stop_retry();
        image_notifications_enabled = false;
        image_retry_delay_ms = IMAGE_RESPONSE_RETRY_INITIAL_MS;
        image_protocol_disconnect(&image_protocol);
        if (app_cache_owner_get() == APP_CACHE_OWNER_IMAGE)
        {
            app_image_staging_cancel();
        }
        image_release_after_notify = false;
        image_committed_for_notify = false;
        prf_image_disconnect();
        image_conhdl = GAP_INVALID_CONHDL;
    }
}
