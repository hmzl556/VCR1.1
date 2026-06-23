#include "usart.h"
#include "rcc.h"

#define CTRL1_UEN_SET   ((uint16_t)0x2000) /*!< USART Enable Mask */
#define CTRL1_UEN_RESET ((uint16_t)0xDFFF) /*!< USART Disable Mask */

#define CTRL1_WUM_MASK ((uint16_t)0xF7FF) /*!< USART WakeUp Method Mask */

#define CTRL1_RCVWU_SET   ((uint16_t)0x0002) /*!< USART mute mode Enable Mask */
#define CTRL1_RCVWU_RESET ((uint16_t)0xFFFD) /*!< USART mute mode Enable Mask */
#define CTRL1_SDBRK_SET   ((uint16_t)0x0001) /*!< USART Break Character send Mask */
#define CTRL1_CLR_MASK    ((uint16_t)0xE9F3) /*!< USART CTRL1 Mask */
#define CTRL2_ADDR_MASK   ((uint16_t)0xFFF0) /*!< USART address Mask */

#define CTRL2_LINMEN_SET   ((uint16_t)0x4000) /*!< USART LIN Enable Mask */
#define CTRL2_LINMEN_RESET ((uint16_t)0xBFFF) /*!< USART LIN Disable Mask */

#define CTRL2_LINBDL_MASK    ((uint16_t)0xFFDF) /*!< USART LIN Break detection Mask */
#define CTRL2_STPB_CLR_MASK  ((uint16_t)0xCFFF) /*!< USART CTRL2 STOP Bits Mask */
#define CTRL2_CLOCK_CLR_MASK ((uint16_t)0xF0FF) /*!< USART CTRL2 Clock Mask */

#define CTRL3_SCMEN_SET   ((uint16_t)0x0020) /*!< USART SC Enable Mask */
#define CTRL3_SCMEN_RESET ((uint16_t)0xFFDF) /*!< USART SC Disable Mask */

#define CTRL3_SCNACK_SET   ((uint16_t)0x0010) /*!< USART SC NACK Enable Mask */
#define CTRL3_SCNACK_RESET ((uint16_t)0xFFEF) /*!< USART SC NACK Disable Mask */

#define CTRL3_HDMEN_SET   ((uint16_t)0x0008) /*!< USART Half-Duplex Enable Mask */
#define CTRL3_HDMEN_RESET ((uint16_t)0xFFF7) /*!< USART Half-Duplex Disable Mask */

#define CTRL3_IRDALP_MASK ((uint16_t)0xFFFB) /*!< USART IrDA LowPower mode Mask */
#define CTRL3_CLR_MASK    ((uint16_t)0xFCFF) /*!< USART CTRL3 Mask */

#define CTRL3_IRDAMEN_SET   ((uint16_t)0x0002) /*!< USART IrDA Enable Mask */
#define CTRL3_IRDAMEN_RESET ((uint16_t)0xFFFD) /*!< USART IrDA Disable Mask */
#define GTP_LSB_MASK        ((uint16_t)0x00FF) /*!< Guard Time Register LSB Mask */
#define GTP_MSB_MASK        ((uint16_t)0xFF00) /*!< Guard Time Register MSB Mask */
#define INT_MASK            ((uint16_t)0x001F) /*!< USART Interrupt Mask */

/*-------------------------------------------------------------------
void USART_DeInit(USART_Module* USARTx)UART设备的初始化 寄存器回归
	USARTx = USART1 USART2
-------------------------------------------------------------------*/
void USART_DeInit(USART_Module* USARTx)
{
    if (USARTx == USART1)
    {
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_USART1, ENABLE);
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_USART1, DISABLE);
    }
    else
    {
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_USART2, ENABLE);
        RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_USART2, DISABLE);
    }

}

