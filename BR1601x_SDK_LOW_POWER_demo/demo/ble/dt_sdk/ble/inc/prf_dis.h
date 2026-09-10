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
 * @file prf_dis.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_DIS_H
#define _PRF_DIS_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "cfg.h"

/**
 * DEFINITIONS
 ****************************************************************************************
 */
/// Manufacturer Name Value
#define APP_DIS_MANUFACTURER_NAME       ("Barrot")
#define APP_DIS_MANUFACTURER_NAME_LEN   (sizeof(APP_DIS_MANUFACTURER_NAME)-1)

/// Model Number String Value
#define APP_DIS_MODEL_NB_STR            ("BR-BLE-1.0")
#define APP_DIS_MODEL_NB_STR_LEN        (sizeof(APP_DIS_MODEL_NB_STR)-1)

/// Serial Number
#define APP_DIS_SERIAL_NB_STR           ("1.0.0.0-LE")
#define APP_DIS_SERIAL_NB_STR_LEN       (sizeof(APP_DIS_SERIAL_NB_STR)-1)

/// Firmware Revision
#define APP_DIS_FIRM_REV_STR            BR_DEFAULT_CONFIG_VERSION
#define APP_DIS_FIRM_REV_STR_LEN        (sizeof(APP_DIS_FIRM_REV_STR)-1)

/// System ID Value - LSB -> MSB
#define APP_DIS_SYSTEM_ID               ("SystemID")
#define APP_DIS_SYSTEM_ID_LEN           (sizeof(APP_DIS_SYSTEM_ID)-1)

/// Hardware Revision String
#define APP_DIS_HARD_REV_STR            ("1.0.0")
#define APP_DIS_HARD_REV_STR_LEN        (sizeof(APP_DIS_HARD_REV_STR)-1)

/// Software Revision String
#define APP_DIS_SW_REV_STR              ("1.0.0")
#define APP_DIS_SW_REV_STR_LEN          (sizeof(APP_DIS_SW_REV_STR)-1)

/// IEEE
#define APP_DIS_IEEE                    ("\xFF\xEE\xDD\xCC\xBB\xAA")
#define APP_DIS_IEEE_LEN                (sizeof(APP_DIS_IEEE)-1)

/**
 * PNP ID Value - LSB -> MSB
 *      Vendor ID Source : 0x01 (USB Implementer’s Forum assigned Vendor ID value)
 *      Vendor ID : 0x33FA      (Microsoft Corp)
 *      Product ID : 0xFF03
 *      Product Version : 0x0001
 */
#define APP_DIS_PNP_ID                  ("\x01\xFA\x33\xFF\x03\x01\x00")
#define APP_DIS_PNP_ID_LEN              (7)

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initializes the Device Information Service (DIS).
 *
 * This function sets up the necessary components and configurations for the DIS.
 * It prepares the service to provide information about the device, such as manufacturer name, model number, and serial number.
 *
 * The DIS is a standard GATT service used to expose basic information about the device to connected clients.
 *
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_dis_init(void);

#endif

