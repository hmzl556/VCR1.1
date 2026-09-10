/**
 * @file app_image_protocol.h
 * @brief Bluetooth label image-transfer protocol core.
 */
#ifndef APP_IMAGE_PROTOCOL_H
#define APP_IMAGE_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE       1211U
#define IMAGE_PROTOCOL_MAX_PACKETS              255U
#define IMAGE_PROTOCOL_MAX_FILE_SIZE            (IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE * IMAGE_PROTOCOL_MAX_PACKETS)
#define IMAGE_PROTOCOL_MAX_RESPONSE_SIZE        (IMAGE_PROTOCOL_BITMAP_SIZE + 8U)
#define IMAGE_PROTOCOL_MAX_FRAME_SIZE           (IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE + 9U)
#define IMAGE_PROTOCOL_BITMAP_SIZE              32U

#define IMAGE_PROTOCOL_TYPE_LABEL               0x01U
#define IMAGE_PROTOCOL_TYPE_DATA                0x02U
#define IMAGE_PROTOCOL_TYPE_RESPONSE            0x04U

#define IMAGE_PROTOCOL_COMMAND_STORAGE          0x01U
#define IMAGE_PROTOCOL_COMMAND_PUSH             0x04U
#define IMAGE_PROTOCOL_COMMAND_DEVICE_MATCH     0x1AU

#define IMAGE_PROTOCOL_RESULT_SUCCESS           0xFFU
#define IMAGE_PROTOCOL_RESULT_FAILED            0x68U

typedef enum
{
    IMAGE_ACTION_SAVE_REFRESH = 0,
    IMAGE_ACTION_SAVE_ONLY = 1,
    IMAGE_ACTION_CAROUSEL = 2,
    IMAGE_ACTION_SCHEDULE = 3,
    IMAGE_ACTION_DELETE = 4,
} image_action_kind_t;

typedef struct
{
    image_action_kind_t kind;
    uint8_t slot;
    uint8_t parameter_1;
    uint8_t parameter_2;
    bool has_staged_image;
} image_action_t;

typedef enum
{
    IMAGE_PROTOCOL_TRACE_COMMAND,
    IMAGE_PROTOCOL_TRACE_DATA,
    IMAGE_PROTOCOL_TRACE_INVALID,
} image_protocol_trace_event_t;

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t encoded_size;
    uint8_t compression;
} image_metadata_t;

typedef struct
{
    void *context;
    bool (*storage_begin)(void *context, uint32_t size);
    bool (*storage_begin_async)(void *context, uint32_t size);
    bool (*storage_write)(void *context, uint32_t offset, const uint8_t *data, uint16_t length);
    bool (*storage_read)(void *context, uint32_t offset, uint8_t *data, uint16_t length);
    bool (*storage_prepare)(void *context, const image_action_t *action);
    void (*storage_apply)(void *context);
    void (*storage_abort)(void *context);
    bool (*display_prepare)(void *context, const image_metadata_t *metadata,
                            const image_action_t *action);
    bool (*display_write)(void *context, const uint8_t *data, uint16_t length);
    bool (*display_finish)(void *context);
    void (*display_apply)(void *context);
    void (*display_abort)(void *context);
    bool (*execute_action)(void *context, const image_action_t *action);
    bool (*notify)(void *context, const uint8_t *data, uint16_t length);
    void (*trace)(void *context, image_protocol_trace_event_t event,
                  uint16_t value1, uint16_t value2);
} image_protocol_ops_t;

typedef struct
{
    image_protocol_ops_t operations;
    uint8_t frame[IMAGE_PROTOCOL_MAX_FRAME_SIZE];
    uint16_t frame_length;
    uint16_t expected_frame_length;
    uint8_t sync_length;
    uint8_t reception_bitmap[IMAGE_PROTOCOL_BITMAP_SIZE];
    uint8_t pending_response[IMAGE_PROTOCOL_MAX_RESPONSE_SIZE];
    uint8_t pending_response_length;
    uint32_t expected_file_size;
    uint8_t expected_packets;
    bool notifications_enabled;
    bool session_active;
    bool staging_preparing;
} image_protocol_t;

void image_protocol_init(image_protocol_t *protocol, const image_protocol_ops_t *operations);
void image_protocol_set_notifications(image_protocol_t *protocol, bool enabled);
void image_protocol_feed(image_protocol_t *protocol, const uint8_t *data, uint16_t length);
bool image_protocol_retry_response(image_protocol_t *protocol);
bool image_protocol_response_pending(const image_protocol_t *protocol);
void image_protocol_disconnect(image_protocol_t *protocol);
bool image_protocol_session_active(const image_protocol_t *protocol);
void image_protocol_storage_ready(image_protocol_t *protocol, uint32_t size, bool success);
bool image_protocol_staging_preparing(const image_protocol_t *protocol);

#endif
