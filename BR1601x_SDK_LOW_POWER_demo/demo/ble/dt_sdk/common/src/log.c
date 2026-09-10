/**
 * @file log.c
 * @author Barrot Firmware Team
 * @brief This file implements log port initialization
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

/**
 * INCLUDE FILES
 *******************************************************************************
 */

#include "log.h"

/**
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 * @brief  init the log feature 
 */
void log_init(void)
{
    co_log_init();
}

void log_deinit(void)
{  
    co_log_deinit();
}
