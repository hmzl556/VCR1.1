/**
 *******************************************************************************
 *
 * Copyright (c) 1999-2023 BRT Techonology Limited
 *
 * All rights reserved.
 *
 * -----------------------------------------------------------------------------
 *
 * @file    gatt.h
 * @author  Weigh
 *
 *******************************************************************************
 */

#ifndef _GATT_H_
#define _GATT_H_

/**
 *******************************************************************************
 * @defgroup GATT   Barrot SDK GATT Definitions
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

#define INVALID_PASSKEY 0xFFFFFFFF
#define INVALID_CHRHDL	0xFFFF

/*
 * TYPE DEFINITIONS
 *******************************************************************************
 */

typedef void *      gatt_svc_t;
typedef void *      gatt_chr_t;
typedef void *      gatt_dsc_t;

#define VALHDL2CHR(val_hdl) (gatt_chr_t)(((INVALID_CHRHDL) << 16) | (val_hdl))
#define ENDHDL(chr) 	   (uint16_t)(((uint32_t)chr >> 16) & 0xFFFF)
#define VALHDL(chr)		   (uint16_t)((uint32_t)chr & 0xFFFF)

/**
 * Characteristic Properties.
 */
typedef enum gatt_chr_props_bits {
    GATT_CHR_PROP_BROADCAST           = (1 << 0), /* 0x01 */
    GATT_CHR_PROP_READ                = (1 << 1), /* 0x02 */
    GATT_CHR_PROP_WRITE_WITHOUT_RSP   = (1 << 2), /* 0x04 */
    GATT_CHR_PROP_WRITE               = (1 << 3), /* 0x08 */
    GATT_CHR_PROP_NOTIFY              = (1 << 4), /* 0x10 */
    GATT_CHR_PROP_INDICATE            = (1 << 5), /* 0x20 */
    GATT_CHR_PROP_AUTH_SIGNED_WRITE   = (1 << 6), /* 0x40 */

	GATT_CHR_PROP_EXT_RELIABLE_WRITE  = (1 << 8), /** If set, permits reliable
                                                   *  writes of the Characteristic
                                                   *  Value
                                                   */
	GATT_CHR_PROP_EXT_WRITABLE_AUX    = (1 << 9), /** If set, permits writes to
                                                   *  the descriptor Characteristic
                                                   *  User Description
                                                   */
} gatt_chr_props_bits_t;

/**
 * Attribute permissions.
 */
typedef enum att_perm_bits {
	ATT_PERM_READ                     = (1 << 1),
	ATT_PERM_READ_AUTHEN_REQUEIRED    = (1 << 12), /** Readable with authentication */
	ATT_PERM_READ_ENCRYPT_REQUEIRED   = (1 << 13), /** Readable with encryption */
	ATT_PERM_READ_MITM_REQUEIRED      = (1 << 14), /** Readable with MITM protection */

	ATT_PERM_WRITE                    = (1 << 3),
	ATT_PERM_WRITE_AUTHEN_REQUEIRED   = (1 << 8 ), /** Writable with authentication */
	ATT_PERM_WRITE_ENCRYPT_REQUEIRED  = (1 << 9 ), /** Writable with encryption */
	ATT_PERM_WRITE_MITM_REQUEIRED     = (1 << 10), /** Writable with MITM protection */
} att_perm_bits_t;

/**
 * Security Requirement bits
 */
typedef enum gatt_secu_bits {
	GATT_SECU_READ_AUTHEN    = (1 << 12), /** Read with authentication */
	GATT_SECU_READ_ENCRYPT   = (1 << 13), /** Read with encryption */
	GATT_SECU_READ_MITM      = (1 << 14), /** Read with MITM protection */

	GATT_SECU_WRITE_AUTHEN   = (1 << 8 ), /** Writ with authentication */
	GATT_SECU_WRITE_ENCRYPT  = (1 << 9 ), /** Writ with encryption */
	GATT_SECU_WRITE_MITM     = (1 << 10), /** Writ with MITM protection */
} gatt_secu_bits_t;

