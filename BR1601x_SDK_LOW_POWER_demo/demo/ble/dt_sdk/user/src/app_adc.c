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
 * @file app_adc.c
 * @author Barrot Firmware Team
 * @version v1.0.1
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "br1601x.h"
#include "porting.h"
#include "log.h"
/** @addtogroup 
 * @{
 */
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Configures the different system clocks.
 */
static void RCC_Configuration(void)
{

    /* Enable ADC clock */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
        
    RCC_ConfigAdcClk(RCC_ADCCLK_SRC_AUDIOPLL);

    /* enable ADC 4M clock */
    RCC_Enable_ADC_CLK_SRC_AUDIOPLL(ENABLE);
}


uint32_t adc_read_battery_voltage(void)
{
#define READ_TIMERS 10
    uint32_t adc_value  = 0;
    uint16_t read_cnt   = 0;

    RCC_Configuration();
    ADC_EnableBypassFilter(ADC, ENABLE);
    ADC_ConfigChannel(ADC, ADC_CTRL_CH_6);    
    for(read_cnt = 0;read_cnt<READ_TIMERS;read_cnt++)
    {
        ADC_Enable(ADC, ENABLE);
        while(ADC_GetFlagStatus(ADC,ADC_FLAG_DONE) == RESET);
        ADC_ClearFlag(ADC,ADC_FLAG_DONE);
        uint16_t single_value = ADC_GetDat(ADC);
        BR_LOG_DEBUG("battery voltage : %d ",single_value);
        adc_value += single_value;
    }
    ADC_Enable(ADC, DISABLE);
    adc_value = adc_value/READ_TIMERS;
    
    return ADC_ConverValueToVoltage(adc_value, ADC_CTRL_CH_6);
}

uint8_t adc_voltage_to_percentage(uint16_t mvolts)
{
    uint8_t battery_level = 100;
    BR_LOG_DEBUG("battery voltage : %d ",mvolts);
    mvolts = mvolts > 3300 ? 3300 : mvolts;
    mvolts = mvolts < 1800 ? 1800 : mvolts;
    battery_level = (uint8_t)((mvolts - 1800)*100/1500);
    return battery_level;
}

/**
 * @}
 */

