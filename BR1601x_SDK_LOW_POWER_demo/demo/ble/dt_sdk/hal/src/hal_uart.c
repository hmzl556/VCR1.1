#include "hal_uart.h"
#include "gpio.h"
#include "usart.h"
#include "lpuart.h"
#include "dma.h"
#include "rcc.h"
#include "log.h"

#define USARTx_DMA_RX_INT_ENABLE    1

#define _USART1_COM_
#ifdef _USART1_COM_
#define USARTx              USART1
#define USARTx_CLK          RCC_APB2_PERIPH_USART1
#define USARTx_GPIO         GPIOB
#define USARTx_GPIO_CLK     RCC_APB2_PERIPH_GPIOB
#define USARTx_RxPin        GPIO_PIN_7
#define USARTx_TxPin        GPIO_PIN_6
#ifdef  i2616e_u1
#define USARTx_RTSPin       GPIO_PIN_10
#elif   i2616e
#define USARTx_RTSPin       GPIO_PIN_0 //GPIO_PIN_8 32k osc
#endif
#define USARTx_CTSPin       GPIO_PIN_1 //GPIO_PIN_9 32k osc
#define USARTx_Rx_GPIO_AF   GPIO_AF4_USART1
#define USARTx_Tx_GPIO_AF   GPIO_AF4_USART1
#define USARTx_RTS_GPIO_AF  GPIO_AF3_USART1
#define USARTx_CTS_GPIO_AF  GPIO_AF3_USART1

#define GPIO_APBxClkCmd     RCC_EnableAPB2PeriphClk
#define USART_APBxClkCmd    RCC_EnableAPB2PeriphClk

#define USARTx_DAT_Base         (USART1_BASE + 0x04)
#define USARTx_Tx_DMA_Channel   DMA_CH1
#define USARTx_Tx_DMA_FLAG      DMA_FLAG_TC1
#define USARTx_Rx_DMA_Channel   DMA_CH2
#define USARTx_Rx_DMA_FLAG      DMA_FLAG_TC2
#define USARTx_Tx_DMA_REMAP     DMA_REMAP_USART1_TX
#define USARTx_Rx_DMA_REMAP     DMA_REMAP_USART1_RX
#define USARTx_IRQn             USART1_IRQn
#define USARTx_IRQHandler       USART1_IRQHandler
#endif

/* Private variables ---------------------------------------------------------*/
static uint8_t s_rx_dma_buf[HAL_USART_RX_DMA_SIZE] = {0};
#if UDATA_PT_STACK
static uint8_t s_tx_dma_buf[HAL_USART_TX_DMA_SIZE] = {0};
#endif
static uint16_t s_rx_old_pos = 0;
static uint8_t s_usart_sending = false;
static hal_data_uart_init_t s_data_init;

/**
 * @brief  Configures the different system clocks.
 */
static void br_data_rcc_configuration(void)
{
    /* Enable GPIO clock */
    GPIO_APBxClkCmd(USARTx_GPIO_CLK, ENABLE);
    /* Enable USARTx Clock */
    USART_APBxClkCmd(USARTx_CLK, ENABLE);
    /* DMA clock enable */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);
}

/**
 * @brief  Configures the different GPIO ports.
 */
