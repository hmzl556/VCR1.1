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
 * @file prf_dfu.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_DFU_H
#define _PRF_DFU_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initializes the Device Firmware Update (DFU) profile.
 *
 * This function sets up the necessary components and configurations for the DFU profile.
 * It prepares the device to handle firmware update operations, allowing it to receive and install new firmware images.
 *
 * The DFU profile is a critical component for enabling over-the-air (OTA) updates, ensuring that devices can be remotely updated with new features and bug fixes.
 *
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_dfu_init(void);
void prf_dfu_disconnect(void);

/**
 * @brief Sends a response to a command received over the DFU Service.
 *
 * This function sends a response to a previously received command over the DFU Service.
 * It is typically used after processing a command to inform the requesting device of the result.
 *
 * @param conhdl The connection handle of the device that sent the command.
 * @param data The response data to be sent back to the device.
 * @param length The length of the response data.
 * @return Returns a status code indicating the result of the operation.
 *         - 0: Response sent successfully.
 *         - Non-zero: An error occurred.
 */
uint32_t prf_dfu_atcmd_rsp_send(uint16_t conhdl, const uint8_t *data, uint16_t length);

#endif
