/**
 *******************************************************************************
 *
 * Copyright (c) 1999-2023 BRT Techonology Limited
 *
 * All rights reserved.
 *
 * -----------------------------------------------------------------------------
 *
 * @file    gap.h
 * @author  Weigh
 *
 *******************************************************************************
 */

#ifndef _GAP_H_
#define _GAP_H_

/**
 *******************************************************************************
 * @defgroup GAP
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef void *  peer_t;

#define INST_INVALID    ((void *)0)

#define _APP_FREE_
#define _SDK_FREE_
#define _FUNCTION_

/*
 * BLUETOOTH DEVICE ADDRESS DEFINITIONS
 *******************************************************************************
 */

#define BD_ADDR_SIZE    6

extern uint8_t local_bd[];

typedef uint8_t          bd_addr_t[BD_ADDR_SIZE];

typedef enum addr_type {
    ADDR_TYPE_PUBLIC             = 0x00, /** Public Device Address (default)
                                             or for Peer Public Identity Address */
    ADDR_TYPE_RANDOM             = 0x01, /** Random Device Address
                                             or for Peer Random (static) Identity
                                             Address */
    ADDR_TYPE_RPA_USE_PUBLIC     = 0x02, /** Just for own Address, Controller
                                             generates Resolvable Private Address
                                             based on the local IRK from the
                                             resolving list. If the resolving list
                                             contains no matching entry, use the
                                             public address.  */
    ADDR_TYPE_RPA_USE_RANDOM     = 0x03, /** Just for own Address, Controller
                                             generates Resolvable Private Address
                                             based on the local IRK from the
                                             resolving list. If the resolving list
                                             contains no matching entry, use the
                                             random address from LE_Set_Random_Address. */
    ADDR_TYPE_RPA_UNABLE_RESOLVE = 0xFE, /** Resolvable Private Address
                                             (Controller unable to resolve) */
} addr_type_t;

typedef enum hci_role {
	ROLE_MASTER =                                                        0x00,
	ROLE_SLAVE =                                                         0x01
} hci_role_t;

typedef enum hci_io_cap {
    IO_CAPABILITY_DISPLAYONLY =                                                    0x00,
    IO_CAPABILITY_DISPLAYYESNO =                                                   0x01,
    IO_CAPABILITY_KEYBOARDONLY =                                                   0x02,
    IO_CAPABILITY_NOINPUTNOOUTPUT =                                                0x03,
    IO_CAPABILITY_KEYBOARDDISPLAY =                                                0x04
} hci_io_cap_t;

typedef enum gap_sec_mode_level{
	GAP_SECURITY_MODE1_LEVEL1 = 0x00,/* No security */
	GAP_SECURITY_MODE1_LEVEL2 = 0x01,/* Unauthenticated pairing and encryption */
	GAP_SECURITY_MODE1_LEVEL3 = 0x02,/* Authenticated Pairing and encryption */
    GAP_SECURITY_MODE1_LEVEL4 = 0x03,/* Authenticated ECDH Pairing and encryption */
	GAP_SECURITY_MODE2_LEVEL1 = 0x04,/* Unauthenticated pairing and data signing */
	GAP_SECURITY_MODE2_LEVEL2 = 0x05,/* Authenticated pairing and data signing */
} gap_sec_mode_level_t;

enum {
	/* Masks */
	SMP_KEY_GENERATION_METHOD_MASK_LE_SECURE_CONNECTIONS =				0x80,
	SMP_KEY_GENERATION_METHOD_MASK_MITM =								0x40,
	SMP_KEY_GENERATION_METHOD_MASK_PASSKEY_ENTRY =						0x10,
	SMP_KEY_GENERATION_METHOD_MASK_BASIC_ENUM = 						0x0F,
	/* Basic_Enum */
	SMP_KEY_GENERATION_METHOD_USE_JUST_WORKS = 							0,/* Unauthenticated */
	SMP_KEY_GENERATION_METHOD_USE_IO_CAPABILITIES = 					1,/* Temp value, to be dispatched into <Just_Works>, <Numeric_Comparison> or <PassKey_Entry> */
	/* Assert OOB transport support MITM */
	SMP_KEY_GENERATION_METHOD_USE_OOB = 								2,/* Unauthenticated or Authenticated, depend on the OOB communication */
	SMP_KEY_GENERATION_METHOD_USE_NUMERIC_COMPARISON = 					3,/* Authenticated, only LESC */
	SMP_KEY_GENERATION_METHOD_USE_PASSKEY_ENTRY_INITIATOR_DISPLAYS = 	4,/* Authenticated */
	SMP_KEY_GENERATION_METHOD_USE_PASSKEY_ENTRY_RESPONDER_DISPLAYS = 	5,/* Authenticated */
	SMP_KEY_GENERATION_METHOD_USE_PASSKEY_ENTRY_BOTH_INPUTS = 			6 /* Authenticated */
};

/*
 * UUID TYPE DEFINITIONS
 *******************************************************************************
 */

