#include "rcc.h"

static const uint8_t s_ApbPresTable[8]     = {0, 0, 0, 0, 1, 2, 3, 4};
static const uint8_t s_AhbPresTable[4]     = {0, 0, 1, 2};

/*-------------------------------------------------------------------
void RCC_DeInit(void)	RCC设备初始化 寄存器返回默认
-------------------------------------------------------------------*/
void RCC_DeInit(void)
{
    /* Set HSIEN bit */
    RCC->CTRL |= (uint32_t)0x00000001;

    /* Reset SCLKSW, AHBPRE, APB1PRES, APB2PRES, MCO and MCOPRES bits */
    RCC->CFG &= (uint32_t)0x21FF0000;

    /* Reset HSEEN, CLKSSEN and PLLEN bits */
    RCC->CTRL &= (uint32_t)0xFEF6FFFF;

    /* Reset PLLBP, PLLOUTEN bit */
    RCC->CTRL &= (uint32_t)0xFFFBFFFF;
  
    /* Reset PLLMULFCT, PLLPRE, PLLOUTDIV and PLLSRC bits */
    RCC->CFG &= (uint32_t)0xFE00FFFF;

    /* Reset CFG2 register */
    RCC->CFG2 = 0x00003800;

    /* Disable all interrupts and clear pending bits  */
    RCC->CLKINT = 0x00BF0000;
}

/*-------------------------------------------------------------------
void RCC_ConfigHse(uint32_t RCC_HSE)设置外部高速振荡 32M
			RCC_HSE = RCC_HSE_DISABLE    HSE oscillator OFF
								RCC_HSE_ENABLE     HSE oscillator ON 
-------------------------------------------------------------------*/
void RCC_ConfigHse(uint32_t RCC_HSE)
{
    /* Reset HSEON and HSEIOSEL bits before configuring the HSE ------------------*/
    /* Reset HSEON bit */
    RCC->CTRL &= ~RCC_CTRL_HSEEN;
    /* Configure HSE (RCC_HSE_DISABLE is already covered by the code section above) */
    switch (RCC_HSE)
    {
        case RCC_HSE_ENABLE:
            /* Set HSEEN bit */
            RCC->CTRL |= RCC_CTRL_HSEEN;
            break;
        default:
            break;
    }
}

/*-------------------------------------------------------------------
ErrorStatus RCC_WaitHseStable(void)等待HSE进入稳定
返回	 SUCCESS，ERROR
-------------------------------------------------------------------*/
ErrorStatus RCC_WaitHseStable(void)
{
    __IO uint32_t StartUpCounter = 0;
	
    ErrorStatus status           = ERROR;
    FlagStatus HSEStatus         = RESET;

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC_GetFlagStatus(RCC_CTRL_FLAG_HSERDF);
        StartUpCounter++;
    } while ((StartUpCounter != HSE_STARTUP_TIMEOUT) && (HSEStatus == RESET));

    if (RCC_GetFlagStatus(RCC_CTRL_FLAG_HSERDF) != RESET)
    {
        status = SUCCESS;
    }
    else
    {
        status = ERROR;
    }
    return (status);
}

/*-------------------------------------------------------------------
void RCC_ConfigHsi(uint32_t RCC_HSI)设置内部HSI振荡
RCC_HSI = RCC_HSI_DISABLE 	HSI oscillator OFF
					RCC_HSI_ENABLE 		HSI oscillator ON
-------------------------------------------------------------------*/
void RCC_ConfigHsi(uint32_t RCC_HSI)
{
    /* Reset HSIEN bit */
    RCC->CTRL &= ~RCC_CTRL_HSIEN;
    /* Configure HSI */
    switch (RCC_HSI)
    {
        case RCC_HSI_ENABLE:
            /* Set HSIEN bit */
            RCC->CTRL |= RCC_CTRL_HSIEN;
            break;

        default:
            break;
    }
}

