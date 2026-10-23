/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
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
 * @file co_printf.c
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "usart.h"
#include "co_printf.h"

#ifdef CFG_DT_SDK
#define _LPUART1_COM_    (1)
#else
#define _LPUART1_COM_    (0)
#endif

#if (_LPUART1_COM_ == 0)
#define LPUARTx_TxPin       GPIO_PIN_1
#define LPUARTx_Tx_GPIO_AF  GPIO_AF4_LPUART1
#else
#define LPUARTx_TxPin       GPIO_PIN_12
#define LPUARTx_Tx_GPIO_AF  GPIO_AF2_LPUART1
#endif

#define CO_PRINTF_ENABLE    (1U)    //开启co_printf功能，1: 开启，0: 关闭

static uint8_t s_dbg_tmp_buf[256];
static uint32_t s_log_baud = 115200;//波特率115200
void co_log_set_baud(uint32_t baud);

// 初始化日志模块，后续可使用 co_printf 输出日志
void co_log_init(void)
{
    LPUART_InitType LPUART_InitStructure;
    /* System Clocks Configuration */
    RCC_ConfigLpuartClk(RCC_LPUART1CLK, RCC_LPUARTCLK_SRC_APB1);
    /* Enable LPUARTx Clock */
    RCC_EnableLpuartClk(ENABLE);

    /* Configure the GPIO ports */
    GPIO_InitType GPIO_InitStructure;
    
    /* Enable GPIO clock */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);

    /* Initialize GPIO_InitStructure */
    GPIO_InitStruct(&GPIO_InitStructure);  
    /* Configure LPUARTx Tx as alternate function push-pull */
    GPIO_InitStructure.Pin            = LPUARTx_TxPin;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = LPUARTx_Tx_GPIO_AF;
    GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);

    LPUART_DeInit(LPUART1);
    /* LPUARTx configuration ------------------------------------------------------*/
    LPUART_InitStructure.BaudRate            = s_log_baud;
    LPUART_InitStructure.Parity              = LPUART_PE_NO;
    LPUART_InitStructure.RtsThreshold        = LPUART_RTSTH_FIFOFU;
    LPUART_InitStructure.HardwareFlowControl = LPUART_HFCTRL_NONE;
    LPUART_InitStructure.Mode                = LPUART_MODE_TX;
    /* Configure LPUARTx */
    LPUART_Init(LPUART1, &LPUART_InitStructure);
}

// 设置日志输出波特率
void co_log_set_baud(uint32_t baud)
{
    s_log_baud = baud;
}

void co_log_deinit(void)
{

}

// 标准输出函数，将格式化的字符串通过 LPUART 发送出去
void STDOUT(uint8_t *_buf, uint32_t _dlen)
{
    static uint8_t func_lock = 1;
    if(func_lock)
    {
        func_lock = 0;
        for (uint32_t i = 0; i < _dlen; i++) {
            LPUART_SendData(LPUART1, _buf[i]);
            while (LPUART_GetFlagStatus(LPUART1, LPUART_FLAG_TXC) == RESET);
            LPUART_ClrFlag(LPUART1, LPUART_FLAG_TXC); 
        }
        func_lock = 1;
    }
}

// 格式化输出函数，类似于 printf，但输出到 LPUART
int co_printf(const char *fmt, ...)
{
#if CO_PRINTF_ENABLE
    int r;
    va_list arglist;
    va_start(arglist, fmt);
    r = vsnprintf((char *)s_dbg_tmp_buf, sizeof(s_dbg_tmp_buf), fmt, arglist);
    STDOUT(s_dbg_tmp_buf, r);
    va_end(arglist);

    return r;
#else
    (void)fmt;
    return 0;
#endif
}

// 带有调试类型的格式化输出函数，根据 dbg_type 决定是否输出日志
int co_printf_type(uint16_t dbg_type, const char *fmt, ...)
{
#if CO_PRINTF_ENABLE
    int r;

    if (dbg_type & DBG_TYPE_ON) {
        va_list arglist;
        va_start(arglist, fmt);
        r = vsnprintf((char *)s_dbg_tmp_buf, sizeof(s_dbg_tmp_buf), fmt, arglist);
        STDOUT(s_dbg_tmp_buf, r);
        va_end(arglist);
    }
    else {
        r = 0;
    }

    return r;
#else
    (void)dbg_type;
    (void)fmt;
    return 0;
#endif
}

