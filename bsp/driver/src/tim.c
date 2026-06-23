#include "tim.h"
#include "rcc.h"

/* ---------------------- TIM registers bit mask ------------------------ */
#define SMCTRL_ETR_MASK   ((uint16_t)0x00FF)
#define CAPCMPMOD_OFFSET  ((uint16_t)0x0018)
#define CAPCMPEN_CCE_SET  ((uint16_t)0x0001)
#define CAPCMPEN_CCNE_SET ((uint16_t)0x0004)

static void ConfigTI1(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter);
static void ConfigTI2(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter);
static void ConfigTI3(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter);
static void ConfigTI4(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter);

/*-------------------------------------------------------------------
void TIM_DeInit(TIM_Module* TIMx) 定时器的设备初始化,寄存器回到默认值
		TIMx = TIM1 TIM3 TIM6 
-------------------------------------------------------------------*/
void TIM_DeInit(TIM_Module* TIMx)
{
    if (TIMx == TIM1)
    {
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_TIM1, ENABLE);
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_TIM1, DISABLE);
    }
    else if (TIMx == TIM3)
    {
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_TIM3, ENABLE);
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_TIM3, DISABLE);
    }
    else if (TIMx == TIM6)
    {
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_TIM6, ENABLE);
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_TIM6, DISABLE);
    }
}

/*-------------------------------------------------------------------
void TIM_InitTimeBase(TIM_Module* TIMx, TIM_TimeBaseInitType* TIM_TimeBaseInitStruct)
		定时器的基础配置
		TIMx = TIM1 TIM3 TIM6
		TIM_TimeBaseInitStruct = 基础配置数据结构
-------------------------------------------------------------------*/
void TIM_InitTimeBase(TIM_Module* TIMx, TIM_TimeBaseInitType* TIM_TimeBaseInitStruct)
{
    uint32_t tmpcr1 = 0;

    tmpcr1 = TIMx->CTRL1;

    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        /* Select the Counter Mode */
        tmpcr1 &= (uint32_t)(~((uint32_t)(TIM_CTRL1_DIR | TIM_CTRL1_CAMSEL)));
        tmpcr1 |= (uint32_t)TIM_TimeBaseInitStruct->CntMode;
    }

    if (TIMx != TIM6)
    {
        /* Set the clock division */
        tmpcr1 &= (uint32_t)(~((uint32_t)TIM_CTRL1_CLKD));
        tmpcr1 |= (uint32_t)TIM_TimeBaseInitStruct->ClkDiv;
    }

    TIMx->CTRL1 = tmpcr1;

    /* Set the Autoreload value */
    TIMx->AR = TIM_TimeBaseInitStruct->Period;

    /* Set the Prescaler value */
    TIMx->PSC = TIM_TimeBaseInitStruct->Prescaler;

    if (TIMx == TIM1)
    {
        /* Set the Repetition Counter value */
        TIMx->REPCNT = TIM_TimeBaseInitStruct->RepetCnt;
    }

    /* Generate an update event to reload the Prescaler and the Repetition counter
       values immediately */
    TIMx->EVTGEN = TIM_PSC_RELOAD_MODE_IMMEDIATE;

    /*channel input from comp or iom*/
    tmpcr1 = TIMx->CTRL1;
    if ((TIMx == TIM1)|| (TIMx == TIM3))
    {
        if (TIM_TimeBaseInitStruct->CapCh1FromCompEn)
            tmpcr1 |= (0x01L << 11);
        else
            tmpcr1 &= ~(0x01L << 11);
    }
    if (TIMx == TIM3) 
    {
        if (TIM_TimeBaseInitStruct->CapCh2FromCompEn)
            tmpcr1 |= (0x01L << 12);
        else
            tmpcr1 &= ~(0x01L << 12);
        if (TIM_TimeBaseInitStruct->CapCh3FromCompEn)
            tmpcr1 |= (0x01L << 13);
        else
            tmpcr1 &= ~(0x01L << 13);
    }
    /*etr input from comp or iom*/
    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        if (TIM_TimeBaseInitStruct->CapEtrClrFromCompEn)
            tmpcr1 |= (0x01L << 15);
        else
            tmpcr1 &= ~(0x01L << 15);
    }
    TIMx->CTRL1 = tmpcr1;
    /*sel etr from iom or tsc*/
    tmpcr1 = TIMx->CTRL2;
    
    TIMx->CTRL2 = tmpcr1;
}

/*-------------------------------------------------------------------
void TIM_InitOc1(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
		定时器的输出比较通道1配置
		TIMx = TIM1 TIM3
		TIM_OCInitStruct 为初始化所需要的数据结构
-------------------------------------------------------------------*/
void TIM_InitOc1(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
{
    uint16_t tmpccmrx = 0;
    uint32_t tmpccer = 0, tmpcr2 = 0;

    /* Disable the Channel 1: Reset the CC1E Bit */
    TIMx->CCEN &= (uint32_t)(~(uint32_t)TIM_CCEN_CC1EN);
    /* Get the TIMx CCEN register value */
    tmpccer = TIMx->CCEN;
    /* Get the TIMx CTRL2 register value */
    tmpcr2 = TIMx->CTRL2;

    /* Get the TIMx CCMOD1 register value */
    tmpccmrx = TIMx->CCMOD1;

    /* Reset the Output Compare Mode Bits */
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD1_OC1M));
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD1_CC1SEL));

    /* Select the Output Compare Mode */
    tmpccmrx |= TIM_OCInitStruct->OcMode;

    /* Reset the Output Polarity level */
    tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC1P));
    /* Set the Output Compare Polarity */
    tmpccer |= TIM_OCInitStruct->OcPolarity;

    /* Set the Output State */
    tmpccer |= TIM_OCInitStruct->OutputState;

    if (TIMx == TIM1)
    {
        /* Reset the Output N Polarity level */
        tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC1NP));
        /* Set the Output N Polarity */
        tmpccer |= TIM_OCInitStruct->OcNPolarity;

        /* Reset the Output N State */
        tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC1NEN));
        /* Set the Output N State */
        tmpccer |= TIM_OCInitStruct->OutputNState;

        /* Reset the Output Compare and Output Compare N IDLE State */
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI1));
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI1N));

        /* Set the Output Idle state */
        tmpcr2 |= TIM_OCInitStruct->OcIdleState;
        /* Set the Output N Idle state */
        tmpcr2 |= TIM_OCInitStruct->OcNIdleState;
    }
    /* Write to TIMx CTRL2 */
    TIMx->CTRL2 = tmpcr2;

    /* Write to TIMx CCMOD1 */
    TIMx->CCMOD1 = tmpccmrx;

    /* Set the Capture Compare Register value */
    TIMx->CCDAT1 = TIM_OCInitStruct->Pulse;

    /* Write to TIMx CCEN */
    TIMx->CCEN = tmpccer;
}

/*-------------------------------------------------------------------
void TIM_InitOc2(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
		定时器的输出比较通道2设置
		TIMx = TIM1 TIM3
		TIM_OCInitStruct = 配置所需要的数据结构
-------------------------------------------------------------------*/
void TIM_InitOc2(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
{
    uint16_t tmpccmrx = 0;
    uint32_t tmpccer = 0, tmpcr2 = 0;

    /* Disable the Channel 2: Reset the CC2E Bit */
    TIMx->CCEN &= (uint32_t)(~((uint32_t)TIM_CCEN_CC2EN));

    /* Get the TIMx CCEN register value */
    tmpccer = TIMx->CCEN;
    /* Get the TIMx CTRL2 register value */
    tmpcr2 = TIMx->CTRL2;

    /* Get the TIMx CCMOD1 register value */
    tmpccmrx = TIMx->CCMOD1;

    /* Reset the Output Compare mode and Capture/Compare selection Bits */
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD1_OC2M));
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD1_CC2SEL));

    /* Select the Output Compare Mode */
    tmpccmrx |= (uint16_t)(TIM_OCInitStruct->OcMode << 8);

    /* Reset the Output Polarity level */
    tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC2P));
    /* Set the Output Compare Polarity */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OcPolarity << 4);

    /* Set the Output State */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OutputState << 4);

    if (TIMx == TIM1) 
    {
        /* Reset the Output N Polarity level */
        tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC2NP));
        /* Set the Output N Polarity */
        tmpccer |= (uint32_t)(TIM_OCInitStruct->OcNPolarity << 4);

        /* Reset the Output N State */
        tmpccer &= (uint32_t)(~((uint16_t)TIM_CCEN_CC2NEN));
        /* Set the Output N State */
        tmpccer |= (uint32_t)(TIM_OCInitStruct->OutputNState << 4);

        /* Reset the Output Compare and Output Compare N IDLE State */
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI2));
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI2N));

        /* Set the Output Idle state */
        tmpcr2 |= (uint32_t)(TIM_OCInitStruct->OcIdleState << 2);
        /* Set the Output N Idle state */
        tmpcr2 |= (uint32_t)(TIM_OCInitStruct->OcNIdleState << 2);
    }
    /* Write to TIMx CTRL2 */
    TIMx->CTRL2 = tmpcr2;

    /* Write to TIMx CCMOD1 */
    TIMx->CCMOD1 = tmpccmrx;

    /* Set the Capture Compare Register value */
    TIMx->CCDAT2 = TIM_OCInitStruct->Pulse;

    /* Write to TIMx CCEN */
    TIMx->CCEN = tmpccer;
}

/*-------------------------------------------------------------------
void TIM_InitOc3(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
		定时器输出比较通道3的配置
		TIMx = TIM1 TIM3
		TIM_OCInitStruct = 配置所需要的数据结构
-------------------------------------------------------------------*/
void TIM_InitOc3(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
{
    uint16_t tmpccmrx = 0;
    uint32_t tmpccer = 0, tmpcr2 = 0;

    /* Disable the Channel 2: Reset the CC2E Bit */
    TIMx->CCEN &= (uint32_t)(~((uint32_t)TIM_CCEN_CC3EN));

    /* Get the TIMx CCEN register value */
    tmpccer = TIMx->CCEN;
    /* Get the TIMx CTRL2 register value */
    tmpcr2 = TIMx->CTRL2;

    /* Get the TIMx CCMOD2 register value */
    tmpccmrx = TIMx->CCMOD2;

    /* Reset the Output Compare mode and Capture/Compare selection Bits */
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD2_OC3MD));
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD2_CC3SEL));
    /* Select the Output Compare Mode */
    tmpccmrx |= TIM_OCInitStruct->OcMode;

    /* Reset the Output Polarity level */
    tmpccer &= (uint32_t)(~((uint16_t)TIM_CCEN_CC3P));
    /* Set the Output Compare Polarity */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OcPolarity << 8);

    /* Set the Output State */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OutputState << 8);

    if (TIMx == TIM1)
    {
        /* Reset the Output N Polarity level */
        tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC3NP));
        /* Set the Output N Polarity */
        tmpccer |= (uint32_t)(TIM_OCInitStruct->OcNPolarity << 8);
        /* Reset the Output N State */
        tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC3NEN));

        /* Set the Output N State */
        tmpccer |= (uint32_t)(TIM_OCInitStruct->OutputNState << 8);
        /* Reset the Output Compare and Output Compare N IDLE State */
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI3));
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI3N));
        /* Set the Output Idle state */
        tmpcr2 |= (uint32_t)(TIM_OCInitStruct->OcIdleState << 4);
        /* Set the Output N Idle state */
        tmpcr2 |= (uint32_t)(TIM_OCInitStruct->OcNIdleState << 4);
    }
    /* Write to TIMx CTRL2 */
    TIMx->CTRL2 = tmpcr2;

    /* Write to TIMx CCMOD2 */
    TIMx->CCMOD2 = tmpccmrx;

    /* Set the Capture Compare Register value */
    TIMx->CCDAT3 = TIM_OCInitStruct->Pulse;

    /* Write to TIMx CCEN */
    TIMx->CCEN = tmpccer;
}