/*-------------------------------------------------------------------
ErrorStatus RCC_WaitHsiStable(void)等待HSI进入稳定状态
返回	SUCCESS，ERROR
-------------------------------------------------------------------*/
ErrorStatus RCC_WaitHsiStable(void)
{
    __IO uint32_t StartUpCounter = 0;
    ErrorStatus status           = ERROR;
    FlagStatus HSIStatus         = RESET;

    /* Wait till HSI is ready and if Time out is reached exit */
    do
    {
        HSIStatus = RCC_GetFlagStatus(RCC_CTRL_FLAG_HSIRDF);
        StartUpCounter++;
    } while ((StartUpCounter != HSI_STARTUP_TIMEOUT) && (HSIStatus == RESET));

    if (RCC_GetFlagStatus(RCC_CTRL_FLAG_HSIRDF) != RESET)
    {
        status = SUCCESS;
    }
    else
    {
        status = ERROR;
    }
    return (status);
}


/*-------------------------------------------------------------------
void RCC_EnableHsi(FunctionalState Cmd)
		开启/禁止 HSI
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableHsi(FunctionalState Cmd)
{
   if(Cmd == ENABLE)
   {
       /* Set HSIEN bit */
       RCC->CTRL |= RCC_CTRL_HSIEN;
   }
   else
   {
       /* Reset HSIEN bit */
       RCC->CTRL &= ~RCC_CTRL_HSIEN;
   }
}


/*-------------------------------------------------------------------
void RCC_ConfigSysclk(uint32_t RCC_SYSCLKSource)
		配置系统时钟
		RCC_SYSCLKSource = 	RCC_SYSCLK_SRC_HSI HSI selected as system clock
												RCC_SYSCLK_SRC_HSE HSE selected as system clock
-------------------------------------------------------------------*/
void RCC_ConfigSysclk(uint32_t RCC_SYSCLKSource)
{
    uint32_t tmpregister = 0;
	
    tmpregister = RCC->CFG;
    /* Clear SW[2:0] bits */
    tmpregister &= RCC_CFG_SCLKSW;
    /* Set SW[2:0] bits according to RCC_SYSCLKSource value */
    tmpregister |= RCC_SYSCLKSource;
    /* Store the new value */
    RCC->CFG = tmpregister;
}

/*-------------------------------------------------------------------
uint32_t RCC_GetSysclkSrc(void)读取当前的时钟源选择	
返回	RCC_CFG_SCLKSTS_HSI 	HSI used as system clock
			RCC_CFG_SCLKSTS_HSE 	HSE used as system clock
-------------------------------------------------------------------*/
uint32_t RCC_GetSysclkSrc(void)
{
    return ((uint32_t)(RCC->CFG & RCC_CFG_SCLKSTS));
}

/*-------------------------------------------------------------------
void RCC_ConfigHclk(uint32_t RCC_SYSCLK)配置AHB HCLK时钟 高速设备时钟
		RCC_SYSCLK = 	RCC_SYSCLK_DIV1 	AHB clock = SYSCLK
									RCC_SYSCLK_DIV2 	AHB clock = SYSCLK/2
									RCC_SYSCLK_DIV4 	AHB clock = SYSCLK/4
-------------------------------------------------------------------*/
void RCC_ConfigHclk(uint32_t RCC_SYSCLK)
{
    uint32_t tmpregister = 0;
  
    tmpregister = RCC->CFG;
    /* Clear HPRE[3:0] bits */
    tmpregister &= ~RCC_CFG_AHBPRES;
    /* Set HPRE[3:0] bits according to RCC_SYSCLK value */
    tmpregister |= RCC_SYSCLK;
    /* Store the new value */
    RCC->CFG = tmpregister;
}

/*-------------------------------------------------------------------
void RCC_ConfigPclk1( uint32_t RCC_HCLK )配置低速设备时钟 APB PCLK1
		RCC_HCLK = 	RCC_HCLK_DIV1 	APB1 clock = HCLK
								RCC_HCLK_DIV2 	APB1 clock = HCLK/2
								RCC_HCLK_DIV4 	APB1 clock = HCLK/4
								RCC_HCLK_DIV8 	APB1 clock = HCLK/8
								RCC_HCLK_DIV16 	APB1 clock = HCLK/16
-------------------------------------------------------------------*/
void RCC_ConfigPclk1(uint32_t RCC_HCLK)
{
    uint32_t tmpregister = 0;
	
    tmpregister = RCC->CFG;
    /* Clear PPRE1[2:0] bits */
    tmpregister &= ~RCC_CFG_APB1PRES;
    /* Set PPRE1[2:0] bits according to RCC_HCLK value */
    tmpregister |= RCC_HCLK;
    /* Store the new value */
    RCC->CFG = tmpregister;
}

