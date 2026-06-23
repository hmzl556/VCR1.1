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
 * @file adc.c
 * @author Barrot Firmware Team
 * @version v1.0.3
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "adc.h"
#include "rcc.h"

/** @addtogroup BR1601x_StdPeriph_Driver
 * @{
 */

/** @addtogroup ADC
 * @brief ADC driver modules
 * @{
 */

/** @addtogroup ADC_Private_TypesDefinitions
 * @{
 */

/**
 * @}
 */

/** @addtogroup ADC_Private_Defines
 * @{
 */


/**
 * @}
 */

/** @addtogroup ADC_Private_Macros
 * @{
 */

/**
 * @}
 */

/** @addtogroup ADC_Private_Variables
 * @{
 */

/**
 * @}
 */

/** @addtogroup ADC_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/** @addtogroup ADC_Private_Functions
 * @{
 */

/**
 * @brief  Deinitializes the ADCx peripheral registers to their default reset values.
 * @param ADCx can be ADC to select the ADC peripheral.
 */
void ADC_DeInit(ADC_Module* ADCx)
{
	/* 中文说明: 将ADC相关寄存器恢复到默认状态, 常用于重新初始化前清场。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));

	if (ADCx == ADC)
	{
		/* Enable ADC reset state */
		RCC_EnableAHBPeriphReset(RCC_AHB_PERIPH_ADC, ENABLE);
		/* Release ADC from reset state */
		RCC_EnableAHBPeriphReset(RCC_AHB_PERIPH_ADC, DISABLE);
		
		RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
		/* Reset ADC registers */
		ADC->FGA_CFG            = 0x20A08;  
		ADC->CTRL               = 0;
		ADC->SR                 = 0;   
		ADC->OVR_SAMP_CNT       = 0x1F;
		ADC->WDT_THRES          = 0x0FFC0000;
		ADC->VOICE_DET_CR       = 0;
		ADC->VOICE_ED_DWN_THRES = 0;
		ADC->VOICE_ED_THRES     = 0;
		ADC->VOICE_ED_UP_THRES  = 0;
		ADC->VOICE_ZCR_THRES    = 0x00006D2E;
	}
}


/**
 * @brief  Enables or disables the specified ADC peripheral.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the ADCx peripheral.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_Enable(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: ADC总使能开关, 控制CTRL寄存器的EN位。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));
	if (Cmd != DISABLE)
	{
		/* Enable GPADC */
		ADCx->CTRL |= ADC_CTRL_EN;
	}
	else
	{
		/* Disable GPADC */
		ADCx->CTRL &= ~ADC_CTRL_EN;
	}
}

/**
 * @brief  Enables or disables the specified ADC DMA request.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected ADC DMA transfer.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableDMA(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭ADC到DMA的数据请求, 不开DMA时DAT不会自动搬运到内存。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));
	if (Cmd != DISABLE)
	{
		/* Enable the selected ADC DMA request */
		ADCx->CTRL |= ADC_CTRL_DMA_MODE_EN;
	}
	else
	{
		/* Disable the selected ADC DMA request */
		ADCx->CTRL &= ~ADC_CTRL_DMA_MODE_EN;
	}
}

/**
 * @brief  Enables or disables the specified ADC TS request.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected ADC DMA transfer.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableTS(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭温度传感器采样通路(TS)。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));
	if (Cmd != DISABLE)
	{
		/* Enable the selected Temperature Sensor */
		ADCx->CTRL |= ADC_CTRL_TS_EN;
	}
	else
	{
		/* Disable the selected Temperature Sensor */
		ADCx->CTRL &= ~ADC_CTRL_TS_EN;
	}
}

/**
 * @brief  Enables or disables the specified Analog watchdog.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected Analog watchdog.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableAWD(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭模拟看门狗, 用于采样越界监测。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));
	if (Cmd != DISABLE)
	{
		/* Enable the selected Analog watchdog */
		ADCx->CTRL |= ADC_CTRL_AWD_EN;
	}
	else
	{
		/* Disable the selected Analog watchdog */
		ADCx->CTRL &= ~ADC_CTRL_AWD_EN;
	}
}

