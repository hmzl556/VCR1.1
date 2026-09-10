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
 * @file prf_cmd.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_CMD_H
#define _PRF_CMD_H

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
 * @brief Initializes the Command Service.
 *
 * This function sets up the necessary components and configurations for the Command Service.
 * It prepares the service to handle incoming commands and send responses.
 *
 * The Command Service is designed to facilitate the exchange of commands and data over the air,
 * including the execution of AT commands. This initialization process ensures that the service
 * is ready to receive and process commands from remote devices.
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_cmd_init(void);

/**
 * @brief Sends a response to a command received over the Command Service.
 *
 * This function sends a response to a previously received command over the Command Service.
 * It is typically used after processing a command to inform the requesting device of the result.
 *
 * @param conhdl The connection handle of the device that sent the command.
 * @param data The response data to be sent back to the device.
 * @param length The length of the response data.
 * @return Returns a status code indicating the result of the operation.
 *         - 0: Response sent successfully.
 *         - Non-zero: An error occurred.
 */
uint32_t prf_cmd_rsp_send(uint16_t conhdl, const uint8_t *data, uint16_t length);

#endif

