/**
 * @file app_task.c
 * @author Barrot Firmware Team
 * @brief This file contains the implementation of the main business logic
 * @version 0.1
 * @date 2023-09-19
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */


/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "app_task.h"
#include "app_whitelist.h"
#include "task.h"
#include "flash_manage.h"
//#define  BR_DEBUG_LEVEL 0xFF
#include "log.h"
/*
 * DEFINES
 ****************************************************************************************
 */
#define WHITE_LIST_VALID_FLAG   0x5A5A
/// Immediate Save Flag.
/// 0:means writing during baseband idle time, which may result in save failure; 
/// 1:means immediate writing, which may cause connection disconnection in multi-connection scenarios.
#define WHITE_LIST_SAVE_IMMEDIATE       1 
/**
 * LOCAL VARIABLES DECLARATION
 ****************************************************************************************
 */
static local_white_list_data_t s_wlst_data;
static tid_t s_wl_tid;
/**
 * GLOBAL VARIABLES DECLARATION
 ****************************************************************************************
 */

/**
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
static void whilelist_save_task(uint8_t ev, void *arg)
{
#if (WHITE_LIST_SAVE_IMMEDIATE == 0)
    int32_t duration = br_idle_time_get();
    
    if(duration > 80 || duration < -80){
        flash_bt_whitelist_erase(0x00);
        flash_bt_whitelist_write(0x00, (uint8_t *)&s_wlst_data, sizeof(local_white_list_data_t));
    }
    else{
		/// reserve data, Create a new timer
        TIMER_CREATE_ONESHOT(&s_wl_tid, 10, NULL, whilelist_save_task);
    }
#else 
    flash_bt_whitelist_erase(0x00);
    flash_bt_whitelist_write(0x00, (uint8_t *)&s_wlst_data, sizeof(local_white_list_data_t));
#endif
}

static void user_whitelist_save(local_white_list_data_t *param)
{
    memcpy(&s_wlst_data, param, sizeof(local_white_list_data_t));
    if(s_wl_tid == NULL) {
    	TIMER_CREATE_ONESHOT(&s_wl_tid, 10, NULL, whilelist_save_task);
	}
}

/**
 * LOCAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
void user_while_list_init(void)
{
    local_white_list_data_t * p_strored = &s_wlst_data;
	memset(p_strored, 0xFF, sizeof(local_white_list_data_t));
    flash_bt_whitelist_read(0x00, (uint8_t *)p_strored, sizeof(local_white_list_data_t));

    if(p_strored->valid_flag != WHITE_LIST_VALID_FLAG || p_strored->num > MAX_WHTIELIST_NUM) {
        memset(p_strored, 0xFF, sizeof(local_white_list_data_t));
        p_strored->valid_flag = WHITE_LIST_VALID_FLAG;
        p_strored->num = 0;
        flash_bt_whitelist_erase(0);
        //flash_bt_whitelist_write(0x00, (uint8_t *)p_strored, sizeof(local_white_list_data_t));// Reduce the number of erasures
    }
}


uint8_t user_white_list_get_size(void)
{
    local_white_list_data_t * p_strored = &s_wlst_data;
    return p_strored->num;
}

void user_white_list_get_dev(uint8_t index, ble_addr_t *data)
{
    local_white_list_data_t * p_strored = &s_wlst_data;
    if(p_strored->num > index)
    	memcpy(data, &p_strored->data[index], sizeof(ble_addr_t));
}

bool user_while_list_find(ble_addr_t *data)
{
    bool found = false;
    local_white_list_data_t * p_strored = &s_wlst_data;
   
    for(uint8_t i=0; i<p_strored->num; i++){
        if(!memcmp(&p_strored->data[i], data, sizeof(ble_addr_t))){
            found = true;
            break;
        }
    }    
    return found;
}

void user_white_list_add_dev(ble_addr_t *data)
{
    uint8_t dev_cnt = user_white_list_get_size();
    ///check index
    local_white_list_data_t strored_data;
    local_white_list_data_t * p_strored = &strored_data;
    /// init p_strored
    memset(p_strored, 0xFF, sizeof(local_white_list_data_t));
    p_strored->valid_flag = WHITE_LIST_VALID_FLAG;
    p_strored->num = 0;
    /// If the device is not in the whitelist, it is added
    if(user_while_list_find(data) == false) {
        p_strored->num = (dev_cnt >= MAX_WHTIELIST_NUM) ? MAX_WHTIELIST_NUM : (dev_cnt + 1);
        memcpy(&p_strored->data[p_strored->num-1], data, sizeof(ble_addr_t));
        for(uint8_t i=0; i<p_strored->num-1; i++)
            user_white_list_get_dev(dev_cnt-1-i, &p_strored->data[p_strored->num-2-i]);
        user_whitelist_save(p_strored);
    	BR_LOG_DEBUG("Whitelist information add\r\n"); 
    }
}


uint8_t user_white_list_del_dev(ble_addr_t *data)
{
    uint8_t ret = false;
    ble_addr_t dev_data;
    uint8_t dev_cnt = user_white_list_get_size();
    ///check index
    local_white_list_data_t strored_data;
    local_white_list_data_t * p_strored = &strored_data;
    if(p_strored != NULL){
        memset(p_strored, 0xFF, sizeof(local_white_list_data_t));
        p_strored->valid_flag = WHITE_LIST_VALID_FLAG;
        p_strored->num = 0;
        for(uint8_t i=0; i<dev_cnt; i++){
            user_white_list_get_dev(i, &dev_data);
            if(memcmp(data, &dev_data, sizeof(ble_addr_t)) != 0){
                memcpy(&p_strored->data[p_strored->num], &dev_data, sizeof(ble_addr_t));
                p_strored->num++;
            }
        }
        if(dev_cnt != p_strored->num){
            user_whitelist_save(p_strored);
            ret = true;
            BR_LOG_DEBUG("Whitelist information deleted\r\n");
        }
    }
    return ret;
}

void user_white_list_erase_all(void)
{
    if(s_wl_tid != NULL) {
        timer_delete(&s_wl_tid);
    }
    local_white_list_data_t * p_strored = &s_wlst_data;
	memset(p_strored, 0xFF, sizeof(local_white_list_data_t));
    p_strored->valid_flag = WHITE_LIST_VALID_FLAG;
    p_strored->num = 0;
    flash_bt_whitelist_erase(0);
}

void user_white_list_store(void)
{
    if(s_wl_tid != NULL) {
        timer_delete(&s_wl_tid);
        flash_bt_whitelist_erase(0x00);
        flash_bt_whitelist_write(0x00, (uint8_t *)&s_wlst_data, sizeof(local_white_list_data_t));    
    }
}


/**
 * @}
 */
