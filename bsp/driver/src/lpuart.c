#include "lpuart.h"
#include "rcc.h"


#define STS_CLR_MASK        ((uint16_t)0x01BF) /*!< LPUART STS Mask */

#define INTEN_CLR_MASK      ((uint16_t)0x0000) /*!< LPUART INTEN Mask */
#define INT_MASK            ((uint16_t)0x007F) /*!< LPUART Interrupt Mask */

#define CTRL_CLR_MASK           ((uint16_t)0x70F4) /*!< LPUART CTRL Mask */
#define CTRL_SMPCNT_MASK        ((uint16_t)0x3FFF) /*!< LPUART Sampling Method Mask */
#define CTRL_WUSTP_MASK         ((uint16_t)0x4FFF) /*!< LPUART WakeUp Method Mask */
#define CTRL_WUSTP_SET          ((uint16_t)0x0080) /*!< LPUART stop mode Enable Mask */
#define CTRL_WUSTP_RESET        ((uint16_t)0x7F7F) /*!< LPUART stop mode Disable Mask */
#define CTRL_LOOPBACK_SET       ((uint16_t)0x0010) /*!< LPUART Loopback Test Enable Mask */
#define CTRL_LOOPBACK_RESET     ((uint16_t)0xFFEF) /*!< LPUART Loopback Test Disable Mask */
#define CTRL_FLUSH_SET          ((uint16_t)0x0004) /*!< LPUART Flush Receiver FIFO Enable Mask */
#define CTRL_FLUSH_RESET        ((uint16_t)0x7FFB) /*!< LPUART Flush Receiver FIFO Disable Mask */

/*-------------------------------------------------------------------
void LPUART_DeInit( LPUART_Module* LPUARTx )LPUART的设备初始化，
		寄存器回到默认值	 LPUART1
-------------------------------------------------------------------*/		
void LPUART_DeInit(LPUART_Module* LPUARTx)
{
    if(LPUARTx == LPUART1)
    {
        RCC->LSCTRL |=RCC_LSCTRL_LPUARTRST;
        RCC->LSCTRL &=~RCC_LSCTRL_LPUARTRST;
    }
}

