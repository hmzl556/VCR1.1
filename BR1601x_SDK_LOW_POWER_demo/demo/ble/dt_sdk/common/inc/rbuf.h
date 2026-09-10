/**
 * @file rbuf.h
 * @author Barrot Firmware Team
 * @brief This file contains the interface definition for the ring buffer
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */


#ifndef __RBUF__H__
#define __RBUF__H__

/*
 * INCLUDE FILES
 *******************************************************************************
 */
#include "porting.h"

/*
 * TYPEDEF
 ****************************************************************************************
 */

/// Defines a callback function variable type
typedef void (notify_t)(void);

/// ringbuf Callback structure
typedef struct {
    /// Write the callback function after the ring buffer
    notify_t *on_write;
    /// The callback function after reading the ring buffer
    notify_t *on_read; 
} rbuf_cbks_t;

// ringbuf structure
typedef struct {
    /// array pointer
    uint8_t* buffer; 
    /// Size of array
    uint16_t size;
    /// Read pointer
    volatile uint8_t* read;
    /// write pointer
    volatile uint8_t* write;
    /// ring buffer Callback
    rbuf_cbks_t cb;
} rbuf_t;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief Initializes a ring buffer instance
 * 
 * @param rb Ring buffer instance pointer
 * @param buffer Payload of rbuf (array pointer)
 * @param size array of size
 */
void rbuf_init(rbuf_t* rb, uint8_t* buffer, uint16_t size);

/**
 * @brief Write data to the ring buffer
 * 
 * @param rb Ring buffer instance pointer
 * @param data A pointer to the data you want to write
 * @param len The length of the data you want to write
 * @return uint32_t The length of the actual write
 */
uint32_t rbuf_write(rbuf_t* rb,const uint8_t* data, uint32_t len);

/**
 * @brief Read data from the ring buffer
 * 
 * @param rb Ring buffer instance pointer
 * @param data destination address
 * @param len Target length
 * @return uint32_t The length actually read
 */
uint32_t rbuf_read(rbuf_t* rb, uint8_t* data, uint32_t len);

/**
 * @brief get data from the ring buffer,No update pointer.
 * 
 * @param rb Ring buffer instance pointer
 * @param data destination address
 * @param len Target length
 * @return uint32_t The length actually read
 */
uint32_t rbuf_get(rbuf_t* rb, uint8_t* data, uint32_t len);

/**
 * @brief Register function of ring buffer, register read callback
 * 
 * @param rb Ring buffer instance pointer
 * @param notif read callback
 */
void rbuf_rd_notify_reg(rbuf_t* rb, notify_t *notif);

/**
 * @brief Register function of ring buffer, register write callback
 * 
 * @param rb Ring buffer instance pointer
 * @param notif write callback
 */
void rbuf_wr_notify_reg(rbuf_t* rb, notify_t *notif);

/**
 * @brief Gets the valid amount of data for rbuf
 * 
 * @param rb Ring buffer instance pointer
 * @return uint32_t 
 */
uint32_t rbuf_used(rbuf_t* rb);

/**
 * @brief Gets the remaining write length of rbuf
 * 
 * @param rb Ring buffer instance pointer
 * @return uint32_t 
 */
uint32_t rbuf_unused(rbuf_t* rb);

/**
 * @brief Gets the length of continuous data readable by rbuf
 * 
 * @param rb Ring buffer instance pointer
 * @return uint32_t 
 */
uint32_t rbuf_continuous_readable(rbuf_t* rb);

/**
 * @brief Update read pointer
 * 
 * @param rb Ring buffer instance pointer
 * @param len Number of read items
 */
void rbuf_update_read_pointer(rbuf_t* rb, uint32_t len);

/**
 * @brief Scour the ring buffer
 * 
 * @param rb Ring buffer instance pointer
 */
void rbuf_flush(rbuf_t* rb);


/// @} RBUF

#endif /// __RBUF__H__