static void br_data_gpio_configuration(void)
{
    GPIO_InitType GPIO_InitStructure;

    /* Initialize GPIO_InitStructure */
    GPIO_InitStruct(&GPIO_InitStructure);    

    /* Configure USARTx Tx as alternate function push-pull */
    GPIO_InitStructure.Pin            = USARTx_TxPin;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = USARTx_Tx_GPIO_AF;
    GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure);   

    /* Configure USARTx Rx as alternate function push-pull */
    GPIO_InitStructure.Pin            = USARTx_RxPin;
    GPIO_InitStructure.GPIO_Alternate = USARTx_Rx_GPIO_AF;
    GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure); 

    if(s_data_init.flow_ctrl == ENABLE){
        /* Configure USARTx CTS as alternate function push-pull and pull-up */
        GPIO_InitStruct(&GPIO_InitStructure);
        GPIO_InitStructure.Pin            = USARTx_CTSPin;
        GPIO_InitStructure.GPIO_Pull      = GPIO_PULL_DOWN;
        GPIO_InitStructure.GPIO_Alternate = USARTx_CTS_GPIO_AF;
    	GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
        GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure);   


        /* Configure USARTx RTS as GPIO pin push-pull and pull-up */
        /* Configure the GPIO pin */
        GPIO_InitStruct(&GPIO_InitStructure);
        GPIO_InitStructure.Pin = USARTx_RTSPin;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure);
        /* enable rts*/
        GPIO_ResetBits(USARTx_GPIO,USARTx_RTSPin);
    }
}

/**
 * @brief  Configures the DMA.
 */
static void br_data_dma_configuration(void)
{
    DMA_InitType DMA_InitStructure;

    /* USARTx_Tx_DMA_Channel (triggered by USARTx Tx event) Config */
    DMA_DeInit(USARTx_Tx_DMA_Channel);
    DMA_RequestRemap(USARTx_Tx_DMA_REMAP, DMA, USARTx_Tx_DMA_Channel, ENABLE);    
    DMA_ConfigInt(USARTx_Tx_DMA_Channel,DMA_INT_TXC ,ENABLE);

    /* USARTx RX DMA1 Channel (triggered by USARTx Rx event) Config */
    DMA_DeInit(USARTx_Rx_DMA_Channel);
    DMA_RequestRemap(USARTx_Rx_DMA_REMAP, DMA, USARTx_Rx_DMA_Channel, ENABLE);        
#if USARTx_DMA_RX_INT_ENABLE
    DMA_ConfigInt(USARTx_Rx_DMA_Channel,DMA_INT_TXC|DMA_INT_HTX ,ENABLE);
#endif  
    DMA_InitStructure.PeriphAddr     = USARTx_DAT_Base;
    DMA_InitStructure.MemAddr        = (uint32_t)s_rx_dma_buf;
    DMA_InitStructure.Direction      = DMA_DIR_PERIPH_SRC;
    DMA_InitStructure.BufSize        = HAL_USART_RX_DMA_SIZE;
    DMA_InitStructure.CircularMode   = DMA_MODE_CIRCULAR;
    DMA_InitStructure.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    DMA_InitStructure.DMA_MemoryInc  = DMA_MEM_INC_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
    DMA_InitStructure.MemDataSize    = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.Priority       = DMA_PRIORITY_VERY_HIGH;
    DMA_InitStructure.Mem2Mem        = DMA_M2M_DISABLE;
    DMA_Init(USARTx_Rx_DMA_Channel, &DMA_InitStructure);
}


/**
 * @brief  Configures the nested vectored interrupt controller.
 */