/**
 * @brief  Enables or disables the specified ADC interrupts.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  ADC_IT specifies the ADC interrupt sources to be enabled or disabled.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_INT_DONE GPADC Done Interrupt Enable
 *     @arg ADC_INT_AWD Analog watchdog interrupt mask
 *     @arg ADC_INT_PGARDY Used to enable the interrupt PGAREADY to interrupt processor when PGA fails
 * @param Cmd new state of the specified ADC interrupts.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_ConfigInt(ADC_Module* ADCx, uint16_t ADC_IT, FunctionalState Cmd)
{
	uint16_t itmask = 0;
	/* 中文说明: 配置ADC中断源使能位(完成/看门狗/PGA就绪等)。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));
	assert_param(IsAdcInt(ADC_IT));
	/* Get the ADC IT index */
	itmask = (uint16_t)ADC_IT;
	if (Cmd != DISABLE)
	{
		/* Enable the selected ADC interrupts */
		ADCx->CTRL |= itmask;
	}
	else
	{
		/* Disable the selected ADC interrupts */
		ADCx->CTRL &= (~(uint16_t)itmask);
	}
}

/**
 * @brief  Configures for the selected ADC channel
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  ADC_Channel the ADC channel to configure.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_CTRL_CH_0(MIC) ADC Channel0 selected
 *     @arg ADC_CTRL_CH_1(PB10) ADC Channel1 selected
 *     @arg ADC_CTRL_CH_2(PB9) ADC Channel2 selected
 *     @arg ADC_CTRL_CH_3(PB8) ADC Channel3 selected
 *     @arg ADC_CTRL_CH_4(PB7) ADC Channel4 selected
 *     @arg ADC_CTRL_CH_5(PB6) ADC Channel5 selected
 *     @arg ADC_CTRL_CH_6(VCC) ADC Channel6 selected
 *     @arg ADC_CTRL_CH_7(TS) ADC Channel7 selected
 */
void ADC_ConfigChannel(ADC_Module* ADCx, uint16_t Channel)
{
	uint16_t itmask = 0;
	/* 中文说明: 选择ADC采样输入通道, 会先清除旧通道位再写入新通道。 */
	
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IsAdcChannel(Channel));
	/* Get the old register value */
	itmask = ADCx->CTRL;
	/* Clear the old channel value */
	itmask &= ~ADC_CTRL_CH_SEL;
	/* Set the new channel value */
	itmask |= Channel;
	/* Store the new register value */
	ADCx->CTRL = itmask;
}

/**
 * @brief  Configures for the selected ADC Mode.
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the specified ADC Mode.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_ConfigContinuousMode(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 配置连续采样模式; ENABLE为连续转换, DISABLE为单次转换。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Set ADC_MODE bit according to ContinueConvEn value */
		ADCx->CTRL |= ADC_CTRL_MODE;
	}
	else
	{
		/* Set ADC_MODE bit according to SingleEn value */
		ADCx->CTRL &= ~ADC_CTRL_MODE;
	}
}

/**
 * @brief  Configures the high and low thresholds of the analog watchdog.	//中文说明: 设置模拟看门狗的高低阈值, 用于越界检测和告警触发。
 * @param  ADCx can be ADC to select the ADC peripheral.					//中文说明: 选择要配置的ADC外设, 目前只有一个ADC实例。
 * @param  HighThreshold the ADC analog watchdog High threshold value.		//中文说明: 模拟看门狗高阈值, 当采样值超过此值时会触发看门狗事件。
 *   This parameter must be a 10bit value.									//中文说明: 该参数必须是一个10位的值, 范围0~1023, 对应ADC的10位分辨率。
 * @param LowThreshold the ADC analog watchdog Low threshold value.
 *   This parameter must be a 10bit value.
 */
void ADC_ConfigAnalogWatchdogThresholds(ADC_Module* ADCx, uint32_t High_Threshold, uint32_t Low_Threshold)
{
	uint32_t tmp1 = 0;
	uint32_t tmp2 = 0;
	/* 中文说明: 设置模拟看门狗高/低阈值, 超阈时可配合AWD告警。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IsAdcValid(High_Threshold));
	assert_param(IsAdcValid(Low_Threshold));
	/* Get the old register value */
	tmp1 = ADCx->WDT_THRES ;							//得到当前看门狗阈值寄存器的值，准备修改其中的高低阈值字段
	/* Clear the old value */
	tmp1 &= ~(ADC_WDT_THRES_HT | ADC_WDT_THRES_LT);		//清除旧的高低阈值位，为设置新阈值做准备
	/* Calculate the mask to set */
	tmp2 = High_Threshold << 18;						//将高阈值左移18位，准备放入WDT_THRES寄存器的高阈值字段
		/* Set the analog watchdog threshold */
	tmp1 |= (tmp2 | Low_Threshold);						//将新的高阈值和低阈值组合成一个完整的寄存器值，准备写回WDT_THRES寄存器
	/* Store the new register value */
	ADCx->WDT_THRES = tmp1;								//将新的看门狗阈值写入寄存器，完成配置
}