/*---------------------------------------------------------------------------
Description:
	Raw Data outputer.
---------------------------------------------------------------------------*/
#define CO_DBG_OUT(d_, a_)     sprintf(&((char *)(d_))[strlen((char *)(d_))], a_/* DEBUG tool functions */
#define CO_DBG_STR(d_, a_)     strcat((char *)(d_), a_);/* other head file is better? */
#define CO_DBG_BACK(d_)        ((char *)(d_))[strlen((char *)(d_)) - 1] = 0;/* As a backspace, this may cause crash, becareful */
static inline void co_dbg_spacealign(uint8_t *s, uint8_t num)
{
    uint8_t *e = s + strlen((const char*)s);
    size_t k;

    while (s < e) {//backward to search last \n
        if (*e == '\n') {
            s = e + 1;
            break;
        }
        e--;
    }

    k = strlen((const char*)s);
    while (k < num) {
        CO_DBG_STR(s, " ");
        k++;
    }
}

// 输出原始数据的函数，根据 dbg_type 决定是否输出日志，并以十六进制和 ASCII 格式显示数据
void co_print_rawdata(uint16_t dbg_type, uint8_t *p, uint8_t *end)
{
	uint8_t dbg_str[96];
	uint32_t off = 0;
	uint8_t i;

#define OCTETS_ONELINE 16

    if (!(dbg_type & DBG_TYPE_ON)) {
        return;
    }

	while (p < end) {/* Line by line */
        memset(dbg_str, 0, sizeof(dbg_str));
		/* Line number, offest */
		CO_DBG_OUT(dbg_str, "%04u:"), (unsigned)off);
		off += OCTETS_ONELINE;

		/* Raw Data */
		i = 0;
		while ((i < OCTETS_ONELINE) && (p + i < end)) {
			CO_DBG_OUT(dbg_str, " %02X"), p[i]);
			i++;
		}

		/* Displayable Characters ASCII */
		co_dbg_spacealign(dbg_str, 4 + 2 + OCTETS_ONELINE * 3 + 10);
		i = 0;
		while ((i < OCTETS_ONELINE) && (p < end)) {
			if ((*p >= 32) && (*p <= 126)) {/* http://zh.wikipedia.org/zh/ASCII */
				CO_DBG_OUT(dbg_str, "%c"), *p);
			} else {
				CO_DBG_STR(dbg_str, ".");
			}
			p++;
			i++;
		}

		/* Output line */
        STDOUT(dbg_str, strlen((const char*)dbg_str));
        STDOUT("\r\n", 2);
	}
}

extern void system_delay_n_10us(uint32_t value);

// 简单的延时函数，使用系统延时函数实现，单位为 ms
void Delay_ms(uint32_t count)
{
    system_delay_n_10us(100*count);    
}

// 简单的延时函数，使用循环来实现延时，单位为 ms
void Delay(uint32_t count)
{
    int i = 0;
    for (; count > 0; count--)
    {
        i++;
    }
        
}

// 输出缓冲区数据的函数，以十六进制格式显示数据，并在每 16 个字节后换行
void log_buff(uint8_t *data, int len)
{
    int i = 0;
    for(i=0; i<len; i++)
    {
        co_printf("0x%02x, ", data[i]);
        
        if((i != 0) && ((i+1) % 16 == 0))
        {
            co_printf("\r\n");
        }
    }
    
    co_printf("\r\n");
}

// 输出文本数据的函数，以十六进制格式显示数据，并在每 16 个字节后换行
void print_text(uint8_t *data, uint32_t len)
{
    for(int i=0;i<len;i++){
        if(((i%0x10) == 0) && (i >0)) co_printf("\r\n");
        co_printf("%02X ",data[i]);
    }
    co_printf("\r\n");
}