/*-------------------------------------------------------------------
void USART_Init(USART_Module* USARTx, USART_InitType* USART_InitStruct)
		串口初始化
		USARTx = USART1 USART2
		USART_InitStruct 初始化所需的数据结构
-------------------------------------------------------------------*/
void USART_Init(USART_Module* USARTx, USART_InitType* USART_InitStruct)
{
    uint32_t tmpregister = 0x00, apbclock = 0x00;
    uint32_t integerdivider    = 0x00;
    uint32_t fractionaldivider = 0x00;
    uint32_t usartxbase        = 0;
    RCC_ClocksType RCC_ClocksStatus;
	
    usartxbase = (uint32_t)USARTx;

    /*---------------------------- USART CTRL2 Configuration -----------------------*/
    tmpregister = USARTx->CTRL2;
    /* Clear STOP[13:12] bits */
    tmpregister &= CTRL2_STPB_CLR_MASK;
    /* Configure the USART Stop Bits, Clock, CPOL, CPHA and LastBit ------------*/
    /* Set STOP[13:12] bits according to StopBits value */
    tmpregister |= (uint32_t)USART_InitStruct->StopBits;

    /* Write to USART CTRL2 */
    USARTx->CTRL2 = (uint16_t)tmpregister;

    /*---------------------------- USART CTRL1 Configuration -----------------------*/
    tmpregister = USARTx->CTRL1;
    /* Clear M, PCE, PS, TE and RE bits */
    tmpregister &= CTRL1_CLR_MASK;
    /* Configure the USART Word Length, Parity and mode ----------------------- */
    /* Set the M bits according to WordLength value */
    /* Set PCE and PS bits according to Parity value */
    /* Set TE and RE bits according to Mode value */
    tmpregister |= (uint32_t)USART_InitStruct->WordLength | USART_InitStruct->Parity | USART_InitStruct->Mode;
    /* Write to USART CTRL1 */
    USARTx->CTRL1 = (uint16_t)tmpregister;

    /*---------------------------- USART CTRL3 Configuration -----------------------*/
    tmpregister = USARTx->CTRL3;
    /* Clear CTSE and RTSE bits */
    tmpregister &= CTRL3_CLR_MASK;
    /* Configure the USART HFC -------------------------------------------------*/
    /* Set CTSE and RTSE bits according to HardwareFlowControl value */
    tmpregister |= USART_InitStruct->HardwareFlowControl;
    /* Write to USART CTRL3 */
    USARTx->CTRL3 = (uint16_t)tmpregister;

    /*---------------------------- USART PBC Configuration -----------------------*/
    /* Configure the USART Baud Rate -------------------------------------------*/
    RCC_GetClocksFreqValue(&RCC_ClocksStatus);
    if (usartxbase == USART1_BASE)
    {
        apbclock = RCC_ClocksStatus.Pclk2Freq;
    }
    else
    {
        apbclock = RCC_ClocksStatus.Pclk1Freq;
    }

    /* Determine the integer part */
    integerdivider = ((25 * apbclock) / (4 * (USART_InitStruct->BaudRate)));
    tmpregister = (integerdivider / 100) << 4;

    /* Determine the fractional part */
    fractionaldivider = integerdivider - (100 * (tmpregister >> 4));

    /* Implement the fractional part in the register */   
    tmpregister |= ((((fractionaldivider * 16) + 50) / 100)) & ((uint8_t)0x0F);

    /* Write to USART PBC */
    USARTx->BRCF = (uint16_t)tmpregister;
}

/*-------------------------------------------------------------------
void USART_StructInit(USART_InitType* USART_InitStruct)
		初始化UART初始化所需要的数据结构 默认值
-------------------------------------------------------------------*/
void USART_StructInit(USART_InitType* USART_InitStruct)
{
    /* USART_InitStruct members default value */
    USART_InitStruct->BaudRate            = 9600;
    USART_InitStruct->WordLength          = USART_WL_8B;
    USART_InitStruct->StopBits            = USART_STPB_1;
    USART_InitStruct->Parity              = USART_PE_NO;
    USART_InitStruct->Mode                = USART_MODE_RX | USART_MODE_TX;
    USART_InitStruct->HardwareFlowControl = USART_HFCTRL_NONE;
}