/**
 * @brief  Configures the high and low thresholds of the zero cross.	//中文说明: 设置过零检测(ZCR)的高低阈值, 用于语音活动检测等应用。
 * @param  ADCx can be ADC to select the ADC peripheral.				//中文说明: 选择要配置的ADC外设, 目前只有一个ADC实例。
 * @param  HighThreshold the ADC zero cross High threshold value.		//中文说明: 过零检测高阈值, 当采样值从正向负跨越且超过此值时会触发过零事件。
 *   This parameter must be a 8bit value.								//中文说明: 该参数必须是一个8位的值, 范围0~255, 对应过零检测的阈值设置。
 * @param LowThreshold the ADC zero cross Low threshold value.			//中文说明: 过零检测低阈值, 当采样值从负向正跨越且超过此值时会触发过零事件。
 *   This parameter must be a 8bit value.								//中文说明: 该参数必须是一个8位的值, 范围0~255, 对应过零检测的阈值设置。
 */
void ADC_ConfigVoiceZcrThresholds(ADC_Module* ADCx, uint32_t High_Threshold, uint32_t Low_Threshold)
{
	uint32_t tmp1 = 0;
	uint32_t tmp2 = 0;
	/* 中文说明: 配置过零检测(ZCR)阈值, 常用于语音活动检测前端。 */

	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcVoiceZcrValid(High_Threshold));
	assert_param(IsAdcVoiceZcrValid(Low_Threshold));
	/* Get the old register value */
	tmp1 = ADCx->VOICE_ZCR_THRES;						//得到当前过零检测阈值寄存器的值，准备修改其中的高低阈值字段
	/* Clear the old value */
	tmp1 &= ~(ADC_VAD_ZCRD_HIGH_THRES | ADC_VAD_ZCRD_LOW_THRES);	//清除旧的高低阈值位，为设置新阈值做准备
	/* Calculate the mask to set */
	tmp2 = (High_Threshold&0xFF) << 8;					//将高阈值限制在8位范围内并左移8位，准备放入VOICE_ZCR_THRES寄存器的高阈值字段
	/* Set the zero cross threshold */
	tmp1 |= (tmp2 | (Low_Threshold&0xFF));				//将新的高阈值和低阈值组合成一个完整的寄存器值，准备写回VOICE_ZCR_THRES寄存器
	/* Store the new register value */
	ADCx->VOICE_ZCR_THRES = tmp1;						//将新的过零检测阈值写入寄存器，完成配置
}

/**
 * @brief  Configures the high and low thresholds of the energy detection.	//中文说明: 设置能量检测(ED)的高低阈值, 用于语音/静音判定等应用。
 * @param  ADCx can be ADC to select the ADC peripheral.					//中文说明: 选择要配置的ADC外设, 目前只有一个ADC实例。
 * @param  HighThreshold the ADC energy detection High threshold value.		//中文说明: 能量检测高阈值, 当采样能量超过此值时会触发能量事件。
 *   This parameter must be a 23bit value.									//中文说明: 该参数必须是一个23位的值, 范围0~8388607, 对应能量检测的阈值设置。
 * @param  LowThreshold the ADC energy detection Low threshold value.		//中文说明: 能量检测低阈值, 当采样能量低于此值时会触发能量事件。
 *   This parameter must be a 23bit value.									//中文说明: 该参数必须是一个23位的值, 范围0~8388607, 对应能量检测的阈值设置。
 */
void ADC_ConfigEDThreshold(ADC_Module* ADCx, uint32_t High_Threshold, uint32_t Low_Threshold)
{
	uint32_t tmp1 = 0;
	uint32_t tmp2 = 0;
	/* 中文说明: 配置能量检测(ED)高低阈值, 用于语音/静音判定。 */

	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcVoiceEDValid(High_Threshold));
	assert_param(IsAdcVoiceEDValid(Low_Threshold));
	/* Get the old register value */
	tmp1 = ADCx->VOICE_ED_UP_THRES;
	/* Get the old register value */
	tmp2 = ADCx->VOICE_ED_DWN_THRES;
	/* Clear the old value */
	tmp1 &= ~ADC_VOICE_ED_UP_THRES_GATE_MAX;
	/* Clear the old value */
	tmp2 &= ~ADC_VOICE_ED_DWN_THRES_GATE_MIN;
	/* Set the energy detection High threshold value */
	tmp1 |= High_Threshold&ADC_VOICE_ED_UP_THRES_GATE_MAX;
	/* Set the energy detection Low threshold value */
	tmp2 |= Low_Threshold&ADC_VOICE_ED_DWN_THRES_GATE_MIN;
	/* Store the new register value */
	ADCx->VOICE_ED_UP_THRES = tmp1;
	/* Store the new register value */
	ADCx->VOICE_ED_DWN_THRES = tmp2;
}

