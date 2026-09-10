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
 * @file iot_log_ex.h
 * @author Tianlx
 * @version v1.2
 *
 * @copyright Copyright (c) 2025, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef _IOT_LOG_EX_H
#define _IOT_LOG_EX_H

/*************************** INCLUDED FILES ******************************/
#include <stdio.h>

#define CONFIG_LOG_EX_OUTPUT  //turn on/off iot log ex module globally
#define CONFIG_LOG_EX_LINE_OUTPUT   1//enable line output in log
#define CONFIG_LOG_EX_ARRAY_EXT     1//enable print ascii in array

#ifdef CONFIG_LOG_EX_OUTPUT

#define CONFIG_IOT_LOG_EX_SHOW_TAGS //show tag default

#ifdef IN_BR1601_SDK //comform to BR1601 SDK
#include "co_printf.h"
#define _OUTPUT   co_printf
#else
#include <stdio.h>
#define _OUTPUT   printf
#endif

/*************************** EXTERNAL MACROS *****************************/
/* Log tag name */
#ifdef CONFIG_IOT_LOG_EX_SHOW_TAGS
#define IOT_LOG_EX_TAG_ERR         "[ERR]"
#define IOT_LOG_EX_TAG_WRN         "[WRN]"
#define IOT_LOG_EX_TAG_INF         "[INF]"
#define IOT_LOG_EX_TAG_DBG         "[DBG]"
#define IOT_LOG_EX_TAG_VBS         "[VBS]"
#else
#define IOT_LOG_EX_TAG_ERR         ""
#define IOT_LOG_EX_TAG_WRN         ""
#define IOT_LOG_EX_TAG_INF         ""
#define IOT_LOG_EX_TAG_DBG         ""
#define IOT_LOG_EX_TAG_VBS         ""
#endif /* CONFIG_IOT_LOG_EX_SHOW_TAGS */

/* Log domain name */
#ifndef LOG_EX_HEAD
#define LOG_EX_HEAD                "others"
#endif

/* Log level value */
#define IOT_LOG_EX_LEVEL_OFF       0
#define IOT_LOG_EX_LEVEL_ERROR     1
#define IOT_LOG_EX_LEVEL_WARNING   2
#define IOT_LOG_EX_LEVEL_INFO      3
#define IOT_LOG_EX_LEVEL_DEBUG     4
#define IOT_LOG_EX_LEVEL_VERBOSE   5

/* Log level */
#ifndef IOT_LOG_EX_LEVEL
#define IOT_LOG_EX_LEVEL            IOT_LOG_EX_LEVEL_INFO //default log level
#endif

#if CONFIG_LOG_EX_ARRAY_EXT
#define ISPRINT(A) (A < 127 && A > 31)
#define _OUTPUT_ASCII(array, len)                   do{ \
                                                    int __i; for(__i=0;__i<(len);++__i){ \
                                                    if(ISPRINT(((uint8_t *)(array))[__i])){_OUTPUT("%c",((uint8_t *)(array))[__i]);} \
                                                    else {_OUTPUT(".");}}\
                                                    _OUTPUT("\r\n");}while(0)

#define _OUTPUT_ASCII_ANTI(array, len)              do{ \
                                                    int __i; for(__i=len-1;__i>=(0);__i--){ \
                                                    if(ISPRINT(((uint8_t *)(array))[__i])){_OUTPUT("%c",((uint8_t *)(array))[__i]);} \
                                                    else {_OUTPUT(".");}}\
                                                    _OUTPUT("\r\n");}while(0)

