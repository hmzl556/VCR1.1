/**
 * @file flash_manage.h
 * @author Barrot Firmware Team
 * @brief This file defines the interface of the flash management module
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __FLASH__MANAGE__H__
#define __FLASH__MANAGE__H__

/**
 * INCLUDE FILES
 *******************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include "hal_flash.h"

/**
 * DEFINES
 ****************************************************************************************
 */


/**
 * @brief flash partition
 * @note whether the total capacity of flash is sufficient and 
    whether whitelists and pairing records are in the same sector
 */
 
/// bootloader area
#define FLASH_BOOTLOADER_SIZE           12*1024  //8k bootloader + 4k bootsettings
#define FLASH_BOOTLOADER_ADDR           FLASH_START_ADDR

/// Secondary configuration area
#define FLASH_MINOR_CFG_SIZE            4*1024      
#define FLASH_MINOR_CFG_ADDR            FLASH_BOOTLOADER_ADDR + FLASH_BOOTLOADER_SIZE

/// Program run area
#define FLASH_APP_SIZE                  240*1024   
#define FLASH_APP_ADDR                  FLASH_MINOR_CFG_ADDR + FLASH_MINOR_CFG_SIZE

/// ota data and user data area
#define FLASH_APP_CACHE_SIZE            240*1024  
#define FLASH_APP_CACHE_ADDR            FLASH_APP_SIZE + FLASH_APP_ADDR

/// reserve
#define FLASH_RESERVED_SIZE             0*1024   
#define FLASH_RESERVED_ADDR             FLASH_DIAG_ADDR - FLASH_RESERVED_SIZE

/// Diagnostic log area
#define FLASH_DIAG_SIZE                 0*1024    
#define FLASH_DIAG_ADDR                 FLASH_BT_WHITELIST_ADDR - FLASH_DIAG_SIZE

/// Whitelist area
#define FLASH_BT_WHITELIST_SIZE         4*1024   
#define FLASH_BT_WHITELIST_ADDR         FLASH_BT_PAIR_ADDR - FLASH_BT_WHITELIST_SIZE

/// Bluetooth pairing recording area
#define FLASH_BT_PAIR_SIZE              4*1024  
#define FLASH_BT_PAIR_ADDR              FLASH_CONFIG_ADDR - FLASH_BT_PAIR_SIZE

/// Bluetooth configuration area
#define FLASH_CONFIG_SIZE               4*1024  
#define FLASH_CONFIG_ADDR               FLASH_CONFIG_BACK_ADDR - FLASH_CONFIG_SIZE

/// Bluetooth configuration backup area
#define FLASH_CONFIG_BACK_SIZE          4*1024  
#define FLASH_CONFIG_BACK_ADDR          FLASH_TOTAL_SIZE + FLASH_START_ADDR - FLASH_CONFIG_BACK_SIZE

/**
 * ENUM
 ****************************************************************************************
 */ 

/// Returned value of the flash management interface
enum FALSH_MANAGE_ERR{
    FLASH_MANAGE_ERR_ADDR = HAL_FLASH_ERR_END,
    FLASH_MANAGE_ERR_LEN,
    FLASH_MANAGE_ERR_NO_MEM,
};

/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief Example Initialize the flash management module
 * 
 */
void flash_manage_init(void); 

/**
 * @brief flash unlock and retime (if automatic lock is ENABLE).
 * @param enable enable/disable.
 * @return void
 */
void flash_unlock(void);

/**
 * @brief The flash automatic addition function was enabled.
 * @param enable enable/disable.
 * @return void
 */
void flash_manage_autolock(bool enable); 

/**
 * @brief Read data to the Secondary configuration area
 * 
 * @param addr offset address Secondary configuration area
 * @param buf buffer pointer
 * @param len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_minor_cfg_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erase ota and user data area
 * 
 * @param addr offset address of ota and user data area
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_app_cache_erase(uint32_t off_addr);

/**
 * @brief Write data to ota area and user data area
 * 
 * @param addr offset address of ota and user data area
 * @param buf buffer pointer
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_app_cache_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data to ota area and user data area
 * 
 * @param addr offset address of ota and user data area
 * @param buf buffer pointer
 * @param len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_app_cache_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erases the diagnostic log area
 * 
 * @param addr  offset address of diagnostic log area
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_diag_erase(uint32_t off_addr);

/**
 * @brief Writes data to the diagnostic log area
 * 
 * @param addr offset address of diagnostic log area
 * @param buf buffer pointer 
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_diag_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data to the diagnostic log area
 * 
 * @param addr offset address of diagnostic log area
 * @param buf buffer pointer 
 * @param len len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_diag_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erase the whitelist area
 * 
 * @param addr offset address of whitelist area
 * @return uint32_t FALSH_MANAGE_ERR
 * @note 2k is too big, Maybe out of memory
 */
uint32_t flash_bt_whitelist_erase(uint32_t off_addr);

/**
 * @brief Write data to the whitelist area
 * 
 * @param addr offset address of whitelist area
 * @param buf buffer pointer 
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 * @note Must write once，2k is too big, Maybe out of memory
 */
uint32_t flash_bt_whitelist_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data from the whitelist area
 * 
 * @param addr offset address of whitelist area
 * @param buf buffer pointer 
 * @param len len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_bt_whitelist_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erase the bluetooth pairing area
 * 
 * @param addr offset address of bluetooth pairing area
 * @return uint32_t FALSH_MANAGE_ERR
 * @note Erase in sector unit
 * @note 2k is too big, Maybe out of memory
 */
uint32_t flash_bt_pair_erase(uint32_t off_addr);

/**
 * @brief Write data to the bluetooth pairing area
 * 
 * @param addr offset address of bluetooth pairing area
 * @param buf buffer pointer 
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 * @note Must write once，2k is too big, Maybe out of memory
 */
uint32_t flash_bt_pair_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data from the bluetooth pairing area
 * 
 * @param addr offset address of bluetooth pairing area
 * @param buf buffer pointer 
 * @param len len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_bt_pair_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erase the bluetooth config area
 * 
 * @param addr offset address of bluetooth config area
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_erase(uint32_t off_addr);

/**
 * @brief Write data to the bluetooth config area
 * 
 * @param addr offset address of bluetooth config area
 * @param buf buffer pointer 
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data from the bluetooth config area
 * 
 * @param addr offset address of bluetooth config area
 * @param buf buffer pointer 
 * @param len len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erase the bluetooth config backup area
 * 
 * @param addr offset address of bluetooth config backup area
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_back_erase(uint32_t off_addr);

/**
 * @brief Write data to the bluetooth config backup area
 * 
 * @param addr offset address of bluetooth config backup area
 * @param buf buffer pointer 
 * @param len The length of bytes to be written
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_back_write(uint32_t off_addr, uint8_t *buf, uint32_t len);

/**
 * @brief Read data from the bluetooth config backup area
 * 
 * @param addr offset address of bluetooth config backup area
 * @param buf buffer pointer 
 * @param len len Length of bytes to be read
 * @return uint32_t FALSH_MANAGE_ERR
 */
uint32_t flash_config_back_read(uint32_t off_addr, uint8_t *buf, uint32_t len);

#endif
