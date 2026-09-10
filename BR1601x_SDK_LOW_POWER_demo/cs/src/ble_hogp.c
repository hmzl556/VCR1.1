#include "ble_hogp.h"
#include "gatt.h"

local_info_t local_info = {0};

// HID Mouse Report Descriptor
static const uint8_t app_hid_report_map[] =
{
    // Report ID 1: keybaord
    0x05, 0x01, //USAGE_PAGE?Generic Desktop)
    0x09, 0x06, //USAGE (Keyboard)
    0xa1, 0x01, //COLLECTION (Application)
    0x85, APP_HID_KEYBOARD_REPORT_ID,  // REPORT ID (1)
    0x05, 0x07, //USAGE_PAGE (Keyboard)
    0x19, 0xe0, //USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7, //USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00, //LOGICAL_MINIMUM(0)
    0x25, 0x01, //LOGICAL_MAXIMUM(1)
    0x75, 0x01, //REPORT_SIZE(1)
    0x95, 0x08, //REPORT_COUNT(8)
    0x81, 0x02, //INPUT (Data,Var,Abs)
    0x95, 0x01, //REPORT_COUNT(1)
    0x75, 0x08, //REPORT_SIZE(8)
    0x81, 0x03, //INPUT (Cnst,Var,Abs)
    0x95, 0x06, //REPORT_COUNT(6)
    0x75, 0x08, //REPORT_SIZE(8)
    0x15, 0x00, //LOGICAL_MINIMUM(0)
    0x25, 0xFF, //LOGICAL_MAXIMUM(255)
    0x05, 0x07, //USAGE_PAGE (Keyboard)
    0x19, 0x00, //USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0xFF, //USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00, //INPUT?Data,Ary,Abs)
    0xc0,       //END_COLLECTION

    // Report ID 2: Consumer Control
    0x05, 0x0C,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xA1, 0x01,       // Collection (Application)
    0x85, APP_HID_CONSUMER_REPORT_ID,  // Report Id (2)
    0x75, 0x10,       // Report Size
    0x95, 0x02,       // Report Count
    0x15, 0x00,       // Logical minimum
    0x26, 0xFF, 0x03, // Logical maximum
    0x19, 0x00,       //Usage minimum
    0x2A, 0xFF, 0x03, //Usage maximum
    0x81, 0x00,       // Input
    0xC0,             // End Collection

#ifdef MOUSE_KEY_NUMBER
    /**
     *  --------------------------------------------------------------------------
     *  Bit      |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
     *  --------------------------------------------------------------------------
     *  Byte 0   |               Not Used                | Middle| Right | Left  |
     *  --------------------------------------------------------------------------
     *  Byte 1   |                     X Axis Relative Movement                  |
     *  --------------------------------------------------------------------------
     *  Byte 2   |                     Y Axis Relative Movement                  |
     *  --------------------------------------------------------------------------
     *  Byte 3   |                     Wheel Relative Movement                   |
     *  --------------------------------------------------------------------------
     */

    0x05, 0x01,     /// USAGE PAGE (Generic Desktop)
    0x09, 0x02,     /// USAGE (Mouse)

    0xA1, 0x01,     /// COLLECTION (Application)

    0x85, APP_HID_MOUSE_REPORT_ID,     /// REPORT ID (3) - MANDATORY
    0x09, 0x01,     ///     USAGE (Pointer)
    0xA1, 0x00,     ///     COLLECTION (Physical)

    /**
     * ----------------------------------------------------------------------------
     * BUTTONS
     * ----------------------------------------------------------------------------
     */
    0x05, 0x09,     ///         USAGE PAGE (Buttons)
    0x19, 0x01,     ///         USAGE MINIMUM (1)
    0x29, 0x08,     ///         USAGE MAXIMUM (8)
    0x15, 0x00,     ///         LOGICAL MINIMUM (0)
    0x25, 0x01,     ///         LOGICAL MAXIMUM (1)
    0x75, 0x01,     ///         REPORT SIZE (1)
    0x95, 0x08,     ///         REPORT COUNT (8)
    0x81, 0x02,     ///         INPUT (Data, Variable, Absolute)

    /**
     * ----------------------------------------------------------------------------
     * MOVEMENT DATA
     * ----------------------------------------------------------------------------
     */
    0x05, 0x01,     ///         USAGE PAGE (Generic Desktop)
    0x16, 0x08, 0xFF, ///       LOGICAL MINIMUM (-255)
    0x26, 0xFF, 0x00, ///       LOGICAL MAXIMUM (255)
    0x75, 0x10,     ///         REPORT SIZE (16)
    0x95, 0x02,     ///         REPORT COUNT (2)
    0x09, 0x30,     ///         USAGE (X)
    0x09, 0x31,     ///         USAGE (Y)
    0x81, 0x06,     ///         INPUT (Data, Variable, Relative)

    0x15, 0x81,     ///         LOGICAL MINIMUM (-127)
    0x25, 0x7F,     ///         LOGICAL MAXIMUM (127)
    0x75, 0x08,     ///         REPORT SIZE (8)
    0x95, 0x01,     ///         REPORT COUNT (1)
    0x09, 0x38,     ///         USAGE (Wheel)
    0x81, 0x06,     ///         INPUT (Data, Variable, Relative)

    0xC0,           ///     END COLLECTION (Physical)
    0xC0,           /// END COLLECTION (Application)
#endif

    /* Vendor */
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x00,        //   Usage (0x00)
    0xA1, 0x01,        //   Collection (Application)
    0x85, APP_HID_AUDIO_REPORT_ID,       //     Report ID
    0x95, 0xFF,        //     Report Count (-1)
    0x75, 0x08,        //     Report Size (8)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0xFF,        //     Logical Maximum (-1)
    0x19, 0x00,        //     Usage Minimum (0x00)
    0x29, 0xFF,        //     Usage Maximum (0xFF)
    0x81, 0x00,        //     Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
};

