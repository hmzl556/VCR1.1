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
 * @file prf_dtss.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _BLE_DTSS_H_
#define _BLE_DTSS_H_

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
 * @brief Initializes the Data Transparent Service (DTSS).
 *
 * This function sets up the necessary components and configurations for the DTSS.
 * It prepares the service to handle data transmission requests and manage the data flow.
 *
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_dtss_init(void);

/**
 * @brief Sends data over the Data Transparent Service (DTSS).
 *
 * This function transmits the specified data to the connected device using the DTSS.
 *
 * @param conhdl The connection handle of the device to which the data is being sent.
 * @param data The data to be sent.
 * @param length The length of the data.
 * @return Returns a status code indicating the result of the operation.
 *         - 0: Data sent successfully.
 *         - Non-zero: An error occurred.
 */
uint32_t prf_dtss_send_data(uint16_t conhdl,const uint8_t *data, uint16_t length);

/**
 * @brief Sends flow control data over the Data Transparent Service (DTSS).
 *
 * This function transmits flow control data to the connected device using the DTSS.
 * Flow control is used to manage the rate of data transmission and prevent overflow.
 *
 * @param conhdl The connection handle of the device to which the data is being sent.
 * @param data The flow control data to be sent.
 * @param length The length of the flow control data.
 * @return Returns a status code indicating the result of the operation.
 *         - 0: Data sent successfully.
 *         - Non-zero: An error occurred.
 */
uint32_t prf_dtss_send_fc(uint16_t conhdl,const uint8_t *data, uint16_t length);

#endif

