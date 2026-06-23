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
 * @file br_dfu_boot.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

 /** @addtogroup 
 * @{
 */
#ifndef __BR_DFU_BOOT_H__
#define __BR_DFU_BOOT_H__


#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Public define ------------------------------------------------------------*/
/* Public typedef -----------------------------------------------------------*/
typedef struct
{
    uint32_t start_address;
    uint32_t size;
    uint32_t crc;
    uint32_t version;
    uint32_t activation;
    uint32_t reserve[5];
}BR_Bank_t;

typedef struct
{
    uint32_t crc;
    uint32_t master_boot_force_update;
    BR_Bank_t app1;
    BR_Bank_t app2;
    BR_Bank_t ImageUpdate;
    uint8_t public_key[64];
}BR_Bootsetting_t;
/* Public define ------------------------------------------------------------*/  
extern int Image$$ER_IROM1$$Base;
#define CURRENT_APP_START_ADDRESS                      (uint32_t)&Image$$ER_IROM1$$Base

#define BR_MASTERBOOT_START_ADDRESS                    (0x01000000)
#define BR_MASTERBOOT_SIZE                             (0x2000)                                                                 
#define BR_BOOTSETTING_START_ADDRESS                   (0x01002000)
#define BR_BOOTSETTING_SIZE                            (0x1000)
#define BR_APP_DATA_START_ADDRESS                      (0x01003000)
#define BR_APP_DATA_SIZE                               (0x1000)
#define BR_APP1_START_ADDRESS                          (0x01004000)
#define BR_APP1_DEFAULT_SIZE                           (0x1C000)
#define BR_APP2_START_ADDRESS                          (0x01020000)
#define BR_APP2_DEFAULT_SIZE                           (0x1C000)
#define BR_IMAGE_UPDATE_START_ADDRESS                  (0x0103C000)
#define BR_IMAGE_UPDATE_SIZE                           (0x4000)

#define BR_BOOTSETTING_ACTIVATION_YES                       (0x00000001)
#define BR_BOOTSETTING_ACTIVATION_NO                        (0xFFFFFFFF)
#define BR_BOOTSETTING_MASTER_BOOT_FORCE_UPDATE_YES         (0x00000001)
#define BR_BOOTSETTING_MASTER_BOOT_FORCE_UPDATE_NO          (0xFFFFFFFF)

/* Public variables ---------------------------------------------------------*/
extern BR_Bootsetting_t br_bootsetting;
/* Public function prototypes -----------------------------------------------*/
void br_dfu_boot_jump(uint32_t address);
bool br_dfu_boot_force_usart_dfu(void);


#ifdef __cplusplus
}
#endif


#endif //__BR_DFU_BOOT_H__