/**
 * @brief Callback function for the HOGP (Human Interface Device Over GATT Profile) events
 *
 * This function is called when a HOGP event occurs. It handles the event based on its type.
 *
 * @param [in] event  The HOGP event
 * @param [in] ctx  The context in which the event occurred
 * @param [in] par  The parameters associated with the event
 */
static void cbk_hogp(hogp_event_t event, void *ctx, void *par)
{
    switch (event) {
        case HIDS_EV_START:
            break;
        case HIDS_EV_STOP:
            break;
        case HIDS_EV_SUSPEND:
            break;
        case HIDS_EV_SUSPEND_EXIT:
            break;
        case HIDS_EV_RPT_NTF_STATE:
            if (local_info.peer_inf.peer) {
                local_info.peer_inf.status |= BLE_STATUS_BIT_NOTIFY_ENABLED;
                if (local_info.event_cbk) {
                    (*local_info.event_cbk)(BLE_EV_NOTIFY_ENABLED, local_info.peer_inf.peer);
                }
            }
            break;
        case HIDS_EV_REPORT_GET_REQ:
            {
                hids_rpt_get_req_t *rpt_get_req = (hids_rpt_get_req_t *)par;
                hids_rpt_get_rsp_t rsp;
                rsp.peer = rpt_get_req->peer;
                rsp.op_hdl = rpt_get_req->op_hdl;
                rsp.rpt_id = rpt_get_req->rpt_id;
                rsp.rpt_type = rpt_get_req->rpt_type;
                rsp.rpt_sz = 0;
                rsp.rpt = NULL;                 //to be fixed
                hids_rpt_get_rsp(&rsp);
            }
            break;
        case HIDS_EV_REPORT_SET_REQ:
            {
                hids_rpt_set_req_t *rpt_set_req = (hids_rpt_set_req_t *)par;
                hids_rpt_set_rsp_t rsp;
                rsp.peer = rpt_set_req->peer;
                rsp.op_hdl = rpt_set_req->op_hdl;
                rsp.rpt_id = rpt_set_req->rpt_id;
                rsp.rpt_type = rpt_set_req->rpt_type;
                hids_rpt_set_rsp(&rsp);
            }
            break;
        case HIDS_EV_PROTOCOL_GET_REQ:
            {
                hids_prtc_get_req_t *prtc_get_req = (hids_prtc_get_req_t *)par;
                hids_prtc_get_rsp_t rsp;
                rsp.peer = prtc_get_req->peer;
                rsp.op_hdl = prtc_get_req->op_hdl;
                rsp.prtc = 0;               //to be fixed
                hids_prtc_get_rsp(&rsp);
            }
            break;
        case HIDS_EV_PROTOCOL_SET_REQ:
            {
                hids_prtc_set_req_t *prtc_set_req = (hids_prtc_set_req_t *)par;
                hids_prtc_set_rsp_t rsp;
                rsp.peer = prtc_set_req->peer;
                rsp.op_hdl = prtc_set_req->op_hdl;
                rsp.result = 0;             //to be fixed
                hids_prtc_set_rsp(&rsp);
            }
            break;
    }
}

/**
 * @brief Initializes the hid application
 *
 * This function is used to initialize the hid application.
 */
void app_hid_init(void)
{
    /* hid info */
    local_info.hid_inf.feat_msk = HOGP_FEAT_BIT_AUTHEN_REQUEIRED | HOGP_FEAT_BIT_ENCRYPT_REQUEIRED;
    local_info.hid_inf.bcd = 0x0111;       // HID Version 1.11
    local_info.hid_inf.ctry_code = 0x00;   // Not localized

    /* report desc */
    local_info.hid_inf.desc_lst_len = sizeof(app_hid_report_map);
    local_info.hid_inf.desc_lst = (uint8_t *)app_hid_report_map;

    co_printf("%s %d start:%08x\r\n",__FUNCTION__,__LINE__,local_info.hid_inf.desc_lst);

    hids_start(&local_info.hid_inf, cbk_hogp, NULL);
}