typedef struct uuid_stru {
	enum {
		UUID_LEN_16  = 2,
		UUID_LEN_32  = 4,
		UUID_LEN_128 = 16
	} len;

    union {
        uint8_t   uuid128[16];
        uint32_t  uuid32;
        uint16_t  uuid16;
    } val;

#ifndef CFG_BIG_ENDIAN
    #define UUID16_VAL_SET(_val) \
    { \
          (uint8_t)(_val) \
        , (uint8_t)((_val) >> 8) \
    }
    #define UUID32_VAL_SET(_val) \
    { \
          (uint8_t)(_val) \
        , (uint8_t)((_val) >> 8) \
        , (uint8_t)((_val) >> 16) \
        , (uint8_t)((_val) >> 24) \
    }
#else // !CFG_BIG_ENDIAN
    #define UUID16_VAL_SET(_val) \
    { \
          (uint8_t)((_val) >> 8) \
        , (uint8_t)(_val) \
    }
    #define UUID32_VAL_SET(_val) \
    { \
          (uint8_t)((_val) >> 24) \
        , (uint8_t)((_val) >> 16) \
        , (uint8_t)((_val) >> 8) \
        , (uint8_t)(_val) \
    }
#endif // !CFG_BIG_ENDIAN
    #define UUID16_VAL_GET(_uuid) \
    (uint16_t)( \
          (uint8_t)((_uuid)->val.uuid128[0]) \
        | (uint8_t)(((_uuid)->val.uuid128[1]) << 8) \
    )
    #define UUID32_VAL_GET(_uuid) \
    (uint32_t)( \
          (uint8_t)((_uuid)->val.uuid128[0]) \
        | (uint8_t)(((_uuid)->val.uuid128[1]) << 8) \
        | (uint8_t)(((_uuid)->val.uuid128[2]) << 16) \
        | (uint8_t)(((_uuid)->val.uuid128[3]) << 24) \
    )
} uuid_t;


/*
 * GAP EVENT TYPE DEFINITIONS
 *******************************************************************************
 */
typedef enum gap_event {
    GAP_EV_ADV_SET_ADD_CMPL = 0x01, /** adv_add_cmpl_t
                                        Advertising Set added complete */
    GAP_EV_ADV_SET_ADV_DATA_CMPL,   /** adv_set_adv_data_cmpl_t
                                        Advertising Set adv data complete */
    GAP_EV_ADV_SET_SCN_DATA_CMPL,   /** adv_set_scn_data_cmpl_t
                                        Advertising Set scan response data complete */
    GAP_EV_ADV_SET_RMV_CMPL,        /** adv_rmv_cmpl_t
                                        Advertising Set reomved complete */
    GAP_EV_ADV_SET_CLR_CMPL,        /** adv_clr_cmpl_t
                                        Clear all Advertising Sets complete */
    GAP_EV_ADV_SET_EN_CMPL,         /** adv_en_cmpl_t
                                        Advertising Set Enable complete */
    GAP_EV_ADV_SET_DIS_CMPL,        /** adv_dis_cmpl_t
                                        Advertising Set Disable complete */
    GAP_EV_ADV_SET_TERMINATED,      /** le_adv_set_terminated_t
                                        Advertising Set Terminated event */                                    

    GAP_EV_SCN_STARTED,             /** scn_start_t
                                        Scan started */
    GAP_EV_SCN_STOPED,              /** scn_stop_t
                                        Scan stoped */
    GAP_EV_ADV_REPORT,              /** adv_rpt_t
                                        Scanned Advertising report */

    GAP_EV_PEER_INF_UPDATE,         /** peer_inf_update_t
                                      * [NOTE] DON NOT USE, will be de DEPRECATED,
                                      * should be replaced with GAP_EV_PEER_BONDED */
    GAP_EV_PEER_BONDED = GAP_EV_PEER_INF_UPDATE, /** peer_inf_update_t */
    GAP_EV_PEER_RECOVER_CMPL,       /** peer_recover_cmpl_t */
    GAP_EV_PEER_RMV_CMPL,           /** peer_rmv_cmpl_t */
    GAP_EV_PEER_KEY_MISS,           /** peer_key_miss_t */
    GAP_EV_PEER_NO_BOND,            /** peer_no_bond_t */

    GAP_EV_CONN_CMPL,               /** gap_conn_cmpl_t */
    GAP_EV_DISC_CMPL,               /** gap_disc_cmpl_t */

    GAP_EV_PASSKEY_INPUT,           /** passkey_input_t */
    GAP_EV_PASSKEY_DISPLAY,         /** passkey_display_t */
    GAP_EV_NUMERIC_DISPLAY,         /** numeric_display_t */
    GAP_EV_DISPLAY_CMPL,            /** display_cmpl_t */

    GAP_EV_ENCRYPT_CMPL,            /** encrypt_cmpl_t */
    GAP_EV_PAIRING_REQ,				/** pairing_req_t */
    GAP_EV_PHY_UPDATE_CMPL,			/** phy_update_cmpl_t */
    GAP_EV_DATA_LEN_CHANGE,			/** data_len_change_t */

    GAP_EV_LE_CONN_UPDATE_CMPL,     /** le_peer_conn_update_cmpl_t */
    GAP_EV_LE_PEER_CONN_PAR_REQ,    /** le_peer_conn_par_req_t */
    GAP_EV_LE_READ_REMOTE_FEATURE_CMPL, /** le_peer_read_feature_cmpl_t */
    GAP_EV_ATT_MTU_NOTIFY,          /** att_mtu_ntf_t */

    GAP_EV_NOCP,                    /* nocp_t */
    GAP_EV_ADV_SET_RAND_ADDR_CMPL,	/* adv_set_rand_addr_cmpl_t */	
    GAP_EV_READ_RSSI_CMPL,          /* rssi_t */

	GAP_EV_DEBUG_INFO,				/* char * */
    GAP_EV_PROC_ADV_SET_ADD_CMPL,   /** adv_add_cmpl_t*/
    GAP_EV_PROC_ADV_SET_EN_CMPL,    /** adv_en_cmpl_t*/
    GAP_EV_PROC_ADV_SET_DIS_CMPL,   /** adv_dis_cmpl_t*/
    GAP_EV_PROC_ADV_SET_ADV_DATA_CMPL,/** adv_set_adv_data_cmpl_t*/

	GAP_EV_REPEAT_ATTEMPT,			/** repeat_attempt_t */			
} gap_event_t;

/************************************************************************/

/**
 * GAP Callback Function define.
 *
 * @param [in] ev   GAP Event type
 * @param [in] par  GAP Event paramters
 * @param [in] ctx  The costumized context data
 */
typedef void (gap_cbk_t)(gap_event_t ev, void *ctx, void *par);

typedef struct gap_ev_cbk {
    gap_event_t  ev;
    gap_cbk_t   *cbk;
} gap_ev_cbk_t;

