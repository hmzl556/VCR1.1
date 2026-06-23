#include "gpio.h"
      
#define GPIO_MODE                         ((uint32_t)0x00000003)
#define EXTI_MODE                         ((uint32_t)0x10000000)
#define GPIO_MODE_IT                      ((uint32_t)0x00010000)
#define GPIO_MODE_EVT                     ((uint32_t)0x00020000)
#define RISING_EDGE                       ((uint32_t)0x00100000) 
#define FALLING_EDGE                      ((uint32_t)0x00200000)
#define GPIO_OUTPUT_TYPE                  ((uint32_t)0x00000010)

/*-------------------------------------------------------------------
void GPIO_DeInit( GPIO_Module* GPIOx ) GPIO设备初始化
-------------------------------------------------------------------*/
void GPIO_DeInit( GPIO_Module* GPIOx )
{
    if( GPIOx == GPIOA )
    {
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_GPIOA, ENABLE);
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_GPIOA, DISABLE);
    }
    if( GPIOx == GPIOB )
    {
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_GPIOB, ENABLE);
        RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_GPIOB, DISABLE);
    }
}

/**
 * @brief  Deinitializes the GPIOx peripheral registers to their default reset values.
 * @param GPIOx where x can be (A,B) to select the GPIO peripheral.
 * @param GPIO_Pin specifies the port bit to be written.
  *                This parameter can be one of GPIO_PIN_x where x can be (0..13).
  *                All port bits are not necessarily available on all GPIOs.
 */
/*-------------------------------------------------------------------
void GPIO_DeInitPin(GPIO_Module* GPIOx, uint32_t Pin)GPIO.pin的设备
			初始化
参数	GPIOx为PORT设备名称，Pin为引脚位号，多个Pin可以一块初始化
-------------------------------------------------------------------*/
void GPIO_DeInitPin( GPIO_Module* GPIOx,uint32_t Pin )
{
    uint32_t pos = 0x00U, currentpin = 0x00U;

    while((Pin >> pos) != 0)
        {
            /* Get the IO position */
            currentpin = (Pin) & (1U << pos);

            if(currentpin)
            {
                /*------------------------- GPIO Mode Configuration --------------------*/
                /* Configure IO Direction in Analog Mode */
                GPIOx->PMODE |= (GPIO_PMODE0 << (pos * 2U));

                /* Configure the default Alternate Function in current IO */ 
                if(pos & 0x08)
                    GPIOx->AFH |= ((0x0FUL) << ((pos & 0x07U) * 4U));
                else
                    GPIOx->AFL |= ((0x0FUL) << ((pos & 0x07U) * 4U));

                /* Configure the default value IO Output Type */
                GPIOx->POTYPE &= ~(GPIO_POTYPE_POT_0 << pos);

                /* Deactivate the Pull-up and Pull-down resistor for the current IO */
                GPIOx->PUPD &= ~(GPIO_PUPD0 << (pos * 2U));

                /* Configure the default value for IO Speed */
                GPIOx->SR |= (GPIO_SR_SR0 << pos);

                /* Configure the default value for IO Speed */
                GPIOx->DS &= ~(GPIO_DS_DS0 << pos);
            } 
            pos++;
        }
    
}

/*-------------------------------------------------------------------
void GPIO_AFIOInitDefault( void ) GPIO初始化到其默认功能模式 GPIO模式
-------------------------------------------------------------------*/

void GPIO_AFIOInitDefault(void)
{
    RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC_EnableAPB2PeriphReset(RCC_APB2_PERIPH_AFIO, DISABLE);
}