/*-------------------------------------------------------------------
void USART_ClockInit(USART_Module* USARTx, USART_ClockInitType* USART_ClockInitStruct)
		初始化UART的时钟
		USARTx = USART1 USART2
		USART_ClockInitStruct 初始化所需要的数据结构
-------------------------------------------------------------------*/
void USART_ClockInit(USART_Module* USARTx, USART_ClockInitType* USART_ClockInitStruct)
{
    uint32_t tmpregister = 0x00;

    /*---------------------------- USART CTRL2 Configuration -----------------------*/
    tmpregister = USARTx->CTRL2;
    /* Clear CLKEN, CPOL, CPHA and LBCL bits */
    tmpregister &= CTRL2_CLOCK_CLR_MASK;
    /* Configure the USART Clock, CPOL, CPHA and LastBit ------------*/
    /* Set CLKEN bit according to Clock value */
    /* Set CPOL bit according to Polarity value */
    /* Set CPHA bit according to Phase value */
    /* Set LBCL bit according to LastBit value */
    tmpregister |= (uint32_t)USART_ClockInitStruct->Clock | USART_ClockInitStruct->Polarity
                   | USART_ClockInitStruct->Phase | USART_ClockInitStruct->LastBit;
    /* Write to USART CTRL2 */
    USARTx->CTRL2 = (uint16_t)tmpregister;
}

/*-------------------------------------------------------------------
void USART_ClockStructInit(USART_ClockInitType* USART_ClockInitStruct)
		用默认值填充初始化UART 时钟 所需要的数据结构
-------------------------------------------------------------------*/
void USART_ClockStructInit(USART_ClockInitType* USART_ClockInitStruct)
{
    /* USART_ClockInitStruct members default value */
    USART_ClockInitStruct->Clock    = USART_CLK_DISABLE;
    USART_ClockInitStruct->Polarity = USART_CLKPOL_LOW;
    USART_ClockInitStruct->Phase    = USART_CLKPHA_1EDGE;
    USART_ClockInitStruct->LastBit  = USART_CLKLB_DISABLE;
}