/**
 * @brief  Set the environment noise level value.							//中文说明: 设置环境噪声偏置, 用于ED门限自适应修正。适当调整此值可以优化语音活动检测的性能。
 * @param  ADCx can be ADC to select the ADC peripheral.					//中文说明: 选择要配置的ADC外设, 目前只有一个ADC实例。
 * @param  moffset the offset value for the selected environment noise level value.//中文说明: 环境噪声偏置值, 用于修正能量检测(ED)的门限, 以适应不同的环境噪声水平。
 *   This parameter must be a 23bit value.									//中文说明: 该参数必须是一个23位的值, 范围0~8388607, 对应环境噪声偏置的设置范围。0x800000
 */
void ADC_SetNoiseOffset(ADC_Module* ADCx, uint32_t moffset)
{
	uint32_t tmp1 = 0;
	/* 中文说明: 设置环境噪声偏置, 用于ED门限自适应修正。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcNoiseOffsetValid(moffset));
	/* Get the old register value */
	tmp1 = ADCx->VOICE_ED_THRES;
	/* Clear the old value */
	tmp1 &= ~ADC_VOICE_ED_THRES_MOFFSET;
	/* Set the environment noise level value */
	tmp1 |= moffset&ADC_VOICE_ED_THRES_MOFFSET;
	/* Store the new register value */
	ADCx->VOICE_ED_THRES = tmp1;
}

/**
 * @brief  Returns the last ADCx conversion result data for channel.	//中文说明: 读取当前ADC数据寄存器(DAT)的原始转换值, 范围0~1023对应10位分辨率。
 * @param  ADCx can be ADC to select the ADC peripheral.				//中文说明: 选择要读取的ADC外设, 目前只有一个ADC实例。
 * @return The Data conversion value.									//中文说明: 返回ADC当前的转换结果数据, 范围0~1023对应10位分辨率。
 */
uint16_t ADC_GetDat(ADC_Module* ADCx)
{
	/* 中文说明: 读取当前ADC数据寄存器(DAT)的原始转换值。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	/* Return the selected ADC conversion value */
	return (uint16_t)ADCx->DAT;
}

/**
 * @brief  Checks whether the specified ADC flag is set or not.			//中文说明: 查询ADC状态标志位是否置位, 如转换完成/看门狗事件/PGA就绪等。
 * @param  ADCx can be ADC to select the ADC peripheral.				//中文说明: 选择要查询的ADC外设, 目前只有一个ADC实例。
 * @param  ADC_FLAG specifies the flag to check.
 *   This parameter can be one of the following values:
 *     @arg ADC_FLAG_DONE GP-ADC conversion complete status				//中文说明: 转换完成标志, 当ADC完成一次转换时会置位, 可用于查询转换状态。
 *     @arg ADC_FLAG_AWD Analog watchdog flag							//中文说明: 模拟看门狗标志, 当采样值超过看门狗阈值时会置位, 可用于查询越界事件。
 *     @arg ADC_FLAG_PGARDY PGA fail status								//中文说明: PGA就绪标志, 当PGA增益调整完成时会置位, 可用于查询PGA状态。
 * @return The new state of ADC_FLAG (SET or RESET).
 */
FlagStatus ADC_GetFlagStatus(ADC_Module* ADCx, uint8_t ADC_FLAG)
{
	FlagStatus bitstatus = RESET;
	/* 中文说明: 查询ADC状态标志位是否置位。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IsAdcGetFlag(ADC_FLAG));
	/* Check the status of the specified ADC flag */
	if ((ADCx->SR & ADC_FLAG) != (uint8_t)RESET)
	{
		/* ADC_FLAG is set */
		bitstatus = SET;
	}
	else
	{
		/* ADC_FLAG is reset */
		bitstatus = RESET;
	}
	/* Return the ADC_FLAG status */
	return bitstatus;
}