/**
 * GATT Client Characteristic Configuration Value type
 */
typedef enum gatt_ccc_val {
	GATT_SEND_VAL_NONE                = 0x0000,
	GATT_SEND_VAL_NOTIFICATION        = 0x0001, /** The Value shall be notified. */
	GATT_SEND_VAL_INDICATION          = 0x0002  /** The Value shall be indicated. */
} gatt_ccc_val_t;

/**
 * GATT Events
 */
typedef enum gatt_event {

    GATTS_EV_SERVICE_START              = 0x10, /* gatt_svc_start_t */
    GATTS_EV_SERVICE_STOP,                      /* gatt_svc_stop_t */
    GATTS_EV_CHR_VALUE_READ_REQ,                /* gatt_chr_val_read_req_t */
    GATTS_EV_CHR_VALUE_WRITE_REQ,               /* gatt_chr_val_write_req_t */
    GATTS_EV_CHR_VALUE_WRITE,                   /** gatt_chr_val_write_req_t */
    GATTS_EV_CHR_VALUE_SEND_START,              /* gatt_chr_val_send_start_t */
    GATTS_EV_CHR_VALUE_SEND_CFM,                /* gatt_chr_val_send_cfm_t */

    GATTC_EV_SERVICE_DISCOVERED         = 0x20, /* gatt_svc_discovered_t */
    GATTC_EV_SERVICE_DISCOVER_CMPL,             /* gatt_svc_discover_cmpl_t */
    GATTC_EV_CHARACTERISTIC_DISCOVERED,         /* gatt_chr_discovered_t */
    GATTC_EV_CHARACTERISTIC_DISCOVER_CMPL,      /* gatt_chr_discover_cmpl_t */
    GATTC_EV_CHR_DSC_DISCOVERED,                /* gatt_chr_dsc_discovered_t */
    GATTC_EV_CHR_DSC_DISCOVER_CMPL,             /* gatt_chr_dsc_discover_cmpl_t */
    GATTC_EV_CHR_VALUE_READ_RSP,                /* gatt_chr_val_read_rsp_t */
    GATTC_EV_CHR_VALUE_WRITE_RSP,               /* gatt_chr_val_write_rsp_t */
    GATTC_EV_DSC_CCC_ENABLE,                    /* gatt_dsc_ccc_enable_t */
    GATTC_EV_DSC_CCC_DISABLE,                   /* gatt_dsc_ccc_disable_t */
    GATTC_EV_CHR_VALUE_NOTIFIED,                /* gatt_chr_val_notif_t */

} gatt_event_t;

#define GATTS_EV_CHR_VALUE_SEND_STATE	GATTS_EV_CHR_VALUE_SEND_START		//for BR007	

/**
 * GATT Callback Function define.
 *
 * @param [in]      event    The event.
 * @param [in/out]  param    The parameters.
 *
 * return   Result
 */
typedef void (gatt_cbk_t)(gatt_event_t event, void *ctx, void *param);

/**
 * GATT Result Code
 */
