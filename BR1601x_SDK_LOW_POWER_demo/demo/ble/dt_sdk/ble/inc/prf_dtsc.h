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
 * @file prf_dtsc.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_DTSC_H_
#define _PRF_DTSC_H_

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
 * @brief Enables the DTSC profile for a given connection handle.
 *
 * This function initializes the DTSC profile for the specified connection handle,
 * preparing it for data transfer operations.
 *
 * @param conhdl The connection handle for which the DTSC profile is to be enabled.
 */
void prf_dtsc_enable_prf(uint16_t conhdl);

/**
 * @brief Sends data to the server using the DTSC service.
 *
 * This function sends the provided data to the server through the DTSC service.
 *
 * @param conhdl The connection handle for the connection to the server.
 * @param data A pointer to the data buffer to be sent.
 * @param len The length of the data to be sent.
 */

uint32_t prf_dtsc_send_data(uint16_t conhdl, uint8_t *data, uint16_t len);

/**
 * @brief Sends feature data to the server and returns a status code.
 *
 * This function sends feature-related data to the server through the DTSC service
 * and returns a status code indicating the result of the operation.
 *
 * @param conhdl The connection handle for the connection to the server.
 * @param data A pointer to the data buffer to be sent.
 * @param len The length of the data to be sent.
 * @return A status code indicating the result of the operation.
 */
uint32_t prf_dtsc_send_fc(uint16_t conhdl, uint8_t *data, uint16_t len);

/**
 * @brief Writes the notify value to all characteristic descriptors of the service.
 *
 * This function is used to write the notify value to all characteristic descriptors
 * of a specific service on a connected device. Writing to the notify value allows enabling
 * or disabling notifications for all characteristics of that service.
 *
 * @param conhdl The connection handle identifying the connection to the peer device.
 *
 * @return None
 *
 * @note This function should be called after the service and its characteristic descriptors
 *       have been discovered. It is typically used to subscribe or unsubscribe from
 *       notifications for all characteristics of a service. Additionally, when writing
 *       to the notify value for a service with insufficient permissions, it may lead to a
 *       requirement for re-encryption/pairing. After re-encryption/pairing, the notify
 *       value needs to be written again.
 */
void prf_dtsc_dsc_ccc_enable(uint16_t conhdl);

/**
 * @brief Initializes the DTSC profile.
 *
 * This function performs the necessary initialization for the DTSC profile,
 * preparing it for use with any connection.
 *
 * @return An integer status code indicating the success or failure of the initialization.
 */
int prf_dtsc_init(void);

#endif
