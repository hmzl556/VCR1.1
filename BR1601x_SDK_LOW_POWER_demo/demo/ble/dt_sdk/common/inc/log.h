/**
 * @file log.h
 * @author Barrot Firmware Team
 * @brief This file contains the operating interface definitions for the logs, 
 *  and each file has a different debugging level
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __LOG_H__
#define __LOG_H__

/**
 * INCLUDE FILES
 *******************************************************************************
 */
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include "co_printf.h"
/**
 * DEFINES
 ****************************************************************************************
 */

/// Debug level mask.
#define DEBUG_L_DEBUG			(0x01<<0)		//DEBUG | ���ԣ�ָ��ϸ�µ��¼���Ϣ���Ե���Ӧ��������
#define DEBUG_L_INFO			(0x01<<1)		//INFO  | ��Ϣ��ָ��������Ϣ���Ӵ�������������Ӧ�����й��̡�
#define DEBUG_L_WARNING         (0x01<<2) 		//WARN  | ���棺ָ������Ǳ�ڵ�Σ��״��	
#define DEBUG_L_ERROR           (0x01<<3)		//ERROR | ����ָ�������¼�����Ӧ�ÿ��ܻ��ܼ������С�
#define DEBUG_L_FATAL			(0x01<<4)		//FATAL | ������ָ���ǳ����صĿ��ܻᵼ��Ӧ����ִֹ�д����¼� //hardfalut

#define DEBUG_L_ALL             (DEBUG_L_DEBUG | DEBUG_L_INFO | DEBUG_L_WARNING | DEBUG_L_ERROR | DEBUG_L_FATAL)
#define DEBUG_NO_DEBUG          (DEBUG_L_WARNING | DEBUG_L_INFO | DEBUG_L_ERROR | DEBUG_L_FATAL)
#define DEBUG_L_NONE            0x00

/// Debug level.You can individual adjustment the debug level through define this macro in C file.
#ifndef BR_DEBUG_LEVEL
#define BR_DEBUG_LEVEL          DEBUG_NO_DEBUG
#endif

/// For control the color of log information in console.
#define LOG_COLOR_RED               "\033[0;31m"
#define LOG_COLOR_YELLOW            "\033[0;33m"
#define LOG_COLOR_CYAN              "\033[0;36m"
#define LOG_COLOR_GREEN             "\033[0;32m"
#define LOG_COLOR_BLUE              "\033[0;34m"
#define LOG_COLOR_MAGENTA           "\033[0;35m"
#define LOG_COLOR_LRED              "\033[0;91m"
#define LOG_COLOR_LGREEN            "\033[0;92m"
#define LOG_COLOR_LYELLOW           "\033[0;93m"
#define LOG_COLOR_LBLUE             "\033[0;94m"
#define LOG_COLOR_LMAGENTA          "\033[0;95m"
#define LOG_COLOR_CLEAR             "\r\n\033[0;0m"

#define PRINTF_COLOR_ENABLE     0
#if (PRINTF_COLOR_ENABLE == 1) 
#define COLOR_FATAL                 LOG_COLOR_RED
#define COLOR_ERROR                 LOG_COLOR_BLUE 
#define COLOR_WARNING               LOG_COLOR_YELLOW
#define COLOR_INFO                  LOG_COLOR_CYAN
#define COLOR_DEBUG                 LOG_COLOR_GREEN
#define COLOR_END                   LOG_COLOR_CLEAR
#else
#define COLOR_FATAL                 ""
#define COLOR_ERROR                 ""
#define COLOR_WARNING               ""
#define COLOR_INFO                  ""
#define COLOR_DEBUG                 ""
#define COLOR_END                   "\r\n"
#endif

/// Module name, it is used to distinguish the file that print log information.
#define MODULE_NAME                 (LOG_COLOR_MAGENTA""__FILE__)

/// Log out macro
#define BR_LOG_OUTPUT(color, ...)\
do{\
    co_printf("%s",color );\
    co_printf(__VA_ARGS__);\
}while(0)