/*-------------------------------------------------------------------
void GPIO_InitPeripheral(GPIO_Module* GPIOx, GPIO_InitType * GPIO_InitStruct)
			初始化GPIO设备
参数	GPIOx为PORT号，GPIO_InitStruct为初始化结构,指定方式和Pin索引
-------------------------------------------------------------------*/
void GPIO_InitPeripheral(GPIO_Module* GPIOx, GPIO_InitType * GPIO_InitStruct)
{
    uint32_t pos = 0x00U, currentpin = 0x00U;
    uint32_t tmpregister = 0x00U; 
    
    while(((GPIO_InitStruct->Pin) >> pos) != 0)
    {
        /* Get the IO position */
        currentpin = (GPIO_InitStruct->Pin) & (1U << pos);

        if(currentpin)
        {
            /*--------------------- GPIO Mode Configuration ------------------------*/
              /* In case of Alternate function mode selection */
            if((GPIO_InitStruct->GPIO_Mode == GPIO_MODE_AF_PP) || (GPIO_InitStruct->GPIO_Mode == GPIO_MODE_AF_OD))
            {
                /* Check the Alternate function parameters */
                assert_param(IS_GPIO_AF(GPIO_InitStruct->GPIO_Alternate));

                /* Configure Alternate function mapped with the current IO */
                if(pos & 0x08)
                {
                    tmpregister = GPIOx->AFH;
                    tmpregister &= ~((uint32_t)0xF << ((uint32_t)(pos & (uint32_t)0x07) * 4U));
                    tmpregister |= ((uint32_t)(GPIO_InitStruct->GPIO_Alternate) << ((uint32_t)(pos & (uint32_t)0x07) * 4U)) ;
                    GPIOx->AFH = tmpregister;
                }
                else
                {
                  tmpregister = GPIOx->AFL;
                    tmpregister &= ~((uint32_t)0xF << ((uint32_t)(pos & (uint32_t)0x07) * 4U)) ;
                    tmpregister |= ((uint32_t)(GPIO_InitStruct->GPIO_Alternate) << ((uint32_t)(pos & (uint32_t)0x07) * 4U)) ;
                    GPIOx->AFL = tmpregister;
                }
            }

            /* In case of Output or Alternate function mode selection */
            if ((GPIO_InitStruct->GPIO_Mode == GPIO_MODE_OUTPUT_PP) || (GPIO_InitStruct->GPIO_Mode == GPIO_MODE_OUTPUT_OD)
                 ||(GPIO_InitStruct->GPIO_Mode == GPIO_MODE_AF_PP) || (GPIO_InitStruct->GPIO_Mode == GPIO_MODE_AF_OD))
            {
                /* Configure the IO Output Type */
                tmpregister = GPIOx->POTYPE;
                tmpregister &= ~(GPIO_POTYPE_POT_0 << pos);                
                tmpregister |= (((GPIO_InitStruct->GPIO_Mode >> 4U) & 0x01U) << pos);
                GPIOx->POTYPE = tmpregister;
            }

            /*---------------------------- GPIO Mode Configuration -----------------------*/
            /* Configure IO Direction mode (Input, Output, Alternate or Analog) */
            tmpregister = GPIOx->PMODE;
            tmpregister &= ~(GPIO_PMODE0 << (pos * 2U));
            tmpregister |= (((GPIO_InitStruct->GPIO_Mode & 0x03U) << (pos * 2U)));
            GPIOx->PMODE = tmpregister;

            /* Configure pull-down mode */
            tmpregister = GPIOx->PUPD;    
            tmpregister &= ~(GPIO_PUPD0 << (pos * 2U));
            tmpregister |= (GPIO_InitStruct->GPIO_Pull << (pos * 2U));
            GPIOx->PUPD = tmpregister;    

            /* Configure slew rate */
            tmpregister = GPIOx->SR;
            tmpregister &= ~(GPIO_SR_SR0 << pos);
            tmpregister |= (GPIO_InitStruct->GPIO_Speed << pos);
            GPIOx->SR = tmpregister;

            /* Configure driver current */
            tmpregister = GPIOx->DS;
            tmpregister &= ~(GPIO_DS_DS0 << pos* 2U);
            tmpregister |= (GPIO_InitStruct->GPIO_Current << pos* 2U);
            GPIOx->DS = tmpregister;
        }
        pos++;      
    }    
}

/*-------------------------------------------------------------------
void GPIO_InitStruct(GPIO_InitType* GPIO_InitStruct)
			初始化GPIO初始化所需要的数据结构，在结构中设置为：
			所有Pin、无AF、INPUT、无上拉、低速度、
-------------------------------------------------------------------*/
void GPIO_InitStruct(GPIO_InitType* GPIO_InitStruct)
{    
    GPIO_InitStruct->Pin             = GPIO_PIN_ALL;
    GPIO_InitStruct->GPIO_Alternate  = GPIO_NO_AF;
    GPIO_InitStruct->GPIO_Mode       = GPIO_MODE_INPUT;
    GPIO_InitStruct->GPIO_Pull       = GPIO_NO_PULL;
    GPIO_InitStruct->GPIO_Speed      = GPIO_SPEED_LOW;    
    GPIO_InitStruct->GPIO_Current    = GPIO_DC_LOW;
}

/*-------------------------------------------------------------------
uint8_t GPIO_ReadInputDataBit(GPIO_Module* GPIOx, uint16_t Pin)
				读取GPIO输入数据位
参数	GPIOx为GPIO设备号 Pin为位Bit GPIO_Pin_x
-------------------------------------------------------------------*/
uint8_t GPIO_ReadInputDataBit(GPIO_Module* GPIOx, uint16_t Pin)
{
    if ((GPIOx->PID & Pin) != (uint32_t)Bit_RESET)return( (uint8_t)Bit_SET );
		return( (uint8_t)Bit_RESET );
}

