/**
 * @file prf_image.c
 * @brief Bluetooth label image-transfer GATT service.
 */
#include "gap.h"
#include "gatt.h"
#include "prf_image.h"
#include "app_ble.h"
#include "cfg.h"
#include "log.h"

#include <stdbool.h>
#include <string.h>

#define IMAGE_TX_PROP                         GATT_CHR_PROP_NOTIFY
#define IMAGE_CCCD_IMPROPERLY_CONFIGURED      0x12
#define IMAGE_RESPONSE_MAX_SIZE               40U

enum
{
    IMAGE_IDX_TX,
    IMAGE_IDX_RX,
    IMAGE_IDX_SVC,
    IMAGE_IDX_END,
};

static gatt_chr_t image_handle[IMAGE_IDX_END] = {NULL};
static bool image_notifications_enabled;
static bool image_response_pending;
static uint8_t image_response[IMAGE_RESPONSE_MAX_SIZE];
static uint16_t image_response_length;

static void prf_image_send_complete(peer_t peer)
{
    app_ble_msg_t message;

    message.ntf_cmp.conhdl = ble_get_conhdl_by_conidx(peer);
    message.ntf_cmp.handle = IMAGE_IDX_TX;
    ble_event_callback(BLE_GATTS_IMAGE_DATA_CMP, &message);
}

static void prf_image_cbk(gatt_event_t event, void *context, void *parameter)
{
    (void)context;


    switch (event)
    {
        case GATTS_EV_CHR_VALUE_READ_REQ:
        {
            gatt_chr_val_read_req_t *input = (gatt_chr_val_read_req_t *)parameter;

            if (input->chr == image_handle[IMAGE_IDX_TX])
            {
                if (input->val_off > image_response_length)
                {
                    gatts_chr_val_read_rsp(input->peer, input->chr,
                                           GATT_RES_INVALID_OFFSET, NULL, 0U);
                }
                else
                {
                    gatts_chr_val_read_rsp(input->peer, input->chr, GATT_OK,
                        &image_response[input->val_off],
                        image_response_length - input->val_off);
                }
            }
            break;
        }

        case GATTS_EV_CHR_VALUE_WRITE_REQ:
        case GATTS_EV_CHR_VALUE_WRITE:
        {
            gatt_chr_val_write_req_t *input = (gatt_chr_val_write_req_t *)parameter;
            bool write_allowed = image_notifications_enabled &&
                                 !image_response_pending;

            if (event == GATTS_EV_CHR_VALUE_WRITE_REQ)
            {
                gatts_chr_val_write_rsp(input->peer, input->chr,
                    write_allowed ? GATT_OK :
                    (image_notifications_enabled ?
                        GATT_RES_INSUFFICIENT_RESOURCES :
                        (gatt_rst_t)IMAGE_CCCD_IMPROPERLY_CONFIGURED));
            }
            if (!write_allowed)
            {
                BR_LOG_WARNING("[IMG][GATT] write blocked cccd:%d pending:%d len:%d\r\n",
                               image_notifications_enabled,
                               image_response_pending, input->val_len);
            }
            if (write_allowed && (input->chr == image_handle[IMAGE_IDX_RX]))
            {
                app_ble_msg_t message;

                message.data_rcv.conhdl = ble_get_conhdl_by_conidx(input->peer);
                message.data_rcv.len = input->val_len;
                message.data_rcv.data = input->val + input->val_off;
                ble_event_callback(BLE_GATTS_IMAGE_DATA_RPT, &message);
            }
            break;
        }

        case GATTS_EV_CHR_VALUE_SEND_START:
        {
            gatt_chr_val_send_start_t *input = (gatt_chr_val_send_start_t *)parameter;
            app_ble_msg_t message;

            if (input->chr == image_handle[IMAGE_IDX_TX])
            {
                image_notifications_enabled =
                    (input->type == GATT_SEND_VAL_NOTIFICATION);
                BR_LOG_INFO("[IMG][GATT] cccd type:%d enabled:%d\r\n",
                            input->type, image_notifications_enabled);
                message.rcv_ntf.conhdl = ble_get_conhdl_by_conidx(input->peer);
                message.rcv_ntf.data = input->type;
                ble_event_callback(BLE_GATTS_IMAGE_CCCD_RPT, &message);
            }
            break;
        }

        case GATTS_EV_CHR_VALUE_SEND_CFM:
        {
            gatt_chr_val_send_cfm_t *input = (gatt_chr_val_send_cfm_t *)parameter;

            if (input->chr == image_handle[IMAGE_IDX_TX] &&
                image_response_pending)
            {
                image_response_pending = false;
                prf_image_send_complete(input->peer);
            }
            break;
        }

        default:
            break;
    }
}

