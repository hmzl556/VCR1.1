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
 * @file prf_search.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _PRF_SEARCH_H
#define _PRF_SEARCH_H

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "gap.h"
#include "gatt.h"

/**
 * TYPEDEFS
 ****************************************************************************************
 */
/**
 * @brief Descriptor characteristic definition.
 *
 * This structure defines a characteristic descriptor, which is a specific attribute of a characteristic.
 * It includes a UUID, a mandatory flag indicating whether the descriptor is required, and an index to identify the descriptor within a characteristic.
 */
typedef struct 
{
    uuid_t uuid;              /**< UUID of the descriptor. */
    uint8_t mandatory;        /**< Flag indicating if the descriptor is mandatory. */
    uint8_t char_idx;         /**< Index of the descriptor within a characteristic. */
} prf_char_desc_def_t;

/**
 * @brief Characteristic definition.
 *
 * This structure defines a characteristic, which is a specific attribute of a service.
 * It includes a UUID, a mandatory flag indicating whether the characteristic is required, and a properties field.
 */
typedef struct
{
    uuid_t uuid;              /**< UUID of the characteristic. */
    uint8_t mandatory;        /**< Flag indicating if the characteristic is mandatory. */
    uint8_t prop;             /**< Properties of the characteristic. */
} prf_char_def_t;

/**
 * @brief Profile content definition.
 *
 * This structure represents the content of a profile, which includes the discovery state,
 * the number of services, the last characteristic index, and pointers to services, characteristics, and descriptors.
 */
typedef struct
{
    uint8_t disc_state;       /**< Discovery state of the profile. */
    uint8_t nb_svc;           /**< Number of services in the profile. */
    uint8_t last_char;        /**< Index of the last characteristic. */
    gatt_svc_t  *svc;         /**< Pointer to the services. */
    gatt_chr_t  *chars;       /**< Pointer to the characteristics. */
    gatt_dsc_t  *descs;       /**< Pointer to the descriptors. */
} prf_content_t;


/**
 * @brief Profile database configuration.
 *
 * This structure defines the configuration for a profile's database, including pointers to service UUIDs,
 * characteristic definitions, descriptor definitions, and the number of characteristics and descriptors.
 */
typedef struct {
    uuid_t *svc_uuid;         /**< Pointer to the UUID of the service. */
    prf_char_def_t *chr;     /**< Pointer to the characteristic definitions. */
    prf_char_desc_def_t *desc;/**< Pointer to the descriptor definitions. */
    uint8_t chr_nb;           /**< Number of characteristics. */
    uint8_t desc_nb;          /**< Number of descriptors. */
} prf_db_cfg_t;

/**
 * @brief Profile state enumeration.
 *
 * This enumeration lists the possible states of a profile during the discovery process.
 */
typedef enum {
    PRF_STATE_IDLE,          /**< Profile is idle. */
    PRF_STATE_DISC_SVC,      /**< Profile is discovering services. */
    PRF_STATE_DISC_CHAR,     /**< Profile is discovering characteristics. */
    PRF_STATE_DISC_DESC,     /**< Profile is discovering descriptors. */
    PRF_STATE_DISC_COMPLETE /**< Profile discovery is complete. */
} prf_state_t;

/**
 * @brief Profile discovery error enumeration.
 *
 * This enumeration lists the possible errors that can occur during the profile discovery process.
 */
typedef enum {
    PRF_DISC_ERR_NO,         /**< No error. */
    PRF_DISC_ERR_NOSVC,      /**< No service found. */
    PRF_DISC_ERR_NOCHAR,     /**< No characteristic found. */
    PRF_DISC_ERR_NODESC,    /**< No descriptor found. */
    PRF_DISC_ERR_NOAUTH,     /**< Authentication error. */
} prf_disc_err_t;

/**
 * @brief Profile callback function type definition.
 *
 * This type defines the callback function used to report the discovery status or errors.
 * It takes a connection handle and a discovery error as parameters.
 */
typedef void (*prf_cbk_t)(uint16_t conhdl, prf_disc_err_t err);

/**
 * @brief Profile search environment definition.
 *
 * This structure defines the environment for a profile search, including the connection handle,
 * profile database configuration, profile content, and the callback function.
 */
typedef struct {
    uint16_t conhdl;
    prf_db_cfg_t  *prf_cfg;
    prf_content_t prf_content;
    prf_cbk_t  cbk;
}prf_search_env_t;

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Initiates the service discovery process.
 *
 * This function starts the service discovery process for a given connection handle.
 * It discovers services, characteristics, and descriptors step by step based on the provided configuration and content structures.
 *
 * @param conhdl The connection handle identifying the current connection.
 * @param prf_cfg A pointer to the configuration structure containing the UUIDs and properties of services and characteristics.
 * @param prf_content A pointer to the content structure containing the current discovery state, number of services, and pointers to characteristics and descriptors.
 * @param cbk A callback function used to report errors or completion status during the discovery process.
 *
 * @note This function should be called after the connection is established to ensure proper discovery of services on the remote device.
 *       The callback function will be invoked at different stages of the discovery process to notify the application of the current state or errors.
 */
void prf_disc_start(uint16_t conhdl, prf_db_cfg_t  *prf_cfg, prf_content_t *prf_content, prf_cbk_t cbk);

#endif
