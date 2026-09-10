/**
 * @file app_image_protocol.c
 * @brief Bluetooth label image-transfer protocol core.
 */

#include "app_image_protocol.h"

#include <string.h>

#define IMAGE_PROTOCOL_COMMAND_HEADER_SIZE  7U
#define IMAGE_PROTOCOL_DATA_HEADER_SIZE     9U

static uint32_t image_protocol_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           data[3];
}

static uint8_t image_protocol_sum(const uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;

    while (length-- != 0U)
    {
        checksum = (uint8_t)(checksum + *data++);
    }
    return checksum;
}

static void image_protocol_reset_frame(image_protocol_t *protocol)
{
    protocol->frame_length = 0;
    protocol->expected_frame_length = 0;
}

bool image_protocol_retry_response(image_protocol_t *protocol)
{
    if (protocol->pending_response_length == 0U)
    {
        return true;
    }
    if ((protocol->operations.notify != NULL) &&
        protocol->operations.notify(protocol->operations.context,
            protocol->pending_response, protocol->pending_response_length))
    {
        protocol->pending_response_length = 0U;
        return true;
    }
    return false;
}

static void image_protocol_queue_response(image_protocol_t *protocol,
                                          const uint8_t *response, uint8_t length)
{
    if (length <= sizeof(protocol->pending_response))
    {
        memcpy(protocol->pending_response, response, length);
        protocol->pending_response_length = length;
        image_protocol_retry_response(protocol);
    }
}

static void image_protocol_send_result(image_protocol_t *protocol, uint8_t command,
                                       uint8_t result)
{
    uint8_t response[8] = {
        'X', 'T', 'E', IMAGE_PROTOCOL_TYPE_RESPONSE, 8U, 0U, command, result
    };

    response[5] = image_protocol_sum(&response[6], 2U);
    image_protocol_queue_response(protocol, response, sizeof(response));
}

static void image_protocol_send_missing(image_protocol_t *protocol)
{
    uint8_t response[8U + IMAGE_PROTOCOL_BITMAP_SIZE];
    uint8_t bitmap_length = (uint8_t)((protocol->expected_packets + 7U) / 8U);
    uint8_t unused_bits = (uint8_t)(bitmap_length * 8U - protocol->expected_packets);
    uint8_t total_length = (uint8_t)(8U + bitmap_length);

    response[0] = 'X';
    response[1] = 'T';
    response[2] = 'E';
    response[3] = IMAGE_PROTOCOL_TYPE_RESPONSE;
    response[4] = total_length;
    response[5] = 0U;
    response[6] = IMAGE_PROTOCOL_COMMAND_PUSH;
    response[7] = IMAGE_PROTOCOL_RESULT_FAILED;
    memcpy(&response[8], protocol->reception_bitmap, bitmap_length);
    if (unused_bits != 0U)
    {
        response[total_length - 1U] |= (uint8_t)((1U << unused_bits) - 1U);
    }
    response[5] = image_protocol_sum(&response[6], (uint16_t)(total_length - 6U));
    image_protocol_queue_response(protocol, response, total_length);
}

static bool image_protocol_command_valid(const uint8_t *frame, uint16_t length)
{
    return length >= IMAGE_PROTOCOL_COMMAND_HEADER_SIZE &&
           frame[3] == IMAGE_PROTOCOL_TYPE_LABEL &&
           frame[4] == length &&
           frame[5] == image_protocol_sum(&frame[6], (uint16_t)(length - 6U));
}

static void image_protocol_storage_request(image_protocol_t *protocol,
                                           const uint8_t *frame, uint16_t length)
{
    uint32_t requested_size;
    uint8_t packet_count;

    if (!protocol->notifications_enabled || length != 11U)
    {
        return;
    }
    requested_size = image_protocol_read_be32(&frame[7]);
    if (requested_size == 0U || requested_size > IMAGE_PROTOCOL_MAX_FILE_SIZE ||
        (protocol->operations.storage_begin_async == NULL &&
         protocol->operations.storage_begin == NULL))
    {
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                                   IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }
    if (protocol->staging_preparing &&
        protocol->expected_file_size == requested_size)
    {
        return;
    }
    if (protocol->session_active)
    {
        if (protocol->operations.storage_abort != NULL)
        {
            protocol->operations.storage_abort(protocol->operations.context);
        }
        protocol->session_active = false;
    }
    if (protocol->operations.storage_begin_async != NULL)
    {
        if (!protocol->operations.storage_begin_async(
                protocol->operations.context, requested_size))
        {
            image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                                       IMAGE_PROTOCOL_RESULT_FAILED);
            return;
        }
        protocol->expected_file_size = requested_size;
        protocol->staging_preparing = true;
        return;
    }
    if (!protocol->operations.storage_begin(protocol->operations.context, requested_size))
    {
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                                   IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }

    packet_count = (uint8_t)((requested_size + IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE - 1U) /
                             IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE);
    protocol->expected_file_size = requested_size;
    protocol->expected_packets = packet_count;
    memset(protocol->reception_bitmap, 0, sizeof(protocol->reception_bitmap));
    protocol->session_active = true;
    image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                               IMAGE_PROTOCOL_RESULT_SUCCESS);
}

