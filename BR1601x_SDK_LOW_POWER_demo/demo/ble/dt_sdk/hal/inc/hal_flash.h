/**
 * @file hal_flash.h
 * @author Barrot Firmware Team
 * @brief This file defines the operating interface of flash
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */
#ifndef __FLASH__H__
#define __FLASH__H__

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
 
/*
 * DEFINE
 ****************************************************************************************
 */

/// The size of the flash in the chip
#define FLASH_TOTAL_SIZE                512*1024 
/// Start address of a flash operation
#define FLASH_START_ADDR                0x01000000 
/// End address of a flash operation
#define FLASH_END_ADDR                  FLASH_START_ADDR + FLASH_TOTAL_SIZE
/// Minimum size of flash erase
#define FLASH_SECTOR_SIZE               0x1000

/*
 * ENUMERATIONS DEFINITIONS
 ****************************************************************************************
 */

/// flash operation result
enum {
    HAL_FLASH_ERR_NONE,
    HAL_FLASH_ERR_WR_REG,
    HAL_FLASH_ERR_TIMEOUT,
    HAL_FLASH_ERR_BUSY,
    HAL_FLASH_ERR_QUADSPI,
    HAL_FLASH_ERR_ADDR,
    HAL_FLASH_ERR_END = 0x10,
};

/// flash lock state
enum {
	HAL_FLASH_LOCK_STATE_UNLOCK,
	HAL_FLASH_LOCK_STATE_LOCK,
	HAL_FLASH_LOCK_STATE_UNKNOWN,
};

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief  Initialize Qflash.
 */
void hal_flash_init(void);

/**
 * @brief Erase one sector in flash with ble schedule on.
 * @param[in] flash_address erase address.
 * @return none.
 */
uint32_t hal_flash_erase_sector(uint32_t address);

/**
 * @brief Write mutable length data to flash with ble schedule on.
 * @param[in] addr flash address.
 * @param[in] p_data data to write.
 * @param[in] len data len.
 * @return none.
 */
uint32_t hal_flash_write(uint32_t address, uint8_t* p_data, uint32_t len);

/**
 * @brief Read mutable length data to ram.
 * @param[in] addr flash address.
 * @param[out] p_data data to read.
 * @param[in] len data len.
 * @return none.
 */
uint32_t hal_flash_read(uint32_t address, uint8_t* p_data, uint32_t len);

/**
 * @brief flash locks the entire film
 * @return none.
 */
uint32_t hal_flash_lock(void);

/**
 * @brief flash full film unlock
 * @return none.
 */
uint32_t hal_flash_unlock(void);

/**
 * @brief Obtain the flash lock status
 * @return flash lock state.
 */
uint32_t hal_flash_get_lock_state(void);

#endif