/*-------------------------------------------------------------------
void RCC_ConfigPclk2(uint32_t RCC_HCLK)配置APB2 PCLK2时钟
			RCC_HCLK = 	RCC_HCLK_DIV1 	APB2 clock = HCLK
									RCC_HCLK_DIV2 	APB2 clock = HCLK/2
									RCC_HCLK_DIV4 	APB2 clock = HCLK/4
									RCC_HCLK_DIV8 	APB2 clock = HCLK/8
									RCC_HCLK_DIV16 	APB2 clock = HCLK/16
-------------------------------------------------------------------*/
void RCC_ConfigPclk2(uint32_t RCC_HCLK)
{
    uint32_t tmpregister = 0;
	
    tmpregister = RCC->CFG;
    /* Clear PPRE2[2:0] bits */
    tmpregister &= ~RCC_CFG_APB2PRES;
    /* Set PPRE2[2:0] bits according to RCC_HCLK value */
    tmpregister |= RCC_HCLK << 3;
    /* Store the new value */
    RCC->CFG = tmpregister;
}

/*-------------------------------------------------------------------
void RCC_ConfigInt(uint8_t RccInt, FunctionalState Cmd)
			配置RCC中断
			RccInt = 	RCC_INT_LSIRDIF 	LSI ready interrupt
								RCC_INT_LSERDIF 	LSE ready interrupt
								RCC_INT_HSIRDIF 	HSI ready interrupt
								RCC_INT_HSERDIF 	HSE ready interrupt
								RCC_INT_PLLRDIF 	PLL ready interrupt
								RCC_INT_RAMCPIF 	RAMC parity interrupt
			Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_ConfigInt(uint8_t RccInt, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Perform Byte access to RCC_CLKINT bits to enable the selected interrupts */
        RCC->LSCTRL |= (((uint32_t)RccInt) << 8);
    }
    else
    {
        /* Perform Byte access to RCC_CLKINT bits to disable the selected interrupts */
        RCC->LSCTRL &= (~(((uint32_t)RccInt) << 8));
    }
}

/*-------------------------------------------------------------------
void RCC_ConfigTim1Clk(uint32_t RCC_TIM1CLKSource)
			配置Tim1的时钟源
			RCC_TIM1CLKSource = RCC_TIM1CLK_SRC_TIM1CLK
													RCC_TIM1CLK_SRC_SYSCLK
-------------------------------------------------------------------*/
void RCC_ConfigTim1Clk(uint32_t RCC_TIM1CLKSource)
{
    uint32_t tmpregister = 0;

    tmpregister = RCC->CFG2;
    /* Clear TIMCLK_SEL bits */
    tmpregister &= ~RCC_CFG2_TIMCLKSEL;
    /* Set TIMCLK_SEL bits according to RCC_TIM18CLKSource value */
    tmpregister |= RCC_TIM1CLKSource;

    /* Store the new value */
    RCC->CFG2 = tmpregister;
}


/*-------------------------------------------------------------------
void RCC_ConfigAdcClk(uint32_t RCC_ADCCLKSource)
		配置ADC的时钟源
		RCC_ADCCLKSource = 	RCC_ADCCLK_SRC_AUDIOPLL 
												RCC_ADCCLK_SRC_HSE_DIV8 
-------------------------------------------------------------------*/
void RCC_ConfigAdcClk(uint32_t RCC_ADCCLKSource)
{
    uint32_t tmpregister = 0;

    tmpregister = RCC->CFG2;
 
    tmpregister &= ~RCC_CFG2_ADCSEL;
    /* Set ADCSEL bits according to RCC_ADCCLKSource value */
    tmpregister |= RCC_ADCCLKSource;

    /* Store the new value */
    RCC->CFG2 = tmpregister;
}

