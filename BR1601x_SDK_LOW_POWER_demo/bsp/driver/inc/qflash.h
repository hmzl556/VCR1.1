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
 * @file qflash.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef __QFLASH_H__
#define __QFLASH_H__

#ifdef __cplusplus
extern "C" {
#endif
    
#include "br1601x.h"
/** @addtogroup StdPeriph_Driver
 * @{
 */

/** @addtogroup QFLASH
 * @{
 */
    
/** @addtogroup QFLASH_Defines
 * @{
 */ 
#define Qflash_Erase_Sector_Raw     Qflash_Erase_Sector 
#define Qflash_Write_Raw            Qflash_Write
 
#define FLASH_PAGE_SIZE             0x100
#define FLASH_SECTOR_SIZE           0x1000


#define FLASH_LOCK_UNLOCK 0x00		//	NONE
#define FLASH_LOCK_F_4K   0x19		//	1000000H - 1000FFFH 4KB 
#define FLASH_LOCK_F_8K   0x1A		//	1000000H - 1001FFFH 8KB 
#define FLASH_LOCK_F_16K  0x1B		//	1000000H - 1003FFFH 16KB
#define FLASH_LOCK_F_32K  0x1E		//	1000000H - 1007FFFH 32KB
#define FLASH_LOCK_F_64K  0x09		//	1000000H - 100FFFFH 64KB 
#define FLASH_LOCK_F_128K 0x0A		//	1000000H - 101FFFFH 128KB
#define FLASH_LOCK_F_256K 0x0B		//	1000000H - 103FFFFH 256KB
#define FLASH_LOCK_F_512K 0x17		//	1000000H - 107FFFFH 512KB
#define FLASH_LOCK_B_4K   0x11		//	107F000H - 107FFFFH 4KB 
#define FLASH_LOCK_B_8K   0x12		//	107E000H - 107FFFFH 8KB 
#define FLASH_LOCK_B_16K  0x13		//	107C000H - 107FFFFH 16KB
#define FLASH_LOCK_B_32K  0x16		//	1078000H - 107FFFFH 32KB
#define FLASH_LOCK_B_64K  0x01		//	1070000H - 107FFFFH 64KB 
#define FLASH_LOCK_B_128K 0x02		//	1060000H - 107FFFFH 128KB
#define FLASH_LOCK_B_256K 0x03		//	1040000H - 107FFFFH 256KB
/**
 * @}
 */
 
/** @addtogroup QFLASH_ReturnMsg
 * @{
 */
typedef enum{
    FlashOperationSuccess,
    FlashWriteRegFailed,
    FlashTimeOut,
    FlashIsBusy,
    FlashQuadNotEnable,
    FlashAddressInvalid,
}ReturnMsg; 
/**
 * @}
 */

/** @addtogroup RCC_Exported_Functions
 * @{
 */
void Qflash_Init(void);
uint32_t Qflash_Erase_Sector(uint32_t address);
uint32_t Qflash_Write(uint32_t address, uint8_t* p_data, uint32_t len);
uint32_t Qflash_Read(uint32_t address, uint8_t* p_data, uint32_t len);
uint32_t Qflash_Lock(uint16_t lock);
    
#ifdef __cplusplus
}
#endif

#endif /* __QFLASH_H__ */
/**
 * @}
 */    

/**
 * @}
 */

/**
 * @}
 */