/*-------------------------------------------------------------------
void TIM_InitOc4(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
		定时器输出比较通道4的配置
		TIMx = TIM1 TIM3
		TIM_OCInitStruct = 配置所需要的数据结构
-------------------------------------------------------------------*/
void TIM_InitOc4(TIM_Module* TIMx, OCInitType* TIM_OCInitStruct)
{
    uint16_t tmpccmrx = 0;
    uint32_t tmpccer = 0, tmpcr2 = 0;

    /* Disable the Channel 2: Reset the CC4E Bit */
    TIMx->CCEN &= (uint32_t)(~((uint32_t)TIM_CCEN_CC4EN));

    /* Get the TIMx CCEN register value */
    tmpccer = TIMx->CCEN;
    /* Get the TIMx CTRL2 register value */
    tmpcr2 = TIMx->CTRL2;

    /* Get the TIMx CCMOD2 register value */
    tmpccmrx = TIMx->CCMOD2;

    /* Reset the Output Compare mode and Capture/Compare selection Bits */
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD2_OC4MD));
    tmpccmrx &= (uint16_t)(~((uint16_t)TIM_CCMOD2_CC4SEL));

    /* Select the Output Compare Mode */
    tmpccmrx |= (uint16_t)(TIM_OCInitStruct->OcMode << 8);

    /* Reset the Output Polarity level */
    tmpccer &= (uint32_t)(~((uint32_t)TIM_CCEN_CC4P));
    /* Set the Output Compare Polarity */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OcPolarity << 12);

    /* Set the Output State */
    tmpccer |= (uint32_t)(TIM_OCInitStruct->OutputState << 12);

    if (TIMx == TIM1)
    {
        /* Reset the Output Compare IDLE State */
        tmpcr2 &= (uint32_t)(~((uint32_t)TIM_CTRL2_OI4));
        /* Set the Output Idle state */
        tmpcr2 |= (uint32_t)(TIM_OCInitStruct->OcIdleState << 6);
    }
    /* Write to TIMx CTRL2 */
    TIMx->CTRL2 = tmpcr2;

    /* Write to TIMx CCMOD2 */
    TIMx->CCMOD2 = tmpccmrx;

    /* Set the Capture Compare Register value */
    TIMx->CCDAT4 = TIM_OCInitStruct->Pulse;

    /* Write to TIMx CCEN */
    TIMx->CCEN = tmpccer;
}