/*-------------------------------------------------------------------
void RCC_ConfigLpuartClk(uint32_t RCC_LPUARTx, uint32_t RCC_LPUARTCLKSource)
		配置LPUART的时钟源
RCC_LPUARTCLKSource = RCC_LPUARTCLK_SRC_APB1   		APB1 clock selected as LPUART clock
											RCC_LPUARTCLK_SRC_LSI_LSE 	LSI_LSE selected as LPUART clock
-------------------------------------------------------------------*/
void RCC_ConfigLpuartClk(uint32_t RCC_LPUARTx, uint32_t RCC_LPUARTCLKSource)
{
    /* LPUART1 */
    if(RCC_LPUARTx == RCC_LPUART1CLK)
    {
        /* Clear the LPUART1 clock source */
        RCC->LSCTRL &= ~RCC_LSCTRL_LPUARTSEL;

        /* Select the LPTIM clock source */
        RCC->LSCTRL |= RCC_LPUARTCLKSource;
    }
}

/*-------------------------------------------------------------------
uint32_t RCC_GetLpuartClkSrc(uint32_t RCC_LPUARTx)
			读取当前LPUART使用的时钟源
返回	RCC_LPUARTCLK_SRC_APB1:   	APB1 used as LPUART clock
			RCC_LPUARTCLK_SRC_LSI_LSE: 	LSI_LSE used as LPUART clock
-------------------------------------------------------------------*/

uint32_t RCC_GetLpuartClkSrc(uint32_t RCC_LPUARTx)
{
    return ((uint32_t)(RCC->LSCTRL & (RCC_LSCTRL_LPUARTSEL_LSI_LSE)));
}

/*-------------------------------------------------------------------
void RCC_EnableLpuartClk(FunctionalState Cmd)
		许可/禁止LPUART的时钟 这个函数必须在RCC_ConfigLpuartClk函数后调用 
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableLpuartClk(FunctionalState Cmd)
{
    if(Cmd == ENABLE)
    {
        /* Set RTCEN bit */
        RCC->LSCTRL |= RCC_LSCTRL_LPUARTEN;
    }
    else
    {
        /* Reset RTCEN bit */
        RCC->LSCTRL &= ~RCC_LSCTRL_LPUARTEN;
    }
}