typedef struct gap_cmpl {
    uint8_t  st;     /** Status */
} adv_clr_cmpl_t
, scn_start_t
, scn_stop_t;

/**
 * Advertising Set created complete parameters
 */
typedef struct adv_add_cmpl {
    uint8_t  st;     /** Status */
    uint8_t  hdl;    /** Advertising Set Handle */
    uint8_t  tx_pwr; /** Advertising TX power */
} adv_add_cmpl_t;

typedef struct adv_en_cmpl {
    uint8_t  st;     /** Status */
    uint8_t  num;    /** Advertising Set number */
    uint8_t  hdl[]; /** Advertising Set Handles */
} adv_en_cmpl_t,
  adv_dis_cmpl_t;

typedef struct adv_cmpl {
    uint8_t  st;  /** Status */
    uint8_t  hdl; /** Advertising Set Handle */
} adv_rmv_cmpl_t,
  adv_set_adv_data_cmpl_t,
  adv_set_scn_data_cmpl_t,
  adv_set_rand_addr_cmpl_t;

typedef enum adv_rpt_data_st {
    ADV_RPT_DATA_ST_CMPL = 0x00, /** Complete */
    ADV_RPT_DATA_ST_MORE = 0x01, /** Incomplete, more data to come */
    ADV_RPT_DATA_ST_TRNC = 0x02, /** Incomplete, data truncated, no more to come */
} adv_rpt_data_st_t;

typedef struct adv_rpt {
    uint16_t   ev_type;   /** adv_ev_prop_bits
                              Advertising Event type */
    addr_type_t    addr_type; /** addr_type_t
                              Address Type */
    bd_addr_t  addr;      /** Address */
    bd_addr_t  dir_addr;
    uint8_t    prim_phy;  /** adv_phy_t
                              Primary PHY */
    uint8_t    scnd_phy;  /** adv_phy_t
                              Secondary PHY */
    uint8_t    adv_sid;   /** 0x00 - 0x0F: Value of the Advertising SID
                                           subfield in the ADI field of
                                           the PDU or, for scan responses,
                                           in the ADI field of the original
                                           scannable advertisement
                              0xFF       : No ADI field provided */
    int8_t     tx_pwr;    /** TX power
                              0xXX Range: -127 to +20
                                   Units: dBm
                              0x7F Tx Power information not available */
    int8_t    rssi;      /** 0xXX Range: -127 to +20
                                   Units: dBm
                              0x7F RSSI is not available */
    uint8_t    data_st;   /** adv_rpt_data_st_t
                              Data Status */
    uint8_t    data_len;  /** Data length */
    uint8_t   *data;      /** Data */
} adv_rpt_t;

/**
 * The parameter of GAP_EV_PEER_INF_UPDATE
 */
typedef struct peer_inf_update_stru {
    peer_t    peer;    /** Peer device instance. */
    uint8_t  *inf;     /** Peer information */
    uint32_t  inf_sz;  /** Peer information size, in bytes */
} peer_inf_update_t;

/**
 * The parameter of GAP_EV_PEER_BONDED
 */
typedef struct peer_bonded_stru {
    peer_t    peer;    /** Peer device instance. */
    uint8_t  *inf;     /** Peer information */
    uint32_t  inf_sz;  /** Peer information size, in bytes */
} peer_bonded_t;

/**
 * The parameter of GAP_EV_PEER_RECOVER_CMPL
 */
typedef struct peer_recover_cmpl_stru {
    peer_t   peer;   /** Peer device instance. */
    uint8_t  result;
} peer_recover_cmpl_t;

/**
 * The parameter of GAP_EV_PEER_RMV_CMPL
 */
typedef struct peer_rmv_cmpl_stru {
    peer_t   peer;   /** Peer device instance. */
    uint8_t  result;
} peer_rmv_cmpl_t;

/**
 * The parameter of GAP_EV_PEER_KEY_MISS
 */
typedef struct peer_key_miss_stru {
    peer_t   peer;   /** Peer device instance. */
} peer_key_miss_t;

/**
 * The parameter of GAP_EV_PEER_NO_BOND
 */
typedef struct peer_no_bond_stru {
    peer_t   peer;   /** Peer device instance. */
} peer_no_bond_t;

/* The parameter of GAP_EV_CONN_CMPL */
typedef struct gap_conn_cmpl_stru {
    peer_t   peer;   /** Peer device instance. */
    uint8_t  result;
} gap_conn_cmpl_t;

/**
 * The parameter of GAP_EV_DISC_CMPL
 */
typedef struct gap_disc_cmpl_stru {
    peer_t   peer;   /** Peer device instance. */
    uint8_t  result;
} gap_disc_cmpl_t;

typedef struct le_peer_conn_update_cmpl_stru {
    peer_t    peer;         /** Peer device instance. */
    uint8_t   result;
    uint16_t  conn_intv;
    uint16_t  lantency_max;
    uint16_t  timeout;
} le_peer_conn_update_cmpl_t;

/**
 * The parameter of GAP_EV_LE_READ_REMOTE_FEATURE_CMPL
 */
typedef struct le_peer_read_feature_cmpl_stru {
    peer_t    peer;         /** Peer device instance. */
    uint8_t   result;
    uint8_t   le_features[8];
} le_peer_read_feature_cmpl_t;

typedef struct le_peer_conn_par_req_stru {
    peer_t    peer;         /** Peer device instance. */
    uint16_t  intv_min;
    uint16_t  intv_max;
    uint16_t  lantency_max;
    uint16_t  timeout;
} le_peer_conn_par_req_t;

/**
 * The parameter of GAP_EV_PASSKEY_INPUT
 */
typedef struct passkey_input_stru {
    peer_t   peer;         /** Peer device instance. */
} passkey_input_t;

/**
 * The parameter of GAP_EV_PASSKEY_DISPLAY
 */
