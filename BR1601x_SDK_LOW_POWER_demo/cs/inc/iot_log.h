/*****************************************************************************
 * Copyright (c) 2025, Barrot Technology Co.,Ltd..
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
 * @file iot_log.h
 * @author Tianlx
 * @version v1.2
 *
 * @copyright Copyright (c) 2025, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#ifndef _IOT_LOG_H
#define _IOT_LOG_H

#ifdef __cplusplus
extern "C" {
#endif
/*************************** INCLUDED FILES ******************************/
#include <stdint.h>
#include <stdio.h>
/*************************** EXTERNAL MACROS *****************************/
#ifndef IOT_LOG_COLOR_ENABLE
#define IOT_LOG_COLOR_ENABLE    0//disable color output with 0
#endif

#define IOT_LOG_LEVEL_DEBUG     4
#define IOT_LOG_LEVEL_INFOR     3
#define IOT_LOG_LEVEL_WARNING   2
#define IOT_LOG_LEVEL_ERROR     1
#define IOT_LOG_LEVEL_OFF       0

#ifndef IOT_LOG_LEVEL
#define IOT_LOG_LEVEL           IOT_LOG_LEVEL_DEBUG//set static log level
#endif

#define LOG_COLOR_WHITE          "\033[0;37m"
#define LOG_COLOR_RED            "\033[0;31m"
#define LOG_COLOR_YELLOW         "\033[0;33m"
#define LOG_COLOR_CYAN           "\033[0;36m"
#define LOG_COLOR_GREEN          "\033[0;32m"

#if (IOT_LOG_LEVEL >= IOT_LOG_LEVEL_ERROR)
#define IOT_LOG_ERROR(...)        IOT_LOG_OUTPUT(LOG_COLOR_RED, __VA_ARGS__)
#else
#define IOT_LOG_ERROR(...)
#endif

#if (IOT_LOG_LEVEL >= IOT_LOG_LEVEL_WARNING)
#define IOT_LOG_WARNING(...)      IOT_LOG_OUTPUT(LOG_COLOR_YELLOW, __VA_ARGS__)
#else
#define IOT_LOG_WARNING(...)
#endif

#if (IOT_LOG_LEVEL >= IOT_LOG_LEVEL_INFOR)
#define IOT_LOG_INFO(...)         IOT_LOG_OUTPUT(LOG_COLOR_CYAN, __VA_ARGS__)
#else
#define IOT_LOG_INFO(...)
#endif

#if  (IOT_LOG_LEVEL >= IOT_LOG_LEVEL_DEBUG)
#define IOT_LOG_DEBUG(...)        IOT_LOG_OUTPUT(LOG_COLOR_GREEN, __VA_ARGS__)
#else
#define IOT_LOG_DEBUG(...)
#endif


#ifdef IN_BR1601_SDK  
#include "co_printf.h"

#define STDOUT_BUF_MAX  128//use iot_log_type_output in BR1601A01 SDK,max length of string is 128

extern void co_log_set_baud(uint32_t baud);
extern void co_log_init(void);

#define IOT_LOG_OUTPUT(color, ...)       \
do{                                      \
    if(IOT_LOG_COLOR_ENABLE)             \
    {                                    \
        co_printf("%s",color );          \
    }                                    \
    co_printf(__VA_ARGS__);              \
}while(0)
#define IOT_LOG_INIT(BAUD)               \
do{                                      \
    co_log_set_baud(BAUD);              \
    co_log_init();                       \
}while(0)

#define IOT_LOG_DEINIT()                 \
do{                                      \
    co_log_deinit();                     \
}while(0)
#else
#define IOT_LOG_OUTPUT(color, ...)       \
do{                                      \
    if(IOT_LOG_COLOR_ENABLE)             \
    {                                    \
        printf("%s",color );             \
    }                                    \
    printf(__VA_ARGS__);                 \
}while(0)
#define IOT_LOG_INIT(BAUD)               \
do{                                      \
    iot_log_init(BAUD);                  \
}while(0)

#define IOT_LOG_DEINIT()                 \
do{                                      \
    iot_log_deinit();                    \
}while(0)
#endif

/****************************** EXTERNAL TYPE DEFINES ***************************/
#define LOG_TYPE0           (0x01 << 0)
#define LOG_TYPE1           (0x01 << 1)
/*add more type*/
#define LOG_TYPE_MAX        (0x01 << 15)
#define LOG_TYPE_ALL        0xffff

/***************************** EXTERNAL FUNCTIONS *******************************/
int iot_log_init(uint32_t baud);
int iot_log_deinit(void);
int iot_log_type_on(uint16_t dbg_type);
int iot_log_type_off(uint16_t dbg_type);
int iot_log_type_output(uint16_t log_type, const char *format, ...);
#ifdef __cplusplus
}
#endif
#endif /*_IOT_LOG_H */