/**
 * @brief  Clears the ADCx's pending flags.								//中文说明: 清除ADC状态标志位(写SR实现清标志)。注意某些标志位可能需要特殊的清除方式, 具体请参考数据手册。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  ADC_FLAG specifies the flag to clear.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_FLAG_DONE GP-ADC conversion complete status
 *     @arg ADC_FLAG_AWD Analog watchdog flag
 *     @arg ADC_FLAG_PGARDY PGA fail status
 */
void ADC_ClearFlag(ADC_Module* ADCx, uint8_t ADC_FLAG)
{
	/* 中文说明: 清除ADC状态标志位(写SR实现清标志)。 */
	/* Check the parameters */
	assert_param(IsAdcModule(ADCx));
	assert_param(IsAdcClrFlag(ADC_FLAG));
	/* Clear the selected ADC flags */
	ADCx->SR = ~(uint8_t)ADC_FLAG;
}

/**
 * @brief  Enables or disables the selected ADC zero cross.				//中文说明: 使能/关闭过零检测模块, 用于语音活动检测等应用。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected ADC zero cross.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableZeroCross(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭过零检测模块。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enable zero cross */
		ADCx->VOICE_DET_CR |= ADC_VOICE_DET_CR_ZCRD_EN;
	}
	else
	{
		/* Disable zero cross */
		ADCx->VOICE_DET_CR &= ~ADC_VOICE_DET_CR_ZCRD_EN;
	}
}

/**
 * @brief  Enables or disables the selected ADC energy .			//中文说明: 使能/关闭能量检测模块, 用于语音活动检测等应用。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected ADC energy.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableEnergy(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭能量检测模块。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enable energy */
		ADCx->VOICE_DET_CR |= ADC_VOICE_DET_CR_ED_EN;
	}
	else
	{
		/* Disable energy */
		ADCx->VOICE_DET_CR &= ~ADC_VOICE_DET_CR_ED_EN;
	}
}

/**
 * @brief  Enables or disables the selected bypass the digital filtering and ED/ZCRD features .		//中文说明: 使能/关闭数字滤波旁路, 直接连接输入到ED/ZCRD模块, 会影响检测性能和抗干扰能力。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected ADC bypass.
 *   This parameter can be: ENABLE or DISABLE.						//ENABLE=旁路(直通), DISABLE=使用滤波链路
 */
void ADC_EnableBypassFilter(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 数字滤波旁路控制; ENABLE=旁路(直通), DISABLE=使用滤波链路。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enbale bypass mode */
		ADCx->VOICE_DET_CR |= ADC_VOICE_DET_CR_FILTTER_BYP;
	}
	else
	{
		/* Disbale bypass mode */
		ADCx->VOICE_DET_CR &= ~ADC_VOICE_DET_CR_FILTTER_BYP;
	}
}

/**
 * @brief  Enables or disables the selected the ADC MIC bias.		//中文说明: 打开/关闭MIC偏置电源, AMIC前端采集前通常需要先开启。注意开启后可能需要一定的稳定时间。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected the ADC MIC bias.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableMICBIAS(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 打开/关闭MIC偏置电源, AMIC前端采集前通常需要先开启。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enbale  MIC bias*/
		ADCx->FGA_CFG |= ADC_PGA_CFG_MICBIAS_EN;
	}
	else
	{
		/* Disbale  MIC bias*/
		ADCx->FGA_CFG &= ~ADC_PGA_CFG_MICBIAS_EN;
	}
}

/**
 * @brief  Set the MIC bias output voltage.							//中文说明: 设置MIC偏置输出电压档位(1.6V~2.3V), 用于匹配麦克风规格。注意不同的麦克风可能需要不同的偏置电压, 过高或过低都可能导致性能下降。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  MIC_VoltageValue the value for the selected the MIC bias output voltage.
 *   This parameter can be any combination of the following values:
 *     @arg MICBIAS_1V6 
 *     @arg MICBIAS_1V7 
 *     @arg MICBIAS_1V8 
 *     @arg MICBIAS_1V9 
 *     @arg MICBIAS_2V0 
 *     @arg MICBIAS_2V1 
 *     @arg MICBIAS_2V2 
 *     @arg MICBIAS_2V3 
 */
void ADC_SetMICOutputVoltage(ADC_Module* ADCx, uint32_t MIC_VoltageValue)
{
	uint32_t tmp = 0;
	/* 中文说明: 设置MICBIAS输出电压档位(1.6V~2.3V), 用于匹配麦克风规格。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcMicbias(MIC_VoltageValue));
	
	/* Get the old register value */
	tmp = ADC->FGA_CFG;
	/* Clear the old value */
	tmp &= ~ADC_PGA_CFG_MICBIAS;
	/* Set the MIC bias output voltage */
	tmp |= MIC_VoltageValue;
	/* Store the new register value */
	ADC->FGA_CFG = tmp;
}