/*-------------------------------------------------------------------
void RCC_ConfigLse(uint8_t RCC_LSE)
		配置LSE
		RCC_LSE = 	RCC_LSE_DISABLE 	LSE oscillator OFF
								RCC_LSE_ENABLE 		LSE oscillator ON
								RCC_LSE_BYPASS 		LSE oscillator bypassed with external clock
-------------------------------------------------------------------*/
void RCC_ConfigLse(uint8_t RCC_LSE)
{
    /* Reset LSEEN and LSEBYP bits before configuring the LSE ------------------*/
    RCC->LSCTRL &= (~(RCC_LSCTRL_LSEEN | RCC_LSCTRL_LSEBP));
    /* Configure LSE (RCC_LSE_DISABLE is already covered by the code section above) */
    switch (RCC_LSE)
    {
        case RCC_LSE_ENABLE:
            /* Set LSEON bit */
            RCC->LSCTRL |= RCC_LSE_ENABLE;
            break;

        case RCC_LSE_BYPASS:
            /* Set LSEBYP and LSEON bits */
            RCC->LSCTRL |= (RCC_LSE_BYPASS | RCC_LSE_ENABLE);
            break;

        default:
            break;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableLsi(FunctionalState Cmd)
			许可/禁止LSI 	ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableLsi(FunctionalState Cmd)
{
    if(Cmd == ENABLE)
    {
        /* Set LSIEN bit */
        RCC->LSCTRL |= RCC_LSCTRL_LSIEN;
    }
    else
    {
        /* Reset PLLEN bit */
        RCC->LSCTRL &= ~RCC_LSCTRL_LSIEN;
    }
}

/*-------------------------------------------------------------------
void RCC_ConfigLSXSEL(uint32_t RCC_LS_Source)
		配置低速时钟的选择 LSE或者LSI
		RCC_LS_Source = RCC_LSCTRL_LSXSEL_LSE LSE selected as Low Speed Clock Source
										RCC_LSCTRL_LSXSEL_LSI	LSI selected as Low Speed Clock Source
-------------------------------------------------------------------*/
void RCC_ConfigLSXSEL(uint32_t RCC_LS_Source)
{
    /* Clear the RTC clock source */
    RCC->LSCTRL &= (~RCC_LSCTRL_LSXSEL);

    /* Select the RTC clock source */
    RCC->LSCTRL |= RCC_LS_Source;
}

/*-------------------------------------------------------------------
uint32_t RCC_GetRTCClkSrc(void)读取当前RTC时钟源
返回		RCC_RTCCLK_SRC_LSE		LSE used as RTC clock (RTCCLK)
				RCC_RTCCLK_SRC_LSI		LSI used as RTC clock (RTCCLK)
-------------------------------------------------------------------*/
uint32_t RCC_GetRTCClkSrc(void)
{
    return ((uint32_t)(RCC->LSCTRL & RCC_LSCTRL_LSXSEL));
}

/*-------------------------------------------------------------------
void RCC_EnableRtcClk(FunctionalState Cmd)允许RTC时钟
		该函数必须在RCC_ConfigRtcClk函数后调用  ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableRtcClk(FunctionalState Cmd)
{
    if(Cmd == ENABLE)
    {
        /* Set RTCEN bit */
        RCC->LSCTRL |= RCC_LSCTRL_RTCEN;
    }
    else
    {
        /* Reset RTCEN bit */
        RCC->LSCTRL &= ~RCC_LSCTRL_RTCEN;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableRTCReset(FunctionalState Cmd)
			强制/或者释放RTC的复位 ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableRTCReset(FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->LSCTRL |= RCC_LSCTRL_RTCRST;
    }
    else
    {
        RCC->LSCTRL &= ~RCC_LSCTRL_RTCRST;
    }
}

/*-------------------------------------------------------------------
void RCC_GetClocksFreqValue( RCC_ClocksType* RCC_Clocks )
			获取RCC时钟频率值，返回数据在时钟数据结构里面
			RCC_Clocks->SysclkFreq = HSI_VALUE、HSE_VALUE;
			RCC_Clocks->HclkFreq
			RCC_Clocks->Pclk1Freq
			RCC_Clocks->Pclk2Freq
			RCC_Clocks->AdcclkFreq
-------------------------------------------------------------------*/
void RCC_GetClocksFreqValue(RCC_ClocksType* RCC_Clocks)
{
    uint32_t tmp = 0, presc = 0;

    /* Get SYSCLK source -------------------------------------------------------*/
    tmp = RCC->CFG & RCC_CFG_SCLKSTS;

    switch (tmp)
    {
        case RCC_CFG_SCLKSTS_HSI: /* HSI used as system clock */
            RCC_Clocks->SysclkFreq = HSI_VALUE;
            break;
        case RCC_CFG_SCLKSTS_HSE: /* HSE used as system clock */
            RCC_Clocks->SysclkFreq = HSE_VALUE;
            break;
        default:
            RCC_Clocks->SysclkFreq = HSI_VALUE;
            break;
    }

    /* Compute HCLK, PCLK1, PCLK2 and ADCCLK clocks frequencies ----------------*/
    /* Get HCLK prescaler */
    tmp   = RCC->CFG & RCC_CFG_AHBPRES;
    tmp   = tmp >> 4;

    presc = s_AhbPresTable[tmp];
    /* HCLK clock frequency */
    RCC_Clocks->HclkFreq = RCC_Clocks->SysclkFreq >> presc;
    /* Get PCLK1 prescaler */
    tmp   = RCC->CFG & RCC_CFG_APB1PRES;
    tmp   = tmp >> 8;
    presc = s_ApbPresTable[tmp];
    /* PCLK1 clock frequency */
    RCC_Clocks->Pclk1Freq = RCC_Clocks->HclkFreq >> presc;

    /* Get PCLK2 prescaler */
    tmp   = RCC->CFG & RCC_CFG_APB2PRES;
    tmp   = tmp >> 11;
    presc = s_ApbPresTable[tmp];
    /* PCLK2 clock frequency */
    RCC_Clocks->Pclk2Freq = RCC_Clocks->HclkFreq >> presc;
    
    /* HSE_DIV8 is the ADC clock source */ 
    if(RCC->CFG2 & RCC_ADCCLK_SRC_HSE_DIV8 )
    {
        RCC_Clocks->AdcclkFreq = HSE_VALUE/8;
    }
    /* AUDIOPLL is the ADC clock source */ 
    else
    {            
        RCC_Clocks->AdcclkFreq = ADC_AUDIOPLLCLK_VALUE;
    }    
}

/*-------------------------------------------------------------------
void RCC_EnableAHBPeriphClk(uint32_t RCC_AHBPeriph, FunctionalState Cmd)
			允许/禁止 高速设备时钟 AHB设备
			RCC_AHBPeriph = 	RCC_AHB_PERIPH_DMA
												RCC_AHB_PERIPH_SRAM
												RCC_AHB_PERIPH_CRC
												RCC_AHB_PERIPH_ADC
			Cmd = ENABLE / DISABLE
-------------------------------------------------------------------*/
void RCC_EnableAHBPeriphClk(uint32_t RCC_AHBPeriph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->AHBPCLKEN |= RCC_AHBPeriph;
    }
    else
    {
        RCC->AHBPCLKEN &= ~RCC_AHBPeriph;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableAPB2PeriphClk(uint32_t RCC_APB2Periph, FunctionalState Cmd)
			许可/禁止在APB2上的设备时钟
			RCC_APB2Periph = 	RCC_APB2_PERIPH_AFIO
												RCC_APB2_PERIPH_GPIOA
												RCC_APB2_PERIPH_GPIOB,
												RCC_APB2_PERIPH_SPI1
												RCC_APB2_PERIPH_SPI2,
												RCC_APB2_PERIPH_TIM1, 
												RCC_APB2_PERIPH_USART1
			Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableAPB2PeriphClk(uint32_t RCC_APB2Periph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->APB2PCLKEN |= RCC_APB2Periph;
    }
    else
    {
        RCC->APB2PCLKEN &= ~RCC_APB2Periph;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableAPB1PeriphClk(uint32_t RCC_APB1Periph, FunctionalState Cmd)
		许可/禁止 APB1上的设备时钟
		RCC_APB1Periph = 	RCC_APB1_PERIPH_TIM3, 
											RCC_APB1_PERIPH_TIM6, 
											RCC_APB1_PERIPH_WWDG, 
											RCC_APB1_PERIPH_USART2,  
											RCC_APB1_PERIPH_I2C1, 
											RCC_APB1_PERIPH_PWR
		Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableAPB1PeriphClk(uint32_t RCC_APB1Periph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->APB1PCLKEN |= RCC_APB1Periph;
    }
    else
    {
        RCC->APB1PCLKEN &= ~RCC_APB1Periph;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableAHBPeriphReset(uint32_t RCC_AHBPeriph, FunctionalState Cmd)
		强制或者释放AHB时钟域的设备复位
		RCC_AHBPeriph = RCC_AHB_PERIPH_ADC
										RCC_AHB_PERIPH_IRC
		Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableAHBPeriphReset(uint32_t RCC_AHBPeriph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->AHBPRST |= RCC_AHBPeriph;
    }
    else
    {
        RCC->AHBPRST &= ~RCC_AHBPeriph;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableAPB2PeriphReset(uint32_t RCC_APB2Periph, FunctionalState Cmd)
		强制/释放APB2时钟域的设备复位
		RCC_APB2Periph	= RCC_APB2_PERIPH_AFIO
											RCC_APB2_PERIPH_GPIOA
											RCC_APB2_PERIPH_GPIOB,
											RCC_APB2_PERIPH_SPI1, 
											RCC_APB2_PERIPH_SPI2,          
											RCC_APB2_PERIPH_TIM1,  
											RCC_APB2_PERIPH_USART1
		Cmd	= ENABLE or DISABLE
-------------------------------------------------------------------*/
void RCC_EnableAPB2PeriphReset(uint32_t RCC_APB2Periph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->APB2PRST |= RCC_APB2Periph;
    }
    else
    {
        RCC->APB2PRST &= ~RCC_APB2Periph;
    }
}

/*-------------------------------------------------------------------
void RCC_EnableAPB1PeriphReset(uint32_t RCC_APB1Periph, FunctionalState Cmd)
		APB1时钟域的设备时钟许可
		RCC_APB1Periph = 	RCC_APB1_PERIPH_TIM3, 
											RCC_APB1_PERIPH_TIM6, 
											RCC_APB1_PERIPH_USART2, 
											RCC_APB1_PERIPH_I2C1, 
											RCC_APB1_PERIPH_PWR
		Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_EnableAPB1PeriphReset(uint32_t RCC_APB1Periph, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        RCC->APB1PRST |= RCC_APB1Periph;
    }
    else
    {
        RCC->APB1PRST &= ~RCC_APB1Periph;
    }
    
}

/*-------------------------------------------------------------------
void RCC_ConfigMco(uint8_t RCC_MCO)配置内部时钟输出
			RCC_MCO = 	RCC_MCO_NOCLK       No clock selected
									RCC_MCO_SYSCLK      System clock selected
									RCC_MCO_HSI         HSI oscillator clock selected
									RCC_MCO_HSE         HSE oscillator clock selected
									RCC_MCO_PLLCLK_PRES PLL clock prescaler
-------------------------------------------------------------------*/
void RCC_ConfigMco(uint8_t RCC_MCO)
{
    uint32_t tmpregister = 0;

    tmpregister = RCC->CFG;
    /* Clear MCO[2:0] bits */
    tmpregister &= (~RCC_CFG_MCO);
    /* Set MCO[2:0] bits according to RCC_MCO value */
    tmpregister |= ((uint32_t)(RCC_MCO << 25));

    /* Store the new value */
    RCC->CFG = tmpregister;
}

/*-------------------------------------------------------------------
FlagStatus RCC_GetFlagStatus(uint8_t RCC_FLAG)
			读取RCC的标志位	
		RCC_FLAG = 	RCC_CTRL_FLAG_HSIRDF        HSI oscillator clock ready
								RCC_CTRL_FLAG_HSERDF        HSE oscillator clock ready
								RCC_LSCTRL_FLAG_LSIRD       LSI oscillator clock ready
								RCC_LSCTRL_FLAG_LSERD       LSE oscillator clock ready
								RCC_CTRLSTS_FLAG_PINRSTF    Pin reset
								RCC_CTRLSTS_FLAG_PORRSTF    POR reset
								RCC_CTRLSTS_FLAG_SFTRSTF    Software reset
								RCC_CTRLSTS_FLAG_IWDGRSTF   Independent Watchdog reset
								RCC_CTRLSTS_FLAG_WWDGRSTF   Window Watchdog reset
		返回值 = SET or RESET).
-------------------------------------------------------------------*/
FlagStatus RCC_GetFlagStatus(uint8_t RCC_FLAG)
{
    uint32_t tmp         = 0;
    uint32_t statusreg   = 0;
    FlagStatus bitstatus = RESET;

    /* Get the RCC register index */
    tmp = RCC_FLAG >> 5;
    if (tmp == 1) /* The flag to check is in CTRL register */
    {
        statusreg = RCC->CTRL;
    }
    else if (tmp == 2) /* The flag to check is in BDCTRL register */
    {
        statusreg = RCC->LSCTRL;
    }
    else /* The flag to check is in CTRLSTS register */
    {
        statusreg = RCC->CTRLSTS;
    }

    /* Get the flag position */
    tmp = RCC_FLAG & RCC_FLAG_MASK;
    if ((statusreg & ((uint32_t)1 << tmp)) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    /* Return the flag status */
    return bitstatus;
}

/*-------------------------------------------------------------------
void RCC_ClrFlag(void)清除RCC域的标志位 主要指复位标志位
			RCC_CTRLSTS_FLAG_PINRSTF			
			RCC_CTRLSTS_FLAG_PORRSTF
			RCC_CTRLSTS_FLAG_SFTRSTF
			RCC_CTRLSTS_FLAG_IWDGRSTF 
			RCC_CTRLSTS_FLAG_WWDGRSTF
-------------------------------------------------------------------*/
void RCC_ClrFlag(void)
{
    /* Set RMVF bit to clear the reset flags */
    RCC->CTRLSTS |= RCC_CTRLSTS_RMRSTF;
    /* RMVF bit should be reset */
    RCC->CTRLSTS &= ~RCC_CTRLSTS_RMRSTF;
}

/*-------------------------------------------------------------------
INTStatus RCC_GetIntStatus(uint8_t RccInt)获取RCC域的中断标志位
			RccInt = 	RCC_INT_LSIRDIF 	LSI ready interrupt
								RCC_INT_LSERDIF 	LSE ready interrupt
								RCC_INT_HSIRDIF 	HSI ready interrupt
								RCC_INT_HSERDIF 	HSE ready interrupt
			返回 = SET or RESET).
-------------------------------------------------------------------*/
INTStatus RCC_GetIntStatus(uint8_t RccInt)
{
    INTStatus bitstatus = RESET;

    /* Check the status of the specified RCC interrupt */
    if ((RCC->CLKINT & RccInt) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    /* Return the RccInt status */
    return bitstatus;
}

/*-------------------------------------------------------------------
void RCC_ClrIntPendingBit(uint32_t RccClrInt)清除RCC域的中断标志
			RccClrInt = RCC_CLR_LSIRDIF 	Clear LSI ready interrupt flag
									RCC_CLR_LSERDIF 	Clear LSE ready interrupt flag
									RCC_CLR_HSIRDIF 	Clear HSI ready interrupt flag
									RCC_CLR_HSERDIF 	Clear HSE ready interrupt flag
-------------------------------------------------------------------*/
void RCC_ClrIntPendingBit(uint32_t RccClrInt)
{
   /* Software set this bit to clear INT flag. */
    RCC->CLKINT |= RccClrInt;
}

/*-------------------------------------------------------------------
void RCC_Enable_ADC_CLK_SRC_AUDIOPLL(FunctionalState Cmd)
		允许/禁止ADC的  音频采用锁相时钟源
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RCC_Enable_ADC_CLK_SRC_AUDIOPLL(FunctionalState Cmd)
{

    if(Cmd == ENABLE)
   {
       /* Set RAMCERRRST bit */
       RCC->CTRL |= RCC_CTRL_AUDIOPLLEN;
   }
   else
   {
       /* Reset RAMCERRRST bit */
       RCC->CTRL &= ~RCC_CTRL_AUDIOPLLEN;
   }
}

/*-------------------------------------------------------------------
void DBG_ConfigPeriph(uint32_t DBG_Periph, FunctionalState Cmd)
		设置在调试模式下的某些设备的运行许可/禁止
		DBG_Periph = 	DBG_IWDG_STOP 	Debug IWDG stopped when Core is halted
									DBG_WWDG_STOP 	Debug WWDG stopped when Core is halted
									DBG_TIM1_STOP 	TIM1 counter stopped when Core is halted
									DBG_TIM3_STOP 	TIM3 counter stopped when Core is halted
									DBG_I2C1SMBUS_TIMEOUT I2C1 SMBUS timeout mode stopped when Core is halted
									DBG_TIM6_STOP 	TIM6 counter stopped when Core is halted
		Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void DBG_ConfigPeriph(uint32_t DBG_Periph, FunctionalState Cmd)
{

    if (Cmd != DISABLE)
    {
        RCC->DBGMCU_CR |= DBG_Periph;
    }
    else
    {
        RCC->DBGMCU_CR &= ~DBG_Periph;
    }
}

/*-------------------------------------------------------------------
enum reset_reason RCC_GetSystemResetReason(void)获取系统复位原因
-------------------------------------------------------------------*/
enum reset_reason RCC_GetSystemResetReason(void)
{
    uint8_t mask = 0;
    enum reset_reason reason = RESET_FROM_PIN;
    for(uint8_t i = 4; i<8 ; i++)
    {
        mask = 0x01<<i;
        if(mask & RCC->CTRLSTS)
        {
            reason = (enum reset_reason)(i-3);
            break;
        }
    }
    RCC_ClrFlag();
    return reason;
}

