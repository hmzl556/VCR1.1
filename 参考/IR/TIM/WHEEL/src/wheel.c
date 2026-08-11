
#include "wheel.h"
#include "stdio.h"
#include "misc.h"


/** @addtogroup 
 * @{
 */
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define WHEEL_PORT            GPIOB
#define WHEEL_A_PIN           GPIO_PIN_0
#define WHEEL_B_PIN           GPIO_PIN_9
#define WHEEL_PORT_RCC        RCC_APB2_PERIPH_GPIOB

#define ENCODER_LINE_CNT                18//(1024)                  //编码器线数，根据实际编码器参数设定
#define ENCODER_TIM_PERIOD                (ENCODER_LINE_CNT * 4)  //解码定时器计数周期，本例程用的解码模式3，所以是编码器线数*4，模式1和2则是线数*2
#define ENCODER_TOTAL_RESOLUTION         (ENCODER_TIM_PERIOD)    //编码器总分辨率，数值上等同于解码定时器计数周期
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO int16_t Encoder_Overflow_Count = 0;    //编码器码盘也即电机转动的圈数，正转+1，反转-1
__IO FlagStatus Motor_Direction = RESET;    //电机转动方向，RESET代表正转，SET代表反转
__IO int32_t Capture_Count = 0;            //100ms捕获一次定时器3解码计数值
__IO int32_t Last_Count = 0;               //上一个100ms时捕获的定时器3解码计数值
__IO float Shaft_Speed  = 0;                //1s内电机转过的圈数

/* Private function prototypes -----------------------------------------------*/





/* Private functions ---------------------------------------------------------*/

/*判断电机转动方向*/
FlagStatus TIM_IsCountingDown(void)
{
    if(TIM1->CTRL1 & TIM_CNT_MODE_DOWN)
    {
        return SET;
    }
    else
    {
        return RESET;
    }
}

//void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    if(TIM_GetIntStatus(TIM1, TIM_INT_UPDATE) != RESET)
    {
        TIM_ClrIntPendingBit(TIM1, TIM_INT_UPDATE);
        if(TIM_IsCountingDown() != RESET)
        {
            Encoder_Overflow_Count--;
        }
        else
        {
            Encoder_Overflow_Count++;
        }
    }
}

/*100ms定时获取编码器解码计数并计数转速*/
void timer_100ms_to_get_speed(void)
{
    /* 电机旋转方向 = 计数器计数方向 */
    Motor_Direction = TIM_IsCountingDown();
    /* 当前时刻的总计数值 = 计数器值 + 计数溢出次数 * ENCODER_TIM_PERIOD */
    Capture_Count = TIM_GetCnt(TIM1) + Encoder_Overflow_Count * ENCODER_TIM_PERIOD;
//    printf(" TIM_GetCnt:%d,Encoder_Overflow_Count:%d ENCODER_TIM_PERIOD:%d\r\n",TIM_GetCnt(TIM1),Encoder_Overflow_Count,ENCODER_TIM_PERIOD);

    /* 转轴转速 = 单位时间内的计数值 / 编码器的总分辨率 * 时间系数 */
    if(Capture_Count > Last_Count)
    {
        Shaft_Speed = (float)(Capture_Count - Last_Count) / ENCODER_TOTAL_RESOLUTION * 10;
    }
    else{
        Shaft_Speed = (float)(Last_Count - Capture_Count) / ENCODER_TOTAL_RESOLUTION * 10;
    }

    
    if(Last_Count != Capture_Count)
		co_printf("Dir:%d,Cnt:%d,Speed:%0.2f \r\n", \
                Motor_Direction,Capture_Count,Shaft_Speed);
    
    /* 记录当前总计数值 */
    Last_Count = Capture_Count;

}


void app_wheel_timer_init (void)
{
    NVIC_InitType NVIC_InitStructure;
    TIM_TimeBaseInitType TIM_TimeBaseStructure;
    TIM_ICInitType TIM_ICInitStructure;


    /* Enable the TIM1 global Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM1_BRK_UP_TRG_COM_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_TIM1, ENABLE);        
    TIM_DeInit(TIM1);
    TIM_InitTimBaseStruct(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.Period = ENCODER_TIM_PERIOD - 1;
    TIM_TimeBaseStructure.Prescaler = 0;
    TIM_TimeBaseStructure.ClkDiv = TIM_CLK_DIV1;
    TIM_TimeBaseStructure.CntMode = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM1, &TIM_TimeBaseStructure);
    
    TIM_ConfigEncoderInterface(TIM1, TIM_ENCODE_MODE_TI12, TIM_IC_POLARITY_RISING, TIM_IC_POLARITY_RISING);
    TIM_InitIcStruct(&TIM_ICInitStructure);
    TIM_ICInitStructure.IcPolarity = TIM_IC_POLARITY_RISING;
    TIM_ICInitStructure.IcFilter = 6;
    TIM_ICInitStructure.Channel = TIM_CH_1;
    TIM_ICInit(TIM1, &TIM_ICInitStructure);
    TIM_ICInitStructure.Channel = TIM_CH_2;
    TIM_ICInit(TIM1, &TIM_ICInitStructure);
            
    TIM_ConfigArPreload(TIM1, ENABLE);
    TIM_SetCnt(TIM1, 0);
    TIM_Enable(TIM1, ENABLE);
    
    TIM_ClearFlag(TIM1, TIM_FLAG_UPDATE);
    TIM_ConfigInt(TIM1, TIM_INT_UPDATE, ENABLE);
        

}


/**
 * @brief  wheel module Configuration
 * @param   
 * @return  
 * @note    
 */
void app_wheel_Start(void)
{
    GPIO_InitType GPIO_InitStructure;
    #if (defined WHEEL_PORT) && (defined WHEEL_PORT_RCC)
    /* init wheel IO */
    RCC_EnableAPB2PeriphClk(WHEEL_PORT_RCC, ENABLE);        
    
    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.Pin                 = WHEEL_A_PIN|WHEEL_B_PIN;
    GPIO_InitStructure.GPIO_Mode        = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Pull        = GPIO_PULL_UP;
    GPIO_InitStructure.GPIO_Current     = GPIO_DC_LOW;
    GPIO_InitStructure.GPIO_Alternate   = GPIO_AF1_TIM1;
    GPIO_InitPeripheral(WHEEL_PORT, &GPIO_InitStructure);
    
    /* init wheel tim1 */
    app_wheel_timer_init();
    
    #endif
}


/**
 * @brief  wheel module stop
 * @param   
 * @return  
 * @note    
 */
void app_wheel_stop(void)
{    
   
}


/**
 * @}
 */











