typedef struct passkey_display_stru {
    peer_t   peer;         /** Peer device instance. */
    uint8_t  passkey[6];
} passkey_display_t;

/**
 * The parameter of GAP_EV_NUMERIC_DISPLAY
 */
typedef struct numeric_display_stru {
    peer_t   peer;         /** Peer device instance. */
    uint8_t  numeric[6];
} numeric_display_t;

/**
 * The parameter of GAP_EV_DISPLAY_CMPL
 */
typedef struct display_cmpl_stru {
    peer_t   peer;         /** peer device instance. */
} display_cmpl_t;

/**
 * The parameter of GAP_EV_ENCRYPT_CMPL
 */
typedef struct encrypt_cmpl_stru {
    peer_t   peer;         /** peer device instance. */
    uint8_t  status;       /** Encrytion status. */
} encrypt_cmpl_t;

/**
 * The parameter of GAP_EV_PAIRING_REQ
 */
typedef struct pairing_req_stru {
    peer_t   peer;         /** peer device instance. */
    uint8_t  method;
} pairing_req_t;

typedef struct data_len_change {/* LEEvent */
	peer_t peer;                                       /** Peer device instance. */
	uint16_t maxtxoctets;                              /* ^[400],Val */
	uint16_t maxtxtime;                                /* ^[401],Time */
	uint16_t maxrxoctets;                              /* ^[402],Val */
	uint16_t maxrxtime;                                /* ^[403],Time */
} data_len_change_t;/* [Len:2] - 0xaa,0x02 - U16,U16,U16,U16,U16,2T,R4 */

typedef struct phy_update_cmpl {/* LEEvent */
	peer_t peer;    								 /** Peer device instance. */
	uint8_t status;                                    /* ^[001],Enum */
	uint8_t tx_phy;                                    /* ^[412],Enum */
	uint8_t rx_phy;                                    /* ^[413],Enum */
} phy_update_cmpl_t;/* [Len:2] - 0x59,0x00 - U8,U16,U8,U8,2T,R6 */

typedef struct le_adv_set_terminated {/* LEEvent */
	peer_t peer;    								 /** Peer device instance. */
	uint8_t status;                                    /* ^[001],Enum hci_status_t*/
	uint8_t advertising_handle;                        /* ^[430],Val */
	uint8_t num_completed_extended_advertising_events; /* ^[477],Val */
} le_adv_set_terminated_t;/* [Len:2] - 0x65,0x00 - U8,U8,U16,U8,2T,R6 */

typedef struct nocp {
	peer_t peer;    								 /** Peer device instance. */
	uint8_t num;
} nocp_t;

typedef struct att_mtu_ntf {
    peer_t peer;                                       /** Peer device instance. */
    uint16_t mtu;
} att_mtu_ntf_t;

typedef struct rssi {
    peer_t peer;                                      /** Peer device instance. */
    uint8_t status;     
    int8_t rssi;                      
} rssi_t;

typedef struct repeat_attempt {
    peer_t peer;    /** Peer device instance. */
} repeat_attempt_t;

/*
 * LE ADVERTISING SET PARAMETER TYPE DEFINITIONS
 *******************************************************************************
 */

typedef enum adv_ev_prop_bits_enum {
    ADV_EV_PROP_BITS_CONNECTABLE     = (0x01 << 0), /** Connectable adv */
    ADV_EV_PROP_BITS_SCANNABLE       = (0x01 << 1), /** Scannable adv */
    ADV_EV_PROP_BITS_DIRECTED        = (0x01 << 2), /** Directed adv */
    ADV_EV_PROP_BITS_HIGH_DUTY       = (0x01 << 3), /** High Duty Cycle Directed
                                                        Connectable adv (<= 3.75 ms
                                                        Advertising Interval) */
    ADV_EV_PROP_BITS_LEGACY          = (0x01 << 4), /** Use legacy adv PDUs */
    ADV_EV_PROP_BITS_ANONYMOUS       = (0x01 << 5), /** Omit advertiser*s addr
                                                        from all PDUs
                                                        ("anonymous adv") */
    ADV_EV_PROP_BITS_INCLUDE_TXPOWER = (0x01 << 6), /** Include Tx_Power in the
                                                        extended header of at least
                                                        one adv PDU */
} adv_ev_prop_bits_t;

typedef enum adv_ch_map_bits_enum {
    ADV_CH_MAP_37 = (0x1 << 0), /** Channel 37 shall be used */
    ADV_CH_MAP_38 = (0x1 << 1), /** Channel 38 shall be used */
    ADV_CH_MAP_39 = (0x1 << 2), /** Channel 39 shall be used */
} adv_ch_map_t;

typedef enum adv_policy_enum {
    ADV_POLICY_PROC_SCAN_CONN_ALL         = 0x00, /** Process scan and connection
                                                      requests from all devices
                                                      (i.e., the Filter Accept
                                                      List is not in use)
                                                      (default).  */
    ADV_POLICY_PROC_CONN_ALL_SCAN_IN_LIST = 0x01, /** Process connection requests
                                                      from all devices and scan
                                                      requests only from devices
                                                      that are in the Filter Accept
                                                      List.  */
    ADV_POLICY_PROC_SCAN_ALL_CONN_IN_LIST = 0x02, /** Process scan requests from
                                                      all devices and connection
                                                      requests only from devices
                                                      that are in the Filter Accept
                                                      List. */
    ADV_POLICY_PROC_CONN_SCAN_IN_LIST     = 0x03, /** Process scan and connection
                                                      requests only from devices
                                                      in the Filter Accept List.  */
} adv_policy_t;