/*-------------------------------------------------------------------
void LPUART_Init(LPUART_Module* LPUARTx, LPUART_InitType* LPUART_InitStruct)
		LPUART初始化
-------------------------------------------------------------------*/
void LPUART_Init(LPUART_Module* LPUARTx, LPUART_InitType* LPUART_InitStruct)
{
    uint32_t tmpregister = 0x00, clocksrc = 0x00, apbclock = 0x00;
    uint32_t integerdivider    = 0x00;
    uint32_t fractionaldivider = 0x00;
    uint32_t tmpdivider = 0x00, lastdivider = 0x00, i = 0x00;
    uint32_t lpuartxbase       = 0;
    RCC_ClocksType RCC_ClocksStatus;

    lpuartxbase = (uint32_t)LPUARTx;

    /*---------------------------- LPUART CTRL Configuration -----------------------*/
    tmpregister = LPUARTx->CTRL;
    /* Clear FC_RXEN, FC_TXEN, RTS_THSEL[1:0], PCDIS, TRS and PSEL bits */
    tmpregister &= CTRL_CLR_MASK;
    /* Configure the LPUART Parity, Mode, RtsThrehold and HardwareFlowControl ----------------------- */
    /* Set PCDIS and PSEL bits according to Parity value */
    /* Set the TRS bit according to Mode */
    /* Set RTS_THSEL[1:0] bits according to RtsThrehold */
    /* Set FC_RXEN and FC_TXEN bits according to HardwareFlowControl */
    tmpregister |= (uint32_t)LPUART_InitStruct->Parity | LPUART_InitStruct->Mode | LPUART_InitStruct->RtsThreshold | LPUART_InitStruct->HardwareFlowControl;
    /* Write to LPUART CTRL */
    LPUARTx->CTRL = (uint16_t)tmpregister;
    
    /*---------------------------- LPUART BRCFG1 & 2 Configuration -----------------------*/
    /* Configure the LPUART Baud Rate -------------------------------------------*/
    if(lpuartxbase == LPUART1_BASE)
    {
        clocksrc = RCC_GetLpuartClkSrc(RCC_LPUART1CLK);
    }   
    if (clocksrc == RCC_LSCTRL_LPUART_LSI)
    {
        #ifdef LSI_TRIM_32768HZ
        apbclock = 0x8000; // 32.768kHz, should the same with your LSI trim
        #elif defined LSI_TRIM_28800HZ
        apbclock = 28800;  // 28.8kHz, should the same with your LSI trim
        #elif defined LSI_TRIM_32000HZ
        apbclock = 0x7D00;  // 32kHz, should the same with your LSI trim
        #else
        apbclock = 0x8000; // LSI default is 32.768kHz
        #endif
    }
    else if (clocksrc == RCC_LSCTRL_LPUART_LSE)
    {
        apbclock = 0x8000; // 32.768kHz
    }
    else // APB1
    {
        RCC_GetClocksFreqValue(&RCC_ClocksStatus);
        apbclock = RCC_ClocksStatus.Pclk1Freq;
    }

    /* Determine the integer part */
    integerdivider = apbclock / (LPUART_InitStruct->BaudRate);  

    /* Configure sampling method */
    if(integerdivider <= 10)
    {
        LPUART_ConfigSamplingMethod(LPUARTx, LPUART_SMPCNT_1B);
    } 
    else
    {
        LPUART_ConfigSamplingMethod(LPUARTx, LPUART_SMPCNT_3B);
    } 

    /* Check baudrate */ 
    assert_param(IS_LPUART_BAUDRATE(integerdivider));
    /* Write to LPUART BRCFG1 */
    LPUARTx->BRCFG1 = (uint16_t)integerdivider; 

    /* Determine the fractional part */
    fractionaldivider = ((apbclock % (LPUART_InitStruct->BaudRate)) * 10000) / (LPUART_InitStruct->BaudRate);
    
    tmpregister = 0x00;
    tmpdivider = fractionaldivider;
    /* Implement the fractional part in the register */
    for( i = 0; i < 8; i++)
    {   
        lastdivider = tmpdivider;     
        tmpdivider = lastdivider + fractionaldivider;
        if((tmpdivider / 10000) ^ (lastdivider / 10000))
        {
            tmpregister |= (0x01 << i);
        }        
    } 
    /* Write to LPUART BRCFG2 */
    LPUARTx->BRCFG2 = (uint8_t)tmpregister; 
}

/*-------------------------------------------------------------------
void LPUART_StructInit(LPUART_InitType* LPUART_InitStruct)
		LPUART的初始化结构的初始化
-------------------------------------------------------------------*/
void LPUART_StructInit(LPUART_InitType* LPUART_InitStruct)
{
    LPUART_InitStruct->BaudRate            = 9600;
    LPUART_InitStruct->Parity              = LPUART_PE_NO;
    LPUART_InitStruct->Mode                = LPUART_MODE_RX | LPUART_MODE_TX;
    LPUART_InitStruct->RtsThreshold        = LPUART_RTSTH_FIFOFU;
    LPUART_InitStruct->HardwareFlowControl = LPUART_HFCTRL_NONE;
}

/*-------------------------------------------------------------------
void LPUART_FlushRxFifo(LPUART_Module* LPUARTx)
		LPUART清洗接收FiFO 
-------------------------------------------------------------------*/
void LPUART_FlushRxFifo(LPUART_Module* LPUARTx)
{
    /* Clear LPUART Flush Receiver FIFO */
    LPUARTx->CTRL |= CTRL_FLUSH_SET;
    while(LPUART_GetFlagStatus(LPUARTx, LPUART_FLAG_FIFO_NE) != RESET)
    {        
    }
    LPUARTx->CTRL &= CTRL_FLUSH_RESET;
}