/// Log out HEX macro
#define BR_LOG_HEX_DUMP(color, __des, __buf, __len)\
do{\
    int loop_ctrl = ((__len >> 4) + 1) << 4;\
    char chr_str[0x11];\
    loop_ctrl = ((loop_ctrl - __len) < 16) ? loop_ctrl : __len;\
    if(__len == 0){\
        break;\
    }\
    co_printf(">%s][%d] "color"""\r\n%s"" hex:\r\n",MODULE_NAME, __LINE__, __des);\
    for (int _idx = 0; _idx < loop_ctrl; ++_idx) {\
        if (_idx < __len){\
            chr_str[_idx & 0x0F] = isgraph(((const uint8_t*)(__buf))[_idx])? ((const uint8_t*)(__buf))[_idx] : '.';\
            co_printf(" %02X", ((const uint8_t*)(__buf))[_idx]);\
        }\
        else{\
            co_printf("   ");\
            chr_str[_idx & 0x0F] = 0;\
        }\
        if ((_idx & 0x0F) == 0x0F) {\
            chr_str[0x10] = 0,co_printf("\t%s\r\n", chr_str);\
        }\
    }\
    co_printf(COLOR_END);\
}while(0)

#if  (DEBUG_L_FATAL & BR_DEBUG_LEVEL)
#define BR_LOG_FATAL(...)                           BR_LOG_OUTPUT(COLOR_FATAL, __VA_ARGS__)
#define BR_LOG_FATAL_HEX(__des, __buf, __len)       BR_LOG_HEX_DUMP(COLOR_FATAL, __des, __buf, __len)
#else
#define BR_LOG_FATAL( ...) 
#define BR_LOG_FATAL_HEX(__des, __buf, __len)
#endif

#if  (DEBUG_L_ERROR & BR_DEBUG_LEVEL)
#define BR_LOG_ERROR(...)                           BR_LOG_OUTPUT(COLOR_ERROR, __VA_ARGS__)
#define BR_LOG_ERROR_HEX(__des, __buf, __len)       BR_LOG_HEX_DUMP(COLOR_ERROR, __des, __buf, __len)
#else
#define BR_LOG_ERROR( ...) 
#define BR_LOG_ERROR_HEX(__des, __buf, __len)
#endif

#if  (DEBUG_L_WARNING & BR_DEBUG_LEVEL)
#define BR_LOG_WARNING(...)                         BR_LOG_OUTPUT(COLOR_WARNING, __VA_ARGS__)
#define BR_LOG_WARNING_HEX(__des, __buf, __len)     BR_LOG_HEX_DUMP(COLOR_WARNING, __des, __buf, __len)
#else
#define BR_LOG_WARNING( ...) 
#define BR_LOG_WARNING_HEX(__des, __buf, __len)
#endif

#if  (DEBUG_L_INFO & BR_DEBUG_LEVEL)
#define BR_LOG_INFO(...)                            BR_LOG_OUTPUT(COLOR_INFO, __VA_ARGS__)
#define BR_LOG_INFO_HEX(__des, __buf, __len)        BR_LOG_HEX_DUMP(COLOR_INFO, __des, __buf, __len)
#else
#define BR_LOG_INFO( ...) 
#define BR_LOG_INFO_HEX(__des, __buf, __len)
#endif

#if  (DEBUG_L_DEBUG & BR_DEBUG_LEVEL)
#define BR_LOG_DEBUG(...)                           BR_LOG_OUTPUT(COLOR_DEBUG,__VA_ARGS__)
#define BR_LOG_DEBUG_HEX(__des, __buf, __len)       BR_LOG_HEX_DUMP(COLOR_DEBUG, __des, __buf, __len)
#else
#define BR_LOG_DEBUG( ...) 
#define BR_LOG_DEBUG_HEX(__des, __buf, __len)
#endif


/**
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief Initialization the log module
 * 
 * @param baudrate The baud rate you want to set
 */
void log_init(void);
/**
 * @brief Deinitialization the log module
 * 
 */
void log_deinit(void);

#endif /* __LOG_H__ */
/**
 * @}
 */
