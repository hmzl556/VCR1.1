/**
 * @file app_task.h
 * @author Barrot Firmware Team
 * @brief This file contains the interface definition of the main business logic
 * @version 0.1
 * @date 2023-09-19
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __APP__WHITELIST__H__
#define __APP__WHITELIST__H__

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "app_ble.h"

/**
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct {
    uint16_t valid_flag;
    uint8_t num;
    ble_addr_t data[MAX_WHTIELIST_NUM]; 
}local_white_list_data_t;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief Example Initialize the whitelist module
 * 
 * @return 
 */
void user_while_list_init(void);

/**
 * @brief Obtain the number of devices in the whitelist
 * 
 * @return uint8_t  number of device 
 */
uint8_t user_white_list_get_size(void);

/**
 * @brief Obtain the specified device in the whitelist
 * 
 * @param index[in] Device index number
 * @param data[out]  device info
 */
void user_white_list_get_dev(uint8_t index, ble_addr_t *data);

/**
 * @brief Searches for the specified device in the whitelist
 * 
 * @param data[in] device info
 * @return bool  exists
 */
bool user_while_list_find(ble_addr_t *data);

/**
 * @brief Add devices to the whitelist
 * 
 * @param data[in]  device info
 */
void user_white_list_add_dev(ble_addr_t *data);

/**
 * @brief Example Delete a specified device from the whitelist
 * 
 * @param data[in]  device info
 */
uint8_t user_white_list_del_dev(ble_addr_t *data);

/**
 * @brief Clear white list
 * 
 */
void user_white_list_erase_all(void);

/**
 * @brief Save the whitelist immediately
 *
 */
void user_white_list_store(void);

#endif




