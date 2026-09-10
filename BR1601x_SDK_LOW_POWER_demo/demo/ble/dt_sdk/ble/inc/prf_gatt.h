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
 * @file prf_gatt.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_GATT_H
#define _PRF_GATT_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "gap.h"
#include "gatt.h"

/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct svc_change_stru {
    peer_t peer;
    uint16_t start_hdl;
    uint16_t end_hdl;
} svc_change_t;

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initializes the GATT (Generic Attribute Profile) profile.
 *
 * This function sets up the necessary components and configurations for the GATT profile.
 * It prepares the device to handle GATT operations such as service discovery, characteristic access, and notifications.
 *
 * GATT is a fundamental part of Bluetooth Low Energy (BLE) and is used to define the structure and exchange of data between devices.
 *
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_gatt_init(void);

/**
 * @brief Handles changes in the Generic Attribute Profile (GATT) Service (0x1801).
 *
 * This function processes changes in the GATT Service, such as updates to the Service Changed characteristic.
 * It updates the necessary components and configurations based on the provided service change information.
 *
 * @param svc_change A pointer to the service change information.
 * @return Returns a status code indicating the result of the operation.
 *         - GATT_SUCCESS: The operation was successful.
 *         - Other codes: An error occurred.
 */
gatt_rst_t prf_gatt_svc_change(svc_change_t *svc_change);

#endif