#define _OUTPUT_ARRAY(array, len)                   do{int __i; for(__i=0;__i<(len);++__i)_OUTPUT("%02X ",((uint8_t *)(array))[__i]);_OUTPUT("\t");_OUTPUT_ASCII(array, len);}while(0)
#define _OUTPUT_ARRAY_ANTI(array, len)              do{int __i; for(__i=len-1;__i>=(0);__i--)_OUTPUT("%02X ",((uint8_t *)(array))[__i]);_OUTPUT("\t");_OUTPUT_ASCII(array, len);}while(0)
#else
#define _OUTPUT_ARRAY(array, len)                   do{int __i; for(__i=0;__i<(len);++__i)_OUTPUT("%02X ",((uint8_t *)(array))[__i]);_OUTPUT("\r\n");}while(0)
#define _OUTPUT_ARRAY_ANTI(array, len)              do{int __i; for(__i=len-1;__i>=(0);__i--)_OUTPUT("%02X ",((uint8_t *)(array))[__i]);_OUTPUT("\r\n");}while(0)
#endif /*CONFIG_LOG_EX_ARRAY_EXT*/

#if CONFIG_LOG_EX_LINE_OUTPUT
#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_ERROR
#define IOT_LOG_ERR(format, ...)                    do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD,__LINE__); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_ERR_ARRAY(array, len)               do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD,__LINE__); _OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_ERR_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_ERR_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_ERR_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_ERR_FUNC()                          do{_OUTPUT("%s %s [Line:%d] %s", IOT_LOG_EX_TAG_ERR,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_ERR_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_ERR(format, ...)
#define IOT_LOG_ERR_ARRAY(array, len)
#define IOT_LOG_ERR_ARRAY_EX(note, array, len)
#define IOT_LOG_ERR_ARRAY_ANTI(array, len)
#define IOT_LOG_ERR_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_ERR_FUNC()
#define IOT_LOG_ERR_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_ERROR */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_WARNING
#define IOT_LOG_WRN(format, ...)                    do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD,__LINE__); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_WRN_ARRAY(array, len)               do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY(array, len);;}while(0)
#define IOT_LOG_WRN_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY(array, len);_OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_WRN_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_WRN_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_WRN_FUNC()                          do{_OUTPUT("%s %s [Line:%d] %s", IOT_LOG_EX_TAG_WRN,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_WRN_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_WRN(format, ...)
#define IOT_LOG_WRN_ARRAY(array, len)
#define IOT_LOG_WRN_ARRAY_EX(note, array, len)
#define IOT_LOG_WRN_ARRAY_ANTI(array, len)
#define IOT_LOG_WRN_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_WRN_FUNC()
#define IOT_LOG_WRN_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_WARNING */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_INFO
#define IOT_LOG_INF(format, ...)                    do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD,__LINE__); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_INF_ARRAY(array, len)               do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_INF_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_INF_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_INF_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY_ANTI(array, len);; _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_INF_FUNC()                          do{_OUTPUT("%s %s [Line:%d] %s", IOT_LOG_EX_TAG_INF,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_INF_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_INF(format, ...)
#define IOT_LOG_INF_ARRAY(array, len)
#define IOT_LOG_INF_ARRAY_EX(note, array, len)
#define IOT_LOG_INF_ARRAY_ANTI(array, len)
#define IOT_LOG_INF_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_INF_FUNC()
#define IOT_LOG_INF_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_INFO */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_DEBUG
#define IOT_LOG_DBG(format, ...)                    do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD,__LINE__); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_DBG_ARRAY(array, len)               do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_DBG_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_DBG_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_DBG_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_DBG_FUNC()                          do{_OUTPUT("%s %s [Line:%d] %s", IOT_LOG_EX_TAG_DBG,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_DBG_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_DBG(format, ...)
#define IOT_LOG_DBG_ARRAY(array, len)
#define IOT_LOG_DBG_ARRAY_EX(note, array, len)
#define IOT_LOG_DBG_ARRAY_ANTI(array, len)
#define IOT_LOG_DBG_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_DBG_FUNC()
#define IOT_LOG_DBG_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_DEBUG */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_VERBOSE
#define IOT_LOG_VBS(format, ...)                    do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD,__LINE__); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_VBS_ARRAY(array, len)               do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD,__LINE__); _OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_VBS_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY(array, len);_OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_VBS_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s][Line:%d] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD,__LINE__);_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_VBS_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s][Line:%d] %s: ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD,__LINE__, note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_VBS_FUNC()                          do{_OUTPUT("%s %s [Line:%d] %s", IOT_LOG_EX_TAG_VBS,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_VBS_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_VBS(format, ...)
#define IOT_LOG_VBS_ARRAY(array, len)
#define IOT_LOG_VBS_ARRAY_EX(note, array, len)
#define IOT_LOG_VBS_ARRAY_ANTI(array, len)
#define IOT_LOG_VBS_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_VBS_FUNC()
#define IOT_LOG_VBS_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_VERBOSE */

