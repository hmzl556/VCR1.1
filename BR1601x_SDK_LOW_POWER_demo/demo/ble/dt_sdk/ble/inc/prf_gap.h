/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Barrot's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY BARROT'S "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL BARROT'S BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file prf_gap.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_GAP_H
#define _PRF_GAP_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initializes the Generic Access Profile (GAP).
 *
 * This function sets up the necessary components and configurations for the GAP.
 * It prepares the device to handle basic Bluetooth Low Energy (BLE) operations such as advertising, scanning, and connection management.
 *
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */ 
int prf_gap_init(void);

/**
 * @brief Sets the device name for the Generic Access Profile (GAP).
 *
 * This function updates the device name that is visible to other BLE devices.
 * The device name is an important identifier in GAP and is typically stored in the GATT database.
 *
 * @param name Pointer to the buffer containing the device name to be set.
 * @param name_len Length of the device name in bytes.
 *
 * @note The device name should comply with BLE naming conventions and length restrictions.
 */ 
void prf_gap_name_set(uint8_t *name, uint16_t name_len);
#endif