static void image_protocol_device_match_request(image_protocol_t *protocol,
                                                uint16_t length)
{
    image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_DEVICE_MATCH,
        (length == 8U) ? IMAGE_PROTOCOL_RESULT_SUCCESS : IMAGE_PROTOCOL_RESULT_FAILED);
}


static bool image_protocol_packet_received(const image_protocol_t *protocol,
                                           uint8_t packet_number)
{
    uint8_t index = packet_number;

    return (protocol->reception_bitmap[index / 8U] &
            (uint8_t)(0x80U >> (index % 8U))) != 0U;
}

static void image_protocol_mark_packet(image_protocol_t *protocol, uint8_t packet_number)
{
    uint8_t index = packet_number;

    protocol->reception_bitmap[index / 8U] |= (uint8_t)(0x80U >> (index % 8U));
}

static bool image_protocol_all_packets_received(const image_protocol_t *protocol)
{
    uint16_t packet_number;

    for (packet_number = 0U; packet_number < protocol->expected_packets; packet_number++)
    {
        if (!image_protocol_packet_received(protocol, (uint8_t)packet_number))
        {
            return false;
        }
    }
    return true;
}

static void image_protocol_data_packet(image_protocol_t *protocol,
                                       const uint8_t *frame, uint16_t length)
{
    uint8_t total_packets;
    uint8_t packet_number;
    uint16_t payload_length;
    uint16_t expected_payload_length;
    uint32_t offset;

    if (!protocol->session_active || length < IMAGE_PROTOCOL_DATA_HEADER_SIZE ||
        frame[6] != image_protocol_sum(&frame[7], (uint16_t)(length - 7U)))
    {
        return;
    }
    total_packets = frame[7];
    packet_number = frame[8];
    if (total_packets != protocol->expected_packets ||
        packet_number >= total_packets)
    {
        return;
    }

    if (image_protocol_packet_received(protocol, packet_number))
    {
        return;
    }
    payload_length = (uint16_t)(length - IMAGE_PROTOCOL_DATA_HEADER_SIZE);
    offset = (uint32_t)packet_number * IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE;
    expected_payload_length = (packet_number == (uint8_t)(total_packets - 1U)) ?
        (uint16_t)(protocol->expected_file_size - offset) :
        IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE;
    if (payload_length != expected_payload_length ||
        protocol->operations.storage_write == NULL ||
        !protocol->operations.storage_write(protocol->operations.context, offset,
                                            &frame[IMAGE_PROTOCOL_DATA_HEADER_SIZE],
                                            payload_length))
    {
        return;
    }
    image_protocol_mark_packet(protocol, packet_number);
}


#define IMAGE_PROTOCOL_XTEK_HEADER_SIZE 38U
#define IMAGE_PROTOCOL_IO_CHUNK_SIZE    128U

static bool image_protocol_storage_read(image_protocol_t *protocol, uint32_t offset,
                                        uint8_t *data, uint16_t length)
{
    return protocol->operations.storage_read != NULL &&
           protocol->operations.storage_read(protocol->operations.context,
                                             offset, data, length);
}