/**
 * @brief  Enables or disables the selected the Audio PGA fast charge control.//中文说明: 使能/关闭音频PGA快速充电控制, 影响启动瞬态与稳定时间。开启后PGA启动更快, 但可能会增加瞬态噪声; 关闭后PGA启动较慢, 但瞬态更平滑。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected the Audio PGA fast charge control.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnableAUDIOPGA(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 控制音频PGA快速充电路径, 影响启动瞬态与稳定时间。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enbale fast charge */
		ADCx->FGA_CFG |= ADC_PGA_CFG_INIT_ENA;
	}
	else
	{
		/* Disbale fast charge */
		ADCx->FGA_CFG &= ~ADC_PGA_CFG_INIT_ENA;
	}
}

/**
 * @brief  Set the Audio PGA drive signal amplitud.				//中文说明: 设置音频PGA驱动幅度档位, 影响PGA的输出摆幅和线性范围。适当调整此值可以优化音频信号的质量和动态范围。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  AMP_Value the value for the selected the Audio PGA drive signal amplitud.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_PGA_CFG_PEAK_0 
 *     @arg ADC_PGA_CFG_PEAK_1 
 *     @arg ADC_PGA_CFG_PEAK_2
 *     @arg ADC_PGA_CFG_PEAK_3 
 */
void ADC_SetAUDIOAmplitude(ADC_Module* ADCx, uint32_t AMP_Value)
{
	uint32_t tmp = 0;
	/* 中文说明: 设置音频PGA驱动幅度档位。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcPgaPeak(AMP_Value));
	
	/* Get the old register value */
	tmp = ADC->FGA_CFG;
	/* Clear the old value */
	tmp &= ~ADC_PGA_CFG_PEAK;
	/* Set the Audio PGA drive signal amplitud */
	tmp |= AMP_Value;
	/* Store the new register value */
	ADC->FGA_CFG = tmp;
}

/**
 * @brief  Set the Audio PGA drive current.						//中文说明: 设置音频PGA驱动电流档位, 影响PGA的线性范围和输出能力。适当调整此值可以优化音频信号的质量和动态范围。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Drive_Value the value for the selected the Audio PGA drive current.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_PGA_CFG_DRIVE_0 
 *     @arg ADC_PGA_CFG_DRIVE_1 
 *     @arg ADC_PGA_CFG_DRIVE_2
 *     @arg ADC_PGA_CFG_DRIVE_3 
 */
void ADC_SetAUDIOCurrent(ADC_Module* ADCx, uint32_t Drive_Value)
{
	uint32_t tmp = 0;
	/* 中文说明: 设置音频PGA驱动电流档位。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcPgaDriver(Drive_Value));
	
	/* Get the old register value */
	tmp = ADC->FGA_CFG;
	/* Clear the old value */
	tmp &= ~ADC_PGA_CFG_DRIVE;
	/* Set the Audio PGA drive current */
	tmp |= Drive_Value;
	/* Store the new register value */
	ADC->FGA_CFG = tmp;
}

/**
 * @brief  Enables or disables the selected the Audio PGA.		//中文说明: 使能/关闭音频PGA前级放大器, 用于AMIC输入的增益调整。开启后PGA会对输入信号进行放大, 适合较弱的麦克风信号; 关闭后PGA不工作, 适合较强的麦克风信号或直连传感器输出。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cmd new state of the selected the Audio PGA.
 *   This parameter can be: ENABLE or DISABLE.
 */
void ADC_EnablePGA(ADC_Module* ADCx, FunctionalState Cmd)
{
	/* 中文说明: 使能/关闭音频PGA前级放大器。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IS_FUNCTIONAL_STATE(Cmd));

	if (Cmd != DISABLE)
	{
		/* Enable Audio PGA */
		ADCx->FGA_CFG |= ADC_PGA_CFG_EN;
	}
	else
	{
		/* Disable Audio PGA */
		ADCx->FGA_CFG &= ~ADC_PGA_CFG_EN;
	}
}