/*-------------------------------------------------------------------
uint16_t GPIO_ReadInputData(GPIO_Module* GPIOx)	读取GPIO的所有输入
				数据位
-------------------------------------------------------------------*/
uint16_t GPIO_ReadInputData(GPIO_Module* GPIOx)
{
    return ((uint16_t)GPIOx->PID);
}

/*-------------------------------------------------------------------
uint8_t GPIO_ReadOutputDataBit(GPIO_Module* GPIOx, uint16_t Pin)
			读取GPIO输出数据的某个位， GPIO_Pin_x
-------------------------------------------------------------------*/
uint8_t GPIO_ReadOutputDataBit(GPIO_Module* GPIOx, uint16_t Pin)
{
    if ((GPIOx->POD & Pin) != (uint32_t)Bit_RESET)return((uint8_t)Bit_SET);
    return((uint8_t)Bit_RESET);
}

/*-------------------------------------------------------------------
uint16_t GPIO_ReadOutputData(GPIO_Module* GPIOx)
			读取GPIO的输出数据所有内容
-------------------------------------------------------------------*/
uint16_t GPIO_ReadOutputData(GPIO_Module* GPIOx)
{
    return ((uint16_t)GPIOx->POD);
}

/*-------------------------------------------------------------------
void GPIO_SetBits(GPIO_Module* GPIOx, uint16_t Pin)
			设置GPIO的某个位为高 GPIO_PIN_x
-------------------------------------------------------------------*/
void GPIO_SetBits(GPIO_Module* GPIOx, uint16_t Pin)
{
    GPIOx->PBSC = Pin;
}

/*-------------------------------------------------------------------
void GPIO_ResetBits(GPIO_Module* GPIOx, uint16_t Pin)
		设置GPIO的某个位为低 GPIO_Pin_x
-------------------------------------------------------------------*/
void GPIO_ResetBits(GPIO_Module* GPIOx, uint16_t Pin)
{
    GPIOx->PBC = Pin;
}

/*-------------------------------------------------------------------
void GPIO_WriteBit(GPIO_Module* GPIOx, uint16_t Pin, Bit_OperateType BitCmd)
		写GPIO的位 GPIO_Pin_x  Bit_RESET/Bit_SET
-------------------------------------------------------------------*/
void GPIO_WriteBit(GPIO_Module* GPIOx, uint16_t Pin, Bit_OperateType BitCmd)
{
    if(BitCmd != Bit_RESET)
    {
        GPIOx->PBSC = Pin;
    }
    else
    {
        GPIOx->PBC = Pin;
    }
}

/*-------------------------------------------------------------------
void GPIO_Write(GPIO_Module* GPIOx, uint16_t PortVal)写一个GPIO
-------------------------------------------------------------------*/
void GPIO_Write(GPIO_Module* GPIOx, uint16_t PortVal)
{
    GPIOx->POD = PortVal;
}

/*-------------------------------------------------------------------
void GPIO_TogglePin(GPIO_Module *GPIOx, uint16_t Pin)
		反转GPIO的某些口线
-------------------------------------------------------------------*/
void GPIO_TogglePin(GPIO_Module *GPIOx, uint16_t Pin)
{
    GPIOx->POD ^= Pin;
}

/*-------------------------------------------------------------------
void GPIO_ConfigPinLock(GPIO_Module* GPIOx, uint16_t Pin)
		配置GPIO的某些引脚进入配置锁定状态 GPIO_Pin_x
		可防止误配置
-------------------------------------------------------------------*/
void GPIO_ConfigPinLock(GPIO_Module* GPIOx, uint16_t Pin)
{
    uint32_t tmp = 0x00010000;

    tmp |= Pin;
    /* Set LCKK bit */
    GPIOx->PLOCK = tmp;
    /* Reset LCKK bit */
    GPIOx->PLOCK = Pin;
    /* Set LCKK bit */
    GPIOx->PLOCK = tmp;
    /* Read LCKK bit*/
    tmp = GPIOx->PLOCK;
    /* Read LCKK bit*/
    tmp = GPIOx->PLOCK;
}