static void br_data_nvic_configuration(void)
{
    NVIC_InitType NVIC_InitStructure;

    NVIC_DisableIRQ(DMA_Channel1_2_3_4_IRQn);
    /* Enable the DMA Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = DMA_Channel1_2_3_4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);


    NVIC_DisableIRQ(USARTx_IRQn);
    /* Enable the DMA Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = USARTx_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


/**
 * @brief  Configures the USART as 115200 8n1.
 */
static void br_data_uart_configuration(const hal_data_uart_init_t* init)
{

    USART_InitType USART_InitStructure;
    /* record usart init */
    s_data_init.baudrate = init->baudrate;
    s_data_init.flow_ctrl = init->flow_ctrl;
    s_data_init.evt_callback = init->evt_callback;
    s_data_init.data_rcv_callback = init->data_rcv_callback;
         
    /* Configure and enable RCC */
    br_data_rcc_configuration();
    /* Configure GPIO for USART */
    br_data_gpio_configuration();

    /* Configure the DMA */
    br_data_dma_configuration();

    br_data_nvic_configuration();
    /* USARTx and USARTz configuration ------------------------------------------------------*/
    USART_InitStructure.BaudRate            = init->baudrate; 
    USART_InitStructure.WordLength          = USART_WL_8B;
    USART_InitStructure.StopBits            = USART_STPB_1;
    USART_InitStructure.Parity              = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = init->flow_ctrl ? USART_HFCTRL_CTS : USART_HFCTRL_NONE;
    USART_InitStructure.Mode                = USART_MODE_RX | USART_MODE_TX;
    
    /* Configure USARTx */
    USART_Init(USARTx, &USART_InitStructure);
        
    /* Enable USARTz Receive and Transmit interrupts */
    USART_ConfigInt(USARTx, USART_INT_IDLEF, ENABLE);

    /* Enable USARTx DMA Rx and TX request */
    USART_EnableDMA(USARTx, USART_DMAREQ_RX | USART_DMAREQ_TX, ENABLE);
    
    DMA_EnableChannel(USARTx_Rx_DMA_Channel, ENABLE);
    
    /* Enable the USARTx */
    USART_Enable(USARTx, ENABLE);
    
}

/**
 * @brief  check the dma buffer which has been received 
 */
static void br_data_rx_check_in_irq(void)
{
    uint16_t rx_pos;

    rx_pos = HAL_USART_RX_DMA_SIZE - DMA_GetCurrDataCounter(USARTx_Rx_DMA_Channel);
    if(s_data_init.data_rcv_callback == NULL)
        return ;
    
    if(rx_pos <  s_rx_old_pos)
    {
        s_data_init.data_rcv_callback(&s_rx_dma_buf[s_rx_old_pos],(HAL_USART_RX_DMA_SIZE - s_rx_old_pos));
        
        if(rx_pos > 0)
        {
            s_data_init.data_rcv_callback(&s_rx_dma_buf[0],rx_pos);
        }
        
        s_rx_old_pos = rx_pos;
    }
    else if(rx_pos >  s_rx_old_pos)
    {
        s_data_init.data_rcv_callback(&s_rx_dma_buf[s_rx_old_pos],(rx_pos - s_rx_old_pos));
        s_rx_old_pos = rx_pos;
    }
}


/**
 * @brief  usart send data via dma
 */
uint32_t  hal_data_uart_send(const uint8_t *p_data,uint32_t len) 
{
    DMA_InitType DMA_InitStructure;
    if(s_usart_sending)
    {
        return false;
    }
    /* USARTx_Tx_DMA_Channel (triggered by USARTx Tx event) Config */
    DMA_InitStructure.PeriphAddr     = USARTx_DAT_Base;
#if UDATA_PT_STACK
    memcpy(s_tx_dma_buf, p_data, len);
    DMA_InitStructure.MemAddr        = (uint32_t)s_tx_dma_buf;
#else
    DMA_InitStructure.MemAddr        = (uint32_t)p_data;
#endif
    DMA_InitStructure.Direction      = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.BufSize        = len;
    DMA_InitStructure.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    DMA_InitStructure.DMA_MemoryInc  = DMA_MEM_INC_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
    DMA_InitStructure.MemDataSize    = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.CircularMode   = DMA_MODE_NORMAL; 
    DMA_InitStructure.Priority       = DMA_PRIORITY_VERY_HIGH;
    DMA_InitStructure.Mem2Mem        = DMA_M2M_DISABLE;
    DMA_Init(USARTx_Tx_DMA_Channel, &DMA_InitStructure);

    s_usart_sending = true;
    DMA_EnableChannel(USARTx_Tx_DMA_Channel, ENABLE);
     
    return true;
}

/**
 * @brief  enable or disable the usart dma
 */
bool hal_data_uart_init(const hal_data_uart_init_t* init)
{
    br_sleep_lock_acquire();
    s_rx_old_pos = 0;
    s_usart_sending = false;
    br_data_uart_configuration(init);
    return true;
}


bool hal_data_uart_baud_update(uint32_t baud)
{
    s_data_init.baudrate = baud; 
    hal_data_uart_deinit();
    hal_data_uart_init(&s_data_init);
    return true;
}

bool hal_data_uart_fc_updata(uint32_t fc)
{
    s_data_init.flow_ctrl = fc; 
    hal_data_uart_deinit();
    hal_data_uart_init(&s_data_init);
    return true;
}

bool hal_data_uart_deinit(void)
{
    s_usart_sending = true;    
    br_sleep_lock_release();
    USART_Enable(USARTx, DISABLE);
    DMA_EnableChannel(USARTx_Rx_DMA_Channel, DISABLE);
    DMA_EnableChannel(USARTx_Tx_DMA_Channel, DISABLE);
    /* Deinit IO of USART to save power */
    GPIO_InitType GPIO_InitStructure;
    /* Initialize GPIO_InitStructure */
    GPIO_InitStruct(&GPIO_InitStructure);    
    /* Configure USARTx Tx as alternate function push-pull */
    GPIO_InitStructure.Pin            = USARTx_TxPin|USARTx_RxPin;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_ANALOG;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF0;
    GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure);
    
    if(s_data_init.flow_ctrl == ENABLE){
        GPIO_InitStructure.Pin            = USARTx_CTSPin|USARTx_RTSPin;
        GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_ANALOG;
        GPIO_InitStructure.GPIO_Alternate = GPIO_AF0;
        GPIO_InitPeripheral(USARTx_GPIO, &GPIO_InitStructure);
    }
    /*clear rx dma*/
    s_rx_old_pos = 0;
    return true;
}