typedef enum adv_phy_enum {
    ADV_PHY_NONE_SCND = 0x00, /** No packets on the secondary advertising physical
                                  channel */
    ADV_PHY_1M        = 0x01, /** advertisement PHY is LE 1M */
    ADV_PHY_2M        = 0x02, /** advertisement PHY is LE 2M */
    ADV_PHY_CODED     = 0x03, /** If the Advertising Coding Selection (Host Support)
                                  feature bit is set: Advertising PHY is LE Coded
                                  with S=8 data coding
                                  Otherwise: Advertiser PHY is LE Coded */
    ADV_PHY_CODED_S2  = 0x04, /** Just the Advertising Coding Selection (Host
                                  Support) feature bit is set: Advertising PHY
                                  is LE Coded with S=2 data coding */
} adv_phy_t;

typedef enum adv_frag_pref_enum {
    ADV_FRAG_PREF_CNTL_MAY_FRAG = 0x00, /** The Controller may fragment all
                                            advertising data or Scan Response data */
    ADV_FRAG_PREF_CNTL_MIN_FRAG = 0x01, /** The Controller should not fragment
                                            or should minimize fragmentation of
                                            advertising data or Scan Response data */
} adv_frag_pref_t;

typedef struct adv_data_stru {
    uint8_t *data; 					/** advertising data or scan response data */
    uint8_t  len;  					/** data length, 0 to 251 */
} adv_data_t;


/**
 * Advertising Set ADV data parameters
 */
typedef struct adv_set_adv_data_param_stru {
    uint8_t     adv_hdl;            /** Used to identify an advertising set
                                        User defined
                                        Range: 0x00 to 0xEF */
    uint8_t     adv_data_frag_pref; /** adv_frag_pref_t
                                        Advertising data fragments Preference */
    uint8_t     adv_data_frag_num;  /** Advertising data fragments number */
    adv_data_t *adv_data_frag;      /** Advertising data fragments */
} adv_set_adv_data_param_t;

/**
 * Advertising Set Scan Response data parameters
 */
typedef struct adv_set_scn_data_param_stru {
    uint8_t     adv_hdl;            /** Used to identify an advertising set
                                        User defined
                                        Range: 0x00 to 0xEF */
    uint8_t     scn_data_frag_pref; /** adv_frag_pref_t
                                        Scan Response data fragments Preference */
    uint8_t     scn_data_frag_num;  /** Scan Response data fragments number */
    adv_data_t *scn_data_frag;      /** Scan Response data fragments */
} adv_set_scn_data_param_t;


/**
 * Advertising Set parameters
 */
typedef struct adv_param_stru {
    uint8_t     adv_hdl;            /** Used to identify an advertising set
                                        User defined
                                        Range: 0x00 to 0xEF */
    uint16_t    adv_ev_prop;        /** adv_ev_prop_bits_t
                                        Advertising Event Properties */
    uint32_t    prim_adv_intv_min;  /** Minimum advertising interval for undirected
                                        and low duty cycle directed advertising.
                                        Range: 0x000020 to 0xFFFFFF
                                        Time = N * 0.625 ms
                                        Time Range: 20 ms to 10,485.759375 s */
    uint32_t    prim_adv_intv_max;  /** Maximum advertising interval for undirected
                                        and low duty cycle directed advertising.
                                        Range: 0x000020 to 0xFFFFFF
                                        Time = N * 0.625 ms
                                        Time Range: 20 ms to 10,485.759375 s */
    uint8_t     prim_adv_ch_map;    /** adv_ch_map_t
                                        Primary Advertising Channel Map */
    uint8_t     own_addr_type;      /** addr_type_t
                                        Own Address Type */
    bd_addr_t   own_random_addr;    /** Own random Address
                                        If own_addr_type is RANDOM, this
                                        SHALL be VALID */
    uint8_t     peer_addr_type;     /** ONLY ADDR_TYPE_PUBLIC/ADDR_TYPE_RANDOM
                                        Peer Address Type
                                        If Directed advertising is selected, this
                                        SHALL be VALID */
    bd_addr_t   peer_addr;          /** Peer Address
                                        If Directed advertising is selected, this
                                        SHALL be VALID */
    uint8_t     adv_filter_policy;  /** adv_policy_t
                                        Advertising Filter Policy
                                        If Directed advertising is selected, this
                                        will be ignored */
    uint8_t     adv_tx_pwr;         /** Advertising TX power
                                        0xXX Range: -127 to +20
                                             Units: dBm
                                        0x7F Host has no preference */
    uint8_t     prim_adv_phy;       /** ONLY ADV_PHY_1M/ADV_PHY_CODED */
    uint8_t     scnd_adv_max_skip;  /** When 0x00: AUX_ADV_IND shall be sent prior
                                                   to the next adv event.
                                        others   : Maximum adv events the Controller
                                                   can skip before sending the
                                                   AUX_ADV_IND packets on the secondary
                                                   adv physical channel */
    uint8_t     scnd_adv_phy;       /** ONLY ADV_PHY_1M/ADV_PHY_2M/ADV_PHY_CODED */
    uint8_t     adv_sid;            /** Value of the Advertising SID subfield
                                        in the ADI field of the PDU */
    bool        scan_req_notif_en;  /** Scan Request notifications enable or disable */
    uint8_t     adv_data_frag_pref; /** adv_frag_pref_t
                                        Advertising data fragments Preference */
    uint8_t     adv_data_frag_num;  /** Advertising data fragments number */
    adv_data_t *adv_data_frag;      /** Advertising data fragments */
    uint8_t     scn_data_frag_pref; /** adv_frag_pref_t
                                        Scan Response data fragments Preference */
    uint8_t     scn_data_frag_num;  /** Scan Response data fragments number */
    adv_data_t *scn_data_frag;      /** Scan Response data fragments */
} adv_param_t;

typedef struct adv_en_param_stru {
    uint8_t hdl;        /** Advertising Set Handle */
    uint16_t duration;   /** 0x0000 No advertising duration. Advertising to continue
                                   until the Host disables it.
                            0xXXXX Advertising duration
                                   Range: 0x0001 to 0xFFFF
                                   Time = N * 10 ms
                                   Time Range: 10 ms to 655,350 ms */
    uint8_t max_events; /** Max Advertising Events
                            0x00 No maximum number of advertising events.
                            0xXX Maximum number of advertising events the Controller
                                 shall attempt to send prior to terminating the
                                 advertising */
} adv_en_param_t;


