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
 * @file br1601axx_it.c
 * @author Barrot Firmware Team
 * @version v1.0.1
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "br1601axx_it.h"
#include "main.h"

/** @addtogroup BR1601A0X_StdPeriph_Template
 * @{
 */

/******************************************************************************/
/*            Cortex-M0 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief  This function handles NMI exception.
 */
void NMI_Handler(void)
{
}

/**
 * @brief  This function handles Hard Fault exception.
 */
void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

/**
 * @brief  This function handles SVCall exception.
 */
void SVC_Handler(void)
{
}

/**
 * @brief  This function handles PendSV_Handler exception.
 */
void PendSV_Handler(void)
{
}

/**
 * @brief  This function handles SysTick Handler.
 */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 BR1601A0X Peripherals Interrupt Handlers                     */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_br1601axx.s).                                                 */
/******************************************************************************/

/**
 * @brief  This function handles TIM3 global interrupt request.
 */
void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM3, TIM_INT_CC1) != RESET)
    {
        /* Clear TIM3 Capture Compare1 interrupt pending bit*/
        TIM_ClrIntPendingBit(TIM3, TIM_INT_CC1);

        /* PB4 turnoff after 1000 ms */
        GPIO_ResetBits(GPIOB, GPIO_PIN_4);
    }
    else if (TIM_GetIntStatus(TIM3, TIM_INT_CC2) != RESET)
    {
        /* Clear TIM3 Capture Compare2 interrupt pending bit*/
        TIM_ClrIntPendingBit(TIM3, TIM_INT_CC2);

        /* PB5 turnoff after 500 ms */
        GPIO_ResetBits(GPIOB, GPIO_PIN_5);
    }
    else if (TIM_GetIntStatus(TIM3, TIM_INT_CC3) != RESET)
    {
        /* Clear TIM3 Capture Compare3 interrupt pending bit*/
        TIM_ClrIntPendingBit(TIM3, TIM_INT_CC3);

        /* PB6 turnoff after 250 ms */
        GPIO_ResetBits(GPIOB, GPIO_PIN_6);
    }
    else
    {
        /* Clear TIM3 Capture Compare4 interrupt pending bit*/
        TIM_ClrIntPendingBit(TIM3, TIM_INT_CC4);

        /* PB7 turnoff after 125 ms */
        GPIO_ResetBits(GPIOB, GPIO_PIN_7);
    }
}

/**
 * @}
 */