/*-------------------------------------------------------------------
void TIM_ICInit(TIM_Module* TIMx, TIM_ICInitType* TIM_ICInitStruct)
		定时器的输入捕获配置
		TIMx = TIM1 TIM3
		TIM_ICInitStruct = 输入捕获所需要的数据结构
-------------------------------------------------------------------*/
void TIM_ICInit(TIM_Module* TIMx, TIM_ICInitType* TIM_ICInitStruct)
{
    if (TIM_ICInitStruct->Channel == TIM_CH_1)
    {
        /* TI1 Configuration */
        ConfigTI1(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap1Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
    else if (TIM_ICInitStruct->Channel == TIM_CH_2)
    {
        /* TI2 Configuration */
        ConfigTI2(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap2Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
    else if (TIM_ICInitStruct->Channel == TIM_CH_3)
    {
        /* TI3 Configuration */
        ConfigTI3(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap3Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
    else
    {
        /* TI4 Configuration */
        ConfigTI4(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap4Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
}

/*-------------------------------------------------------------------
void TIM_ConfigPwmIc(TIM_Module* TIMx, TIM_ICInitType* TIM_ICInitStruct)
		定时器利用外部捕获功能实现外部PWM脉冲信号测量的初始化
		TIMx = TIM1 TIM3
		TIM_ICInitStruct = 初始化需要的数据结构
-------------------------------------------------------------------*/
void TIM_ConfigPwmIc(TIM_Module* TIMx, TIM_ICInitType* TIM_ICInitStruct)
{
    uint16_t icoppositepolarity  = TIM_IC_POLARITY_RISING;
    uint16_t icoppositeselection = TIM_IC_SELECTION_DIRECTTI;

    /* Select the Opposite Input Polarity */
    if (TIM_ICInitStruct->IcPolarity == TIM_IC_POLARITY_RISING)
    {
        icoppositepolarity = TIM_IC_POLARITY_FALLING;
    }
    else
    {
        icoppositepolarity = TIM_IC_POLARITY_RISING;
    }
    /* Select the Opposite Input */
    if (TIM_ICInitStruct->IcSelection == TIM_IC_SELECTION_DIRECTTI)
    {
        icoppositeselection = TIM_IC_SELECTION_INDIRECTTI;
    }
    else
    {
        icoppositeselection = TIM_IC_SELECTION_DIRECTTI;
    }
    if (TIM_ICInitStruct->Channel == TIM_CH_1)
    {
        /* TI1 Configuration */
        ConfigTI1(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap1Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
        /* TI2 Configuration */
        ConfigTI2(TIMx, icoppositepolarity, icoppositeselection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap2Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
    else
    {
        /* TI2 Configuration */
        ConfigTI2(TIMx, TIM_ICInitStruct->IcPolarity, TIM_ICInitStruct->IcSelection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap2Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
        /* TI1 Configuration */
        ConfigTI1(TIMx, icoppositepolarity, icoppositeselection, TIM_ICInitStruct->IcFilter);
        /* Set the Input Capture Prescaler value */
        TIM_SetInCap1Prescaler(TIMx, TIM_ICInitStruct->IcPrescaler);
    }
}

/*-------------------------------------------------------------------
void TIM_ConfigBkdt(TIM_Module* TIMx, TIM_BDTRInitType* TIM_BDTRInitStruct)
		定时器阻断特性（刹车）配置，阻断、死区、锁定时输出电平、OSSI、OSSR、AOE
		TIMx = TIM1 TIM3
		TIM_BDTRInitStruct 配置所需要的数据结构
-------------------------------------------------------------------*/
void TIM_ConfigBkdt(TIM_Module* TIMx, TIM_BDTRInitType* TIM_BDTRInitStruct)
{
    uint32_t tmp;
    /* Set the Lock level, the Break enable Bit and the Ploarity, the OSSR State,
       the OSSI State, the dead time value and the Automatic Output Enable Bit */
    TIMx->BKDT = (uint32_t)TIM_BDTRInitStruct->OssrState | TIM_BDTRInitStruct->OssiState | TIM_BDTRInitStruct->LockLevel
                 | TIM_BDTRInitStruct->DeadTime | TIM_BDTRInitStruct->Break | TIM_BDTRInitStruct->BreakPolarity
                 | TIM_BDTRInitStruct->AutomaticOutput;

    /*cofigure other break in*/
    tmp = TIMx->CTRL1;
    /*IOMBKPEN 0 meaning iom as break enable*/
    if (TIM_BDTRInitStruct->IomBreakEn)
        tmp &= ~(0x01L << 10);
    else
        tmp |= (0x01L << 10);
    if (TIM_BDTRInitStruct->LockUpBreakEn)
        tmp |= (0x01L << 16);
    else
        tmp &= ~(0x01L << 16);
    if (TIM_BDTRInitStruct->PvdBreakEn)
        tmp |= (0x01L << 17);
    else
        tmp &= ~(0x01L << 17);
    TIMx->CTRL1 = tmp;
}

/*-------------------------------------------------------------------
void TIM_InitTimBaseStruct(TIM_TimeBaseInitType* TIM_TimeBaseInitStruct)
		定时器基本配置 数据结构的默认值初始化
-------------------------------------------------------------------*/
void TIM_InitTimBaseStruct(TIM_TimeBaseInitType* TIM_TimeBaseInitStruct)
{
    /* Set the default configuration */
    TIM_TimeBaseInitStruct->Period    = 0xFFFF;
    TIM_TimeBaseInitStruct->Prescaler = 0x0000;
    TIM_TimeBaseInitStruct->ClkDiv    = TIM_CLK_DIV1;
    TIM_TimeBaseInitStruct->CntMode   = TIM_CNT_MODE_UP;
    TIM_TimeBaseInitStruct->RepetCnt  = 0x0000;

    TIM_TimeBaseInitStruct->CapCh1FromCompEn    = false;
    TIM_TimeBaseInitStruct->CapCh2FromCompEn    = false;
    TIM_TimeBaseInitStruct->CapCh3FromCompEn    = false;
    TIM_TimeBaseInitStruct->CapEtrClrFromCompEn = false;
    TIM_TimeBaseInitStruct->CapEtrSelFromTscEn  = false;
}

/*-------------------------------------------------------------------
void TIM_InitOcStruct(OCInitType* TIM_OCInitStruct)
		定时器配置所需要的 输出比较配置数据结构默认值初始化
-------------------------------------------------------------------*/
void TIM_InitOcStruct(OCInitType* TIM_OCInitStruct)
{
    /* Set the default configuration */
    TIM_OCInitStruct->OcMode       = TIM_OCMODE_TIMING;
    TIM_OCInitStruct->OutputState  = TIM_OUTPUT_STATE_DISABLE;
    TIM_OCInitStruct->OutputNState = TIM_OUTPUT_NSTATE_DISABLE;
    TIM_OCInitStruct->Pulse        = 0x0000;
    TIM_OCInitStruct->OcPolarity   = TIM_OC_POLARITY_HIGH;
    TIM_OCInitStruct->OcNPolarity  = TIM_OC_POLARITY_HIGH;
    TIM_OCInitStruct->OcIdleState  = TIM_OC_IDLE_STATE_RESET;
    TIM_OCInitStruct->OcNIdleState = TIM_OCN_IDLE_STATE_RESET;
}

/*-------------------------------------------------------------------
void TIM_InitIcStruct(TIM_ICInitType* TIM_ICInitStruct)
		定时器输入捕获配置所需的数据结构默认值初始化
-------------------------------------------------------------------*/
void TIM_InitIcStruct(TIM_ICInitType* TIM_ICInitStruct)
{
    /* Set the default configuration */
    TIM_ICInitStruct->Channel     = TIM_CH_1;
    TIM_ICInitStruct->IcPolarity  = TIM_IC_POLARITY_RISING;
    TIM_ICInitStruct->IcSelection = TIM_IC_SELECTION_DIRECTTI;
    TIM_ICInitStruct->IcPrescaler = TIM_IC_PSC_DIV1;
    TIM_ICInitStruct->IcFilter    = 0x00;
}

/*-------------------------------------------------------------------
void TIM_InitBkdtStruct(TIM_BDTRInitType* TIM_BDTRInitStruct)
		定时器阻断特性（刹车）配置用数据结构默认值初始化
-------------------------------------------------------------------*/
void TIM_InitBkdtStruct(TIM_BDTRInitType* TIM_BDTRInitStruct)
{
    /* Set the default configuration */
    TIM_BDTRInitStruct->OssrState       = TIM_OSSR_STATE_DISABLE;
    TIM_BDTRInitStruct->OssiState       = TIM_OSSI_STATE_DISABLE;
    TIM_BDTRInitStruct->LockLevel       = TIM_LOCK_LEVEL_OFF;
    TIM_BDTRInitStruct->DeadTime        = 0x00;
    TIM_BDTRInitStruct->Break           = TIM_BREAK_IN_DISABLE;
    TIM_BDTRInitStruct->BreakPolarity   = TIM_BREAK_POLARITY_LOW;
    TIM_BDTRInitStruct->AutomaticOutput = TIM_AUTO_OUTPUT_DISABLE;
}

/*-------------------------------------------------------------------
void TIM_Enable(TIM_Module* TIMx, FunctionalState Cmd)
		定时器的许可/停止
-------------------------------------------------------------------*/
void TIM_Enable(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the TIM Counter */
        TIMx->CTRL1 |= TIM_CTRL1_CNTEN;
    }
    else
    {
        /* Disable the TIM Counter */
        TIMx->CTRL1 &= (uint32_t)(~((uint32_t)TIM_CTRL1_CNTEN));
    }
}

/*-------------------------------------------------------------------
void TIM_EnableCtrlPwmOutputs(TIM_Module* TIMx, FunctionalState Cmd)
		定时器输出比较生成PWM的许可/禁止控制
-------------------------------------------------------------------*/
void TIM_EnableCtrlPwmOutputs(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the TIM Main Output */
        TIMx->BKDT |= TIM_BKDT_MOEN;
    }
    else
    {
        /* Disable the TIM Main Output */
        TIMx->BKDT &= (uint16_t)(~((uint16_t)TIM_BKDT_MOEN));
    }
}

/*-------------------------------------------------------------------
void TIM_ConfigInt(TIM_Module* TIMx, uint16_t TIM_IT, FunctionalState Cmd)
		定时器中断设置
		TIM_IT = 	中断类型
							TIM_INT_UPDATE 		TIM update Interrupt source
							TIM_INT_CC1 			TIM Capture Compare 1 Interrupt source
							TIM_INT_CC2 			TIM Capture Compare 2 Interrupt source
							TIM_INT_CC3 			TIM Capture Compare 3 Interrupt source
							TIM_INT_CC4 			TIM Capture Compare 4 Interrupt source
							TIM_INT_COM 			TIM Commutation Interrupt source
							TIM_INT_TRIG 			TIM Trigger Interrupt source
							TIM_INT_BREAK 		TIM Break Interrupt source
 
		说明	TIM6 只有 TIM_INT_UPDATE 其他中断不支持

					TIM_INT_BREAK 只有TIM1支持
					TIM_INT_COM 	只有TIM1支持
-------------------------------------------------------------------*/
void TIM_ConfigInt(TIM_Module* TIMx, uint16_t TIM_IT, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the Interrupt sources */
        TIMx->DINTEN |= TIM_IT;
    }
    else
    {
        /* Disable the Interrupt sources */
        TIMx->DINTEN &= (uint16_t)~TIM_IT;
    }
}

/*-------------------------------------------------------------------
void TIM_GenerateEvent(TIM_Module* TIMx, uint16_t TIM_EventSource)
		用软件产生定时器事件，用于在初始化阶段作为同步
		可以产生的事件包括：
				TIM_EVT_SRC_UPDATE 		Timer update Event source
				TIM_EVT_SRC_CC1 			Timer Capture Compare 1 Event source
				TIM_EVT_SRC_CC2 			Timer Capture Compare 2 Event source
				TIM_EVT_SRC_CC3 			Timer Capture Compare 3 Event source
				TIM_EVT_SRC_CC4 			Timer Capture Compare 4 Event source
				TIM_EVT_SRC_COM 			Timer COM event source
				TIM_EVT_SRC_TRIG 			Timer Trigger Event source
				TIM_EVT_SRC_BREAK 		Timer Break event source
		说明：
				TIM6 只能产生 update event.
				TIM_EVT_SRC_COM  TIM_EVT_SRC_BREAK 只有TIM1支持
-------------------------------------------------------------------*/
void TIM_GenerateEvent(TIM_Module* TIMx, uint16_t TIM_EventSource)
{
    /* Set the event sources */
    TIMx->EVTGEN = TIM_EventSource;
}

/*-------------------------------------------------------------------
void TIM_ConfigDma(TIM_Module* TIMx,uint16_t TIM_DMABase,uint16_t TIM_DMABurstLength)
		设置定时器的DMA
		TIMx = TIM1 TIM3	
		TIM_DMABase = DMA在定时器端的基础地址：
									TIM_DMABASE_CTRL1     
									TIM_DMABASE_CTRL2     
									TIM_DMABASE_SMCTRL    
									TIM_DMABASE_DMAINTEN  
									TIM_DMABASE_STS       
									TIM_DMABASE_EVTGEN    
									TIM_DMABASE_CAPCMPMOD1
									TIM_DMABASE_CAPCMPMOD2
									TIM_DMABASE_CAPCMPEN  
									TIM_DMABASE_CNT       
									TIM_DMABASE_PSC       
									TIM_DMABASE_AR        
									TIM_DMABASE_REPCNT    
									TIM_DMABASE_CAPCMPDAT1
									TIM_DMABASE_CAPCMPDAT2
									TIM_DMABASE_CAPCMPDAT3
									TIM_DMABASE_CAPCMPDAT4
									TIM_DMABASE_BKDT      
									TIM_DMABASE_DMACTRL   
									TIM_DMABASE_CAPCMPMOD3
									TIM_DMABASE_CAPCMPDAT5
									TIM_DMABASE_CAPCMPDAT6
		TIM_DMABurstLength DMA一次突发传输的字节数，只有两个选择
									TIM_DMABURST_LENGTH_1TRANSFER
									TIM_DMABURST_LENGTH_18TRANSFERS.
-------------------------------------------------------------------*/
void TIM_ConfigDma(TIM_Module* TIMx, uint16_t TIM_DMABase, uint16_t TIM_DMABurstLength)
{
    /* Set the DMA Base and the DMA Burst Length */
    TIMx->DCTRL = TIM_DMABase | TIM_DMABurstLength;
}

/*-------------------------------------------------------------------
void TIM_EnableDma(TIM_Module* TIMx,uint16_t TIM_DMASource,FunctionalState Cmd)
		定时器DMA传输允许/禁止
		TIMx = TIM1 TIM3 TIM6
		TIM_DMASource 为定时器触发DMA的信号源选择:
									TIM_DMA_UPDATE 		TIM update Interrupt source
									TIM_DMA_CC1 			TIM Capture Compare 1 DMA source
									TIM_DMA_CC2 			TIM Capture Compare 2 DMA source
									TIM_DMA_CC3 			TIM Capture Compare 3 DMA source
									TIM_DMA_CC4 			TIM Capture Compare 4 DMA source
									TIM_DMA_COM 			TIM Commutation DMA source
									TIM_DMA_TRIG 			TIM Trigger DMA source
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void TIM_EnableDma(TIM_Module* TIMx, uint16_t TIM_DMASource, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the DMA sources */
        TIMx->DINTEN |= TIM_DMASource;
    }
    else
    {
        /* Disable the DMA sources */
        TIMx->DINTEN &= (uint16_t)~TIM_DMASource;
    }
}

/*-------------------------------------------------------------------
void TIM_ConfigInternalClk( TIM_Module* TIMx )配置定时器使用内部时钟源
		TIMx = TIM1 TIM3
-------------------------------------------------------------------*/
void TIM_ConfigInternalClk(TIM_Module* TIMx)
{
    /* Disable slave mode to clock the prescaler directly with the internal clock */
    TIMx->SMCTRL &= (uint16_t)(~((uint16_t)TIM_SMCTRL_SMSEL));
}

/*-------------------------------------------------------------------
void TIM_ConfigInternalTrigToExt(TIM_Module* TIMx,uint16_t TIM_InputTriggerSource)
			配置定时 的内部的触发信号作为 外部时钟输入
			TIMx = TIM1 TIM3
			TIM_InputTriggerSource =	TIM_TRIG_SEL_IN_TR0 Internal Trigger 0
																TIM_TRIG_SEL_IN_TR1 Internal Trigger 1
																TIM_TRIG_SEL_IN_TR2 Internal Trigger 2
																TIM_TRIG_SEL_IN_TR3 Internal Trigger 3
-------------------------------------------------------------------*/
void TIM_ConfigInternalTrigToExt(TIM_Module* TIMx, uint16_t TIM_InputTriggerSource)
{
    /* Select the Internal Trigger */
    TIM_SelectInputTrig(TIMx, TIM_InputTriggerSource);
    /* Select the External clock mode1 */
    TIMx->SMCTRL |= TIM_SLAVE_MODE_EXT1;
}

/*-------------------------------------------------------------------
void TIM_ConfigExtTrigAsClk(TIM_Module* TIMx,uint16_t TIM_TIxExternalCLKSource,uint16_t IcPolarity,uint16_t ICFilter)
	配置定时器外部触发输入作为外部时钟源
	TIMx = TIM1 TIM3
	TIM_TIxExternalCLKSource = 外部输入的触发信号：
														TIM_EXT_CLK_SRC_TI1ED 		TI1 Edge Detector
														TIM_EXT_CLK_SRC_TI1 			Filtered Timer Input 1
														TIM_EXT_CLK_SRC_TI2 			Filtered Timer Input 2
	IcPolarity = 输入信号的极性选择：
														TIM_IC_POLARITY_RISING
														TIM_IC_POLARITY_FALLING
	ICFilter = 输入信号滤波周期数量	0-15之间	
-------------------------------------------------------------------*/
void TIM_ConfigExtTrigAsClk(TIM_Module* TIMx, uint16_t TIM_TIxExternalCLKSource, uint16_t IcPolarity, uint16_t ICFilter)
{
    /* Configure the Timer Input Clock Source */
    if (TIM_TIxExternalCLKSource == TIM_EXT_CLK_SRC_TI2)
    {
        ConfigTI2(TIMx, IcPolarity, TIM_IC_SELECTION_DIRECTTI, ICFilter);
    }
    else
    {
        ConfigTI1(TIMx, IcPolarity, TIM_IC_SELECTION_DIRECTTI, ICFilter);
    }
    /* Select the Trigger source */
    TIM_SelectInputTrig(TIMx, TIM_TIxExternalCLKSource);
    /* Select the External clock mode1 */
    TIMx->SMCTRL |= TIM_SLAVE_MODE_EXT1;
}

/*-------------------------------------------------------------------
void TIM_ConfigExtClkMode1(TIM_Module* TIMx,
                           uint16_t TIM_ExtTRGPrescaler,
                           uint16_t TIM_ExtTRGPolarity,
                           uint16_t ExtTRGFilter)
		设置定时器外部时钟模式1的配置 	
		TIMx = TIM1 TIM3
		TIM_ExtTRGPrescaler = 外部触发信号的预分频：
													TIM_EXT_TRG_PSC_OFF 			ETRP Prescaler OFF.
													TIM_EXT_TRG_PSC_DIV2 			ETRP frequency divided by 2.
													TIM_EXT_TRG_PSC_DIV4 			ETRP frequency divided by 4.
													TIM_EXT_TRG_PSC_DIV8 			ETRP frequency divided by 8.
		TIM_ExtTRGPolarity = 外部触发信号的极性选择：
													TIM_EXT_TRIG_POLARITY_INVERTED 		active low or falling edge active.
													TIM_EXT_TRIG_POLARITY_NONINVERTED active high or rising edge active.
		ExtTRGFilter	= 输入信号滤波采样周期 = 0-15
-------------------------------------------------------------------*/
void TIM_ConfigExtClkMode1(TIM_Module* TIMx,
                           uint16_t TIM_ExtTRGPrescaler,
                           uint16_t TIM_ExtTRGPolarity,
                           uint16_t ExtTRGFilter)
{
    uint16_t tmpsmcr = 0;
	
    /* Configure the ETR Clock source */
    TIM_ConfigExtTrig(TIMx, TIM_ExtTRGPrescaler, TIM_ExtTRGPolarity, ExtTRGFilter);

    /* Get the TIMx SMCTRL register value */
    tmpsmcr = TIMx->SMCTRL;
    /* Reset the SMS Bits */
    tmpsmcr &= (uint16_t)(~((uint16_t)TIM_SMCTRL_SMSEL));
    /* Select the External clock mode1 */
    tmpsmcr |= TIM_SLAVE_MODE_EXT1;
    /* Select the Trigger selection : ETRF */
    tmpsmcr &= (uint16_t)(~((uint16_t)TIM_SMCTRL_TSEL));
    tmpsmcr |= TIM_TRIG_SEL_ETRF;
    /* Write to TIMx SMCTRL */
    TIMx->SMCTRL = tmpsmcr;
}

/*-------------------------------------------------------------------
void TIM_ConfigExtClkMode2(TIM_Module* TIMx,
                           uint16_t TIM_ExtTRGPrescaler,
                           uint16_t TIM_ExtTRGPolarity,
                           uint16_t ExtTRGFilter)
		定时器外部时钟配置模式2 
		TIMx = TIM1 TIM3
		TIM_ExtTRGPrescaler = 外部输入信号的预分频选择：
													TIM_EXT_TRG_PSC_OFF 			ETRP Prescaler OFF.
													TIM_EXT_TRG_PSC_DIV2 			ETRP frequency divided by 2.
													TIM_EXT_TRG_PSC_DIV4 			ETRP frequency divided by 4.
													TIM_EXT_TRG_PSC_DIV8 			ETRP frequency divided by 8.
		TIM_ExtTRGPolarity = 外部输入信号的极性选择：
													TIM_EXT_TRIG_POLARITY_INVERTED 		active low or falling edge active.
													TIM_EXT_TRIG_POLARITY_NONINVERTED active high or rising edge active.
		ExtTRGFilter =外部信号的滤波周期数 0-15
-------------------------------------------------------------------*/
void TIM_ConfigExtClkMode2(TIM_Module* TIMx,
                           uint16_t TIM_ExtTRGPrescaler,
                           uint16_t TIM_ExtTRGPolarity,
                           uint16_t ExtTRGFilter)
{
    /* Configure the ETR Clock source */
    TIM_ConfigExtTrig(TIMx, TIM_ExtTRGPrescaler, TIM_ExtTRGPolarity, ExtTRGFilter);
    /* Enable the External clock mode2 */
    TIMx->SMCTRL |= TIM_SMCTRL_EXCEN;
}

/*-------------------------------------------------------------------
void TIM_ConfigExtTrig(TIM_Module* TIMx,
                       uint16_t TIM_ExtTRGPrescaler,
                       uint16_t TIM_ExtTRGPolarity,
                       uint16_t ExtTRGFilter)
		定时器ETR输入引脚的输入模式配置
		TIMx = TIM1 TIM3
		TIM_ExtTRGPrescaler = ETR信号的预分频值：
													TIM_EXT_TRG_PSC_OFF 	ETRP Prescaler OFF.
													TIM_EXT_TRG_PSC_DIV2 	ETRP frequency divided by 2.
													TIM_EXT_TRG_PSC_DIV4 	ETRP frequency divided by 4.
													TIM_EXT_TRG_PSC_DIV8 	ETRP frequency divided by 8.
		TIM_ExtTRGPolarity = ETR的极性选择
													TIM_EXT_TRIG_POLARITY_INVERTED 		active low or falling edge active.
													TIM_EXT_TRIG_POLARITY_NONINVERTED active high or rising edge active.
		ExtTRGFilter = ETR信号的滤波周期数 0-15
-------------------------------------------------------------------*/
void TIM_ConfigExtTrig(TIM_Module* TIMx,
                       uint16_t TIM_ExtTRGPrescaler,
                       uint16_t TIM_ExtTRGPolarity,
                       uint16_t ExtTRGFilter)
{
    uint16_t tmpsmcr = 0;
	
    tmpsmcr = TIMx->SMCTRL;
    /* Reset the ETR Bits */
    tmpsmcr &= SMCTRL_ETR_MASK;
    /* Set the Prescaler, the Filter value and the Polarity */
    tmpsmcr |=
        (uint16_t)(TIM_ExtTRGPrescaler | (uint16_t)(TIM_ExtTRGPolarity | (uint16_t)(ExtTRGFilter << (uint16_t)8)));
    /* Write to TIMx SMCTRL */
    TIMx->SMCTRL = tmpsmcr;
}

/*-------------------------------------------------------------------
void TIM_ConfigPrescaler(TIM_Module* TIMx,uint16_t Prescaler,uint16_t TIM_PSCReloadMode)
		定时器的预分频设置
		TIMx = TIM1 TIM3 TIM6
		Prescaler = 预分频寄存器的值
		TIM_PSCReloadMode = 预分频寄存器重载方式：
												TIM_PSC_RELOAD_MODE_UPDATE 		定时器update时重载
												TIM_PSC_RELOAD_MODE_IMMEDIATE 立刻重载
-------------------------------------------------------------------*/
void TIM_ConfigPrescaler(TIM_Module* TIMx, uint16_t Prescaler, uint16_t TIM_PSCReloadMode)
{
    /* Set the Prescaler value */
    TIMx->PSC = Prescaler;
    /* Set or reset the UG Bit */
    TIMx->EVTGEN = TIM_PSCReloadMode;
}

/*-------------------------------------------------------------------
void TIM_ConfigCntMode(TIM_Module* TIMx, uint16_t CntMode)
		配置定时器的计数模式
		TIMx = TIM1 TIM3
		CntMode = 可选择的计数模式：
							TIM_CNT_MODE_UP 						TIM Up Counting Mode
							TIM_CNT_MODE_DOWN 					TIM Down Counting Mode
							TIM_CNT_MODE_CENTER_ALIGN1 	TIM Center Aligned Mode1
							TIM_CNT_MODE_CENTER_ALIGN2 	TIM Center Aligned Mode2
							TIM_CNT_MODE_CENTER_ALIGN3 	TIM Center Aligned Mode3
-------------------------------------------------------------------*/
void TIM_ConfigCntMode(TIM_Module* TIMx, uint16_t CntMode)
{
    uint32_t tmpcr1 = 0;
	
    tmpcr1 = TIMx->CTRL1;
    /* Reset the CMS and DIR Bits */
    tmpcr1 &= (uint32_t)(~((uint32_t)(TIM_CTRL1_DIR | TIM_CTRL1_CAMSEL)));
    /* Set the Counter Mode */
    tmpcr1 |= CntMode;
    /* Write to TIMx CTRL1 register */
    TIMx->CTRL1 = tmpcr1;
}

/*-------------------------------------------------------------------
void TIM_SelectInputTrig(TIM_Module* TIMx, uint16_t TIM_InputTriggerSource)
		选择定时器的输入触发源
		TIMx = TIM1 TIM3
		TIM_InputTriggerSource = 输入的触发信号选择：
														TIM_TRIG_SEL_IN_TR0 	Internal Trigger 0
														TIM_TRIG_SEL_IN_TR1 	Internal Trigger 1
														TIM_TRIG_SEL_IN_TR2 	Internal Trigger 2
														TIM_TRIG_SEL_IN_TR3 	Internal Trigger 3
														TIM_TRIG_SEL_TI1F_ED 	TI1 Edge Detector
														TIM_TRIG_SEL_TI1FP1 	Filtered Timer Input 1
														TIM_TRIG_SEL_TI2FP2 	Filtered Timer Input 2
														TIM_TRIG_SEL_ETRF 		External Trigger input
-------------------------------------------------------------------*/
void TIM_SelectInputTrig(TIM_Module* TIMx, uint16_t TIM_InputTriggerSource)
{
    uint16_t tmpsmcr = 0;
    /* Get the TIMx SMCTRL register value */
    tmpsmcr = TIMx->SMCTRL;
    /* Reset the TS Bits */
    tmpsmcr &= (uint16_t)(~((uint16_t)TIM_SMCTRL_TSEL));
    /* Set the Input Trigger source */
    tmpsmcr |= TIM_InputTriggerSource;
    /* Write to TIMx SMCTRL */
    TIMx->SMCTRL = tmpsmcr;
}

/*-------------------------------------------------------------------
void TIM_ConfigEncoderInterface(TIM_Module* TIMx,
                                uint16_t TIM_EncoderMode,
                                uint16_t TIM_IC1Polarity,
                                uint16_t TIM_IC2Polarity)
		设置定时器解码界面
		TIMx = TIM1 TIM3
		TIM_EncoderMode = 解码模式
											TIM_ENCODE_MODE_TI1 	Counter counts on TI1FP1 edge depending on TI2FP2 level.
											TIM_ENCODE_MODE_TI2 	Counter counts on TI2FP2 edge depending on TI1FP1 level.
											TIM_ENCODE_MODE_TI12 	Counter counts on both TI1FP1 and TI2FP2 edges depending
																						on the level of the other input.
		TIM_IC1Polarity = 输入通道1的极性选择
											TIM_IC_POLARITY_FALLING 	IC Falling edge.
											TIM_IC_POLARITY_RISING 		IC Rising edge.
		TIM_IC2Polarity = 输入通道2的极性选择
											TIM_IC_POLARITY_FALLING 	IC Falling edge.
											TIM_IC_POLARITY_RISING 		IC Rising edge.
-------------------------------------------------------------------*/
void TIM_ConfigEncoderInterface(TIM_Module* TIMx,
                                uint16_t TIM_EncoderMode,
                                uint16_t TIM_IC1Polarity,
                                uint16_t TIM_IC2Polarity)
{
    uint16_t tmpsmcr  = 0;
    uint16_t tmpccmr1 = 0;
    uint32_t tmpccer  = 0;

    /* Get the TIMx SMCTRL register value */
    tmpsmcr = TIMx->SMCTRL;

    /* Get the TIMx CCMOD1 register value */
    tmpccmr1 = TIMx->CCMOD1;

    /* Get the TIMx CCEN register value */
    tmpccer = TIMx->CCEN;

    /* Set the encoder Mode */
    tmpsmcr &= (uint16_t)(~((uint16_t)TIM_SMCTRL_SMSEL));
    tmpsmcr |= TIM_EncoderMode;

    /* Select the Capture Compare 1 and the Capture Compare 2 as input */
    tmpccmr1 &= (uint16_t)(((uint16_t) ~((uint16_t)TIM_CCMOD1_CC1SEL)) & (uint16_t)(~((uint16_t)TIM_CCMOD1_CC2SEL)));
    tmpccmr1 |= TIM_CCMOD1_CC1SEL_0 | TIM_CCMOD1_CC2SEL_0;

    /* Set the TI1 and the TI2 Polarities */
    tmpccer &= (uint32_t)(((uint32_t) ~((uint32_t)TIM_CCEN_CC1P)) & ((uint32_t) ~((uint32_t)TIM_CCEN_CC2P)));
    tmpccer |= (uint32_t)(TIM_IC1Polarity | (uint16_t)(TIM_IC2Polarity << (uint16_t)4));

    /* Write to TIMx SMCTRL */
    TIMx->SMCTRL = tmpsmcr;
    /* Write to TIMx CCMOD1 */
    TIMx->CCMOD1 = tmpccmr1;
    /* Write to TIMx CCEN */
    TIMx->CCEN = tmpccer;
}

/*-------------------------------------------------------------------
void TIM_ConfigForcedOc1(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
		设置定时器通道1的强迫输出  指阻断状态下的强迫输出
		TIMx = TIM1 TIM3
		TIM_ForcedAction = 强迫输出模式选择
											TIM_FORCED_ACTION_ACTIVE 		Force active level on OC1REF
											TIM_FORCED_ACTION_INACTIVE 	Force inactive level on OC1REF.
-------------------------------------------------------------------*/
void TIM_ConfigForcedOc1(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
{
    uint16_t tmpccmr1 = 0;
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC1M Bits */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC1M);
    /* Configure The Forced output Mode */
    tmpccmr1 |= TIM_ForcedAction;
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigForcedOc2(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
		配置定时器输出比较通道2的强制输出模式，也就是在阻断状态下的输出
		TIMx = TIM1 TIM3
		TIM_ForcedAction = 
				阻断状态下的输出模式
				TIM_FORCED_ACTION_ACTIVE 		Force active level on OC2REF
				TIM_FORCED_ACTION_INACTIVE 	Force inactive level on OC2REF.
-------------------------------------------------------------------*/
void TIM_ConfigForcedOc2(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
{
    uint16_t tmpccmr1 = 0;
	
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC2M Bits */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC2M);
    /* Configure The Forced output Mode */
    tmpccmr1 |= (uint16_t)(TIM_ForcedAction << 8);
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigForcedOc3(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
		定时器比较输出通道3的强制输出模式配置，也就是在阻断状态下的输出
		TIMx = TIM1 TIM3
		TIM_ForcedAction = 强制输出模式
				TIM_FORCED_ACTION_ACTIVE 		Force active level on OC3REF
				TIM_FORCED_ACTION_INACTIVE 	Force inactive level on OC3REF.
-------------------------------------------------------------------*/
void TIM_ConfigForcedOc3(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
{
    uint16_t tmpccmr2 = 0;
	
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC1M Bits */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC3MD);
    /* Configure The Forced output Mode */
    tmpccmr2 |= TIM_ForcedAction;
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}

/*-------------------------------------------------------------------
void TIM_ConfigForcedOc4(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
		定时器比较输出通道4的强制输出模式配置，也就是在阻断状态下的输出
		TIMx = TIM1 TIM3
		TIM_ForcedAction = 强制输出模式
				TIM_FORCED_ACTION_ACTIVE 		Force active level on OC4REF
				TIM_FORCED_ACTION_INACTIVE 	Force inactive level on OC4REF.
-------------------------------------------------------------------*/
void TIM_ConfigForcedOc4(TIM_Module* TIMx, uint16_t TIM_ForcedAction)
{
    uint16_t tmpccmr2 = 0;
	
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC2M Bits */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC4MD);
    /* Configure The Forced output Mode */
    tmpccmr2 |= (uint16_t)(TIM_ForcedAction << 8);
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}

/*-------------------------------------------------------------------
void TIM_ConfigArPreload(TIM_Module* TIMx, FunctionalState Cmd)
		设置定时器周期寄存器的预装载许可/禁止
		TIMx = TIM1 TIM3 TIM6
		ENABLE or DISABLE 
-------------------------------------------------------------------*/
void TIM_ConfigArPreload(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Set the AR Preload Bit */
        TIMx->CTRL1 |= TIM_CTRL1_ARPEN;
    }
    else
    {
        /* Reset the AR Preload Bit */
        TIMx->CTRL1 &= (uint32_t) ~((uint32_t)TIM_CTRL1_ARPEN);
    }
}

/*-------------------------------------------------------------------
void TIM_SelectComEvt(TIM_Module* TIMx, FunctionalState Cmd)
		选择/禁止定时器的通信事件
		TIMx = TIM1
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void TIM_SelectComEvt(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Set the COM Bit */
        TIMx->CTRL2 |= TIM_CTRL2_CCUSEL;
    }
    else
    {
        /* Reset the COM Bit */
        TIMx->CTRL2 &= (uint32_t) ~((uint32_t)TIM_CTRL2_CCUSEL);
    }
}

/*-------------------------------------------------------------------
void TIM_SelectCapCmpDmaSrc(TIM_Module* TIMx, FunctionalState Cmd)
		选择/禁止 捕获或比较作为的DMA源
		TIMx = TIM1 TIM3
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void TIM_SelectCapCmpDmaSrc(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Set the CCDS Bit */
        TIMx->CTRL2 |= TIM_CTRL2_CCDSEL;
    }
    else
    {
        /* Reset the CCDS Bit */
        TIMx->CTRL2 &= (uint32_t) ~((uint32_t)TIM_CTRL2_CCDSEL);
    }
}

/*-------------------------------------------------------------------
void TIM_EnableCapCmpPreloadControl(TIM_Module* TIMx, FunctionalState Cmd)
		许可/禁止 捕获或者比较的预装载控制
		TIMx = TIM1 TIM3
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void TIM_EnableCapCmpPreloadControl(TIM_Module* TIMx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Set the CCPC Bit */
        TIMx->CTRL2 |= TIM_CTRL2_CCPCTL;
    }
    else
    {
        /* Reset the CCPC Bit */
        TIMx->CTRL2 &= (uint32_t) ~((uint32_t)TIM_CTRL2_CCPCTL);
    }
}

/*-------------------------------------------------------------------
void TIM_EnableOc1Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
		许可/禁止 输出比较器通道1的预装载功能
		TIMx = TIM1 TIM3
		TIM_OCPreload = TIM_OC_PRE_LOAD_ENABLE
										TIM_OC_PRE_LOAD_DISABLE
-------------------------------------------------------------------*/
void TIM_EnableOc1Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
{
    uint16_t tmpccmr1 = 0;
	
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC1PE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC1PEN);
    /* Enable or Disable the Output Compare Preload feature */
    tmpccmr1 |= TIM_OCPreload;
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc2Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
		允许/禁止 比较器通道2的预装载功能
		TIMx = TIM1 TIM3
		TIM_OCPreload = TIM_OC_PRE_LOAD_ENABLE
										TIM_OC_PRE_LOAD_DISABLE
-------------------------------------------------------------------*/
void TIM_ConfigOc2Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
{
    uint16_t tmpccmr1 = 0;
	
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC2PE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC2PEN);
    /* Enable or Disable the Output Compare Preload feature */
    tmpccmr1 |= (uint16_t)(TIM_OCPreload << 8);
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc3Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
		许可/禁止 输出比较通道3的预装载功能
		TIMx = TIM1 TIM3
		TIM_OCPreload = TIM_OC_PRE_LOAD_ENABLE
										TIM_OC_PRE_LOAD_DISABLE
-------------------------------------------------------------------*/
void TIM_ConfigOc3Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
{
    uint16_t tmpccmr2 = 0;
	
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC3PE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC3PEN);
    /* Enable or Disable the Output Compare Preload feature */
    tmpccmr2 |= TIM_OCPreload;
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc4Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
		许可/禁止 输出比较通道4的预装载功能
		TIMx = TIM1 TIM3
		TIM_OCPreload = TIM_OC_PRE_LOAD_ENABLE
										TIM_OC_PRE_LOAD_DISABLE
-------------------------------------------------------------------*/
void TIM_ConfigOc4Preload(TIM_Module* TIMx, uint16_t TIM_OCPreload)
{
    uint16_t tmpccmr2 = 0;
	
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC4PE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC4PEN);
    /* Enable or Disable the Output Compare Preload feature */
    tmpccmr2 |= (uint16_t)(TIM_OCPreload << 8);
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}


/*-------------------------------------------------------------------
void TIM_ConfigOc1Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
		允许/禁止 定时器输出比较通道1的加速特性
		TIMx = TIM1 TIM3
		TIM_OCFast = 	TIM_OC_FAST_ENABLE 	TIM output compare fast enable
									TIM_OC_FAST_DISABLE TIM output compare fast disable
-------------------------------------------------------------------*/
void TIM_ConfigOc1Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
{
    uint16_t tmpccmr1 = 0;
	
    /* Get the TIMx CCMOD1 register value */
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC1FE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC1FEN);
    /* Enable or Disable the Output Compare Fast Bit */
    tmpccmr1 |= TIM_OCFast;
    /* Write to TIMx CCMOD1 */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc2Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
		允许/禁止 定时器输出比较通道2的加速特性
		TIMx = TIM1 TIM3
		TIM_OCFast = 	TIM_OC_FAST_ENABLE 	TIM output compare fast enable
									TIM_OC_FAST_DISABLE TIM output compare fast disable
-------------------------------------------------------------------*/
void TIM_ConfigOc2Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
{
    uint16_t tmpccmr1 = 0;
	
    /* Get the TIMx CCMOD1 register value */
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC2FE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC2FEN);
    /* Enable or Disable the Output Compare Fast Bit */
    tmpccmr1 |= (uint16_t)(TIM_OCFast << 8);
    /* Write to TIMx CCMOD1 */
    TIMx->CCMOD1 = tmpccmr1;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc3Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
		允许/禁止 定时器比较通道3的加速特性
		TIMx = TIM1 TIM3
		TIM_OCFast = 	TIM_OC_FAST_ENABLE 	TIM output compare fast enable
									TIM_OC_FAST_DISABLE TIM output compare fast disable
-------------------------------------------------------------------*/
void TIM_ConfigOc3Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
{
    uint16_t tmpccmr2 = 0;
	
    /* Get the TIMx CCMOD2 register value */
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC3FE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC3FEN);
    /* Enable or Disable the Output Compare Fast Bit */
    tmpccmr2 |= TIM_OCFast;
    /* Write to TIMx CCMOD2 */
    TIMx->CCMOD2 = tmpccmr2;
}

/*-------------------------------------------------------------------
void TIM_ConfigOc4Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
		允许/禁止 定时器输出比较通道4的加速特性
		TIMx = TIM1 TIM3
		TIM_OCFast = 	TIM_OC_FAST_ENABLE 	TIM output compare fast enable
									TIM_OC_FAST_DISABLE TIM output compare fast disable
-------------------------------------------------------------------*/
void TIM_ConfigOc4Fast(TIM_Module* TIMx, uint16_t TIM_OCFast)
{
    uint16_t tmpccmr2 = 0;
	
    /* Get the TIMx CCMOD2 register value */
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC4FE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC4FEN);
    /* Enable or Disable the Output Compare Fast Bit */
    tmpccmr2 |= (uint16_t)(TIM_OCFast << 8);
    /* Write to TIMx CCMOD2 */
    TIMx->CCMOD2 = tmpccmr2;
}

