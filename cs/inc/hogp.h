/**
 *******************************************************************************
 *
 * Copyright (c) 1999-2023 BRT Techonology Limited
 *
 * All rights reserved.
 *
 * -----------------------------------------------------------------------------
 *
 * @file    hogp.h
 * @author  Weigh
 *
 *******************************************************************************
 */

#ifndef _HOGP_H_
#define _HOGP_H_

/**
 *******************************************************************************
 * @defgroup HOGP
 *
 * @brief
 *
 * @{
 *******************************************************************************
 */

/*
 * INCLUDE FILES
 *******************************************************************************
 */

#include "gap.h"
#include "gatt.h"


/*
 * TYPE DEFINITIONS
 *******************************************************************************
 */

typedef enum hogp_event {

    HID_EV_REPORT,            /** hid_rpt_t */

    HIDS_EV_START,            /** hids_start_t */
    HIDS_EV_STOP,             /** void */
    HIDS_EV_SUSPEND,          /** hids_suspend_t */
    HIDS_EV_SUSPEND_EXIT,     /** hids_suspend_exit_t */
    HIDS_EV_RPT_NTF_STATE,    /** hids_rpt_ntf_t */
    HIDS_EV_REPORT_GET_REQ,   /** hids_rpt_get_req_t */
    HIDS_EV_REPORT_SET_REQ,   /** hids_rpt_set_req_t */
    HIDS_EV_PROTOCOL_GET_REQ, /** hids_prtc_get_req_t */
    HIDS_EV_PROTOCOL_SET_REQ, /** hids_prtc_set_req_t */

} hogp_event_t;

typedef void (hogp_cbk_t)(hogp_event_t event, void *ctx, void *par);
typedef uint8_t (bat_level_get_t)(void);


typedef enum {
    HOGP_FEAT_BIT_BOOT_RMT_WAKE     = 0x01, /** HOGP Device is capable of sending
                                                a wake-signal to a HOGP Host */
    HOGP_FEAT_BIT_BOOT_NORM_CONN    = 0x02, /** HOGP Device will be advertising when
                                                bonded but not connecte */

    HOGP_FEAT_BIT_BOOT_KEYBOARD     = 0x10, /** HOGP Device operating in Boot Protocol
                                                Mode as a keyboard */
    HOGP_FEAT_BIT_BOOT_MOUSE        = 0x20, /** HOGP Device operating in Boot Protocol
                                                Mode as a mouse */
    HOGP_FEAT_BIT_BOOT_COMBO        = 0x30, /** HOGP Device operating in Boot Protocol
                                                Mode as both keyboard and mouse */

	HOGP_FEAT_BIT_AUTHEN_REQUEIRED  = (1 << 8 ), /** Require authentication */
	HOGP_FEAT_BIT_ENCRYPT_REQUEIRED = (1 << 9 ), /** Require encryption */
	HOGP_FEAT_BIT_MITM_REQUEIRED    = (1 << 10), /** Require MITM protection */
} hid_feat_bit_t;

/** Report type */
typedef enum hid_rpt_type {
    HID_RPT_TYPE_OTHER,

    HID_RPT_TYPE_INPUT,
    HID_RPT_TYPE_OUTPUT,
    HID_RPT_TYPE_FEATURE,

	HID_RPT_TYPE_BOOT_KB_IN  = HOGP_FEAT_BIT_BOOT_KEYBOARD | HID_RPT_TYPE_INPUT,
	HID_RPT_TYPE_BOOT_KB_OUT = HOGP_FEAT_BIT_BOOT_KEYBOARD | HID_RPT_TYPE_OUTPUT,
	HID_RPT_TYPE_BOOT_MOUSE  = HOGP_FEAT_BIT_BOOT_MOUSE    | HID_RPT_TYPE_INPUT,

	HID_REPORT_TYPE_INVALID = 0xFF
} hid_rpt_type_t;

/**
 * The parameter of HID_EV_REPORT
 */
typedef struct hid_rpt_stru {
    peer_t    peer;         /** Peer device instance. */
    uint8_t   rpt_id;
    uint8_t   rpt_type;
    uint16_t  rpt_sz;
    const uint8_t  *rpt;
} hid_rpt_t;

/**
 * The parameter of HIDS_EV_START
 */
typedef struct hids_start_stru {
    int8_t  result; /** Result
                        0     : Successful
                        others: Failed */
} hids_start_t;

/**
 * The parameter of HIDS_EV_SUSPEND
 */
