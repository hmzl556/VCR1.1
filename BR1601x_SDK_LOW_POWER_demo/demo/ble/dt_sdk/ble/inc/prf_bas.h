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
 * @file prf_bas.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_BAS_H
#define _PRF_BAS_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "gap.h"

/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct prf_bat_level_ntf_stru {
    peer_t   peer;   /**< Peer device instance. */
    uint8_t  bat_level; /**< Battery level value. */
} prf_bat_level_ntf_t;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initializes the Battery Service (BAS) profile.
 *
 * This function sets up the necessary components and configurations for the Battery Service profile.
 * @return Returns an integer indicating the result of the initialization.
 *         - 0: Initialization successful.
 *         - Non-zero: An error occurred during initialization.
 */
int prf_bas_init(void);

/**
 * @brief Sends the battery level to a peer device.
 *
 * This function transmits the current battery level to the specified peer device.
 *
 * @param peer The peer device instance to which the battery level is sent.
 * @param bat_voltage The battery voltage level to be sent.
 */
void prf_bas_bat_level_send(peer_t peer, uint8_t bat_voltage);

#endif

