#include <string.h>
#include "app_keyscan.h"
#include "co_printf.h"
#include "gpio.h"
#include "userhard.h"
#include "task_keyscan.h"
#include "pm_sleep.h"       /* g_pm_sleeping — 休眠唤醒瞬间亮 LED1 */


/**
 * @brief  KEYSCAN Configuration
 * @param  @KEY_MASK     :   KEY_65, KEY_44, KEY_104
 * @param  @KEY_MODE     :   MODE_FIXED_INTV, MODE_SW_TRIG, MODE_PRESS_TRIG,
 * @param  @KEY_DTS      :   DTS_10MS, DTS_20MS, DTS_40MS, DTS_80MS, DTS_160MS, DTS_320MS, DTS_640MS, DTS_640MS_2
 * @param  @KEY_WTS      :   WTS_0MS, WTS_32MS, WTS_64MS, WTS_96MS, WTS_128MS, WTS_160MS, WTS_192MS, WTS_224MS
 * @param  @KEY_INT      :   INT_DIS, INT_EN
 * @return Return value
 * @note   Note
 * 键盘扫描配置函数
 */
void KEYSCAN_Configuration(uint8_t key_mask, uint8_t mode, uint8_t dts, uint8_t wts, bool int_en)
{
	NVIC_InitType NVIC_InitStructure;
	EXTI_InitType EXTI_InitStructure;
	GPIO_InitType GPIO_InitStructure; 
	KEYSCAN_InitType KEYSCAN_InitStructure;
	
	/* KEYSCAN CLK EN */
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);
	RCC->LSCTRL |= RCC_LSCTRL_KEYSCANEN;
	
	/* config GPIO */
	GPIO_InitStruct(&GPIO_InitStructure);
	if(key_mask == KEY_65)
	{
		GPIO_InitStructure.Pin =     GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;        
	}
	else if(key_mask == KEY_44)
	{
		GPIO_InitStructure.Pin =     GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6;
	}
	else
	{
		GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
	}
	
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_OD;         //开漏输出
	GPIO_InitStructure.GPIO_Alternate = GPIO_AF5;
	GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.Pin = 0; //deinit
	if(key_mask == KEY_65 ||key_mask == KEY_44)
	{
		GPIO_InitStructure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;        
	}
	else
	{
		GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
	}
	
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_OD;         //开漏输出
	GPIO_InitStructure.GPIO_Alternate = GPIO_AF5;
	GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);

	/* config KEYSCAN */
	KEYSCAN_InitStructure.Mask = key_mask;
	KEYSCAN_InitStructure.Mode = mode;
	KEYSCAN_InitStructure.Wts = wts;
	KEYSCAN_InitStructure.Dts = dts;
	KEYSCAN_InitStructure.Int_en = int_en;
	KEYSCAN_Init(&KEYSCAN_InitStructure);
	/* config intterrupt */
	if(int_en)
	{
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStructure.EXTI_Line = EXTI_LINE13;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
		EXTI_InitPeripheral(&EXTI_InitStructure);
				
		NVIC_DisableIRQ(KEYSCAN_IRQn);
		/* Enable the KEYSCAN Interrupt */
		NVIC_InitStructure.NVIC_IRQChannel                   = KEYSCAN_IRQn;
		NVIC_InitStructure.NVIC_IRQChannelPriority           = 3;
		NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
		
		NVIC_EnableIRQ(KEYSCAN_IRQn);
	}
	/* Enable KEYSCAN */
	KEYSCAN_Enable(ENABLE);
}

/*
 * KEY_44 在自动扫描模式下为 44 键，在低功耗扫描模式下为 20 键。
 * 两者共用 MASK=1（8 IO：PA0,PA1,PA2,PA3,PA6,PB8,PB9,PB10），
 * 硬件根据 MODE 位自动切换扫描方式和键数上限。
 */
#define KEY_20 KEY_44

// 键盘扫描初始化 — 低功耗模式：按键 EXTI 触发唤醒，扫一轮后回休眠
void ble_keyscan_init(void)
{
	//                      44        自动扫描     消抖时间  扫描时间   使能
	KEYSCAN_Configuration(KEY_44,MODE_FIXED_INTV, DTS_10MS, WTS_32MS, INT_EN);
	//                      8 IO / 20键  低功耗扫描 消抖时间  扫描时间   中断使能
	//KEYSCAN_Configuration(KEY_20, MODE_PRESS_TRIG, DTS_10MS, WTS_0MS, INT_EN);
}
// 轮询当前键号：无键返回 0，与 IRQ 内解析方式一致
uint8_t app_keyscan_read_key_id(void)
{
	uint32_t key_data[5] = {0};
	uint8_t  num;

	KEYSCAN_ReadKeyData(key_data);
	for(num = 1; num < KEYSCAN_KEY_MAX; num++)
	{
		if(memcmp(key_data, &keyscan_map[num][0], (KEYSCAN_INFO_CHECK << 2)) == 0)
		{
			return num;
		}
	}
	return 0;
}

/* Matrix activity includes simultaneous keys that have no single-key ID. */
bool app_keyscan_any_pressed(void)
{
    uint32_t data[5] = {0};
    KEYSCAN_ReadKeyData(data);
    return (data[0] | data[1]) != 0u;
}

//键盘扫描中断服务函数
void KEYSCAN_IRQHandler(void)
{
    /*
     * 休眠唤醒瞬间亮 LED1：WFI 返回后 CPU 第一段执行的代码就是此 ISR。
     * 先确保 GPIOB 时钟开启（Sleep 模式可能关断了 APB2），
     * 再点亮 LED1 并清除标志。后续 keyscan_process 的释放路径负责灭灯。
     */
    if (g_pm_sleeping) {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
        SETLED1;
        g_pm_sleeping = false;
    }

    /* KEYSCAN 原始扫描结果缓存（最多 5 个 32-bit 数据字）。 */
    uint32_t key_data[5] = {0};
    /* num 表示匹配到的按键编号，未匹配时最终等于 KEYSCAN_KEY_MAX。 */
    uint8_t  num;
    /* 清除 KEYSCAN 外设自身中断标志。 */
    if(KEYSCAN_GetInterruptState() == SET)
    {
        KEYSCAN_ClearInterrupt();
    }
    
    /* 清除 EXTI13 挂起位，避免重复进入中断。 */
    if(EXTI_GetITStatus(EXTI_LINE13))
    {
        EXTI_ClrITPendBit(EXTI_LINE13);
    }
    
    /* 读取当前按键扫描数据。 */
    KEYSCAN_ReadKeyData(key_data);
    #if 0
    co_printf("REG data: ");
    co_printf("0x%08x,", key_data[0]);
    co_printf("0x%08x,", key_data[1]);
    co_printf("0x%08x,", key_data[2]);
    co_printf("0x%08x,", key_data[3]);
    co_printf("0x%08x ", key_data[4]);
    co_printf("\r\n");
    #endif
    
    /* 逐个与按键映射表比较，定位当前按下的是哪个 KEY_x。 */
    for(num = 1; num < KEYSCAN_KEY_MAX; num++)
    {
        if(memcmp(key_data,&keyscan_map[num][0],(KEYSCAN_INFO_CHECK<<2)) == 0)
        {
            break;
        }
    }
    
    /* 匹配成功后将按键编号上报给主循环。 */
    if(num != KEYSCAN_KEY_MAX)
    {
        extern volatile uint16_t key_receive;

        //co_printf("KEYSCAN_KEY%d pressed\r\n", num);
        key_receive = (uint16_t)num;
    }
}