/*-------------------------------------------------------------------
void USART_Enable(USART_Module* USARTx, FunctionalState Cmd)
	许可/禁止 UART
	USART1 or USART2
	ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_Enable(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the selected USART by setting the UE bit in the CTRL1 register */
        USARTx->CTRL1 |= CTRL1_UEN_SET;
    }
    else
    {
        /* Disable the selected USART by clearing the UE bit in the CTRL1 register */
        USARTx->CTRL1 &= CTRL1_UEN_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_ConfigInt(USART_Module* USARTx, uint16_t USART_INT, FunctionalState Cmd)
		UART的中断配置
		USART1 or USART2
		USART_INT = USART_INT_CTSF 		CTS change interrupt
								USART_INT_LINBD 	LIN Break detection interrupt
								USART_INT_TXDE 		Transmit Data Register empty interrupt
								USART_INT_TXC 		Transmission complete interrupt
								USART_INT_RXDNE 	Receive Data register not empty interrupt
								USART_INT_IDLEF 	Idle line detection interrupt
								USART_INT_PEF 		Parity Error interrupt
								USART_INT_ERRF 		Error interrupt(Frame error, noise error, overrun error)
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_ConfigInt(USART_Module* USARTx, uint16_t USART_INT, FunctionalState Cmd)
{
    uint32_t usartreg = 0x00, itpos = 0x00, itmask = 0x00;
    uint32_t usartxbase = 0x00;
 
    usartxbase = (uint32_t)USARTx;

    /* Get the USART register index */
    usartreg = (((uint8_t)USART_INT) >> 0x05);

    /* Get the interrupt position */
    itpos  = USART_INT & INT_MASK;
    itmask = (((uint32_t)0x01) << itpos);

    if (usartreg == 0x01) /* The IT is in CTRL1 register */
    {
        usartxbase += 0x0C;
    }
    else if (usartreg == 0x02) /* The IT is in CTRL2 register */
    {
        usartxbase += 0x10;
    }
    else /* The IT is in CTRL3 register */
    {
        usartxbase += 0x14;
    }
    if (Cmd != DISABLE)
    {
        *(__IO uint32_t*)usartxbase |= itmask;
    }
    else
    {
        *(__IO uint32_t*)usartxbase &= ~itmask;
    }
}

/*-------------------------------------------------------------------
void USART_EnableDMA(USART_Module* USARTx, uint16_t USART_DMAReq, FunctionalState Cmd)
		许可/禁止 UART的DMA功能
		USART1 or USART2
		USART_DMAReq = 	USART_DMAREQ_TX 	USART DMA transmit request
										USART_DMAREQ_RX 	USART DMA receive request
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableDMA(USART_Module* USARTx, uint16_t USART_DMAReq, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the DMA transfer for selected requests by setting the DMAT and/or
           DADDR bits in the USART CTRL3 register */
        USARTx->CTRL3 |= USART_DMAReq;
    }
    else
    {
        /* Disable the DMA transfer for selected requests by clearing the DMAT and/or
           DADDR bits in the USART CTRL3 register */
        USARTx->CTRL3 &= (uint16_t)~USART_DMAReq;
    }
}

/*-------------------------------------------------------------------
void USART_SetAddr(USART_Module* USARTx, uint8_t USART_Addr)
		设置多机通信模式下的 UART地址
		USART1 or USART2
		USART_Addr 
-------------------------------------------------------------------*/
void USART_SetAddr(USART_Module* USARTx, uint8_t USART_Addr)
{
    /* Clear the USART address */
    USARTx->CTRL2 &= CTRL2_ADDR_MASK;
    /* Set the USART address node */
    USARTx->CTRL2 |= USART_Addr;
}

/*-------------------------------------------------------------------
void USART_ConfigWakeUpMode(USART_Module* USARTx, uint16_t USART_WakeUpMode)
		设置UART的唤醒模式
		USART1 or USART2
		USART_WakeUpMode = 	USART_WUM_IDLELINE WakeUp by an idle line detection
												USART_WUM_ADDRMASK WakeUp by an address mark
-------------------------------------------------------------------*/
void USART_ConfigWakeUpMode(USART_Module* USARTx, uint16_t USART_WakeUpMode)
{
    USARTx->CTRL1 &= CTRL1_WUM_MASK;
    USARTx->CTRL1 |= USART_WakeUpMode;
}