void prf_image_disconnect(void)
{
    BR_LOG_INFO("[IMG][GATT] reset on disconnect\r\n");
    image_notifications_enabled = false;
    image_response_length = 0U;
    image_response_pending = false;
}

uint32_t prf_image_send(uint16_t conhdl, const uint8_t *data, uint16_t length)
{
    void *conidx = ble_get_conidx_by_conhdl(conhdl);
    if ((data == NULL) || (length > sizeof(image_response)))
    {
        BR_LOG_ERROR("[IMG][GATT] notify invalid len:%d\r\n", length);
        return GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH;
    }
    memcpy(image_response, data, length);
    image_response_length = length;

    {
        uint32_t result = gatts_chr_val_notif(conidx, image_handle[IMAGE_IDX_TX],
                                              (uint8_t *)data, length);
        image_response_pending = (result == GATT_OK);
        BR_LOG_INFO("[IMG][GATT] notify len:%d result:%08x\r\n",
                    length, result);
        if (result == GATT_OK)
        {
            /* This SDK does not reliably emit SEND_CFM for notifications. */
            image_response_pending = false;
            prf_image_send_complete(conidx);
        }
        return result;
    }
}

int prf_image_init(void)
{
    uint8_t security_level = 0;
    gatt_chr_add_params_t characteristics[] =
    {
        [IMAGE_IDX_TX] =
        {
            &image_handle[IMAGE_IDX_TX],
            {UUID_LEN_128, {0x02, 0x00, 0x2E, 0xC7, 0x8A, 0x0E, 0x73, 0x90,
                            0xE1, 0x11, 0xC2, 0x08, 0x60, 0x27, 0x00, 0x00}},
            GATT_CHR_PROP_READ, 0, IMAGE_TX_PROP, 0, prf_image_cbk, NULL
        },
        [IMAGE_IDX_RX] =
        {
            &image_handle[IMAGE_IDX_RX],
            {UUID_LEN_128, {0x01, 0x00, 0x2E, 0xC7, 0x8A, 0x0E, 0x73, 0x90,
                            0xE1, 0x11, 0xC2, 0x08, 0x60, 0x27, 0x00, 0x00}},
            GATT_CHR_PROP_WRITE_WITHOUT_RSP | GATT_CHR_PROP_WRITE,
            0, 0, 0, prf_image_cbk, NULL
        },
    };
    gatt_svc_create_params_t services[] =
    {
        {
            &image_handle[IMAGE_IDX_SVC],
            {UUID_LEN_128, {0x01, 0x10, 0x2E, 0xC7, 0x8A, 0x0E, 0x73, 0x90,
                            0xE1, 0x11, 0xC2, 0x08, 0x60, 0x27, 0x00, 0x00}},
            prf_image_cbk, NULL,
            sizeof(characteristics) / sizeof(characteristics[0]),
            characteristics
        },
    };

    cfg_reserver_get(AUTH_OFF, &security_level, AUTH_LEN);
    if (security_level == 1)
    {
        characteristics[IMAGE_IDX_TX].secu =
            GATT_SECU_READ_AUTHEN | GATT_SECU_READ_ENCRYPT;
        characteristics[IMAGE_IDX_TX].ccc_secu =
            GATT_SECU_WRITE_AUTHEN | GATT_SECU_WRITE_ENCRYPT;
        characteristics[IMAGE_IDX_RX].secu =
            GATT_SECU_WRITE_AUTHEN | GATT_SECU_WRITE_ENCRYPT;
    }

    image_notifications_enabled = false;
    gatts_svc_create_ex(sizeof(services) / sizeof(services[0]), services);
    image_response_length = 0U;
    image_response_pending = false;
    gatts_svc_start(*services[0].psvc);
    BR_LOG_INFO("[IMG][GATT] service ready security:%d\r\n", security_level);
    return 0;
}