/*-------------------------------------------------------------------
void TIM_ClrOc1Ref(TIM_Module* TIMx, uint16_t TIM_OCClear)
		
 * @brief  Clears or safeguards the OCREF1 signal on an external event
 * @param TIMx where x can be  1 or 3 to select the TIM peripheral.
 * @param TIM_OCClear new state of the Output Compare Clear Enable Bit.
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_CLR_ENABLE TIM Output clear enable
 *     @arg TIM_OC_CLR_DISABLE TIM Output clear disable
-------------------------------------------------------------------*/
void TIM_ClrOc1Ref(TIM_Module* TIMx, uint16_t TIM_OCClear)
{
    uint16_t tmpccmr1 = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcClrState(TIM_OCClear));

    tmpccmr1 = TIMx->CCMOD1;

    /* Reset the OC1CE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC1CEN);
    /* Enable or Disable the Output Compare Clear Bit */
    tmpccmr1 |= TIM_OCClear;
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/**
 * @brief  Clears or safeguards the OCREF2 signal on an external event
 * @param TIMx where x can be  1 or 3 to select the TIM peripheral.
 * @param TIM_OCClear new state of the Output Compare Clear Enable Bit.
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_CLR_ENABLE TIM Output clear enable
 *     @arg TIM_OC_CLR_DISABLE TIM Output clear disable
 */