static bool image_protocol_validate_xtek(image_protocol_t *protocol,
                                         image_metadata_t *metadata)
{
    uint8_t header[IMAGE_PROTOCOL_XTEK_HEADER_SIZE];
    uint8_t chunk[IMAGE_PROTOCOL_IO_CHUNK_SIZE];
    uint32_t expected_checksum;
    uint32_t checksum = 0U;
    uint32_t offset;
    uint32_t remaining;
    uint16_t read_length;
    uint16_t index;

    if (protocol->expected_file_size < IMAGE_PROTOCOL_XTEK_HEADER_SIZE ||
        !image_protocol_storage_read(protocol, 0U, header, sizeof(header)) ||
        memcmp(header, "XTEK", 4U) != 0 ||
        image_protocol_read_be32(&header[8]) != protocol->expected_file_size ||
        header[12] != 1U || image_protocol_read_be32(&header[13]) != 0x11U)
    {
        return false;
    }

    metadata->x = image_protocol_read_be32(&header[17]);
    metadata->y = image_protocol_read_be32(&header[21]);
    metadata->width = image_protocol_read_be32(&header[25]);
    metadata->height = image_protocol_read_be32(&header[29]);
    metadata->compression = header[33];
    metadata->encoded_size = image_protocol_read_be32(&header[34]);
    if (metadata->width == 0U || metadata->height == 0U ||
        metadata->encoded_size != protocol->expected_file_size - IMAGE_PROTOCOL_XTEK_HEADER_SIZE ||
        metadata->compression > 1U ||
        (metadata->compression == 1U && (metadata->encoded_size & 1U) != 0U))
    {
        return false;
    }

    expected_checksum = image_protocol_read_be32(&header[4]);
    for (index = 12U; index < sizeof(header); index++)
    {
        checksum += header[index];
    }
    offset = IMAGE_PROTOCOL_XTEK_HEADER_SIZE;
    remaining = metadata->encoded_size;
    while (remaining != 0U)
    {
        read_length = (remaining > sizeof(chunk)) ? sizeof(chunk) : (uint16_t)remaining;
        if (!image_protocol_storage_read(protocol, offset, chunk, read_length))
        {
            return false;
        }
        for (index = 0U; index < read_length; index++)
        {
            checksum += chunk[index];
        }
        if (metadata->compression == 1U)
        {
            for (index = 0U; index < read_length; index += 2U)
            {
                if (chunk[index] == 0U)
                {
                    return false;
                }
            }
        }
        offset += read_length;
        remaining -= read_length;
    }
    return checksum == expected_checksum;
}

static bool image_protocol_stream_pixels(image_protocol_t *protocol,
                                         const image_metadata_t *metadata)
{
    uint8_t input[IMAGE_PROTOCOL_IO_CHUNK_SIZE];
    uint8_t output[IMAGE_PROTOCOL_IO_CHUNK_SIZE];
    uint32_t offset = IMAGE_PROTOCOL_XTEK_HEADER_SIZE;
    uint32_t remaining = metadata->encoded_size;
    uint16_t read_length;
    uint16_t input_index;
    uint16_t output_length = 0U;

    while (remaining != 0U)
    {
        read_length = (remaining > sizeof(input)) ? sizeof(input) : (uint16_t)remaining;
        if (!image_protocol_storage_read(protocol, offset, input, read_length))
        {
            return false;
        }
        if (metadata->compression == 0U)
        {
            if (!protocol->operations.display_write(protocol->operations.context,
                                                    input, read_length))
            {
                return false;
            }
        }
        else
        {
            for (input_index = 0U; input_index < read_length; input_index += 2U)
            {
                uint16_t repeat = input[input_index];
                uint8_t value = input[input_index + 1U];

                while (repeat-- != 0U)
                {
                    output[output_length++] = value;
                    if (output_length == sizeof(output))
                    {
                        if (!protocol->operations.display_write(protocol->operations.context,
                                                                output, output_length))
                        {
                            return false;
                        }
                        output_length = 0U;
                    }
                }
            }
        }
        offset += read_length;
        remaining -= read_length;
    }
    return output_length == 0U ||
           protocol->operations.display_write(protocol->operations.context,
                                              output, output_length);
}

static bool image_protocol_parse_action(const uint8_t *frame, uint16_t length,
                                        image_action_t *action)
{
    memset(action, 0, sizeof(*action));
    action->has_staged_image = true;
    if (length == 8U && frame[7] == 0U)
    {
        action->kind = IMAGE_ACTION_SAVE_REFRESH;
        action->slot = 1U;
        return true;
    }
    if (length != 12U || frame[7] != 1U || frame[8] > 6U ||
        frame[9] > IMAGE_ACTION_DELETE)
    {
        return false;
    }
    action->slot = frame[8];
    action->kind = (image_action_kind_t)frame[9];
    action->parameter_1 = frame[10];
    action->parameter_2 = frame[11];
    if (((action->slot == 0U) && (action->kind != IMAGE_ACTION_DELETE)) ||
        (action->kind == IMAGE_ACTION_CAROUSEL && action->parameter_1 == 0U) ||
        (action->kind == IMAGE_ACTION_SCHEDULE &&
         (action->parameter_1 > 23U || action->parameter_2 > 59U)))
    {
        return false;
    }
    return true;
}