/*-------------------------------------------------------------------
void GPIO_ConfigEXTILine(uint8_t PortSource, uint8_t PinSource)
			配置GPIO.Pin 与 EXIT中断关联
		 PortSource = GPIO_PortSourceGPIOx A B
		 PinSource = GPIO_PinSourcex 0-13
-------------------------------------------------------------------*/
void GPIO_ConfigEXTILine( uint8_t PortSource,uint8_t PinSource )
{
    if(PortSource == GPIOA_PORT_SOURCE)
    {
        switch(PinSource)
        {
            case 0:
                AFIO->EXTI_CFG[0] &= ~(0x03U);
                break;
            case 1:
                AFIO->EXTI_CFG[0] &= ~(0x03U);
                AFIO->EXTI_CFG[0] |= (0x01U);
                break;
            case 2:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 4U);
                break;
            case 3:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 4U);
                AFIO->EXTI_CFG[0] |= (0x01U << 4U);
                break;
            case 4:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 8U);   
                AFIO->EXTI_CFG[1] |= (0x02U << 8U);
                break;
            case 5:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 12U);  
                break;
            case 6:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 8U);           
                break;
             default:
                break;
        }            
    }
    else{
        switch(PinSource)
        {
            case 0:
                AFIO->EXTI_CFG[0] &= ~(0x03U);
                AFIO->EXTI_CFG[0] |= (0x02U);
                break;
            case 1:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 4U);
                AFIO->EXTI_CFG[0] |= (0x02U << 4U);
                break;
            case 2:
                AFIO->EXTI_CFG[0] &= ~(0x03U << 8U);
                AFIO->EXTI_CFG[0] |= (0x01U << 8U);
                break;
            case 3:            
                AFIO->EXTI_CFG[0] &= ~(0x03U << 12U);
                AFIO->EXTI_CFG[0] |= (0x01U << 12U);
                break;
            case 4:
                AFIO->EXTI_CFG[1] &= ~(0x03U);
                AFIO->EXTI_CFG[1] |= (0x01U);
                break;
            case 5:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 4U);
                AFIO->EXTI_CFG[1] |= (0x02U << 4U);
                break;
            case 6:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 8U);
                AFIO->EXTI_CFG[1] |= (0x01U << 8U);
                break;
            case 7:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 12U);
                AFIO->EXTI_CFG[1] |= (0x01U << 12U);
                break;
            case 8:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 12U);
                AFIO->EXTI_CFG[1] |= (0x02U << 12U);
                break;
            case 9:
                AFIO->EXTI_CFG[1] &= ~(0x03U);
                break;
            case 10:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 4U);
                break;
            case 11:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 4U);
                AFIO->EXTI_CFG[1] |= (0x01U << 4U);
                break;
            case 12:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 8U);
                break;
            case 13:
                AFIO->EXTI_CFG[1] &= ~(0x03U << 12U); 
                break;
             default:
                break;
        }
    }
}

/*-------------------------------------------------------------------
void GPIO_ConfigPinRemap(uint8_t PortSource, uint8_t PinSource, uint32_t AlternateFunction)
		配置GPIO的引脚特殊功能模式
		GPIO_PortSourceGPIOx A B，GPIO_PinSourcex
-------------------------------------------------------------------*/
void GPIO_ConfigPinRemap(uint8_t PortSource, uint8_t PinSource, uint32_t AlternateFunction)
{
    uint32_t tmp = 0x00, tmpregister = 0x00;
    GPIO_Module *GPIOx;
	
    /*Get Peripheral point*/
    GPIOx = GPIO_GET_PERIPH(PortSource);
    /**/
     if(PinSource & (uint8_t)0x08)
    {
        tmp = (uint32_t)(PinSource & (uint8_t)0x07);
        /*Read GPIO_AFH register*/
        tmpregister  = GPIOx->AFH;
        /*Reset corresponding bits*/
        tmpregister &=~((uint32_t)0x0F <<(tmp*4U));
        /*Set corresponding bits*/
        tmpregister |= AlternateFunction;
        /*Write to the GPIO_AFH register*/
        GPIOx->AFH = tmpregister;
    }
    else
    {
        tmp = (uint32_t)(PinSource & (uint8_t)0x07);
        /*Read GPIO_AFL register*/
        tmpregister  = GPIOx->AFL;
        /*Reset corresponding bits*/
        tmpregister &=~((uint32_t)0x0F <<(tmp*4U));
        /*Set corresponding bits*/
        tmpregister |= AlternateFunction;
        /*Write to the GPIO_AFL register*/
        GPIOx->AFL = tmpregister;
    }
}


/*-------------------------------------------------------------------
void AFIO_ConfigSPINSSMode(uint32_t AFIO_SPIx_NSS, uint32_t SpiNssMode)
		特殊功能配置 SPIx NSS的模式
		AFIO_SPIx_NSS  AFIO_SPI_NSS_High_IMPEDANCE/AFIO_SPI_NSS_High_LEVEL.
-------------------------------------------------------------------*/
void AFIO_ConfigSPINSSMode(uint32_t AFIO_SPIx_NSS, uint32_t SpiNssMode)
{
    uint32_t tmpregister = 0x00U;

    tmpregister = AFIO->CFG;    
    if(SpiNssMode != AFIO_SPI_NSS_High_IMPEDANCE)
    {
        tmpregister |= AFIO_SPIx_NSS;
    }
    else 
    {
        tmpregister &= ~AFIO_SPIx_NSS;
    }    
    AFIO->CFG = tmpregister;
}