#else
#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_ERROR
#define IOT_LOG_ERR(format, ...)                    do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD ); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_ERR_ARRAY(array, len)               do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD ); _OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_ERR_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD , note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_ERR_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD );_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_ERR_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_ERR, LOG_EX_HEAD , note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_ERR_FUNC()                          do{_OUTPUT("%s %s %d %s", IOT_LOG_EX_TAG_ERR,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_ERR_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_ERR(format, ...)
#define IOT_LOG_ERR_ARRAY(array, len)
#define IOT_LOG_ERR_ARRAY_EX(note, array, len)
#define IOT_LOG_ERR_ARRAY_ANTI(array, len)
#define IOT_LOG_ERR_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_ERR_FUNC()
#define IOT_LOG_ERR_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_ERROR */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_WARNING
#define IOT_LOG_WRN(format, ...)                    do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_WRN_ARRAY(array, len)               do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD );_OUTPUT_ARRAY(array, len);;}while(0)
#define IOT_LOG_WRN_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD , note); _OUTPUT_ARRAY(array, len);_OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_WRN_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD );_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_WRN_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_WRN, LOG_EX_HEAD , note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_WRN_FUNC()                          do{_OUTPUT("%s %s %d %s", IOT_LOG_EX_TAG_WRN,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_WRN_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_WRN(format, ...)
#define IOT_LOG_WRN_ARRAY(array, len)
#define IOT_LOG_WRN_ARRAY_EX(note, array, len)
#define IOT_LOG_WRN_ARRAY_ANTI(array, len)
#define IOT_LOG_WRN_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_WRN_FUNC()
#define IOT_LOG_WRN_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_WARNING */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_INFO
#define IOT_LOG_INF(format, ...)                    do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD ); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_INF_ARRAY(array, len)               do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD );_OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_INF_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD , note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_INF_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD );_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_INF_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_INF, LOG_EX_HEAD , note); _OUTPUT_ARRAY_ANTI(array, len);; _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_INF_FUNC()                          do{_OUTPUT("%s %s %d %s", IOT_LOG_EX_TAG_INF,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_INF_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_INF(format, ...)
#define IOT_LOG_INF_ARRAY(array, len)
#define IOT_LOG_INF_ARRAY_EX(note, array, len)
#define IOT_LOG_INF_ARRAY_ANTI(array, len)
#define IOT_LOG_INF_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_INF_FUNC()
#define IOT_LOG_INF_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_INFO */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_DEBUG
#define IOT_LOG_DBG(format, ...)                    do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD ); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_DBG_ARRAY(array, len)               do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD );_OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_DBG_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD , note); _OUTPUT_ARRAY(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_DBG_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD );_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_DBG_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_DBG, LOG_EX_HEAD , note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_DBG_FUNC()                          do{_OUTPUT("%s %s %d %s", IOT_LOG_EX_TAG_DBG,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_DBG_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_DBG(format, ...)
#define IOT_LOG_DBG_ARRAY(array, len)
#define IOT_LOG_DBG_ARRAY_EX(note, array, len)
#define IOT_LOG_DBG_ARRAY_ANTI(array, len)
#define IOT_LOG_DBG_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_DBG_FUNC()
#define IOT_LOG_DBG_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_DEBUG */

