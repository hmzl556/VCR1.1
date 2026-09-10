/**
 * @file hal_flash.c
 * @author Barrot Firmware Team
 * @brief This file implements the flash interface, and specific platform related
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

 /**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include <string.h>
#include "br1601x.h"
#include "qflash.h"
#include "hal_flash.h"
//Records whether it is currently locked
static volatile uint32_t flash_lock_state = HAL_FLASH_LOCK_STATE_UNKNOWN;
static bool hal_flash_lock_auto = true;

/**
 * GLOBAL FUNCTIONS DEFINITIONS
 ******
 */

void hal_flash_init(void)
{
    Qflash_Init();
}

uint32_t hal_flash_erase_sector(uint32_t address)
{
    return Qflash_Erase_Sector(address);
}


uint32_t hal_flash_write(uint32_t address, uint8_t* p_data, uint32_t len)
{
    return Qflash_Write(address, p_data, len);
}


uint32_t hal_flash_read(uint32_t address, uint8_t* p_data, uint32_t len)
{
#ifdef BR1601A01
    memcpy(p_data, (uint8_t *)address, len);
    return 0;
#else
#define _min(a,b)   (a) > (b) ? (b) : (a)
#define FLASH_READ_CACHE_SIZE    4
    uint32_t ret = 0;
    uint8_t buf[FLASH_READ_CACHE_SIZE] = {0};
    uint8_t addr_mod = address & 0x03;
    /// Read with 4-byte alignment at the address.
    if(addr_mod != 0){
        address &= 0xfffffffc;
        ret = Qflash_Read(address, buf, FLASH_READ_CACHE_SIZE);
        if (ret != HAL_FLASH_ERR_NONE) return ret;
        uint32_t copy_len = _min(FLASH_READ_CACHE_SIZE - addr_mod, len);
        memcpy(p_data, buf + addr_mod, copy_len);
        p_data += copy_len;
        len -= copy_len;
        /// Offset 4 bytes backward.
        address += FLASH_READ_CACHE_SIZE;
    }
    /// Read data that is a multiple of 4 bytes in length.
    uint32_t len_mod = len & 0x03;
    if(len > len_mod){
        uint32_t copy_len = len & 0xfffffffc;
        ret = Qflash_Read(address, p_data, copy_len);
        if (ret != HAL_FLASH_ERR_NONE) return ret;
        address += copy_len;  
        p_data += copy_len;
        len -= copy_len;
    }
    /// Read the remaining bytes.
    if (len > 0) {
        ret = Qflash_Read(address, buf, FLASH_READ_CACHE_SIZE);
        if (ret != HAL_FLASH_ERR_NONE) return ret;
        memcpy(p_data, buf, len);
    }
    return ret;
#endif
}


uint32_t hal_flash_lock(void)
{
    if(flash_lock_state == HAL_FLASH_LOCK_STATE_LOCK || hal_flash_lock_auto == false)
        return 0;
    
    flash_lock_state = HAL_FLASH_LOCK_STATE_LOCK;
    return Qflash_Lock(FLASH_LOCK_F_256K);
}

uint32_t hal_flash_unlock(void)
{
    if(flash_lock_state == HAL_FLASH_LOCK_STATE_UNLOCK)
        return 0;

    flash_lock_state = HAL_FLASH_LOCK_STATE_UNLOCK;
    return Qflash_Lock(FLASH_LOCK_UNLOCK);
}

uint32_t hal_flash_get_lock_state(void)
{
    return flash_lock_state;
}