void TIM_ClrOc2Ref(TIM_Module* TIMx, uint16_t TIM_OCClear)
{
    uint16_t tmpccmr1 = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcClrState(TIM_OCClear));
    tmpccmr1 = TIMx->CCMOD1;
    /* Reset the OC2CE Bit */
    tmpccmr1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_OC2CEN);
    /* Enable or Disable the Output Compare Clear Bit */
    tmpccmr1 |= (uint16_t)(TIM_OCClear << 8);
    /* Write to TIMx CCMOD1 register */
    TIMx->CCMOD1 = tmpccmr1;
}

/**
 * @brief  Clears or safeguards the OCREF3 signal on an external event
 * @param TIMx where x can be  1 or 3 to select the TIM peripheral.
 * @param TIM_OCClear new state of the Output Compare Clear Enable Bit.
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_CLR_ENABLE TIM Output clear enable
 *     @arg TIM_OC_CLR_DISABLE TIM Output clear disable
 */
void TIM_ClrOc3Ref(TIM_Module* TIMx, uint16_t TIM_OCClear)
{
    uint16_t tmpccmr2 = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcClrState(TIM_OCClear));
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC3CE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC3CEN);
    /* Enable or Disable the Output Compare Clear Bit */
    tmpccmr2 |= TIM_OCClear;
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}

/**
 * @brief  Clears or safeguards the OCREF4 signal on an external event
 * @param TIMx where x can be  1 or 3 to select the TIM peripheral.
 * @param TIM_OCClear new state of the Output Compare Clear Enable Bit.
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_CLR_ENABLE TIM Output clear enable
 *     @arg TIM_OC_CLR_DISABLE TIM Output clear disable
 */
void TIM_ClrOc4Ref(TIM_Module* TIMx, uint16_t TIM_OCClear)
{
    uint16_t tmpccmr2 = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcClrState(TIM_OCClear));
    tmpccmr2 = TIMx->CCMOD2;
    /* Reset the OC4CE Bit */
    tmpccmr2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_OC4CEN);
    /* Enable or Disable the Output Compare Clear Bit */
    tmpccmr2 |= (uint16_t)(TIM_OCClear << 8);
    /* Write to TIMx CCMOD2 register */
    TIMx->CCMOD2 = tmpccmr2;
}