/*
 * LE SCAN PARAMETER TYPE DEFINITIONS
 *******************************************************************************
 */

typedef enum scn_policy_enum {
    SCN_POLICY_BASIC_UNFILTERED    = 0x00, /** Basic unfiltered scanning filter policy */
    SCN_POLICY_BASIC_FILTERED      = 0x01, /** Basic filtered scanning filter policy */
    SCN_POLICY_EXTENDED_UNFILTERED = 0x02, /** Extended unfiltered scanning filter policy */
    SCN_POLICY_EXTENDED_FILTERED   = 0x03, /** Extended filtered scanning filter policy */
} scn_policy_t;

typedef enum scn_phy_bits_enum {
    SCH_PHY_BITS_1M    = (0x01 << 0), /** Scan advertisements on the LE 1M PHY */
    SCH_PHY_BITS_CODED = (0x01 << 2), /** Scan advertisements on the LE Coded PHY */
} scn_phy_bits_t;

typedef enum scn_type_enum {
    SCN_TYPE_PASSIVE = 0x00, /** Passive Scanning. No scan request PDUs shall be sent. */
    SCN_TYPE_ACTIVE  = 0x01, /** Active Scanning. Scan request PDUs may be sent. */
} scn_type_t;

typedef struct scn_phy_param_stru {
    uint8_t   scn_type; /** scn_type_t */
    uint16_t  scn_intv; /** Scan Interval, Time interval from when the Controller
                            started its last scan until it begins the subsequent
                            scan on the primary advertising physical channel.
                            Range: 0x0004 to 0xFFFF
                            Time = N * 0.625 ms
                            Time Range: 2.5 ms to 40.959375 s */
    uint16_t  scn_win;  /** Scan Window, Duration of the scan on the primary
                            advertising physical channel.
                            Range: 0x0004 to 0xFFFF
                            Time = N * 0.625 ms
                            Time Range: 2.5 ms to 40.959375 s */
} scn_phy_param_t;

typedef struct scn_param_stru {
    bool             filter_duplicates; /** Filter Duplicates enable or not */
    uint16_t         duration;          /** 0x0000 Scan continuously until
                                                   explicitly disable
                                            0xXXXX Scan duration
                                                   Range: 0x0001 to 0xFFFF
                                                   Time = N * 10 ms
                                                   Time Range: 10 ms to 655.35 s */
    uint16_t         period;            /** 0x0000 Scan continuously
                                            0xXXXX Time interval from when the
                                                   Controller started its last
                                                   Scan_Duration until it begins
                                                   the subsequent Scan_Duration.
                                                   Range: 0x0001 to 0xFFFF
                                                   Time = N * 1.28 sec
                                                   Time Range: 1.28s to 83,884.8s */
    uint8_t          own_addr_type;     /** addr_type_t
                                            Own Address Type */
    uint8_t          filter_policy;     /** scn_policy_t
                                            scanning filter policy */
    uint8_t          phys;              /** scn_phy_bits_t
                                            scanning PHYs */
    const scn_phy_param_t *phys_par;    /** Parameters for scanning PHYs */
} scn_param_t;

typedef struct le_conn_cmpl_stru {
    uint16_t   conn_hdl;
    uint8_t    role;
    uint8_t    peer_addr_type;
    bd_addr_t  peer_addr;
    uint16_t   conn_intv;
    uint16_t   peri_latency;
    uint16_t   sv_timeout;
    uint8_t    cent_clk_acc;
} le_conn_cmpl_t;

typedef struct le_conn_par_stru {
    uint8_t    init_policy;
    uint8_t    own_addr_type;
    uint8_t    peer_addr_type;
    bd_addr_t  peer_addr;
    uint8_t    phys;
    uint16_t   scn_intv;
    uint16_t   scn_win;
    uint16_t   conn_intv_min;
    uint16_t   conn_intv_max;
    uint16_t   latency_max;
    uint16_t   sv_timeout;
    uint16_t   ce_len_min;
    uint16_t   ce_len_max;
} le_conn_par_t;

typedef struct le_conn_update_stru {
    peer_t     peer;
    uint16_t   intv_min;
    uint16_t   intv_max;
    uint16_t   latency_max;
    uint16_t   timeout;
    uint16_t   ce_len_min;
    uint16_t   ce_len_max;
} le_conn_update_t;

typedef enum 
{
    TX_POWER_0_DBM = 0, /*  0 dBm */
    TX_POWER_NEG2_DBM,  /* -2 dBm */  
    TX_POWER_NEG4_DBM,  /* -4 dBm */
    TX_POWER_NEG8_DBM,  /* -8 dBm */
    TX_POWER_NEG15_DBM, /* -12 dBm */
    TX_POWER_NEG20_DBM, /* -20 dBm */ 
    TX_POWER_POS2_DBM,  /* +2 dBm */
    TX_POWER_POS3_DBM,  /* +3 dBm */
    TX_POWER_POS4_DBM,  /* +4 dBm */
    TX_POWER_POS6_DBM,  /* +6 dBm */    
    TX_POWER_POS8_DBM,  /* +8 dBm */ 
}ble_rf_tx_power_t;

