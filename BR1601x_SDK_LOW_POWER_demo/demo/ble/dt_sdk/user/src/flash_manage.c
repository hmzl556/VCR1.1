/**
 * @file flash_manage.c
 * @author Barrot Firmware Team
 * @brief This file implements the interface of flash management module
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

#include "hal_flash.h"
#include "flash_manage.h"
#include "porting.h"
#include "task.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// Check whether the off_addr is in the area range, fix me
#define IS_VALID_ADDR(off_addr, area_len)\
do{\
    if(off_addr >= area_len)\
        return FLASH_MANAGE_ERR_ADDR;\
}while(0)

/// Check whether the len is in the area range, fix me
#define IS_VALID_LEN(off_addr, len, area_len)\
do{\
    if(len > area_len || len == 0 || off_addr + len > area_len)\
        return FLASH_MANAGE_ERR_LEN;\
}while(0)

/// Offset address translation
#define AREA_ADDR_BASE(off_addr, area_base)    (off_addr + area_base)

/// Automatic locking delay
#define FLASH_AUTOLOCK_DELAY    10000

/*
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */
#ifdef BR1601A01
static tid_t autolock_id = NULL;
static bool autolock_enable = DISABLE;
#endif
/*
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
#ifdef BR1601A01
static void flash_autolock(uint8_t ev, void *arg)
{
    int32_t duration = br_idle_time_get();
    if(duration>64 || duration<-64)
        hal_flash_lock();
    else
        TIMER_CREATE(10, NULL, (task_func *)flash_autolock);
}

static void flash_manage_autolock_start(uint8_t ev, void *arg)
{
	flash_manage_autolock(ENABLE);
}
#endif
/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

void flash_manage_init(void) 
{
    hal_flash_init();
	/// Before rwip initialization, the flash must be unlocked during configuration area initialization
#ifdef BR1601A01
    hal_flash_unlock();
	/// flash autolock task, When the system is powered on, you cannot directly create a timer task. You need to transfer the task
    TASK_CREATE_ONESHOT(flash_manage_autolock_start, NULL, NULL);
#elif BR1601A02
    hal_flash_lock();//todo
#endif
}

void flash_manage_autolock(bool enable)
{
#ifdef BR1601A01
    autolock_enable = enable;
#endif
    flash_unlock();
}

void flash_unlock(void)
{
    hal_flash_unlock();
#ifdef BR1601A01
    if(autolock_enable == ENABLE){
        if(autolock_id != NULL){
           timer_restart(&autolock_id);
        }
        else {
           TIMER_CREATE_ONESHOT(&autolock_id, FLASH_AUTOLOCK_DELAY, NULL, (task_func *)flash_autolock);
        }
    }
	else if(autolock_id != NULL){
		timer_delete(&autolock_id);
	}
#endif
}

uint32_t flash_minor_cfg_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_MINOR_CFG_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_MINOR_CFG_SIZE);
    
    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_MINOR_CFG_ADDR), buf, len);
}

uint32_t flash_app_cache_erase(uint32_t off_addr)
{
    IS_VALID_ADDR(off_addr, FLASH_APP_CACHE_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif
    
    return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_APP_CACHE_ADDR));
}


uint32_t flash_app_cache_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_APP_CACHE_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_APP_CACHE_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif
    
    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_APP_CACHE_ADDR), buf, len);
}

uint32_t flash_app_cache_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_APP_CACHE_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_APP_CACHE_SIZE);

    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_APP_CACHE_ADDR), buf, len);
}

#if (FLASH_DIAG_SIZE != 0)
uint32_t flash_diag_erase(uint32_t off_addr)
{
    IS_VALID_ADDR(off_addr, FLASH_DIAG_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif
    
    return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_DIAG_ADDR));
}

uint32_t flash_diag_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{ 
    IS_VALID_ADDR(off_addr, FLASH_DIAG_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_DIAG_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_DIAG_ADDR), buf, len);
}

uint32_t flash_diag_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{ 
    IS_VALID_ADDR(off_addr, FLASH_DIAG_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_DIAG_SIZE);

    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_DIAG_ADDR), buf, len);
}
#endif

uint32_t flash_bt_whitelist_erase(uint32_t off_addr)
{
    IS_VALID_ADDR(off_addr, FLASH_BT_WHITELIST_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_BT_WHITELIST_ADDR));
}


uint32_t flash_bt_whitelist_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{  
    IS_VALID_ADDR(off_addr, FLASH_BT_WHITELIST_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_BT_WHITELIST_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_BT_WHITELIST_ADDR), buf, len);
}

uint32_t flash_bt_whitelist_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_BT_WHITELIST_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_BT_WHITELIST_SIZE);

    
    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_BT_WHITELIST_ADDR), buf, len);
}

uint32_t flash_bt_pair_erase(uint32_t off_addr)
{  
    IS_VALID_ADDR(off_addr, FLASH_BT_PAIR_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_BT_PAIR_ADDR));

}

uint32_t flash_bt_pair_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{    
    IS_VALID_ADDR(off_addr, FLASH_BT_PAIR_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_BT_PAIR_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_BT_PAIR_ADDR), buf, len);
}

uint32_t flash_bt_pair_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{ 
    IS_VALID_ADDR(off_addr, FLASH_BT_PAIR_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_BT_PAIR_SIZE);

    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_BT_PAIR_ADDR), buf, len);
}

uint32_t flash_config_erase(uint32_t off_addr)
{
    IS_VALID_ADDR(off_addr, FLASH_CONFIG_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif  
    return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_ADDR));
}


uint32_t flash_config_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_CONFIG_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_CONFIG_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif

    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_ADDR), buf, len);
}

uint32_t flash_config_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{ 
    IS_VALID_ADDR(off_addr, FLASH_CONFIG_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_CONFIG_SIZE);

    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_ADDR), buf, len);
}

uint32_t flash_config_back_erase(uint32_t off_addr)
{
   IS_VALID_ADDR(off_addr, FLASH_CONFIG_BACK_SIZE);
#ifdef BR1601A01
   flash_unlock();
#endif

   return hal_flash_erase_sector(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_BACK_ADDR));
}


uint32_t flash_config_back_write(uint32_t off_addr, uint8_t *buf, uint32_t len)
{ 
    IS_VALID_ADDR(off_addr, FLASH_CONFIG_BACK_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_CONFIG_BACK_SIZE);
#ifdef BR1601A01
    flash_unlock();
#endif
    return hal_flash_write(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_BACK_ADDR), buf, len);
}

uint32_t flash_config_back_read(uint32_t off_addr, uint8_t *buf, uint32_t len)
{
    IS_VALID_ADDR(off_addr, FLASH_CONFIG_BACK_SIZE);
    IS_VALID_LEN(off_addr, len, FLASH_CONFIG_BACK_SIZE);

    return hal_flash_read(AREA_ADDR_BASE(off_addr, FLASH_CONFIG_BACK_ADDR), buf, len);
}