/**
 * @brief  Set the Audio PGA gain configuration.				//中文说明: 设置音频PGA增益配置, 影响输入信号的放大倍数。适当调整此值可以优化音频信号的质量和动态范围。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Gain_Value the value for the selected the Audio PGA gain configuration.
 *   This parameter can be any combination of the following values:
 *     @arg ADC_PGA_CFG_GAIN_0dB	对应1倍
 *     @arg ADC_PGA_CFG_GAIN_6dB 	对应2倍
 *     @arg ADC_PGA_CFG_GAIN_12dB	对应4倍
 *     @arg ADC_PGA_CFG_GAIN_18dB 	对应8倍
 *     @arg ADC_PGA_CFG_GAIN_24dB 	对应16倍
 *     @arg ADC_PGA_CFG_GAIN_30dB 	对应32倍
 *     @arg ADC_PGA_CFG_GAIN_36dB	对应64倍
 *     @arg ADC_PGA_CFG_GAIN_42dB	对应128倍
 */
void ADC_SetPGAGain(ADC_Module* ADCx, uint32_t Gain_Value)
{
	uint32_t tmp = 0;
	/* 中文说明: 设置PGA增益档位(0dB~42dB), 增益越高人声和底噪都会同时抬升。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	assert_param(IsAdcPgaGain(Gain_Value));
	
	/* Get the old register value */
	tmp = ADC->FGA_CFG;
	/* Clear the old value */
	tmp &= ~ADC_PGA_CFG_GAIN;
	/* Set the MIC bias output voltage */
	tmp |= Gain_Value;
	/* Store the new register value */
	ADC->FGA_CFG = tmp;
}

/**
 * @brief  Set the Over Sampling Counter.							//中文说明: 设置过采样计数器, 实际倍率通常为2^Cnt。过采样可以提高分辨率和信噪比, 但会增加转换时间和功耗。
 * @param  ADCx can be ADC to select the ADC peripheral.
 * @param  Cnt the value for the selected the Over Sampling Counter.
 *   This parameter must be a 5bit value.
 */
void ADC_SetOverSampleCounter(ADC_Module* ADCx, uint32_t Cnt)
{
	uint32_t tmp = 0;
	/* 中文说明: 设置过采样计数器, 实际倍率通常为2^Cnt。 */
	/* Check the parameters */
	assert_param(IsAdcDmaModule(ADCx));
	
	/* Get the old register value */
	tmp = ADC->OVR_SAMP_CNT;
	/* Clear the old value */
	tmp &= ~ADC_OS_CNT_LD_CNT;
	/* Set the MIC bias output voltage */
	tmp |= Cnt&ADC_OS_CNT_LD_CNT;
	/* Store the new register value */
	ADC->OVR_SAMP_CNT = tmp;
}
	
/**
 * @brief  Conver adc value to voltage									//中文说明: 将10bit ADC码值按通道标定参数换算为毫伏(mV)。注意不同的输入通道可能有不同的标定参数, 需要根据实际情况选择正确的通道参数进行换算。
 * @param  adc_value the adv value want to conver to voltage.
 * @param  Channel the adc value is from which ADC channel(1 to 6) 
 * @return The voltage in mV unit after conver.
 */
uint32_t ADC_ConverValueToVoltage(uint16_t adc_value, uint16_t channel)
{
	/* 中文说明: 将10bit ADC码值按通道标定参数换算为毫伏(mV)。 */

	uint16_t value_3400mv;
	uint16_t value_600mv;
	uint16_t value_850mv;
	uint16_t value_150mv;
	int32_t voltage_mV = 0;
	/* get trim value */
	trim_stored_t *p_trim = SystemTrimValueGet();
	/* Check the parameters */
	assert_param(IsAdcValid(adc_value));
	assert_param(IsAdcChannel(channel));

	if(p_trim == 0 || 
	   p_trim->rc_gpadc_value_3400mv == 0 || 
	   p_trim->rc_gpadc_value_600mv == 0)
	{
		/* default value if chip without trim value*/
		value_3400mv = value_850mv = 999; // 822;
		value_600mv  = value_150mv = 61; //177;
	}
	else
	{
		value_3400mv = p_trim->rc_gpadc_value_3400mv;
		value_600mv  = p_trim->rc_gpadc_value_600mv;
		value_850mv  = p_trim->rc_gpadc_value_3400mv;
		value_150mv  = p_trim->rc_gpadc_value_600mv;
	}

	if( channel == ADC_CTRL_CH_1 || channel == ADC_CTRL_CH_2 )
	{
		/* CH1 and CH2 150mV to 850mV */
		voltage_mV =(((int32_t)adc_value - value_150mv) * 700)/(value_850mv - value_150mv) + 150;
		
		//make sure the voltage valid(0~1000mV)
		if(voltage_mV < 0) 
		{
			voltage_mV = 0;
		}
		else if(voltage_mV > 1000)
		{
			voltage_mV = 1000;
		}
	}
	else if(channel == ADC_CTRL_CH_3 || channel == ADC_CTRL_CH_4|| channel == ADC_CTRL_CH_5 || channel == ADC_CTRL_CH_6)
	{
		/* CH3, CH4 and CH5 600mV to 3400mV */
		voltage_mV =(((int32_t)adc_value - value_600mv)* 2800)/(value_3400mv - value_600mv) + 600;
		
		//make sure the voltage valid(0~3600mV)
		if(voltage_mV < 0) 
		{
			voltage_mV = 0;
		}
		else if(voltage_mV > 3600)
		{
			voltage_mV = 3600;
		}
	}
	return  voltage_mV;
}