/**
 * @brief  Configures the TIMx channel 1 polarity.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param OcPolarity specifies the OC1 Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OC_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc1Polarity(TIM_Module* TIMx, uint16_t OcPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    assert_param(IsTimOcPolarity(OcPolarity));
    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC1P Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC1P);
    tmpccer |= OcPolarity;
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx Channel 1N polarity.
 * @param TIMx where x can be 1, 8 to select the TIM peripheral.
 * @param OcNPolarity specifies the OC1N Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OCN_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OCN_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc1NPolarity(TIM_Module* TIMx, uint16_t OcNPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList2Module(TIMx));
    assert_param(IsTimOcnPolarity(OcNPolarity));

    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC1NP Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC1NP);
    tmpccer |= OcNPolarity;
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx channel 2 polarity.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param OcPolarity specifies the OC2 Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OC_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc2Polarity(TIM_Module* TIMx, uint16_t OcPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    assert_param(IsTimOcPolarity(OcPolarity));
    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC2P Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC2P);
    tmpccer |= (uint32_t)(OcPolarity << 4);
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx Channel 2N polarity.
 * @param TIMx where x can be 1 or 8 to select the TIM peripheral.
 * @param OcNPolarity specifies the OC2N Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OCN_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OCN_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc2NPolarity(TIM_Module* TIMx, uint16_t OcNPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList1Module(TIMx));
    assert_param(IsTimOcnPolarity(OcNPolarity));

    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC2NP Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC2NP);
    tmpccer |= (uint32_t)(OcNPolarity << 4);
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx channel 3 polarity.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param OcPolarity specifies the OC3 Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OC_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc3Polarity(TIM_Module* TIMx, uint16_t OcPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcPolarity(OcPolarity));
    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC3P Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC3P);
    tmpccer |= (uint32_t)(OcPolarity << 8);
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx Channel 3N polarity.
 * @param TIMx where x can be 1 or 8 to select the TIM peripheral.
 * @param OcNPolarity specifies the OC3N Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OCN_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OCN_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc3NPolarity(TIM_Module* TIMx, uint16_t OcNPolarity)
{
    uint32_t tmpccer = 0;

    /* Check the parameters */
    assert_param(IsTimList1Module(TIMx));
    assert_param(IsTimOcnPolarity(OcNPolarity));

    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC3NP Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC3NP);
    tmpccer |= (uint32_t)(OcNPolarity << 8);
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Configures the TIMx channel 4 polarity.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param OcPolarity specifies the OC4 Polarity
 *   This parameter can be one of the following values:
 *     @arg TIM_OC_POLARITY_HIGH Output Compare active high
 *     @arg TIM_OC_POLARITY_LOW Output Compare active low
 */
void TIM_ConfigOc4Polarity(TIM_Module* TIMx, uint16_t OcPolarity)
{
    uint32_t tmpccer = 0;
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimOcPolarity(OcPolarity));
    tmpccer = TIMx->CCEN;
    /* Set or Reset the CC4P Bit */
    tmpccer &= (uint32_t) ~((uint32_t)TIM_CCEN_CC4P);
    tmpccer |= (uint32_t)(OcPolarity << 12);
    /* Write to TIMx CCEN register */
    TIMx->CCEN = tmpccer;
}

/**
 * @brief  Enables or disables the TIM Capture Compare Channel x.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Channel specifies the TIM Channel
 *   This parameter can be one of the following values:
 *     @arg TIM_CH_1 TIM Channel 1
 *     @arg TIM_CH_2 TIM Channel 2
 *     @arg TIM_CH_3 TIM Channel 3
 *     @arg TIM_CH_4 TIM Channel 4
 * @param TIM_CCx specifies the TIM Channel CCxE bit new state.
 *   This parameter can be: TIM_CAP_CMP_ENABLE or TIM_CAP_CMP_DISABLE.
 */
void TIM_EnableCapCmpCh(TIM_Module* TIMx, uint16_t Channel, uint32_t TIM_CCx)
{
    uint16_t tmp = 0;

    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    assert_param(IsTimCh(Channel));
    assert_param(IsTimCapCmpState(TIM_CCx));

    tmp = CAPCMPEN_CCE_SET << Channel;

    /* Reset the CCxE Bit */
    TIMx->CCEN &= (uint32_t)~tmp;

    /* Set or reset the CCxE Bit */
    TIMx->CCEN |= (uint32_t)(TIM_CCx << Channel);
}

/**
 * @brief  Enables or disables the TIM Capture Compare Channel xN.
 * @param TIMx where x can be 1, 8 to select the TIM peripheral.
 * @param Channel specifies the TIM Channel
 *   This parameter can be one of the following values:
 *     @arg TIM_CH_1 TIM Channel 1
 *     @arg TIM_CH_2 TIM Channel 2
 *     @arg TIM_CH_3 TIM Channel 3
 * @param TIM_CCxN specifies the TIM Channel CCxNE bit new state.
 *   This parameter can be: TIM_CAP_CMP_N_ENABLE or TIM_CAP_CMP_N_DISABLE.
 */
void TIM_EnableCapCmpChN(TIM_Module* TIMx, uint16_t Channel, uint32_t TIM_CCxN)
{
    uint16_t tmp = 0;

    /* Check the parameters */
    assert_param(IsTimList2Module(TIMx));
    assert_param(IsTimComplementaryCh(Channel));
    assert_param(IsTimCapCmpNState(TIM_CCxN));

    tmp = CAPCMPEN_CCNE_SET << Channel;

    /* Reset the CCxNE Bit */
    TIMx->CCEN &= (uint32_t)~tmp;

    /* Set or reset the CCxNE Bit */
    TIMx->CCEN |= (uint32_t)(TIM_CCxN << Channel);
}

/**
 * @brief  Selects the TIM Output Compare Mode.
 * @note   This function disables the selected channel before changing the Output
 *         Compare Mode.
 *         User has to enable this channel using TIM_EnableCapCmpCh and TIM_EnableCapCmpChN functions.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Channel specifies the TIM Channel
 *   This parameter can be one of the following values:
 *     @arg TIM_CH_1 TIM Channel 1
 *     @arg TIM_CH_2 TIM Channel 2
 *     @arg TIM_CH_3 TIM Channel 3
 *     @arg TIM_CH_4 TIM Channel 4
 * @param OcMode specifies the TIM Output Compare Mode.
 *   This parameter can be one of the following values:
 *     @arg TIM_OCMODE_TIMING
 *     @arg TIM_OCMODE_ACTIVE
 *     @arg TIM_OCMODE_TOGGLE
 *     @arg TIM_OCMODE_PWM1
 *     @arg TIM_OCMODE_PWM2
 *     @arg TIM_FORCED_ACTION_ACTIVE
 *     @arg TIM_FORCED_ACTION_INACTIVE
 */
void TIM_SelectOcMode(TIM_Module* TIMx, uint16_t Channel, uint16_t OcMode)
{
    uint32_t tmp  = 0;
    uint16_t tmp1 = 0;

    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    assert_param(IsTimCh(Channel));
    assert_param(IsTimOc(OcMode));

    tmp = (uint32_t)TIMx;
    tmp += CAPCMPMOD_OFFSET;

    tmp1 = CAPCMPEN_CCE_SET << (uint16_t)Channel;

    /* Disable the Channel: Reset the CCxE Bit */
    TIMx->CCEN &= (uint16_t)~tmp1;

    if ((Channel == TIM_CH_1) || (Channel == TIM_CH_3))
    {
        tmp += (Channel >> 1);

        /* Reset the OCxM bits in the CCMRx register */
        *(__IO uint32_t*)tmp &= (uint32_t) ~((uint32_t)TIM_CCMOD1_OC1M);

        /* Configure the OCxM bits in the CCMRx register */
        *(__IO uint32_t*)tmp |= OcMode;
    }
    else
    {
        tmp += (uint16_t)(Channel - (uint16_t)4) >> (uint16_t)1;

        /* Reset the OCxM bits in the CCMRx register */
        *(__IO uint32_t*)tmp &= (uint32_t) ~((uint32_t)TIM_CCMOD1_OC2M);

        /* Configure the OCxM bits in the CCMRx register */
        *(__IO uint32_t*)tmp |= (uint16_t)(OcMode << 8);
    }
}

/**
 * @brief  Enables or Disables the TIMx Update event.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param Cmd new state of the TIMx UDIS bit
 *   This parameter can be: ENABLE or DISABLE.
 */
void TIM_EnableUpdateEvt(TIM_Module* TIMx, FunctionalState Cmd)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IS_FUNCTIONAL_STATE(Cmd));
    if (Cmd != DISABLE)
    {
        /* Set the Update Disable Bit */
        TIMx->CTRL1 |= TIM_CTRL1_UPDIS;
    }
    else
    {
        /* Reset the Update Disable Bit */
        TIMx->CTRL1 &= (uint32_t) ~((uint32_t)TIM_CTRL1_UPDIS);
    }
}

/**
  * @brief  Configures the TIMx Update Request Interrupt source.
  * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
  * @param TIM_UpdateSource specifies the Update source.
  *   This parameter can be one of the following values:
  *     @arg TIM_UPDATE_SRC_REGULAr Source of update is the counter overflow/underflow
                                       or the setting of UG bit, or an update generation
                                       through the slave mode controller.
  *     @arg TIM_UPDATE_SRC_GLOBAL Source of update is counter overflow/underflow.
  */
void TIM_ConfigUpdateRequestIntSrc(TIM_Module* TIMx, uint16_t TIM_UpdateSource)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimUpdateSrc(TIM_UpdateSource));
    if (TIM_UpdateSource != TIM_UPDATE_SRC_GLOBAL)
    {
        /* Set the URS Bit */
        TIMx->CTRL1 |= TIM_CTRL1_UPRS;
    }
    else
    {
        /* Reset the URS Bit */
        TIMx->CTRL1 &= (uint32_t) ~((uint32_t)TIM_CTRL1_UPRS);
    }
}

/**
 * @brief  Enables or disables the TIMx's Hall sensor interface.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Cmd new state of the TIMx Hall sensor interface.
 *   This parameter can be: ENABLE or DISABLE.
 */
void TIM_SelectHallSensor(TIM_Module* TIMx, FunctionalState Cmd)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    assert_param(IS_FUNCTIONAL_STATE(Cmd));
    if (Cmd != DISABLE)
    {
        /* Set the TI1S Bit */
        TIMx->CTRL2 |= TIM_CTRL2_TI1SEL;
    }
    else
    {
        /* Reset the TI1S Bit */
        TIMx->CTRL2 &= (uint32_t) ~((uint32_t)TIM_CTRL2_TI1SEL);
    }
}

/**
 * @brief  Selects the TIMx's One Pulse Mode.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param TIM_OPMode specifies the OPM Mode to be used.
 *   This parameter can be one of the following values:
 *     @arg TIM_OPMODE_SINGLE
 *     @arg TIM_OPMODE_REPET
 */
void TIM_SelectOnePulseMode(TIM_Module* TIMx, uint16_t TIM_OPMode)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimOpMOde(TIM_OPMode));
    /* Reset the OPM Bit */
    TIMx->CTRL1 &= (uint32_t) ~((uint32_t)TIM_CTRL1_ONEPM);
    /* Configure the OPM Mode */
    TIMx->CTRL1 |= TIM_OPMode;
}

/**
 * @brief  Selects the TIMx Trigger Output Mode.
 * @param TIMx where x can be 1, 3, 6 to select the TIM peripheral.
 * @param TIM_TRGOSource specifies the Trigger Output source.
 *   This paramter can be one of the following values:
 *
 *  - For all TIMx
 *     @arg TIM_TRGO_SRC_RESET The UG bit in the TIM_EVTGEN register is used as the trigger output (TRGO).
 *     @arg TIM_TRGO_SRC_ENABLE The Counter Enable CEN is used as the trigger output (TRGO).
 *     @arg TIM_TRGO_SRC_UPDATE The update event is selected as the trigger output (TRGO).
 *
 *  - For all TIMx except TIM6 
 *     @arg TIM_TRGO_SRC_OC1 The trigger output sends a positive pulse when the CC1IF flag
 *                              is to be set, as soon as a capture or compare match occurs (TRGO).
 *     @arg TIM_TRGO_SRC_OC1REF OC1REF signal is used as the trigger output (TRGO).
 *     @arg TIM_TRGO_SRC_OC2REF OC2REF signal is used as the trigger output (TRGO).
 *     @arg TIM_TRGO_SRC_OC3REF OC3REF signal is used as the trigger output (TRGO).
 *     @arg TIM_TRGO_SRC_OC4REF OC4REF signal is used as the trigger output (TRGO).
 *
 */
void TIM_SelectOutputTrig(TIM_Module* TIMx, uint16_t TIM_TRGOSource)
{
    /* Check the parameters */
    assert_param(IsTimList7Module(TIMx));
    assert_param(IsTimTrgoSrc(TIM_TRGOSource));
    /* Reset the MMS Bits */
    TIMx->CTRL2 &= (uint32_t) ~((uint32_t)TIM_CTRL2_MMSEL);
    /* Select the TRGO source */
    TIMx->CTRL2 |= TIM_TRGOSource;
}

/**
 * @brief  Selects the TIMx Slave Mode.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_SlaveMode specifies the Timer Slave Mode.
 *   This parameter can be one of the following values:
 *     @arg TIM_SLAVE_MODE_RESET Rising edge of the selected trigger signal (TRGI) re-initializes
 *                               the counter and triggers an update of the registers.
 *     @arg TIM_SLAVE_MODE_GATED The counter clock is enabled when the trigger signal (TRGI) is high.
 *     @arg TIM_SLAVE_MODE_TRIG The counter starts at a rising edge of the trigger TRGI.
 *     @arg TIM_SLAVE_MODE_EXT1 Rising edges of the selected trigger (TRGI) clock the counter.
 */