typedef enum gatt_rst {
    GATT_OK,

    /* [TODO] Others */
    GATT_RES_INVALID_HANDLE,            /** 0x01 The attribute handle given was not valid on
                                          this server. */
    GATT_RES_READ_NOT_PERMITTED,        /** 0x02 The attribute cannot be read. */
    GATT_RES_WRITE_NOT_PERMITTED,       /** 0x03 The attribute cannot be written. */
    GATT_RES_INVALID_PDU,               /** 0x04 The attribute PDU was invalid. */
    GATT_RES_INSUFFICIENT_AUTHENTICATION,/** 0x05 The attribute requires authentication before it can
                                           be read or written. */
    GATT_RES_REQUEST_NOT_SUPPORTED,     /** 0x06 Attribute server does not support the request
                                          received from the client. */
    GATT_RES_INVALID_OFFSET,            /** 0x07 Offset specified was past the end of the attribute */
    GATT_RES_INSUFFICIENT_AUTHORIZATION, /** 0x08 The attribute requires authorization before it can
                                           be read or written. */
    GATT_RES_PREPARE_QUEUE_FULL,        /** 0x09 Too many prepare writes have been queued,
                                          for PrepareWrite */
    GATT_RES_ATTRIBUTE_NOT_FOUND,       /** 0x0A No attribute found within the given attribute handle
                                          range. */
    GATT_RES_ATTRIBUTE_NOT_LONG,        /** 0x0B The attribute cannot be read or written using the
                                          Read Blob Request, errara 4887 I suggest remove it */
    GATT_RES_INSUFFICIENT_ENCRYPTION_KEYSIZE,/** 0x0C The Encryption Key Size used for encrypting this
                                               link is insufficient. */
    GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH,/** 0x0D The attribute value length is invalid for
                                              the operation. */
    GATT_RES_UNLIKELY_ERROR,            /** 0x0E The attribute request that was requested has
                                          encountered an error that was unlikely, and therefore
                                          could not be completed as requested. */
    GATT_RES_INSUFFICIENT_ENCRYPTION,   /** 0x0F The attribute requires encryption before it can be
                                          read or written. */
    GATT_RES_UNSUPPORTED_GROUPTYPE,     /** 0x10 ReadByGroupType, The attribute type is not a supported
                                          grouping attribute as defined by a higher layer
                                          specification. */
    GATT_RES_INSUFFICIENT_RESOURCES,    /** 0x11 Insufficient Resources to complete the request.
                                          For Write, too long. */

    GATT_RES_PENDING   = (-1),          /** Waiting for the application response */

} gatt_rst_t;

/**
 * The parameter of GATTS_EV_SERVICE_START
 */
typedef struct gatt_svc_start {
    gatt_svc_t  svc;    /** Service instance. */
    gatt_rst_t  result;
} gatt_svc_start_t;

/**
 * The parameter of GATTS_EV_SERVICE_STOP
 */
typedef struct gatt_svc_stop {
    gatt_svc_t  svc;    /** Service instance. */
    gatt_rst_t  result;
} gatt_svc_stop_t;

/**
 * The parameter of GATTS_EV_CHR_VALUE_READ_REQ
 */
typedef struct gatt_chr_val_read_req {
    peer_t      peer;    /** Peer device instance. */
    gatt_chr_t  chr;     /** Characteristic instance. */
    uint16_t    val_off; /** Characteristic Value Offset */
} gatt_chr_val_read_req_t;

/**
 * The parameter of GATTS_EV_CHR_VALUE_WRITE_REQ
 */
typedef struct gatt_chr_val_write_req {
    peer_t      peer;    /** Peer device instance. */
    gatt_chr_t  chr;     /** Characteristic instance. */
    const uint8_t *val;     /** Characteristic value. */
    uint16_t    val_len; /** Characteristic value length. */
    uint16_t    val_off; /** Characteristic Value Offset */
} gatt_chr_val_write_req_t;

/**
 * The parameter of GATTS_EV_CHR_VALUE_SEND_START
 */
typedef struct gatt_chr_val_send_start {
    peer_t          peer; /** Peer device instance. */
    gatt_chr_t      chr;  /** Characteristic instance. */
    gatt_ccc_val_t  type; /** Client Characteristic Configuration value type */
} gatt_chr_val_send_start_t,
  gatt_chr_val_send_state_t;

/**
 * The parameter of GATTS_EV_CHR_VALUE_SEND_CFM
 */
typedef struct gatt_chr_val_send_cfm {
    peer_t     peer; /** Peer device instance. */
    gatt_chr_t chr;  /** Characteristic instance. */
} gatt_chr_val_send_cfm_t;

/**
 * The parameter of GATTC_EV_SERVICE_DISCOVERED
 */