/*-------------------------------------------------------------------
void USART_EnableRcvWakeUp(USART_Module* USARTx, FunctionalState Cmd)
		许可/禁止 UART接收唤醒
		USART1 or USART2
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableRcvWakeUp(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the USART mute mode  by setting the RWU bit in the CTRL1 register */
        USARTx->CTRL1 |= CTRL1_RCVWU_SET;
    }
    else
    {
        /* Disable the USART mute mode by clearing the RWU bit in the CTRL1 register */
        USARTx->CTRL1 &= CTRL1_RCVWU_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_ConfigLINBreakDetectLength(USART_Module* USARTx, uint16_t USART_LINBreakDetectLength)
		设置UART 线路中断检测长度
		USART1 or USART2
		USART_LINBreakDetectLength = 	USART_LINBDL_10B 10-bit break detection
																	USART_LINBDL_11B 11-bit break detection
-------------------------------------------------------------------*/
void USART_ConfigLINBreakDetectLength(USART_Module* USARTx, uint16_t USART_LINBreakDetectLength)
{
    USARTx->CTRL2 &= CTRL2_LINBDL_MASK;
    USARTx->CTRL2 |= USART_LINBreakDetectLength;
}

/*-------------------------------------------------------------------
void USART_EnableLIN(USART_Module* USARTx, FunctionalState Cmd)
		许可/禁止 UART的线路检测功能
		USART1 or USART2
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableLIN(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the LIN mode by setting the LINEN bit in the CTRL2 register */
        USARTx->CTRL2 |= CTRL2_LINMEN_SET;
    }
    else
    {
        /* Disable the LIN mode by clearing the LINEN bit in the CTRL2 register */
        USARTx->CTRL2 &= CTRL2_LINMEN_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_SendData(USART_Module* USARTx, uint16_t Data)
		发送一个字数据
		USART1 or USART2
-------------------------------------------------------------------*/
void USART_SendData(USART_Module* USARTx, uint16_t Data)
{
    /* Transmit Data */
    USARTx->DAT = (Data & (uint16_t)0x01FF);
}

/*-------------------------------------------------------------------
uint16_t USART_ReceiveData(USART_Module* USARTx)
		读取最近接收到的数据
		USART1 or USART2
-------------------------------------------------------------------*/
uint16_t USART_ReceiveData(USART_Module* USARTx)
{
    /* Receive Data */
    return (uint16_t)(USARTx->DAT & (uint16_t)0x01FF);
}

/*-------------------------------------------------------------------
void USART_SendBreak(USART_Module* USARTx)发送一个线路中断信号
		USART1 or USART2
-------------------------------------------------------------------*/
void USART_SendBreak(USART_Module* USARTx)
{
    /* Send break characters */
    USARTx->CTRL1 |= CTRL1_SDBRK_SET;
}

/*-------------------------------------------------------------------
void USART_SetGuardTime(USART_Module* USARTx, uint8_t USART_GuardTime)
		设置串口保护时间
 * @brief  Sets the specified USART guard time.
 * @param USARTx where x can be 1 or 2 to select the USART peripheral.
 * @param USART_GuardTime specifies the guard time.
 * @note 
-------------------------------------------------------------------*/
void USART_SetGuardTime(USART_Module* USARTx, uint8_t USART_GuardTime)
{
    /* Clear the USART Guard time */
    USARTx->GTP &= GTP_LSB_MASK;
    /* Set the USART guard time */
    USARTx->GTP |= (uint16_t)((uint16_t)USART_GuardTime << 0x08);
}

/*-------------------------------------------------------------------
void USART_SetPrescaler(USART_Module* USARTx, uint8_t USART_Prescaler)
		串口时钟的预分频设置
		USART1 or USART2
-------------------------------------------------------------------*/
void USART_SetPrescaler(USART_Module* USARTx, uint8_t USART_Prescaler)
{
    /* Clear the USART prescaler */
    USARTx->GTP &= GTP_MSB_MASK;
    /* Set the USART prescaler */
    USARTx->GTP |= USART_Prescaler;
}

/*-------------------------------------------------------------------
void USART_EnableSmartCard(USART_Module* USARTx, FunctionalState Cmd)
		允许/禁止 UART的SM卡模式
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableSmartCard(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the SC mode by setting the SCEN bit in the CTRL3 register */
        USARTx->CTRL3 |= CTRL3_SCMEN_SET;
    }
    else
    {
        /* Disable the SC mode by clearing the SCEN bit in the CTRL3 register */
        USARTx->CTRL3 &= CTRL3_SCMEN_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_SetSmartCardNACK(USART_Module* USARTx, FunctionalState Cmd)
		允许/禁止在SM卡模式下发送 NACK
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_SetSmartCardNACK(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the NACK transmission by setting the NACK bit in the CTRL3 register */
        USARTx->CTRL3 |= CTRL3_SCNACK_SET;
    }
    else
    {
        /* Disable the NACK transmission by clearing the NACK bit in the CTRL3 register */
        USARTx->CTRL3 &= CTRL3_SCNACK_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_EnableHalfDuplex(USART_Module* USARTx, FunctionalState Cmd)
		允许/禁止 UART半双工模式
		USART1 or USART2
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableHalfDuplex(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the Half-Duplex mode by setting the HDSEL bit in the CTRL3 register */
        USARTx->CTRL3 |= CTRL3_HDMEN_SET;
    }
    else
    {
        /* Disable the Half-Duplex mode by clearing the HDSEL bit in the CTRL3 register */
        USARTx->CTRL3 &= CTRL3_HDMEN_RESET;
    }
}

/*-------------------------------------------------------------------
void USART_ConfigIrDAMode(USART_Module* USARTx, uint16_t USART_IrDAMode)
		设置串口的红外调制模式
		USART1 or USART2
		USART_IrDAMode = 	USART_IRDAMODE_LOWPOWER
											USART_IRDAMODE_NORMAL
-------------------------------------------------------------------*/
void USART_ConfigIrDAMode(USART_Module* USARTx, uint16_t USART_IrDAMode)
{
    USARTx->CTRL3 &= CTRL3_IRDALP_MASK;
    USARTx->CTRL3 |= USART_IrDAMode;
}

/*-------------------------------------------------------------------
void USART_EnableIrDA(USART_Module* USARTx, FunctionalState Cmd)
		许可/禁止 UART的红外调制模式
		USART1 or USART2
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void USART_EnableIrDA(USART_Module* USARTx, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the IrDA mode by setting the IREN bit in the CTRL3 register */
        USARTx->CTRL3 |= CTRL3_IRDAMEN_SET;
    }
    else
    {
        /* Disable the IrDA mode by clearing the IREN bit in the CTRL3 register */
        USARTx->CTRL3 &= CTRL3_IRDAMEN_RESET;
    }
}