typedef struct hids_suspend_stru {
    peer_t   peer;         /** Peer device instance. */
} hids_suspend_t;

/**
 * The parameter of HIDS_EV_SUSPEND_EXIT
 */
typedef struct hids_suspend_exit_stru {
    peer_t   peer;         /** Peer device instance. */
} hids_suspend_exit_t;

/**
 * The parameter of HIDS_EV_RPT_NTF_STATE
 */
typedef struct hids_rpt_ntf_stru {
    peer_t    peer;     /** Peer device instance. */
    bool      ntf_en;   /** Report notification enable or not */
    uint8_t   rpt_id;
    uint8_t   rpt_type;
} hids_rpt_ntf_t;

/**
 * The parameter of HIDS_EV_REPORT_GET_REQ
 */
typedef struct hids_rpt_get_req_stru {
    peer_t    peer;     /** Peer device instance. */
    void     *op_hdl;  /** Operation handle, MUST be returned when response */
    uint8_t   rpt_id;
    uint8_t   rpt_type;
    uint16_t  rpt_off;
} hids_rpt_get_req_t;

/**
 * The parameter of HIDS_EV_REPORT_SET_REQ
 */
typedef struct hids_rpt_set_req_stru {
    peer_t    peer;     /** Peer device instance. */
    void     *op_hdl;   /** Operation handle, MUST be returned when response */
    uint8_t   rpt_id;
    uint8_t   rpt_type;
    uint16_t  rpt_sz;
    uint16_t  rpt_off;
    const uint8_t  *rpt;
} hids_rpt_set_req_t;

/**
 * The parameter of HIDS_EV_PROTOCOL_GET_REQ
 */
typedef struct hids_prtc_get_req_stru {
    peer_t  peer;   /** Peer device instance. */
    void   *op_hdl; /** Operation handle, MUST be returned when response */
} hids_prtc_get_req_t;

/**
 * The parameter of HIDS_EV_PROTOCOL_SET_REQ
 */
typedef struct hids_prtc_set_req_stru {
    peer_t   peer;   /** Peer device instance. */
    void    *op_hdl; /** Operation handle, MUST be returned when response */
    uint8_t  prtc;
} hids_prtc_set_req_t;


typedef struct hid_inf {
    uint16_t  feat_msk;     /** hid_feat_bit_t */
    uint16_t  bcd;          /** Version number of base USB HOGP Specification implemented
                                by HOGP Device */
    uint8_t   ctry_code;    /** Country HID Device hardware is localized for.
                                Most hardware is not localized (value 0x00) */
    uint16_t  desc_lst_len;
    uint8_t  *desc_lst;
} hid_inf_t;

typedef struct hids_rpt_get_rsp_stru {
    peer_t    peer;     /** Peer device instance. */
    void     *op_hdl;   /** Operation handle */
    uint8_t   rpt_id;
    uint8_t   rpt_type;
    uint16_t  rpt_sz;
    const uint8_t  *rpt;
} hids_rpt_get_rsp_t;

typedef struct hids_rpt_set_rsp_stru {
    peer_t   peer;     /** Peer device instance. */
    void    *op_hdl;   /** Operation handle */
    uint8_t  rpt_id;
    uint8_t  rpt_type;
} hids_rpt_set_rsp_t;

typedef struct hids_prtc_get_rsp_stru {
    peer_t   peer;   /** Peer device instance. */
    void    *op_hdl; /** Operation handle */
    uint8_t  prtc;   /** Protocol Mode */
} hids_prtc_get_rsp_t;

typedef struct hids_prtc_set_rsp_stru {
    peer_t   peer;   /** Peer device instance. */
    void    *op_hdl; /** Operation handle */
    uint8_t  result;
} hids_prtc_set_rsp_t;

/*
 * API FUNCTION DECLARATIONS
 *******************************************************************************
 */

void hid_rpt_send(uint8_t ev,void *in_rpt);

void hids_start(hid_inf_t *hid_inf, hogp_cbk_t *cbk, void *ctx);
void hids_stop(void);
void hids_rpt_get_rsp(hids_rpt_get_rsp_t *par);
void hids_rpt_set_rsp(hids_rpt_set_rsp_t *par);
void hids_prtc_get_rsp(hids_prtc_get_rsp_t *par);
void hids_prtc_set_rsp(hids_prtc_set_rsp_t *par);


/// @} HOGP

#endif // !_HOGP_H_

