#include "pwr.h"

/*-------------------------------------------------------------------
void PWR_DeInit(void)电源域设备的初始化
-------------------------------------------------------------------*/
void PWR_DeInit(void)
{
    RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_PWR, ENABLE);
    RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_PWR, DISABLE);
}

/*-------------------------------------------------------------------
void PWR_EnterIDLEMode(uint8_t IDLEONEXIT, uint8_t PWR_IDLEEntry)
		单片机进入IDLE模式
		IDLEONEXIT = 推出IDLE后是否继续SLEEP
									DISABLE: SLEEP mode with SLEEPONEXIT disable
									ENABLE : SLEEP mode with SLEEPONEXIT enable
		PWR_IDLEEntry = PWR_IDLEENTRY_WFI: enter IDLE mode with WFI instruction
										PWR_IDLEENTRY_WFE: enter IDLE mode with WFE instruction
-------------------------------------------------------------------*/
void PWR_EnterIDLEMode(uint8_t IDLEONEXIT, uint8_t PWR_IDLEEntry)
{  
  
    /* CLEAR SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP); 

      /* Select SLEEPONEXIT mode entry --------------------------------------------------*/
    if(IDLEONEXIT == ENABLE)
    {   
        /* the MCU enters Sleep mode as soon as it exits the lowest priority ISR */
        SCB->SCR |= SCB_SCR_SLEEPONEXIT;
    }
    else if(IDLEONEXIT == DISABLE)
    {
        /* Sleep-now */
        SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPONEXIT);
    }       
      
    /* Select SLEEP mode entry --------------------------------------------------*/
    if(PWR_IDLEEntry == PWR_IDLEENTRY_WFI)
    {   
        /* Request Wait For Interrupt */
        __WFI();        
    }
    else
    {
        /* Request Wait For Event */
        __SEV();
        __WFE();
        __WFE();
    }    
}

/*-------------------------------------------------------------------
void PWR_EnterSLEEPMode(uint8_t PWR_SleepEntry)进入SLEEP模式
PWR_SleepEntry = 	PWR_SLEEPENTRY_WFI: enter SLEEP mode with WFI instruction
									PWR_SLEEPENTRY_WFE: enter SLEEP mode with WFE instruction
-------------------------------------------------------------------*/
void PWR_EnterSLEEPMode(uint8_t PWR_SleepEntry)
{
    uint32_t tmpreg = 0;
    /* Set BLE modem sleep */
    *(uint32_t *)(BLE_BASE + 0x30) = 0x07;
    while(PWR->CR1&PWR_CR1_OSC_EN);
      
    /* Select the regulator state in SLEEP mode ---------------------------------*/
    tmpreg = PWR->CR1;
    /* Clear PDDS and FLPDS bits */
    tmpreg &= ~PWR_CR1_MODE_SEL;
    /* Set FLPDS bit according to PWR_Regulator value */    
    tmpreg |= (PWR_CR1_MODE_SLEEP |PWR_CR1_MODE_EN);
    /* Store the new value */
    PWR->CR1 = tmpreg;
    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP;

    /* Select SLEEP mode entry --------------------------------------------------*/
    if(PWR_SleepEntry == PWR_SLEEPENTRY_WFI)
    {   
        /* Request Wait For Interrupt */
        __WFI();
    }
    else
    {
        /* Request Wait For Event */
        __SEV();
        __WFE();
        __WFE();
    }

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP);  
}

/*-------------------------------------------------------------------
void PWR_EnterPDMode(uint8_t PWR_PDEntry)进入掉电模式
		PWR_PDEntry = PWR_PDENTRY_WFI: enter PD mode with WFI instruction
									PWR_PDENTRY_WFE: enter PD mode with WFE instruction
-------------------------------------------------------------------*/
void PWR_EnterPDMode(uint8_t PWR_PDEntry)
{
    uint32_t tmpreg = 0; 
    /* Select the regulator state in SHUTDOWN mode ---------------------------------*/
    tmpreg = PWR->CR1;
    /* Clear PDDS  bits */
    tmpreg &= ~PWR_CR1_MODE_SEL;
    /* Set FLPDS bit according to PWR_Regulator value */    
    tmpreg |= PWR_CR1_MODE_PD|PWR_CR1_MODE_EN;
    /* Store the new value */
    PWR->CR1 = tmpreg;    

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP;
    /* This option is used to ensure that store operations are completed */
    #if defined ( __CC_ARM   )
    __force_stores();
    #endif
    /* Select SHUTDOWN mode entry --------------------------------------------------*/
    if(PWR_PDEntry == PWR_PDENTRY_WFI)
    {   
        /* Request Wait For Interrupt */
        __WFI();
    }
    else
    {
        /* Request Wait For Event */
        __SEV();
        __WFE();
        __WFE();
    }
  
}