typedef struct gatt_svc_discovered {
    peer_t      peer;   /** Peer device instance. */
    gatt_svc_t  svc;    /** Service instance. */
    uuid_t       uuid;   /** 16, 32, 128 bit UUID */
    gatt_rst_t  result;
} gatt_svc_discovered_t
, gatt_svc_discover_cmpl_t;

/**
 * The parameter of GATTC_EV_CHARACTERISTIC_DISCOVERED
 */
typedef struct gatt_chr_discovered {
    peer_t      peer;   /** Peer device instance. */
    gatt_svc_t  svc;    /** Service instance, the Chararicteristic belonged to */
    gatt_chr_t  chr;    /** Characteristic instance. */
    uuid_t       uuid;   /** 16, 32, 128 bit UUID */
    uint8_t     props;  /** The characteristic properties */
    gatt_rst_t  result;
} gatt_chr_discovered_t
, gatt_chr_discover_cmpl_t;

/**
 * The parameter of GATTC_EV_CHR_DSC_DISCOVERED
 */
typedef struct gatt_chr_dsc_discovered {
    peer_t      peer;   /** Peer device instance. */
    gatt_chr_t  chr;    /** Characteristic instance. */
    gatt_dsc_t  dsc;    /** Characteristic desc */
    uuid_t      uuid;   /** 16, 32, 128 bit UUID */
    gatt_rst_t  result;
} gatt_chr_dsc_discovered_t
, gatt_chr_dsc_discover_cmpl_t;

/**
 * The parameter of GATTC_EV_CHR_VALUE_READ_RSP
 */
typedef struct {
    peer_t      peer;    /** Peer device instance. */
    gatt_chr_t  chr;     /** Characteristic instance, the Descriptor belonged to */
    const uint8_t    *val;     /** Characteristic value. */
    uint32_t    val_len; /** Characteristic value length. */
    gatt_rst_t  result;
} gatt_chr_val_read_rsp_t;

/**
 * The parameter of GATTC_EV_CHR_VALUE_WRITE_RSP
 */
typedef struct {
    peer_t      peer;   /** Peer device instance. */
    gatt_chr_t  chr;    /** Characteristic instance, the Descriptor belonged to */
    gatt_rst_t  result;
} gatt_chr_val_write_rsp_t;

/**
 * The parameter of GATTC_EV_DSC_CCC_WRITE_RSP
 */
typedef struct {
    peer_t      peer;   /** Peer device instance. */
    gatt_dsc_t  dsc;    /** Characteristic desc. */
    gatt_ccc_val_t val;
    gatt_rst_t  result;
} gatt_dsc_ccc_enable_t
, gatt_dsc_ccc_disable_t;

/**
 * The parameter of GATTC_EV_CHR_VALUE_NOTIFIED
 */
typedef struct {
    peer_t      peer;    /** Peer device instance. */
    gatt_chr_t  chr;     /** Characteristic instance, the Descriptor belonged to */
    const uint8_t    *val;     /** Characteristic value. */
    uint32_t    val_len; /** Characteristic value length. */
} gatt_chr_val_notif_t;

typedef struct {
    gatt_chr_t *pchr;       //to save created characteristic instance
    uuid_t uuid;
    uint16_t props;         //gatt_chr_props_bits_t, 0xFFFF for not available
    uint16_t secu;          //gatt_secu_bits_t, 0xFFFF for not available
    uint16_t ccc_props;     //gatt_chr_props_bits_t, 0xFFFF for not available
    uint16_t ccc_secu;      //gatt_secu_bits_t, 0xFFFF for not available
    gatt_cbk_t *cbk;
    void *ctx;
} gatt_chr_add_params_t;

typedef struct {
    gatt_svc_t *psvc;        //to save created service instance
    uuid_t uuid;
    gatt_cbk_t *cbk;
    void *ctx;
    uint8_t chr_num;
    gatt_chr_add_params_t *chrs;
} gatt_svc_create_params_t;

void gatts_svc_create_ex(uint8_t svc_num, gatt_svc_create_params_t *svc_create_params);

/*
 * FUNCTION DECLARATIONS
 *******************************************************************************
 */