static void image_protocol_abort_session(image_protocol_t *protocol)
{
    if (protocol->operations.storage_abort != NULL)
    {
        protocol->operations.storage_abort(protocol->operations.context);
    }
    if (protocol->operations.display_abort != NULL)
    {
        protocol->operations.display_abort(protocol->operations.context);
    }
    protocol->session_active = false;
}
static void image_protocol_push_request(image_protocol_t *protocol,
                                        const uint8_t *frame, uint16_t length)
{
    image_action_t action;
    image_metadata_t metadata;
    bool prepared = false;

    if (!protocol->notifications_enabled ||
        !image_protocol_parse_action(frame, length, &action))
    {
        return;
    }
    if (!protocol->session_active)
    {
        bool success = false;

        action.has_staged_image = false;
        if ((action.kind == IMAGE_ACTION_CAROUSEL ||
             action.kind == IMAGE_ACTION_SCHEDULE ||
             action.kind == IMAGE_ACTION_DELETE) &&
            protocol->operations.execute_action != NULL)
        {
            success = protocol->operations.execute_action(
                protocol->operations.context, &action);
        }
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_PUSH,
            success ? IMAGE_PROTOCOL_RESULT_SUCCESS : IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }
    if (!image_protocol_all_packets_received(protocol))
    {
        image_protocol_send_missing(protocol);
        return;
    }
    if (!image_protocol_validate_xtek(protocol, &metadata) ||
        protocol->operations.display_prepare == NULL ||
        protocol->operations.display_write == NULL ||
        protocol->operations.display_finish == NULL ||
        protocol->operations.storage_prepare == NULL ||
        protocol->operations.storage_apply == NULL ||
        protocol->operations.display_apply == NULL)
    {
        image_protocol_abort_session(protocol);
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_PUSH,
                                   IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }

    prepared = protocol->operations.display_prepare(protocol->operations.context,
                                                    &metadata, &action);
    if (prepared)
    {
        prepared = image_protocol_stream_pixels(protocol, &metadata) &&
                   protocol->operations.display_finish(protocol->operations.context) &&
                   protocol->operations.storage_prepare(protocol->operations.context,
                                                        &action);
    }
    if (!prepared)
    {
        image_protocol_abort_session(protocol);
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_PUSH,
                                   IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }

    protocol->operations.storage_apply(protocol->operations.context);
    protocol->operations.display_apply(protocol->operations.context);
    protocol->session_active = false;
    image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_PUSH,
                               IMAGE_PROTOCOL_RESULT_SUCCESS);
}
static void image_protocol_process_frame(image_protocol_t *protocol)
{
    const uint8_t *frame = protocol->frame;
    uint16_t length = protocol->frame_length;

    if (frame[3] == IMAGE_PROTOCOL_TYPE_DATA)
    {
        if (protocol->operations.trace != NULL)
        {
            if (length >= IMAGE_PROTOCOL_DATA_HEADER_SIZE)
            {
                protocol->operations.trace(protocol->operations.context,
                                           IMAGE_PROTOCOL_TRACE_DATA,
                                           frame[8], frame[7]);
            }
            else
            {
                protocol->operations.trace(protocol->operations.context,
                                           IMAGE_PROTOCOL_TRACE_INVALID,
                                           frame[3], length);
            }
        }
        image_protocol_data_packet(protocol, frame, length);
    }
    else if (image_protocol_command_valid(frame, length))
    {
        if (protocol->operations.trace != NULL)
        {
            protocol->operations.trace(protocol->operations.context,
                                       IMAGE_PROTOCOL_TRACE_COMMAND,
                                       frame[6], frame[3]);
        }
        if (frame[6] == IMAGE_PROTOCOL_COMMAND_STORAGE)
        {
            image_protocol_storage_request(protocol, frame, length);
        }
        else if (frame[6] == IMAGE_PROTOCOL_COMMAND_PUSH)
        {
            image_protocol_push_request(protocol, frame, length);
        }
        else if (frame[6] == IMAGE_PROTOCOL_COMMAND_DEVICE_MATCH)
        {
            image_protocol_device_match_request(protocol, length);
        }
    }
    else if (protocol->operations.trace != NULL)
    {
        protocol->operations.trace(protocol->operations.context,
                                   IMAGE_PROTOCOL_TRACE_INVALID,
                                   frame[3], length);
    }
}

