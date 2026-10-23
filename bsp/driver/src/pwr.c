#include "pwr.h"
#include "co_printf.h"

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
    volatile uint32_t _delay;

    /* Set BLE modem sleep */
    *(uint32_t *)(BLE_BASE + 0x30) = 0x07;
    while(PWR->CR1 & PWR_CR1_OSC_EN);

    /* Select SLEEP mode — 电压调节器开始从正常模式切换到低功耗模式 */
    tmpreg = PWR->CR1;
    tmpreg &= ~PWR_CR1_MODE_SEL;
    tmpreg |= (PWR_CR1_MODE_SLEEP | PWR_CR1_MODE_EN);
    PWR->CR1 = tmpreg;

    /*
     * 问题：由待机态切入休眠态时，切入后瞬间就被唤醒，偶然加入打印后问题解决，
     * 后将打印替换为延时，问题依旧解决以下为原因猜测。
     * ★ 关键延时: PWR_CR1 写入 MODE_SLEEP 后, 芯片内部电压调节器从 Active
     *    模式切换到低功耗模式需要约 3-10ms。必须等它稳定后再设 SLEEPDEEP + WFI,
     *    否则 WFI 进去时调节器还在过渡态 → HardFault (RST flags=0x28)。
     *
     * ── 为什么 PAIRING/RECONNECTING 进 SLEEP 不需要等？ ──
     *   配对/回连态下 CPU 全程在跑, BLE 仅广播, 电压调节器始终稳定在 Active。
     *   PWR_CR1 一切, 调节器立刻完成过渡。PAIRING→SLEEP 路径正常即因此。
     *
     * ── 为什么 STANDBY 进 SLEEP 必须等？ ──
     *   待机态下 br_sleep() 在 Standby ↔ Active 之间反复切。STANDBY 超时触发
     *   SLEEP 时, 调节器刚被 br_sleep() 从 Standby 唤醒, 正从低功耗往全功率爬升,
     *   此刻 PWR_CR1 突然命令它往 SLEEP 走——方向反转, 过渡态异常长。
     *   若无此延时, 后续 SLEEPDEEP+WFI 进去时调节器尚未就绪 → HardFault。
     *
     *   co_printf(~3.5ms @115200bps) 曾无意中卡在这个位置提供了足够延时。
     *   此处用 80000×NOP(~5ms@64MHz) 空循环替代, 不依赖任何外设。
     */
    for (_delay = 0u; _delay < 80000u; _delay++) { __NOP(); }

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP;

    /* Enter SLEEP mode */
    if (PWR_SleepEntry == PWR_SLEEPENTRY_WFI) {
        __WFI();
    } else {
        __SEV(); __WFE(); __WFE();
    }

    /* Reset SLEEPDEEP bit */
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


void PWR_EnterSTANDBYMode(uint8_t PWR_StandbyEntry)
{
    uint32_t tmpreg;

    co_printf("[STBY S1] CR2\r\n");
    tmpreg = PWR->CR2;
    tmpreg &= ~PWR_CR2_BLE_STATE;
    tmpreg |= (PWR_CR2_PAD_STA | PWR_CR2_BLE_STATE_ACTIVE
               | PWR_CR2_CORE_32KMEM_LP | PWR_CR2_CORE_16KMEM_LP);
    PWR->CR2 = (uint16_t)tmpreg;

    co_printf("[STBY S2] CR1\r\n");
    tmpreg = PWR->CR1;
    tmpreg &= ~PWR_CR1_MODE_SEL;
    tmpreg |= (PWR_CR1_MODE_STANDBY | PWR_CR1_MODE_EN);
    PWR->CR1 = tmpreg;

    co_printf("[STBY S3] SLEEPDEEP+WFI\r\n");
    SCB->SCR |= SCB_SCR_SLEEPDEEP;

    if (PWR_StandbyEntry == PWR_STANDBYENTRY_WFI) {
        __WFI();
    } else {
        __SEV(); __WFE(); __WFE();
    }

    co_printf("[STBY S4] WFI returned\r\n");
    SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP);
}