void hal_data_uart_rcv_enable(bool enable)
{
    if(s_data_init.flow_ctrl == ENABLE){
        if(enable)
            GPIO_ResetBits(USARTx_GPIO,USARTx_RTSPin);
        else
            GPIO_SetBits(USARTx_GPIO,USARTx_RTSPin);
    }
}

uint32_t hci_evt_rsp(const uint8_t *p_data,uint32_t len) 
{
    uint32_t data_len = len;
    while(data_len--)
    {
         USART_SendData(USARTx, *p_data);
         while(USART_GetFlagStatus(USARTx,USART_FLAG_TXDE) == RESET);
         p_data++;
    }
    return data_len;
}

/**
 * @brief  dma irq handler
 */
void DMA_Channel1_2_3_4_IRQHandler(void)
{
    //TX
    if(DMA_GetFlagStatus(DMA_FLAG_TC1, DMA))
    {
        s_usart_sending = false;
        //TX Transfer complete interrupt
        if(s_data_init.evt_callback){
            hal_uart_event_t ev = HAL_DATA_UART_EV_TX_CMP;
            s_data_init.evt_callback(&ev);
        }
        DMA_ClearFlag(DMA_FLAG_TC1, DMA);
    }
    //RX 
    if(DMA_GetFlagStatus(DMA_FLAG_TC2, DMA))
    {
        //RX Transfer complete interrupt
        DMA_ClearFlag(DMA_FLAG_TC2, DMA);
        br_data_rx_check_in_irq();
        
    }
    if(DMA_GetFlagStatus(DMA_FLAG_HT2, DMA))
    {
        //RX Half transfer interrupt 
        DMA_ClearFlag(DMA_FLAG_HT2, DMA);
        br_data_rx_check_in_irq();
    }
}

/**
 * @brief  usart irq handler
 */
void USARTx_IRQHandler(void)
{
    //usart idlle interrupt
    if(USART_GetFlagStatus(USARTx,USART_FLAG_IDLEF))
    {             
        //read sts and data will clear rx idle interrupt
        uint8_t temp = USARTx->DAT;
        br_data_rx_check_in_irq();
    
        if(s_data_init.evt_callback){
            hal_uart_event_t ev = HAL_DATA_UART_EV_RX_IDLE;
            s_data_init.evt_callback(&ev);
        }
    }
}

