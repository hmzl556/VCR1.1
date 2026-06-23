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
 * @file keyscan.c
 * @author Barrot Firmware Team
 * @version v1.0.1
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "keyscan.h"
#include "rcc.h"

/** @addtogroup BR1601x_StdPeriph_Driver
 * @{
 */

/**
 * @brief  Initializes the KEYSCAN peripheral according to the specified
 *         parameters in the KEYSCAN_InitType.
 * @param KEYSCAN_InitStruct pointer to a KEYSCAN_InitType structure that
 *         contains the configuration information for the KEYSCAN peripheral.
 * 根据指定的参数初始化KEYSCAN外设
 */
 void KEYSCAN_Init(KEYSCAN_InitType* KEYSCAN_InitStruct)
{
    KEYSCAN->KEYCR = 0;
        
    KEYSCAN->KEYCR =   (KEYSCAN_InitStruct->Mask<<KEY_MASK_POS)
                    | (KEYSCAN_InitStruct->Mode<<KEY_MODE_POS)
                    | (KEYSCAN_InitStruct->Dts<<KEY_DTS_POS)
                    | (KEYSCAN_InitStruct->Wts<<KEY_WTS_POS)
                    | (KEYSCAN_InitStruct->Int_en<<KEY_INT_EN_POS);
    //configer retention voltag
    *(uint32_t*)0x40007014 = 0x00000814; 
}


/**
 * @brief Enables or disables the KEYSCAN.
 * @param Cmd new state of KEYSCAN.This parameter can be: ENABLE or DISABLE.
 * 启用或禁用KEYSCAN（参数0/1）
 */
void KEYSCAN_Enable(FunctionalState Cmd)
{
    /* Check the parameters */
    assert_param(IS_FUNCTIONAL_STATE(Cmd));

    if (Cmd != DISABLE)
    {
        /* Enable the selected USART by setting the UE bit in the CTRL1 register */
        KEYSCAN->KEYCR |= (1<<KEYSCAN_EN_POS);
    }
    else
    {
        /* Disable the selected USART by clearing the UE bit in the CTRL1 register */
        KEYSCAN->KEYCR &= ~(1<<KEYSCAN_EN_POS);
    }
}

/**
 * @brief Clear the KEYDATA information of KEYSCAN peripheral.
 * 清除KEYSCAN外设的KEYDATA信息
 */
void KEYSCAN_InfoClear(void)
{
    KEYSCAN->KEYCR |= (1<<KEY_INFO_CLR_POS);
}

/**
 * @brief Start Software mode scan with KEYSCAN peripheral.
 * 通过KEYSCAN外设启动软件模式扫描
 */
void KEYSCAN_SoftwareStartScan(void)
{
    KEYSCAN->KEYCR |= (1<<KEY_SW_START_POS);
}


/**
 * @brief Clear the interrupt pending status of KEYSCAN peripheral.
 * 清除KEYSCAN外设的中断待处理状态
 */
void KEYSCAN_ClearInterrupt(void)
{
    KEYSCAN->KEYCR |= (1<<KEY_IRP_POS);
}

/**
 * @brief Get the interrupt status of KEYSCAN peripheral.
 * 获取KEYSCAN外设的中断状态
 */
FlagStatus KEYSCAN_GetInterruptState(void)
{
    if(KEYSCAN->KEYCR & (1<<KEY_IRP_POS))
    {
        return SET;
    }
    return RESET;
}

/**
 * @brief Read the key data.
 * @param key_data  An array with 5 word len to read out the key data.
 * 读取按键数据
 */
void KEYSCAN_ReadKeyData(uint32_t *key_data)
{
    key_data[0] = KEYSCAN->KEYDATA0;
    key_data[1] = KEYSCAN->KEYDATA1;
    key_data[2] = KEYSCAN->KEYDATA2;
    key_data[3] = KEYSCAN->KEYDATA3;
    key_data[4] = KEYSCAN->KEYDATA4;
}

/**
 * @}
 */