/**
 * Create a Service.
 *
 * @param[in]  uuid  The Service UUID.
 * @param[in]  cbk   The Event callback function.
 * @param[in]  ctx   Customized parameters.
 *
 * @return     Service Instance.
 */
gatt_svc_t gatts_svc_create(uuid_t *uuid, gatt_cbk_t *cbk, void *ctx);
gatt_svc_t gatts_svc_create_uuid16(uint16_t uuid, gatt_cbk_t *cbk, void *ctx);

/**
 * Delete a Service.
 *
 * This service SHOULD BE stopped or not started, otherwise the function will fail.
 *
 * @param[in]  svc   The Service instance.
 */
void gatts_svc_delete(gatt_svc_t svc);

void gatts_inc_add(gatt_svc_t svc, gatt_svc_t inc);

/**
 * Create a Characteristic, and add it to a Service.
 *
 * @param[in]  svc        The Service that the Characteristic will be added to.
 * @param[in]  uuid       The Characteristic UUID.
 * @param[in]  props      The Characteristic Properties bits field.
 *                        (gatt_chr_props_bits_t)
 * @param[in]  val_perms  The Characteristic Value attribute permissions.
 *                        (att_perm_bits_t)
 * @param[in]  cbk        The Event callback function.
 * @param[in]  ctx        Customized parameters.
 *
 * @return     Descriptor instance.
 */
gatt_chr_t gatts_chr_add(gatt_svc_t svc, const uuid_t *uuid
                        , uint16_t props, uint16_t val_perms
                        , gatt_cbk_t *cbk, void *ctx);

gatt_chr_t gatts_chr_add_uuid16(gatt_svc_t svc, uint16_t uuid
                        , uint16_t props, uint16_t val_perms
                        , gatt_cbk_t *cbk, void *ctx);

/**
 * Create a Characteristic, and add it to a Service.
 *
 * @param[in]  svc        The Service that the Characteristic will be added to.
 * @param[in]  uuid       The Characteristic UUID.
 * @param[in]  props      The Characteristic Properties bits field.
 *                        (gatt_chr_props_bits_t)
 * @param[in]  val_perms  The Characteristic Value attribute permissions.
 *                        (att_perm_bits_t)
 * @param[in]  cbk        The Event callback function.
 * @param[in]  ctx        Customized parameters.
 *
 * @param[in]  v_len      characteristic value len .
 * @param[in]  vlue       characteristic value..
 * @return     Descriptor instance.
 */

gatt_chr_t gatts_chr_ro_add(gatt_svc_t svc, const uuid_t *uuid
						, uint16_t props, uint16_t val_perms
						, gatt_cbk_t *cbk, void *ctx, 
						uint8_t *val, uint8_t val_len);

gatt_chr_t gatts_chr_ro_add_uuid16(gatt_svc_t svc, uint16_t uuid
						, uint16_t props, uint16_t val_perms
						, gatt_cbk_t *cbk, void *ctx, 
						uint8_t *val, uint8_t val_len);
                                      
/**
 * Create a Client Characteristic Configuration Descriptor, and add it to the
 * Characteristic belonged to.
 *
 * @param[in]  chr    The Characteristic that Descriptor will be added to
 * @param[in]  props  The Characteristic Properties bits field.
 *                    (Just GATT_CHR_PROP_NOTIFY or GATT_CHR_PROP_INDICATE)
 * @param[in]  perms  The attribute permissions (att_perm_bits_t)
 */
void gatts_dsc_ccc_add(gatt_chr_t chr, uint8_t props, uint16_t perms);
void gatts_ref_dsc_ccc_add(gatt_chr_t chr, uint8_t props, uint16_t perms,uint16_t v_len,uint8_t *val);
/**
 * Start a Service.
 *
 * When the Service is started, the Attribute Handle of each attribute in this
 * service will be returned by the callback function.
 *
 * @param[in]  svc  The service instance.
 */
void gatts_svc_start(gatt_svc_t svc);