/*-------------------------------------------------------------------
void LPUART_ConfigInt(LPUART_Module* LPUARTx,uint16_t LPUART_INT,FunctionalState Cmd )
			配置LPUART的中断  可以同时使能多个中断
			LPUART_INT = LPUART_INT_WUF							wake up				
									 LPUART_INT_FIFO_NE FIFO		Non-Empty Interrupt
									 LPUART_INT_FIFO_HF FIFO 		Half Full Interrupt
									 LPUART_INT_FIFO_FU FIFO 		Full Interrupt Enable
									 LPUART_INT_FIFO_OV FIFO		Overflow Interrupt
									 LPUART_INT_TXC 						TX Complete Interrupt
									 LPUART_INT_PE 							Parity Check Error Interrupt
			Cmd =	ENABLE or DISABLE
-------------------------------------------------------------------*/
void LPUART_ConfigInt(LPUART_Module* LPUARTx, uint16_t LPUART_INT, FunctionalState Cmd)
{    
    if (Cmd != DISABLE)
    {
       LPUARTx->INTEN |= (uint8_t)LPUART_INT;
    }
    else
    {
        LPUARTx->INTEN &= (uint8_t)(~LPUART_INT);
    }
}

/*-------------------------------------------------------------------
void LPUART_EnableDMA(LPUART_Module* LPUARTx,uint16_t LPUART_DMAReq,FunctionalState Cmd)
		使能/禁止 LPUART使用DMA   可以同时使能发送和接收DMA
		LPUART_DMAReq = LPUART_DMAREQ_TX LPUART DMA transmit request
										LPUART_DMAREQ_RX LPUART DMA receive request 		
		Cmd = ENABLE or DISABLE.
-------------------------------------------------------------------*/
void LPUART_EnableDMA(LPUART_Module* LPUARTx, uint16_t LPUART_DMAReq, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the DMA transfer by setting the DMA_RXEN and/or DMA_TXEN bits in the LPUART_CTRL register */
        LPUARTx->CTRL |= LPUART_DMAReq;
    }
    else
    {
        /* Disable the DMA transfer by clearing the DMA_RXEN and/or DMA_TXEN bits in the LPUART_CTRL register */
        LPUARTx->CTRL &= (uint16_t)(~LPUART_DMAReq);
    }
}

/*-------------------------------------------------------------------
void LPUART_ConfigWakeUpMethod(LPUART_Module* LPUARTx,uint16_t LPUART_WakeUpMethod)
		设置LPUART的唤醒模式
		LPUART_WakeUpMethod = LPUART_WUSTP_STARTBIT WakeUp by Start Bit Detection
													LPUART_WUSTP_RXNE 		WakeUp by RXNE Detection
													LPUART_WUSTP_BYTE 		WakeUp by A Configurable Received Byte
													LPUART_WUSTP_FRAME 		WakeUp by A Programmed 4-Byte Frame
-------------------------------------------------------------------*/
void LPUART_ConfigWakeUpMethod(LPUART_Module* LPUARTx, uint16_t LPUART_WakeUpMethod)
{
    LPUARTx->CTRL &= CTRL_WUSTP_MASK;
    LPUARTx->CTRL |= LPUART_WakeUpMethod;
}

/*-------------------------------------------------------------------
void LPUART_EnableWakeUpStop(LPUART_Module* LPUARTx, FunctionalState Cmd)
			允许/禁止在LPUART STOP模式下的唤醒 ENABLE or DISABLE.
-------------------------------------------------------------------*/
void LPUART_EnableWakeUpStop(LPUART_Module* LPUARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable Wakeup in STOP mode by setting the WUSTP bit in the CTRL register */        
        LPUARTx->CTRL |= CTRL_WUSTP_SET;
    }
    else
    {
        /* Disable Wakeup in STOP mode by clearing the WUSTP bit in the CTRL register */
        LPUARTx->CTRL &= CTRL_WUSTP_RESET;
    }
}