static void image_protocol_start_frame(image_protocol_t *protocol)
{
    protocol->frame[0] = 'X';
    protocol->frame[1] = 'T';
    protocol->frame[2] = 'E';
    protocol->frame_length = 3U;
    protocol->expected_frame_length = 0U;
    protocol->sync_length = 0U;
}

static void image_protocol_sync_byte(image_protocol_t *protocol, uint8_t value)
{
    static const uint8_t marker[] = {'X', 'T', 'E'};

    if (value == marker[protocol->sync_length])
    {
        protocol->sync_length++;
        if (protocol->sync_length == sizeof(marker))
        {
            image_protocol_start_frame(protocol);
        }
    }
    else
    {
        protocol->sync_length = (value == marker[0]) ? 1U : 0U;
    }
}

static void image_protocol_frame_byte(image_protocol_t *protocol, uint8_t value)
{
    if (protocol->frame_length >= sizeof(protocol->frame))
    {
        image_protocol_reset_frame(protocol);
        image_protocol_sync_byte(protocol, value);
        return;
    }

    protocol->frame[protocol->frame_length++] = value;
    if (protocol->frame_length == 5U && protocol->frame[3] != IMAGE_PROTOCOL_TYPE_DATA)
    {
        protocol->expected_frame_length = protocol->frame[4];
    }
    else if (protocol->frame_length == 6U && protocol->frame[3] == IMAGE_PROTOCOL_TYPE_DATA)
    {
        protocol->expected_frame_length = (uint16_t)(((uint16_t)protocol->frame[4] << 8) |
                                                     protocol->frame[5]);
    }

    if (protocol->expected_frame_length != 0U &&
        (protocol->expected_frame_length < IMAGE_PROTOCOL_COMMAND_HEADER_SIZE ||
         protocol->expected_frame_length > sizeof(protocol->frame)))
    {
        image_protocol_reset_frame(protocol);
        return;
    }
    if (protocol->expected_frame_length != 0U &&
        protocol->frame_length == protocol->expected_frame_length)
    {
        image_protocol_process_frame(protocol);
        image_protocol_reset_frame(protocol);
    }
}

void image_protocol_init(image_protocol_t *protocol, const image_protocol_ops_t *operations)
{
    memset(protocol, 0, sizeof(*protocol));
    if (operations != NULL)
    {
        protocol->operations = *operations;
    }
}

void image_protocol_set_notifications(image_protocol_t *protocol, bool enabled)
{
    protocol->notifications_enabled = enabled;
}

void image_protocol_feed(image_protocol_t *protocol, const uint8_t *data, uint16_t length)
{
    if (image_protocol_response_pending(protocol))
    {
        return;
    }
    while (length-- != 0U)
    {
        if (protocol->frame_length == 0U)
        {
            image_protocol_sync_byte(protocol, *data++);
        }
        else
        {
            image_protocol_frame_byte(protocol, *data++);
        }
    }
}

bool image_protocol_response_pending(const image_protocol_t *protocol)
{
    return protocol->pending_response_length != 0U;
}

void image_protocol_disconnect(image_protocol_t *protocol)
{
    if ((protocol->session_active || protocol->staging_preparing) &&
        protocol->operations.storage_abort != NULL)
    {
        protocol->operations.storage_abort(protocol->operations.context);
    }
    protocol->session_active = false;
    protocol->staging_preparing = false;
    protocol->notifications_enabled = false;
    protocol->pending_response_length = 0U;
    image_protocol_reset_frame(protocol);
}

bool image_protocol_session_active(const image_protocol_t *protocol)
{
    return protocol->session_active;
}

void image_protocol_storage_ready(image_protocol_t *protocol, uint32_t size, bool success)
{
    if (!protocol->staging_preparing || size != protocol->expected_file_size)
    {
        return;
    }
    protocol->staging_preparing = false;
    if (!success)
    {
        image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                                   IMAGE_PROTOCOL_RESULT_FAILED);
        return;
    }
    protocol->expected_packets = (uint8_t)(
        (size + IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE - 1U) /
        IMAGE_PROTOCOL_LARGE_PAYLOAD_SIZE);
    memset(protocol->reception_bitmap, 0, sizeof(protocol->reception_bitmap));
    protocol->session_active = true;
    image_protocol_send_result(protocol, IMAGE_PROTOCOL_COMMAND_STORAGE,
                               IMAGE_PROTOCOL_RESULT_SUCCESS);
}

bool image_protocol_staging_preparing(const image_protocol_t *protocol)
{
    return protocol->staging_preparing;
}