/*-------------------------------------------------------------------
FlagStatus USART_GetFlagStatus(USART_Module* USARTx, uint16_t USART_FLAG)
		读取串口状态位
		USART1 or USART2
		USART_FLAG  =	USART_FLAG_CTSF 		CTS Change flag 
									USART_FLAG_LINBD 		LIN Break detection flag
									USART_FLAG_TXDE 		Transmit data register empty flag
									USART_FLAG_TXC 			Transmission Complete flag
									USART_FLAG_RXDNE 		Receive data register not empty flag
									USART_FLAG_IDLEF 		Idle Line detection flag
									USART_FLAG_OREF 		OverRun Error flag
									USART_FLAG_NEF 			Noise Error flag
									USART_FLAG_FEF 			Framing Error flag
									USART_FLAG_PEF 			Parity Error flag
		SET or RESET
-------------------------------------------------------------------*/
FlagStatus USART_GetFlagStatus(USART_Module* USARTx, uint16_t USART_FLAG)
{
    FlagStatus bitstatus = RESET;

    if (USART_FLAG == USART_FLAG_CTSF)
    {
        assert_param(IS_USART_12_PERIPH(USARTx));
    }

    if ((USARTx->STS & USART_FLAG) != (uint16_t)RESET)
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
void USART_ClrFlag(USART_Module* USARTx, uint16_t USART_FLAG)
		清除串口状态位
		USART1 or USART2
		USART_FLAG += USART_FLAG_CTSF 		CTS Change flag 
									USART_FLAG_LINBD 		LIN Break detection flag.
									USART_FLAG_TXC 			Transmission Complete flag.
									USART_FLAG_RXDNE 		Receive data register not empty flag.
		注意：
				1、PE、FE、NE、ORE 状态位通过以下软件流程清除：
					a read operation to USART_SR register (USART_GetFlagStatus())
					followed by a read operation to USART_DR register (USART_ReceiveData()).
				2、RXNE flag can be also cleared by a read to the USART_DR register
					USART_ReceiveData()
				3、TC flag can be also cleared by software sequence: 
					a read operation to USART_SR register (USART_GetFlagStatus()) 
					followed by a write operation to USART_DR register (USART_SendData()).
				4、TXE flag is cleared only by a write to the USART_DR register
					(USART_SendData()).
-------------------------------------------------------------------*/
void USART_ClrFlag(USART_Module* USARTx, uint16_t USART_FLAG)
{
    USARTx->STS = (uint16_t)~USART_FLAG;
}

/*-------------------------------------------------------------------
INTStatus USART_GetIntStatus(USART_Module* USARTx, uint16_t USART_INT)
		读取中断状态
		USART1 or USART2.
		USART_INT = USART_INT_CTSF 		CTS change interrupt 
								USART_INT_LINBD 	LIN Break detection interrupt
								USART_INT_TXDE 		Tansmit Data Register empty interrupt
								USART_INT_TXC 		Transmission complete interrupt
								USART_INT_RXDNE 	Receive Data register not empty interrupt
								USART_INT_IDLEF 	Idle line detection interrupt
								USART_INT_OREF 		OverRun Error interrupt
								USART_INT_NEF 		Noise Error interrupt
								USART_INT_FEF 		Framing Error interrupt
								USART_INT_PEF 		Parity Error interrupt
		SET or RESET
-------------------------------------------------------------------*/
INTStatus USART_GetIntStatus(USART_Module* USARTx, uint16_t USART_INT)
{
    uint32_t bitpos = 0x00, itmask = 0x00, usartreg = 0x00;
    INTStatus bitstatus = RESET;

    /* Get the USART register index */
    usartreg = (((uint8_t)USART_INT) >> 0x05);
    /* Get the interrupt position */
    itmask = USART_INT & INT_MASK;
    itmask = (uint32_t)0x01 << itmask;

    if (usartreg == 0x01) /* The IT  is in CTRL1 register */
    {
        itmask &= USARTx->CTRL1;
    }
    else if (usartreg == 0x02) /* The IT  is in CTRL2 register */
    {
        itmask &= USARTx->CTRL2;
    }
    else /* The IT  is in CTRL3 register */
    {
        itmask &= USARTx->CTRL3;
    }

    bitpos = USART_INT >> 0x08;
    bitpos = (uint32_t)0x01 << bitpos;
    bitpos &= USARTx->STS;
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
void USART_ClrIntPendingBit(USART_Module* USARTx, uint16_t USART_INT)
		清除串口中断
		USART1 or USART2.
		USART_INT = USART_INT_CTSF 			CTS change interrupt  
								USART_INT_LINBD 		LIN Break detection interrupt
								USART_INT_TXC 			Transmission complete interrupt.
								USART_INT_RXDNE 		Receive Data register not empty interrupt.
		注意：
				1、PE、FE、NE、ORE、IDLE pending bits are cleared by
					software sequence: a read operation to USART_SR register
					(USART_GetIntStatus()) followed by a read operation to USART_DR register
					(USART_ReceiveData()).
				2、RXNE pending bit can be also cleared by a read to the USART_DR register
					(USART_ReceiveData()).
				3、TC pending bit can be also cleared by software sequence: a read
					operation to USART_SR register (USART_GetIntStatus()) followed by a write
					operation to USART_DR register (USART_SendData()).
				4、TXE pending bit is cleared only by a write to the USART_DR register
					(USART_SendData()).
-------------------------------------------------------------------*/
void USART_ClrIntPendingBit(USART_Module* USARTx, uint16_t USART_INT)
{
    uint16_t bitpos = 0x00, itmask = 0x00;

    bitpos      = USART_INT >> 0x08;
    itmask      = ((uint16_t)0x01 << (uint16_t)bitpos);
    USARTx->STS = (uint16_t)~itmask;
}