void TIM_SelectSlaveMode(TIM_Module* TIMx, uint16_t TIM_SlaveMode)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    assert_param(IsTimSlaveMode(TIM_SlaveMode));
    /* Reset the SMS Bits */
    TIMx->SMCTRL &= (uint16_t) ~((uint16_t)TIM_SMCTRL_SMSEL);
    /* Select the Slave Mode */
    TIMx->SMCTRL |= TIM_SlaveMode;
}

/**
 * @brief  Sets or Resets the TIMx Master/Slave Mode.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_MasterSlaveMode specifies the Timer Master Slave Mode.
 *   This parameter can be one of the following values:
 *     @arg TIM_MASTER_SLAVE_MODE_ENABLE synchronization between the current timer
 *                                      and its slaves (through TRGO).
 *     @arg TIM_MASTER_SLAVE_MODE_DISABLE No action
 */
void TIM_SelectMasterSlaveMode(TIM_Module* TIMx, uint16_t TIM_MasterSlaveMode)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    assert_param(IsTimMasterSlaveMode(TIM_MasterSlaveMode));
    /* Reset the MSM Bit */
    TIMx->SMCTRL &= (uint16_t) ~((uint16_t)TIM_SMCTRL_MSMD);

    /* Set or Reset the MSM Bit */
    TIMx->SMCTRL |= TIM_MasterSlaveMode;
}

/**
 * @brief  Sets the TIMx Counter Register value
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param Counter specifies the Counter register new value.
 */
void TIM_SetCnt(TIM_Module* TIMx, uint16_t Counter)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    /* Set the Counter Register value */
    TIMx->CNT = Counter;
}

/**
 * @brief  Sets the TIMx Autoreload Register value
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param Autoreload specifies the Autoreload register new value.
 */
void TIM_SetAutoReload(TIM_Module* TIMx, uint16_t Autoreload)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    /* Set the Autoreload Register value */
    TIMx->AR = Autoreload;
}

/**
 * @brief  Sets the TIMx Capture Compare1 Register value
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Compare1 specifies the Capture Compare1 register new value.
 */
void TIM_SetCmp1(TIM_Module* TIMx, uint16_t Compare1)
{
    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    /* Set the Capture Compare1 Register value */
    TIMx->CCDAT1 = Compare1;
}

/**
 * @brief  Sets the TIMx Capture Compare2 Register value
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Compare2 specifies the Capture Compare2 register new value.
 */
void TIM_SetCmp2(TIM_Module* TIMx, uint16_t Compare2)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    /* Set the Capture Compare2 Register value */
    TIMx->CCDAT2 = Compare2;
}

/**
 * @brief  Sets the TIMx Capture Compare3 Register value
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Compare3 specifies the Capture Compare3 register new value.
 */
void TIM_SetCmp3(TIM_Module* TIMx, uint16_t Compare3)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    /* Set the Capture Compare3 Register value */
    TIMx->CCDAT3 = Compare3;
}

/**
 * @brief  Sets the TIMx Capture Compare4 Register value
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param Compare4 specifies the Capture Compare4 register new value.
 */
void TIM_SetCmp4(TIM_Module* TIMx, uint16_t Compare4)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    /* Set the Capture Compare4 Register value */
    TIMx->CCDAT4 = Compare4;
}


/**
 * @brief  Sets the TIMx Input Capture 1 prescaler.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_ICPSC specifies the Input Capture1 prescaler new value.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_PSC_DIV1 no prescaler
 *     @arg TIM_IC_PSC_DIV2 capture is done once every 2 events
 *     @arg TIM_IC_PSC_DIV4 capture is done once every 4 events
 *     @arg TIM_IC_PSC_DIV8 capture is done once every 8 events
 */
void TIM_SetInCap1Prescaler(TIM_Module* TIMx, uint16_t TIM_ICPSC)
{
    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    assert_param(IsTimIcPrescaler(TIM_ICPSC));
    /* Reset the IC1PSC Bits */
    TIMx->CCMOD1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_IC1PSC);
    /* Set the IC1PSC value */
    TIMx->CCMOD1 |= TIM_ICPSC;
}

/**
 * @brief  Sets the TIMx Input Capture 2 prescaler.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_ICPSC specifies the Input Capture2 prescaler new value.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_PSC_DIV1 no prescaler
 *     @arg TIM_IC_PSC_DIV2 capture is done once every 2 events
 *     @arg TIM_IC_PSC_DIV4 capture is done once every 4 events
 *     @arg TIM_IC_PSC_DIV8 capture is done once every 8 events
 */
void TIM_SetInCap2Prescaler(TIM_Module* TIMx, uint16_t TIM_ICPSC)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    assert_param(IsTimIcPrescaler(TIM_ICPSC));
    /* Reset the IC2PSC Bits */
    TIMx->CCMOD1 &= (uint16_t) ~((uint16_t)TIM_CCMOD1_IC2PSC);
    /* Set the IC2PSC value */
    TIMx->CCMOD1 |= (uint16_t)(TIM_ICPSC << 8);
}

/**
 * @brief  Sets the TIMx Input Capture 3 prescaler.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_ICPSC specifies the Input Capture3 prescaler new value.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_PSC_DIV1 no prescaler
 *     @arg TIM_IC_PSC_DIV2 capture is done once every 2 events
 *     @arg TIM_IC_PSC_DIV4 capture is done once every 4 events
 *     @arg TIM_IC_PSC_DIV8 capture is done once every 8 events
 */
void TIM_SetInCap3Prescaler(TIM_Module* TIMx, uint16_t TIM_ICPSC)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimIcPrescaler(TIM_ICPSC));
    /* Reset the IC3PSC Bits */
    TIMx->CCMOD2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_IC3PSC);
    /* Set the IC3PSC value */
    TIMx->CCMOD2 |= TIM_ICPSC;
}

/**
 * @brief  Sets the TIMx Input Capture 4 prescaler.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param TIM_ICPSC specifies the Input Capture4 prescaler new value.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_PSC_DIV1 no prescaler
 *     @arg TIM_IC_PSC_DIV2 capture is done once every 2 events
 *     @arg TIM_IC_PSC_DIV4 capture is done once every 4 events
 *     @arg TIM_IC_PSC_DIV8 capture is done once every 8 events
 */
void TIM_SetInCap4Prescaler(TIM_Module* TIMx, uint16_t TIM_ICPSC)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    assert_param(IsTimIcPrescaler(TIM_ICPSC));
    /* Reset the IC4PSC Bits */
    TIMx->CCMOD2 &= (uint16_t) ~((uint16_t)TIM_CCMOD2_IC4PSC);
    /* Set the IC4PSC value */
    TIMx->CCMOD2 |= (uint16_t)(TIM_ICPSC << 8);
}

/**
 * @brief  Sets the TIMx Clock Division value.
 * @param TIMx where x can be 1 or 3 to select
 *   the TIM peripheral.
 * @param TIM_CKD specifies the clock division value.
 *   This parameter can be one of the following value:
 *     @arg TIM_CLK_DIV1 TDTS = Tck_tim
 *     @arg TIM_CLK_DIV2 TDTS = 2*Tck_tim
 *     @arg TIM_CLK_DIV4 TDTS = 4*Tck_tim
 */
void TIM_SetClkDiv(TIM_Module* TIMx, uint16_t TIM_CKD)
{
    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    assert_param(IsTimClkDiv(TIM_CKD));
    /* Reset the CKD Bits */
    TIMx->CTRL1 &= (uint32_t) ~((uint32_t)TIM_CTRL1_CLKD);
    /* Set the CKD value */
    TIMx->CTRL1 |= TIM_CKD;
}

/**
 * @brief  Gets the TIMx Input Capture 1 value.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @return Capture Compare 1 Register value.
 */
uint16_t TIM_GetCap1(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimList8Module(TIMx));
    /* Get the Capture 1 Register value */
    return TIMx->CCDAT1;
}

/**
 * @brief  Gets the TIMx Input Capture 2 value.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @return Capture Compare 2 Register value.
 */
uint16_t TIM_GetCap2(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimList6Module(TIMx));
    /* Get the Capture 2 Register value */
    return TIMx->CCDAT2;
}

/**
 * @brief  Gets the TIMx Input Capture 3 value.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @return Capture Compare 3 Register value.
 */
uint16_t TIM_GetCap3(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    /* Get the Capture 3 Register value */
    return TIMx->CCDAT3;
}

/**
 * @brief  Gets the TIMx Input Capture 4 value.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @return Capture Compare 4 Register value.
 */
uint16_t TIM_GetCap4(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimList3Module(TIMx));
    /* Get the Capture 4 Register value */
    return TIMx->CCDAT4;
}

/**
 * @brief  Gets the TIMx Counter value.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @return Counter Register value.
 */
uint16_t TIM_GetCnt(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    /* Get the Counter Register value */
    return TIMx->CNT;
}

/**
 * @brief  Gets the TIMx Prescaler value.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @return Prescaler Register value.
 */
uint16_t TIM_GetPrescaler(TIM_Module* TIMx)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    /* Get the Prescaler Register value */
    return TIMx->PSC;
}

/**
 * @brief  Checks whether the specified TIM flag is set or not.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param TIM_FLAG specifies the flag to check.
 *   This parameter can be one of the following values:
 *     @arg TIM_FLAG_UPDATE TIM update Flag
 *     @arg TIM_FLAG_CC1 TIM Capture Compare 1 Flag
 *     @arg TIM_FLAG_CC2 TIM Capture Compare 2 Flag
 *     @arg TIM_FLAG_CC3 TIM Capture Compare 3 Flag
 *     @arg TIM_FLAG_CC4 TIM Capture Compare 4 Flag
 *     @arg TIM_FLAG_COM TIM Commutation Flag
 *     @arg TIM_FLAG_TRIG TIM Trigger Flag
 *     @arg TIM_FLAG_BREAK TIM Break Flag
 *     @arg TIM_FLAG_CC1OF TIM Capture Compare 1 overcapture Flag
 *     @arg TIM_FLAG_CC2OF TIM Capture Compare 2 overcapture Flag
 *     @arg TIM_FLAG_CC3OF TIM Capture Compare 3 overcapture Flag
 *     @arg TIM_FLAG_CC4OF TIM Capture Compare 4 overcapture Flag
 *     @arg TIM_FLAG_CC5 TIM Capture Compare 5 Flag
 *     @arg TIM_FLAG_CC6 TIM Capture Compare 6 Flag
 * @note
 *   - TIM6 can have only one update flag.
 *   - TIM_FLAG_BREAK is used only with TIM1.
 *   - TIM_FLAG_COM is used only with TIM1.
 * @return The new state of TIM_FLAG (SET or RESET).
 */
FlagStatus TIM_GetFlagStatus(TIM_Module* TIMx, uint32_t TIM_FLAG)
{
    INTStatus bitstatus = RESET;
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimGetFlag(TIM_FLAG));

    if ((TIMx->STS & TIM_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    return bitstatus;
}

/**
 * @brief  Clears the TIMx's pending flags.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param TIM_FLAG specifies the flag bit to clear.
 *   This parameter can be any combination of the following values:
 *     @arg TIM_FLAG_UPDATE TIM update Flag
 *     @arg TIM_FLAG_CC1 TIM Capture Compare 1 Flag
 *     @arg TIM_FLAG_CC2 TIM Capture Compare 2 Flag
 *     @arg TIM_FLAG_CC3 TIM Capture Compare 3 Flag
 *     @arg TIM_FLAG_CC4 TIM Capture Compare 4 Flag
 *     @arg TIM_FLAG_COM TIM Commutation Flag
 *     @arg TIM_FLAG_TRIG TIM Trigger Flag
 *     @arg TIM_FLAG_BREAK TIM Break Flag
 *     @arg TIM_FLAG_CC1OF TIM Capture Compare 1 overcapture Flag
 *     @arg TIM_FLAG_CC2OF TIM Capture Compare 2 overcapture Flag
 *     @arg TIM_FLAG_CC3OF TIM Capture Compare 3 overcapture Flag
 *     @arg TIM_FLAG_CC4OF TIM Capture Compare 4 overcapture Flag
 * @note
 *   - TIM6 can have only one update flag.
 *   - TIM_FLAG_BREAK is used only with TIM1.
 *   - TIM_FLAG_COM is used only with TIM1.
 */
void TIM_ClearFlag(TIM_Module* TIMx, uint32_t TIM_FLAG)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimClrFlag(TIM_FLAG));

    /* Clear the flags */
    TIMx->STS &= (uint32_t)~TIM_FLAG;
}