/**
 * Stop a Service.
 *
 * @param[in]  svc  The service instance.
 */
void gatts_svc_stop(gatt_svc_t svc);

/**
 * Send response for the read request.
 *
 * @param[in]  peer     The peer device instance.
 * @param[in]  chr      The Characteristic instance.
 * @param[in]  res      Error code.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 */
void gatts_chr_val_read_rsp(peer_t peer, gatt_chr_t chr, gatt_rst_t res
                           , uint8_t *val, uint16_t val_len);

/**
 * Send response for the Write request.
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  chr   The Characteristic instance.
 * @param[in]  res   Result.
 */
void gatts_chr_val_write_rsp(peer_t peer, gatt_chr_t chr, gatt_rst_t res);

/**
 * Send a Notification of a Characteristic Value.
 *
 * When the Client Characteristic Configuration Descriptor in this Characteristic
 * is set as Notification, this Value will be notified.
 *
 * @param[in]  peer     The peer device instance.
 * @param[in]  chr      The Characteristic instance.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 *
 * @return  Result.
 */
gatt_rst_t gatts_chr_val_notif(peer_t peer, gatt_chr_t chr
                              , uint8_t *val, uint16_t val_len);

/**
 * Send a Indication of a Characteristic Value.
 *
 * When the Client Characteristic Configuration Descriptor in this Characteristic
 * is set as Indication, this Value will be indicated.
 *
 * @param[in]  peer     The peer device instance.
 * @param[in]  chr      The Characteristic instance.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 *
 * @return  Result.
 */
gatt_rst_t gatts_chr_val_indicate(peer_t peer, gatt_chr_t chr
                                 , uint8_t *val, uint16_t val_len);

/**
 * Discover all primary services or discover primary service by service UUID.
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  uuid  If uuid is NULL, discover all services. If uuid is set,
 *                   discover servic by this service uuid.
 * @param[in]  cbk   The callback function to return the result.
 *                   It SHALL NOT be NULL.
 *                   The event is GATTC_EV_SERVICE_DISCOVERED(1 or repeated)
 * @param[in]  ctx   The costumized Context data for this task, will be returned
 *                   in the callback function.
 */
void gattc_svc_discover(peer_t peer, uuid_t *uuid, gatt_cbk_t *cbk, void *ctx);


/**
 * Discover all characteristic of a service or discover characteristics by UUID.
 * @param[in]  peer  The peer device instance.
 * @param[in]  svc   The service instance.
 * @param[in]  uuid  If uuid is NULL, discover all characteristic. If uuid is set,
 *                   discover characteristic by this characteristic uuid.
 * @param[in]  cbk   The callback function to return the result.
 *                   It SHALL NOT be NULL.
 *                   The event is GATTC_EV_CHARACTERISTIC_DISCOVERED(1 or repeated)
 * @param[in]  ctx   The costumized Context data for this task, will be returned
 *                   in the callback function.
 */
void gattc_chr_discover(peer_t peer, gatt_svc_t svc, uuid_t *uuid
                       , gatt_cbk_t *cbk, void *ctx);

/**
 * Read the value of a Characteristic.
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  chr   The Characteristic instance.
 * @param[in]  cbk   The callback function to return the result.
 *                   It SHALL NOT be NULL.
 *                   The event is GATTC_EV_CHR_VALUE_READ_RSP
 * @param[in]  ctx   The costumized Context data for this task, will be returned
 *                   in the callback function.
 */
void gattc_chr_val_read(peer_t peer, gatt_chr_t chr, gatt_cbk_t *cbk, void *ctx);

/**
 * Write the value of a Characteristic using WriteRequest operation.
 *
 * @param[in]  peer     The peer device instance.
 * @param[in]  chr      The Characteristic instance.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 * @param[in]  cbk      The callback function to return the result.
 *                      It SHALL NOT be NULL.
 *                      The event is GATTC_EV_CHR_VALUE_WRITE_RSP
 * @param[in]  ctx      The costumized Context data for this task, will be returned
 *                      in the callback function.
 */