/*-------------------------------------------------------------------
void LPUART_ConfigSamplingMethod(LPUART_Module* LPUARTx,uint16_t LPUART_SamplingMethod)
		配置LPUART的采样模式
		LPUART_SamplingMethod = LPUART_SMPCNT_3B 3 Sample bit
														LPUART_SMPCNT_1B 1 Sample bit
-------------------------------------------------------------------*/
void LPUART_ConfigSamplingMethod(LPUART_Module* LPUARTx, uint16_t LPUART_SamplingMethod)
{
    LPUARTx->CTRL &= CTRL_SMPCNT_MASK;
    LPUARTx->CTRL |= LPUART_SamplingMethod;
}

/*-------------------------------------------------------------------
void LPUART_EnableLoopBack(LPUART_Module* LPUARTx, FunctionalState Cmd)
			允许/禁止LPUART的 回路反馈自测模式 ENABLE or DISABLE.
-------------------------------------------------------------------*/
void LPUART_EnableLoopBack(LPUART_Module* LPUARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable LPUART Loop Back Self-Test by setting the LOOKBACK bit in the CTRL register */        
        LPUARTx->CTRL |= CTRL_LOOPBACK_SET;
    }
    else
    {
        /* Disable LPUART Loop Back Self-Test by clearing the LOOKBACK bit in the CTRL register */
        LPUARTx->CTRL &= CTRL_LOOPBACK_RESET;
    }
}

/*-------------------------------------------------------------------
void LPUART_SendData(LPUART_Module* LPUARTx, uint8_t Data)
		启动LPUART发送一个字节
-------------------------------------------------------------------*/
void LPUART_SendData(LPUART_Module* LPUARTx, uint8_t Data)
{
    /* Transmit Data */
    LPUARTx->DAT = (Data & (uint8_t)0xFF);
}

/*-------------------------------------------------------------------
uint8_t LPUART_ReceiveData(LPUART_Module* LPUARTx)
			读取LPUART收到的字节
-------------------------------------------------------------------*/
uint8_t LPUART_ReceiveData(LPUART_Module* LPUARTx)
{
    /* Receive Data */
    return (uint8_t)(LPUARTx->DAT & (uint8_t)0xFF);
}

/*-------------------------------------------------------------------
void LPUART_ConfigWakeUpData(LPUART_Module* LPUARTx, uint32_t LPUART_WakeUpData)
			设置LPUART的唤醒数据 	当收到的数据字节或者帧相符时把CPU从STOP模式唤醒
			LPUART_WakeUpData指定的唤醒字节或者帧 最多4字节
-------------------------------------------------------------------*/
void LPUART_ConfigWakeUpData(LPUART_Module* LPUARTx,uint32_t LPUART_WakeUpData)
{     
    LPUARTx->WUDAT = LPUART_WakeUpData;
}