typedef enum hci_status_stru {
	GAP_HCI_STATUS_OK =                                                          0x00,
	GAP_HCI_STATUS_UNKNOWN_HCI_COMMAND =                                         0x01,
	GAP_HCI_STATUS_UNKNOWN_CONNECTION_IDENTIFIER =                               0x02,
	GAP_HCI_STATUS_HARDWARE_FAILURE =                                            0x03,
	GAP_HCI_STATUS_PAGE_TIMEOUT =                                                0x04,
	GAP_HCI_STATUS_AUTHENTICATION_FAILURE =                                      0x05,
	GAP_HCI_STATUS_PIN_OR_KEY_MISSING =                                          0x06,
	GAP_HCI_STATUS_MEMORY_CAPACITY_EXCEEDED =                                    0x07,
	GAP_HCI_STATUS_CONNECTION_TIMEOUT =                                          0x08,
	GAP_HCI_STATUS_CONNECTION_LIMIT_EXCEEDED =                                   0x09,
	GAP_HCI_STATUS_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED =           0x0a,
	GAP_HCI_STATUS_ACL_CONNECTION_ALREADY_EXISTS =                               0x0b,
	GAP_HCI_STATUS_COMMAND_DISALLOWED =                                          0x0c,
	GAP_HCI_STATUS_CONNECTION_REJECTED_LIMITED_RESOURCES =                       0x0d,
	GAP_HCI_STATUS_CONNECTION_REJECTED_SECURITY_REASONS =                        0x0e,
	GAP_HCI_STATUS_CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR =                    0x0f,
	GAP_HCI_STATUS_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED =                          0x10,
	GAP_HCI_STATUS_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE =                      0x11,
	GAP_HCI_STATUS_INVALID_HCI_COMMAND_PARAMETERS =                              0x12,
	GAP_HCI_STATUS_REMOTE_USER_TERMINATED_CONNECTION =                           0x13,
	GAP_HCI_STATUS_REMOTE_TERMINATED_CONNECTION_LOW_RESOURCES =                  0x14,
	GAP_HCI_STATUS_REMOTE_TERMINATED_CONNECTION_POWER_OFF =                      0x15,
	GAP_HCI_STATUS_CONNECTION_TERMINATED_BY_LOCAL_HOST =                         0x16,
	GAP_HCI_STATUS_REPEATED_ATTEMPTS =                                           0x17,
	GAP_HCI_STATUS_PAIRING_NOT_ALLOWED =                                         0x18,
	GAP_HCI_STATUS_UNKNOWN_LMP_PDU =                                             0x19,
	GAP_HCI_STATUS_UNSUPPORTED_REMOTE_OR_LMP_FEATURE =                           0x1a,
	GAP_HCI_STATUS_SCO_OFFSET_REJECTED =                                         0x1b,
	GAP_HCI_STATUS_SCO_INTERVAL_REJECTED =                                       0x1c,
	GAP_HCI_STATUS_SCO_AIR_MODE_REJECTED =                                       0x1d,
	GAP_HCI_STATUS_INVALID_LMP_PARAMETERS =                                      0x1e,
	GAP_HCI_STATUS_UNSPECIFIED_ERROR =                                           0x1f,
	GAP_HCI_STATUS_UNSUPPORTED_LMP_PARAMETER_VALUE =                             0x20,
	GAP_HCI_STATUS_ROLE_CHANGE_NOT_ALLOWED =                                     0x21,
	GAP_HCI_STATUS_LMP_OR_LL_RESPONSE_TIMEOUT =                                  0x22,
	GAP_HCI_STATUS_LMP_ERROR_TRANSACTION_COLLISION =                             0x23,
	GAP_HCI_STATUS_LMP_PDU_NOT_ALLOWED =                                         0x24,
	GAP_HCI_STATUS_ENCRYPTION_MODE_NOT_ACCEPTABLE =                              0x25,
	GAP_HCI_STATUS_LINK_KEY_CANNOT_BE_CHANGED =                                  0x26,
	GAP_HCI_STATUS_REQUESTED_QOS_NOT_SUPPORTED =                                 0x27,
	GAP_HCI_STATUS_INSTANT_PASSED =                                              0x28,
	GAP_HCI_STATUS_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED =                         0x29,
	GAP_HCI_STATUS_DIFFERENT_TRANSACTION_COLLISION =                             0x2a,
	GAP_HCI_STATUS_QOS_UNACCEPTABLE_PARAMETER =                                  0x2c,
	GAP_HCI_STATUS_QOS_REJECTED =                                                0x2d,
	GAP_HCI_STATUS_CHANNEL_ASSESSMENT_NOT_SUPPORTED =                            0x2e,
	GAP_HCI_STATUS_INSUFFICIENT_SECURITY =                                       0x2f,
	GAP_HCI_STATUS_PARAMETER_OUT_OF_MANDATORY_RANGE =                            0x30,
	GAP_HCI_STATUS_ROLE_SWITCH_PENDING =                                         0x32,
	GAP_HCI_STATUS_RESERVED_SLOT_VIOLATION =                                     0x34,
	GAP_HCI_STATUS_ROLE_SWITCH_FAILED =                                          0x35,
	GAP_HCI_STATUS_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE =                         0x36,
	GAP_HCI_STATUS_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST =                        0x37,
	GAP_HCI_STATUS_HOST_BUSY_PAIRING =                                           0x38,
	GAP_HCI_STATUS_CONNECTION_REJECTED_NO_SUITABLE_CHANNEL_FOUND =               0x39,
	GAP_HCI_STATUS_CONTROLLER_BUSY =                                             0x3a,
	GAP_HCI_STATUS_UNACCEPTABLE_CONNECTION_PARAMETERS =                          0x3b,
	GAP_HCI_STATUS_DIRECTED_ADVERTISING_TIMEOUT =                                0x3c,
	GAP_HCI_STATUS_CONNECTION_TERMINATED_DUE_TO_MIC_FAILURE =                    0x3d,
	GAP_HCI_STATUS_CONNECTION_FAILED_TO_BE_ESTABLISHED =                         0x3e,
	GAP_HCI_STATUS_MAC_CONNECTION_FAILED =                                       0x3f,
	GAP_HCI_STATUS_COARSE_CLOCK_ADJUSTMENT_REJECTED_BUT_WILL_TRY_TO_ADJUST_USING_CLOCK_DRAGGING = 0x40,
	GAP_HCI_STATUS_TYPE0_SUBMAP_NOT_DEFINED =                                    0x41,
	GAP_HCI_STATUS_UNKNOWN_ADVERTISING_IDENTIFIER =                              0x42,
	GAP_HCI_STATUS_LIMIT_REACHED =                                               0x43,
	GAP_HCI_STATUS_OPERATION_CANCELLED_BY_HOST =                                 0x44,
	GAP_HCI_STATUS_PACKET_TOO_LONG =                                             0x45
} hci_status_t;