/**
 * @brief  Conver 16bit adc value to voltage									//中文说明: 将16bit有符号码值换算为毫伏(mV), 并叠加外部offset补偿。
 * @param  adc_value the adv value want to conver to voltage.
 * @param  Channel the adc value is from which ADC channel(1 to 6) 
 * @param  offset the voltage offset of 16bit ADC in mV. it can get from VCC channel.
 * @return The voltage in mV unit after conver.
 */
uint32_t ADC16Bit_ConverValueToVoltage(int16_t value_16bit, uint16_t channel,int16_t offset)
{
	/* 中文说明: 将16bit有符号码值换算为毫伏(mV), 并叠加外部offset补偿。 */
	uint16_t value_3400mv;
	uint16_t value_600mv;
	uint16_t value_850mv;
	uint16_t value_150mv;
	int32_t voltage_mV = 0;
	int32_t adc_value = value_16bit+0x7fff;
	/* get trim value */
	trim_stored_t *p_trim = SystemTrimValueGet();
	
	if(p_trim == 0 || p_trim->rc_gpadc_value_3400mv == 0)
	{
		return 0;
	}
	else
	{
		value_850mv = value_3400mv = p_trim->rc_gpadc_value_3400mv<<6;
		value_150mv = value_600mv =  p_trim->rc_gpadc_value_600mv<<6;
	}
	if( channel == ADC_CTRL_CH_1 || channel == ADC_CTRL_CH_2 )
	{
		/* CH1 and CH2 150mV to 850mV */
		voltage_mV =((adc_value - value_150mv) * 700)/(value_850mv - value_150mv) + 150 + (offset/4);
		
		//make sure the voltage valid(0~1000mV)
		if(voltage_mV < 0) 
		{
			voltage_mV = 0;
		}
		else if(voltage_mV > 1000)
		{
			voltage_mV = 1000;
		}
	}
	else if(channel == ADC_CTRL_CH_3 || channel == ADC_CTRL_CH_4|| channel == ADC_CTRL_CH_5 || channel == ADC_CTRL_CH_6)
	{
		/* CH3, CH4 and CH5 600mV to 3400mV */
		voltage_mV =((adc_value - value_600mv)* 2800)/(value_3400mv - value_600mv) + 600 +offset;
		
		//make sure the voltage valid(0~3600mV)
		if(voltage_mV < 0) 
		{
			voltage_mV = 0;
		}
		else if(voltage_mV > 3600)
		{
			voltage_mV = 3600;
		}
	}
	return  voltage_mV;
}

/**
 * @brief  Conver adc value to temperature in centigrade				//中文说明: 将10bit ADC码值按温度传感器标定参数换算为摄氏度。注意温度传感器通常需要特殊的标定参数, 需要根据实际情况选择正确的参数进行换算。
 * @param  adc_value the adv value want to conver to voltage.
 * @param  Channel the adc value is from which ADC channel(1 to 7) 
 * @return The voltage in mA unit after conver.
 */
float ADC_ConverValueToTemperature(uint16_t adc_value)
{
	/* 中文说明: 根据温度通道码值和trim参数估算摄氏温度。 */
	float Temperature;
	float adc_ts_trim;
	/* get trim value */
	trim_stored_t *p_trim = SystemTrimValueGet();
	/* Check the parameters */
	assert_param(IsAdcValid(adc_value));

	if(p_trim == 0 || p_trim->rc_adc_ts_25c == 0)
	{
		/* default value if chip without trim value*/
		adc_ts_trim  = 750; // 660;
	}
	else{
		adc_ts_trim = p_trim->rc_adc_ts_25c;
	}
	
	Temperature = ((adc_ts_trim - adc_value)/(2.14)) + 25;
	return Temperature;
}



/**
 * @}
 */

/**
 * @}S
 */

/**
 * @}
 */