/*-------------------------------------------------------------------
FlagStatus LPUART_GetFlagStatus(LPUART_Module* LPUARTx,uint16_t LPUART_FLAG)
			读取LPUART的某个标志位
			LPUART_FLAG = LPUART_FLAG_PEF 			Parity Check Error Flag.
										LPUART_FLAG_TXC 			TX Complete Flag.
										LPUART_FLAG_FIFO_OV 	FIFO Overflow Flag.
										LPUART_FLAG_FIFO_FU 	FIFO Full Flag.
										LPUART_FLAG_FIFO_HF 	FIFO Half Full Flag.
										LPUART_FLAG_FIFO_NE 	FIFO Non-Empty Flag.
										LPUART_FLAG_CTS 			CTS Change(Hardware Flow Control) Flag.
										LPUART_FLAG_WUF				Wakeup from STOP mode Flag.
										LPUART_FLAG_NF 				Noise Detection Flag.
返回	SET  RESET
-------------------------------------------------------------------*/
FlagStatus LPUART_GetFlagStatus(LPUART_Module* LPUARTx, uint16_t LPUART_FLAG)
{
    FlagStatus bitstatus = RESET;

    if ((LPUARTx->STS & LPUART_FLAG) != (uint16_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    return bitstatus;
}

/*-------------------------------------------------------------------
void LPUART_ClrFlag(LPUART_Module* LPUARTx, uint16_t LPUART_FLAG)
		清除LPUART的标志位
		LPUART_FLAG = LPUART_FLAG_PEF 		Parity Check Error Flag.
									LPUART_FLAG_TXC 		TX Complete Flag.
									LPUART_FLAG_FIFO_OV FIFO Overflow Flag.
									LPUART_FLAG_FIFO_FU FIFO Full Flag.
									LPUART_FLAG_FIFO_HF FIFO Half Full Flag.
									LPUART_FLAG_FIFO_NE FIFO Non-Empty Flag.
									LPUART_FLAG_CTS CTS Change(Hardware Flow Control) Flag.
									LPUART_FLAG_WUF			Wakeup from STOP mode Flag.
									LPUART_FLAG_NF 			Noise Detection Flag.
-------------------------------------------------------------------*/
void LPUART_ClrFlag(LPUART_Module* LPUARTx, uint16_t LPUART_FLAG)
{
    LPUARTx->STS = (uint16_t)LPUART_FLAG;
}

/*-------------------------------------------------------------------
INTStatus LPUART_GetIntStatus(LPUART_Module* LPUARTx, uint16_t LPUART_INT)
			获取中断标志位
			LPUART_INT = 	LPUART_INT_WUF 			Wake-Up Interrupt
										LPUART_INT_FIFO_NE 	FIFO Non-Empty Interrupt
										LPUART_INT_FIFO_HF 	FIFO Half Full Interrupt
										LPUART_INT_FIFO_FU 	FIFO Full Interrupt Enable
										LPUART_INT_FIFO_OV 	FIFO Overflow Interrupt
										LPUART_INT_TXC 			TX Complete Interrupt
										LPUART_INT_PE 			Parity Check Error Interrupt
返回值 = SET or RESET
-------------------------------------------------------------------*/
INTStatus LPUART_GetIntStatus(LPUART_Module* LPUARTx, uint16_t LPUART_INT)
{
    uint32_t bitpos = 0x00, itmask = 0x00;
    INTStatus bitstatus = RESET;
    
    /* Get the interrupt position */
    itmask = (uint8_t)(LPUART_INT >> 0x08) & INT_MASK;
    itmask = (uint32_t)0x01 << itmask;
    itmask &= LPUARTx->INTEN;

    bitpos = ((uint8_t)LPUART_INT) & 0xFF;
    bitpos &= LPUARTx->STS;
    if ((itmask != (uint16_t)RESET) && (bitpos != (uint16_t)RESET))
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}

/*-------------------------------------------------------------------
void LPUART_ClrIntPendingBit(LPUART_Module* LPUARTx,uint16_t LPUART_INT)
		清除LPUART的中断标志位	
		LPUART_INT = 	LPUART_INT_WUF 			Wake-Up Interrupt
									LPUART_INT_FIFO_NE 	FIFO Non-Empty Interrupt
									LPUART_INT_FIFO_HF 	FIFO Half Full Interrupt
									LPUART_INT_FIFO_FU 	FIFO Full Interrupt Enable
									LPUART_INT_FIFO_OV 	FIFO Overflow Interrupt
									LPUART_INT_TXC TX 	Complete Interrupt
									LPUART_INT_PE 			Parity Check Error Interrupt 
-------------------------------------------------------------------*/
void LPUART_ClrIntPendingBit(LPUART_Module* LPUARTx,uint16_t LPUART_INT)
{
    uint16_t itmask = 0x00;
    
    itmask      = ((uint8_t)LPUART_INT) & 0xFF;
    LPUARTx->STS = (uint16_t)itmask;
}