/**
 * @brief  Checks whether the TIM interrupt has occurred or not.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param TIM_IT specifies the TIM interrupt source to check.
 *   This parameter can be one of the following values:
 *     @arg TIM_INT_UPDATE TIM update Interrupt source
 *     @arg TIM_INT_CC1 TIM Capture Compare 1 Interrupt source
 *     @arg TIM_INT_CC2 TIM Capture Compare 2 Interrupt source
 *     @arg TIM_INT_CC3 TIM Capture Compare 3 Interrupt source
 *     @arg TIM_INT_CC4 TIM Capture Compare 4 Interrupt source
 *     @arg TIM_INT_COM TIM Commutation Interrupt source
 *     @arg TIM_INT_TRIG TIM Trigger Interrupt source
 *     @arg TIM_INT_BREAK TIM Break Interrupt source
 * @note
 *   - TIM6 can generate only an update interrupt.
 *   - TIM_INT_BREAK is used only with TIM1.
 *   - TIM_INT_COM is used only with TIM1.
 * @return The new state of the TIM_IT(SET or RESET).
 */
INTStatus TIM_GetIntStatus(TIM_Module* TIMx, uint32_t TIM_IT)
{
    INTStatus bitstatus = RESET;
    uint32_t itstatus = 0x0, itenable = 0x0;
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimGetInt(TIM_IT));

    itstatus = TIMx->STS & TIM_IT;

    itenable = TIMx->DINTEN & TIM_IT;
    if ((itstatus != (uint32_t)RESET) && (itenable != (uint32_t)RESET))
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    return bitstatus;
}

/**
 * @brief  Clears the TIMx's interrupt pending bits.
 * @param TIMx where x can be 1, 3 or 6 to select the TIM peripheral.
 * @param TIM_IT specifies the pending bit to clear.
 *   This parameter can be any combination of the following values:
 *     @arg TIM_INT_UPDATE TIM1 update Interrupt source
 *     @arg TIM_INT_CC1 TIM Capture Compare 1 Interrupt source
 *     @arg TIM_INT_CC2 TIM Capture Compare 2 Interrupt source
 *     @arg TIM_INT_CC3 TIM Capture Compare 3 Interrupt source
 *     @arg TIM_INT_CC4 TIM Capture Compare 4 Interrupt source
 *     @arg TIM_INT_COM TIM Commutation Interrupt source
 *     @arg TIM_INT_TRIG TIM Trigger Interrupt source
 *     @arg TIM_INT_BREAK TIM Break Interrupt source
 * @note
 *   - TIM6 can generate only an update interrupt.
 *   - TIM_INT_BREAK is used only with TIM1.
 *   - TIM_INT_COM is used only with TIM1.
 */
void TIM_ClrIntPendingBit(TIM_Module* TIMx, uint32_t TIM_IT)
{
    /* Check the parameters */
    assert_param(IsTimAllModule(TIMx));
    assert_param(IsTimInt(TIM_IT));
    /* Clear the IT pending Bit */
    TIMx->STS &= (uint32_t)~TIM_IT;
}

/**
 * @brief  Configure the TI1 as Input.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param IcPolarity The Input Polarity.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_POLARITY_RISING
 *     @arg TIM_IC_POLARITY_FALLING
 * @param IcSelection specifies the input to be used.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_SELECTION_DIRECTTI TIM Input 1 is selected to be connected to IC1.
 *     @arg TIM_IC_SELECTION_INDIRECTTI TIM Input 1 is selected to be connected to IC2.
 *     @arg TIM_IC_SELECTION_TRC TIM Input 1 is selected to be connected to TRC.
 * @param IcFilter Specifies the Input Capture Filter.
 *   This parameter must be a value between 0x00 and 0x0F.
 */
static void ConfigTI1(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter)
{
    uint16_t tmpccmr1 = 0;
    uint32_t tmpccer  = 0;
    /* Disable the Channel 1: Reset the CC1E Bit */
    TIMx->CCEN &= (uint32_t) ~((uint32_t)TIM_CCEN_CC1EN);
    tmpccmr1 = TIMx->CCMOD1;
    tmpccer  = TIMx->CCEN;
    /* Select the Input and set the filter */
    tmpccmr1 &= (uint16_t)(((uint16_t) ~((uint16_t)TIM_CCMOD1_CC1SEL)) & ((uint16_t) ~((uint16_t)TIM_CCMOD1_IC1F)));
    tmpccmr1 |= (uint16_t)(IcSelection | (uint16_t)(IcFilter << (uint16_t)4));

    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        /* Select the Polarity and set the CC1E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC1P));
        tmpccer |= (uint32_t)(IcPolarity | (uint32_t)TIM_CCEN_CC1EN);
    }
    else
    {
        /* Select the Polarity and set the CC1E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC1P | TIM_CCEN_CC1NP));
        tmpccer |= (uint32_t)(IcPolarity | (uint32_t)TIM_CCEN_CC1EN);
    }

    /* Write to TIMx CCMOD1 and CCEN registers */
    TIMx->CCMOD1 = tmpccmr1;
    TIMx->CCEN   = tmpccer;
}

/**
 * @brief  Configure the TI2 as Input.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param IcPolarity The Input Polarity.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_POLARITY_RISING
 *     @arg TIM_IC_POLARITY_FALLING
 * @param IcSelection specifies the input to be used.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_SELECTION_DIRECTTI TIM Input 2 is selected to be connected to IC2.
 *     @arg TIM_IC_SELECTION_INDIRECTTI TIM Input 2 is selected to be connected to IC1.
 *     @arg TIM_IC_SELECTION_TRC TIM Input 2 is selected to be connected to TRC.
 * @param IcFilter Specifies the Input Capture Filter.
 *   This parameter must be a value between 0x00 and 0x0F.
 */
static void ConfigTI2(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter)
{
    uint16_t tmpccmr1 = 0;
    uint32_t tmpccer = 0, tmp = 0;
    /* Disable the Channel 2: Reset the CC2E Bit */
    TIMx->CCEN &= (uint32_t) ~((uint32_t)TIM_CCEN_CC2EN);
    tmpccmr1 = TIMx->CCMOD1;
    tmpccer  = TIMx->CCEN;
    tmp      = (uint32_t)(IcPolarity << 4);
    /* Select the Input and set the filter */
    tmpccmr1 &= (uint16_t)(((uint16_t) ~((uint16_t)TIM_CCMOD1_CC2SEL)) & ((uint16_t) ~((uint16_t)TIM_CCMOD1_IC2F)));
    tmpccmr1 |= (uint16_t)(IcFilter << 12);
    tmpccmr1 |= (uint16_t)(IcSelection << 8);

    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        /* Select the Polarity and set the CC2E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC2P));
        tmpccer |= (uint32_t)(tmp | (uint32_t)TIM_CCEN_CC2EN);
    }
    else
    {
        /* Select the Polarity and set the CC2E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC2P | TIM_CCEN_CC2NP));
        tmpccer |= (uint32_t)(IcPolarity | (uint32_t)TIM_CCEN_CC2EN);
    }

    /* Write to TIMx CCMOD1 and CCEN registers */
    TIMx->CCMOD1 = tmpccmr1;
    TIMx->CCEN   = tmpccer;
}

/**
 * @brief  Configure the TI3 as Input.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param IcPolarity The Input Polarity.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_POLARITY_RISING
 *     @arg TIM_IC_POLARITY_FALLING
 * @param IcSelection specifies the input to be used.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_SELECTION_DIRECTTI TIM Input 3 is selected to be connected to IC3.
 *     @arg TIM_IC_SELECTION_INDIRECTTI TIM Input 3 is selected to be connected to IC4.
 *     @arg TIM_IC_SELECTION_TRC TIM Input 3 is selected to be connected to TRC.
 * @param IcFilter Specifies the Input Capture Filter.
 *   This parameter must be a value between 0x00 and 0x0F.
 */
static void ConfigTI3(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter)
{
    uint16_t tmpccmr2 = 0;
    uint32_t tmpccer = 0, tmp = 0;
    /* Disable the Channel 3: Reset the CC3E Bit */
    TIMx->CCEN &= (uint32_t) ~((uint32_t)TIM_CCEN_CC3EN);
    tmpccmr2 = TIMx->CCMOD2;
    tmpccer  = TIMx->CCEN;
    tmp      = (uint32_t)(IcPolarity << 8);
    /* Select the Input and set the filter */
    tmpccmr2 &= (uint16_t)(((uint16_t) ~((uint16_t)TIM_CCMOD2_CC3SEL)) & ((uint16_t) ~((uint16_t)TIM_CCMOD2_IC3F)));
    tmpccmr2 |= (uint16_t)(IcSelection | (uint16_t)(IcFilter << (uint16_t)4));

    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        /* Select the Polarity and set the CC3E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC3P));
        tmpccer |= (uint32_t)(tmp | (uint32_t)TIM_CCEN_CC3EN);
    }
    else
    {
        /* Select the Polarity and set the CC3E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC3P | TIM_CCEN_CC3NP));
        tmpccer |= (uint32_t)(IcPolarity | (uint32_t)TIM_CCEN_CC3EN);
    }

    /* Write to TIMx CCMOD2 and CCEN registers */
    TIMx->CCMOD2 = tmpccmr2;
    TIMx->CCEN   = tmpccer;
}

/**
 * @brief  Configure the TI4 as Input.
 * @param TIMx where x can be 1 or 3 to select the TIM peripheral.
 * @param IcPolarity The Input Polarity.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_POLARITY_RISING
 *     @arg TIM_IC_POLARITY_FALLING
 * @param IcSelection specifies the input to be used.
 *   This parameter can be one of the following values:
 *     @arg TIM_IC_SELECTION_DIRECTTI TIM Input 4 is selected to be connected to IC4.
 *     @arg TIM_IC_SELECTION_INDIRECTTI TIM Input 4 is selected to be connected to IC3.
 *     @arg TIM_IC_SELECTION_TRC TIM Input 4 is selected to be connected to TRC.
 * @param IcFilter Specifies the Input Capture Filter.
 *   This parameter must be a value between 0x00 and 0x0F.
 */
static void ConfigTI4(TIM_Module* TIMx, uint16_t IcPolarity, uint16_t IcSelection, uint16_t IcFilter)
{
    uint16_t tmpccmr2 = 0;
    uint32_t tmpccer = 0, tmp = 0;

    /* Disable the Channel 4: Reset the CC4E Bit */
    TIMx->CCEN &= (uint32_t) ~((uint32_t)TIM_CCEN_CC4EN);
    tmpccmr2 = TIMx->CCMOD2;
    tmpccer  = TIMx->CCEN;
    tmp      = (uint32_t)(IcPolarity << 12);
    /* Select the Input and set the filter */
    tmpccmr2 &= (uint16_t)((uint16_t)(~(uint16_t)TIM_CCMOD2_CC4SEL) & ((uint16_t) ~((uint16_t)TIM_CCMOD2_IC4F)));
    tmpccmr2 |= (uint16_t)(IcSelection << 8);
    tmpccmr2 |= (uint16_t)(IcFilter << 12);

    if ((TIMx == TIM1) || (TIMx == TIM3))
    {
        /* Select the Polarity and set the CC4E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC4P));
        tmpccer |= (uint32_t)(tmp | (uint32_t)TIM_CCEN_CC4EN);
    }
    else
    {
        /* Select the Polarity and set the CC4E Bit */
        tmpccer &= (uint32_t) ~((uint32_t)(TIM_CCEN_CC4P));
        tmpccer |= (uint32_t)(IcPolarity | (uint32_t)TIM_CCEN_CC4EN);
    }
    /* Write to TIMx CCMOD2 and CCEN registers */
    TIMx->CCMOD2 = tmpccmr2;
    TIMx->CCEN   = tmpccer;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