void gattc_chr_val_write(peer_t peer, gatt_chr_t chr, uint8_t *val, uint16_t val_len
                        , gatt_cbk_t *cbk, void *ctx);

/**
 * Write the value of a Characteristic using WriteWithoutResponse operation.
 *
 * The event GATTC_EV_CHR_VALUE_WRITE_RSP is reported after the WriteCommand packet
 * is sent over the air which is identified by HCI Number of Completed Packet event(s).
 * This provides a simple flow control when WriteCommand is used to send packets
 * continuously.
 *
 * @param[in]  peer     The peer device instance.
 * @param[in]  chr      The Characteristic instance.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 * @param[in]  cbk      The callback function to return the result.
 *                      It SHALL NOT be NULL.
 *                      The event is GATTC_EV_CHR_VALUE_WRITE_RSP
 * @param[in]  ctx      The costumized Context data for this task, will be returned
 *                      in the callback function.
 */
void gattc_chr_val_write_cmd(peer_t peer, gatt_chr_t chr
                                    , uint8_t *val, uint16_t val_len
                                    , gatt_cbk_t *cbk, void *ctx);

/**
 * Write the value of a Client Characteristic Configuration Descriptor.
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  chr   The Characteristic instance.
 * @param[in]  dsc   The Characteristic desc.
 * @prarm[in]  val   The value will be written.
 * @param[in]  cbk   The callback function to return the result.
 *                   It SHALL NOT be NULL.
 *                   The event is GATTC_EV_DSC_CCC_WRITE_RSP
 * @param[in]  ctx   The costumized Context data for this task, will be returned
 *                   in the callback function.
 */
void gattc_dsc_ccc_enable(peer_t peer, gatt_chr_t chr, gatt_dsc_t dsc, gatt_ccc_val_t val
                        , gatt_cbk_t *cbk, void *ctx);

/**
 * Write the value of a Client Characteristic Configuration Descriptor.
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  chr   The Characteristic instance.
 */
void gattc_dsc_ccc_disable(peer_t peer, gatt_chr_t chr);

/**
 * Exchange MTU.
 *
 * @param peer [in] The peer device instance.
 * @param mtu  [in] mtu.
 */
void gatts_exchange_mtu(peer_t peer, uint16_t mtu);

/**
 * Get MTU.
 *
 * @param peer [in] The peer device instance.
 *
 * @return mtu.
 */
uint16_t gatt_get_mtu(peer_t peer);

void gatt_set_default_mtu(uint16_t mtu);

/**
 * Get Identity Address.
 */
bool smp_get_ident_addr(peer_t peer, uint8_t *addr_type, bd_addr_t addr);

/**
 * Accept pair requst.
 *
 * @param peer [in] The peer device instance.
 * @param accept [in] accept or not
 *
 */
void smp_pair_accept(peer_t peer, bool accept);

/**
 * Input passkey.
 *
 * @param peer [in] The peer device instance.
 * @param passkey [in] passkey
 *
 */
void smp_passkey_entry(peer_t peer, uint32_t passkey);

/**
 * User confirm.
 *
 * @param peer [in] The peer device instance.
 * @param is_same [in] 1 = Same(OK) or 0 = Different
 *
 */
void smp_user_confirm(peer_t peer, bool is_same);

//just for local display
bool smp_set_local_display_passkey(uint32_t passkey);

uint32_t smp_get_local_display_passkey(void);

/**
 * discover remote ble service
 *
 * @param[in]  peer  The peer device instance.
 * @param[in]  svc   The Characteristic instance.
 * @prarm[in]  uuid  The uuid to be discovered
 * @param[in]  cbk   The callback function to return the result.
 * @param[in]  context   The costumized Context data for this task, will be returned
 *                   in the callback function.
 */
void gattc_des_discover(peer_t peer, gatt_svc_t svc, uuid_t *uuid
                                    , gatt_cbk_t *cbk, void *context);


/// @} GATT

#endif // !_GATT_H_