#if IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_VERBOSE
#define IOT_LOG_VBS(format, ...)                    do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD ); _OUTPUT(format,  ## __VA_ARGS__); _OUTPUT("\r\n");}while(0)
#define IOT_LOG_VBS_ARRAY(array, len)               do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD ); _OUTPUT_ARRAY(array, len);}while(0)
#define IOT_LOG_VBS_ARRAY_EX(note, array, len)      do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD , note); _OUTPUT_ARRAY(array, len);_OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_VBS_ARRAY_ANTI(array, len)          do{_OUTPUT("%s[%s] ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD );_OUTPUT_ARRAY_ANTI(array, len);}while(0)
#define IOT_LOG_VBS_ARRAY_ANTI_EX(note, array, len) do{_OUTPUT("%s[%s] %s: ", IOT_LOG_EX_TAG_VBS, LOG_EX_HEAD , note); _OUTPUT_ARRAY_ANTI(array, len); _OUTPUT("[%d bytes]\r\n", len);}while(0)
#define IOT_LOG_VBS_FUNC()                          do{_OUTPUT("%s %s %d %s", IOT_LOG_EX_TAG_VBS,__FUNCTION__, __LINE__, __FILE__);}while(0)
#define IOT_LOG_VBS_NULL()                          do{_OUTPUT("\r\n");}while(0)
#else
#define IOT_LOG_VBS(format, ...)
#define IOT_LOG_VBS_ARRAY(array, len)
#define IOT_LOG_VBS_ARRAY_EX(note, array, len)
#define IOT_LOG_VBS_ARRAY_ANTI(array, len)
#define IOT_LOG_VBS_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_VBS_FUNC()
#define IOT_LOG_VBS_NULL()
#endif /* IOT_LOG_EX_LEVEL >= IOT_LOG_EX_LEVEL_VERBOSE */

#endif /*CONFIG_LOG_EX_LINE_OUTPUT == 0*/

#else
#define IOT_LOG_ERR(format, ...)
#define IOT_LOG_ERR_ARRAY(array, len)
#define IOT_LOG_ERR_ARRAY_EX(note, array, len)
#define IOT_LOG_ERR_ARRAY_ANTI(array, len)
#define IOT_LOG_ERR_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_ERR_FUNC()
#define IOT_LOG_ERR_NULL()

#define IOT_LOG_WRN(format, ...)
#define IOT_LOG_WRN_ARRAY(array, len)
#define IOT_LOG_WRN_ARRAY_EX(note, array, len)
#define IOT_LOG_WRN_ARRAY_ANTI(array, len)
#define IOT_LOG_WRN_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_WRN_FUNC()
#define IOT_LOG_WRN_NULL()

#define IOT_LOG_INF(format, ...)
#define IOT_LOG_INF_ARRAY(array, len)
#define IOT_LOG_INF_ARRAY_EX(note, array, len)
#define IOT_LOG_INF_ARRAY_ANTI(array, len)
#define IOT_LOG_INF_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_INF_FUNC()
#define IOT_LOG_INF_NULL()

#define IOT_LOG_DBG(format, ...)
#define IOT_LOG_DBG_ARRAY(array, len)
#define IOT_LOG_DBG_ARRAY_EX(note, array, len)
#define IOT_LOG_DBG_ARRAY_ANTI(array, len)
#define IOT_LOG_DBG_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_DBG_FUNC()
#define IOT_LOG_DBG_NULL()

#define IOT_LOG_VBS(format, ...)
#define IOT_LOG_VBS_ARRAY(array, len)
#define IOT_LOG_VBS_ARRAY_EX(note, array, len)
#define IOT_LOG_VBS_ARRAY_ANTI(array, len)
#define IOT_LOG_VBS_ARRAY_ANTI_EX(note, array, len)
#define IOT_LOG_VBS_FUNC()
#define IOT_LOG_VBS_NULL()
#endif

#endif  //_IOT_LOG_EX_H