typedef enum local_info_pkt_slots_stru {/* packet_length[x] & packet_num[x] in struct HCI_LocalStru */
	GAP_HCI_LINFO_PACKET_ACL,
	GAP_HCI_LINFO_PACKET_LE,/* Maybe different with ACL, check HCI_LINFO_MASK_LE_SHARE_ACL_BUFFER */
} local_info_pkt_slots_t;

/*
 * FUNCTION DECLARATIONS
 *******************************************************************************
 */
void gap_init(void);

bool gap_set_local_addr(addr_type_t addr_type, bd_addr_t addr);
void gap_set_io_cap(hci_io_cap_t io_cap);
void gap_set_bondable(bool bondable);
void gap_set_sec_level(gap_sec_mode_level_t level);
void gap_set_mitm_protect(bool enable);
void gap_set_lesc_enable(bool enable);
void gap_pair_req(peer_t peer);
uint16_t gap_get_local_pkt_num(local_info_pkt_slots_t slot);
uint16_t gap_get_local_pkt_len(local_info_pkt_slots_t slot);
void gap_write_suggested_def_data_len(uint16_t max_tx_octets, uint16_t max_tx_time);

void gap_ev_subscribe(const _FUNCTION_ gap_ev_cbk_t *ev_cbk, uint8_t nm);
void gap_ev_unsubscribe(const _FUNCTION_ gap_ev_cbk_t *ev_cbk);

void le_adv_add(const _APP_FREE_ adv_param_t *par, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_adv_remove(uint8_t adv_hdl, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_adv_clear(_FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_adv_set_adv_data(const _APP_FREE_ adv_set_adv_data_param_t *par, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_adv_set_scn_data(const _APP_FREE_ adv_set_scn_data_param_t *par, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_adv_set_random_addr(uint8_t adv_hdl, bd_addr_t addr , _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);

void le_adv_set_periodic_adv_param(const adv_param_t *par, gap_cbk_t *cbk, void *ctx);
void le_adv_set_periodic_adv_enable(uint8_t adv_hdl, bool en, gap_cbk_t *cbk,void *ctx);
void le_adv_set_periodic_adv_data(const adv_set_adv_data_param_t *par, gap_cbk_t *cbk, void *ctx);



void le_adv_enable(uint8_t adv_set_nm, const _APP_FREE_ adv_en_param_t *par
                   , _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);

void le_adv_disable(uint8_t adv_set_nm, const _APP_FREE_ uint8_t *adv_hdl
                    , _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);

void le_scan_enable(const _APP_FREE_ scn_param_t *par, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
uint8_t le_scan_disable(void);

void peer_recover(uint8_t *info, uint32_t info_sz, gap_cbk_t *cbk, void *ctx);
bool peer_addr_get(peer_t peer, uint8_t *addr_type, bd_addr_t addr);

void peer_bonded_remove(peer_t peer, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void peer_bonded_clear(void);
uint8_t peer_bonded_count(void);
bool peer_is_bonded(peer_t peer);

void le_conn_create(const _APP_FREE_ le_conn_par_t *par, _FUNCTION_ gap_cbk_t *cbk);
void le_conn_cancel(void);
void le_conn_destory(peer_t peer);
uint16_t le_conn_get_sending_num(peer_t peer);
hci_role_t le_conn_get_local_role(peer_t peer);
void le_conn_set_data_length(peer_t peer, uint16_t max_tx_octets, uint16_t max_tx_time);
void le_conn_read_feature(peer_t peer);
void le_conn_read_rssi(peer_t peer);
void le_conn_set_slave_latency(peer_t peer, uint16_t latency);

void le_conn_update(_APP_FREE_ le_conn_update_t *par);
void le_conn_par_reply(_APP_FREE_ le_conn_update_t *par, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_conn_par_neg_reply(peer_t peer, uint8_t reason, _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);

//兼容旧版本接口
#define le_peer_conn_update			le_conn_update
#define le_peer_conn_par_reply		le_conn_par_reply
#define le_peer_conn_par_neg_reply	le_conn_par_neg_reply		

void le_aoa_adv_add(const _APP_FREE_ adv_param_t *par 
                  , uint16_t adv_int
                  , uint8_t cte_len
                  , _FUNCTION_ gap_cbk_t *cbk 
                  , _APP_FREE_ void *ctx);
void le_aoa_adv_enable(uint8_t adv_set_nm, const _APP_FREE_ adv_en_param_t *par
                   , _FUNCTION_ gap_cbk_t *cbk, void *ctx);
void le_aoa_adv_disable(uint8_t adv_set_nm, const _APP_FREE_ uint8_t *adv_hdl
                    , _FUNCTION_ gap_cbk_t *cbk, _APP_FREE_ void *ctx);
void le_aoa_adv_data_set(const uint8_t adv_hdl
                       , _APP_FREE_ adv_data_t *aoa_data
                       , _FUNCTION_ gap_cbk_t *cbk
                       , _APP_FREE_ void *ctx);

void le_periodic_adv_enable(const adv_en_param_t *par , gap_cbk_t *cbk, void *ctx);
void le_periodic_adv_disable(const adv_en_param_t *par , gap_cbk_t *cbk, void *ctx);

void le_wl_remove(addr_type_t atype, bd_addr_t bd);
void le_wl_add(addr_type_t atype, bd_addr_t bd);
void le_wl_clear(void);

/// @} GAP

#endif // !_GAP_H_

