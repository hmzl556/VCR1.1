/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 2017-2023  Barrot Technology Limited
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/*
 * @file direct_2.4g_fotile.h
 *
 * Detailed description.
 *
 * @author Asher
 *
*/

#ifndef __DIRECT_24G_FOTILE_H__
#define __DIRECT_24G_FOTILE_H__ 1

#include "stdint.h"

#if 1
#define ANT_TX_SIDE 0
#define ANT_RX_SIDE 1
#else
#define ANT_TX_SIDE 1
#define ANT_RX_SIDE 0
#endif


/*
 * Error code
 */
#define D24G_ERRCODE_SUCCESS            (0)
#define D24G_ERRCODE_NOINIT             (-1)
#define D24G_ERRCODE_INVALID_PARAM      (-2)
#define D24G_ERRCODE_STATE_ERR          (-3)
#define D24G_ERRCODE_SCH_ERR            (-4)
#define D24G_ERRCODE_RESOURCE_LIMITED   (-5)



enum direct_24g_event {
    D24G_EVT_RX_DONE,
    D24G_EVT_TX_DONE,
};

typedef enum direct_24g_mode_enum {
    D24G_MODE_TX,
    D24G_MODE_RX,
}direct_24g_mode;

typedef struct direct_24g_tx_rx_param_t{
    uint8_t *access_code;
    uint16_t freq;
    uint8_t *data;
    uint16_t data_len;
    uint16_t rx_win_size;
    void *tx_cbk;
    void *rx_cbk;
    void *rx_timeout_cbk;
}direct_24g_tx_rx_param;


typedef void (*cbk_direct_24g)(enum direct_24g_event evt, void *param, uint8_t param_len);
typedef void (*cbk_direct_24g_tx)(uint16_t freq);
typedef void (*cbk_direct_24g_rx_timeout)(uint16_t freq);
typedef void (*cbk_direct_24g_rx)(uint16_t freq,int8_t rssi,void *param, uint8_t param_len);

uint32_t ble_error_report(void);
uint32_t ip_error_report(void);
uint32_t rwip_timer(void);
/*
 * Initial the direct 2.4g
 *
 * @param
 *  access_code[4]:   access code
 *  mode:             TX/RX
 * @retval
 *  error code
 */
int direct_24g_init(uint8_t access_code[4],direct_24g_mode mode);

/*
 * Start the direct 2.4g rx
 *
 * @param
 *  access_code[4]:   access code
 *  freq:               RF Frequancy(2423/2470)
 *  rx_win_sz:          Window size for RF RX
 *
 * @retval
 *  error code
 */
int direct_24g_rx(uint8_t access_code[4],uint16_t freq, uint16_t rx_win_size,cbk_direct_24g_rx_timeout rx_timeout_cbk,cbk_direct_24g_rx rx_cbk);

/*
 * Start the direct 2.4g tx
 *
 * @param
 *  access_code[4]:   access code
 *  freq:             RF Frequancy(2423/2470)
 *  data:             Data Pointer
 *  data_len:         Data length
 *
 * @retval
 *  error code
 */
int direct_24g_tx(uint8_t access_code[4], uint16_t freq, const uint8_t *data, uint8_t len, cbk_direct_24g_tx tx_cbk);
extern uint8_t tx_fail_flag;

int direct_24g_trx_pass(uint8_t access_code[4], uint16_t freq, const uint8_t *data, uint8_t len, cbk_direct_24g_tx tx_cbk
                        ,cbk_direct_24g_rx_timeout rx_timeout_cbk,cbk_direct_24g_rx rx_cbk);
int user_close_rx(void);





#endif /* __DIRECT_2_4G_FOTILE_H__ */
