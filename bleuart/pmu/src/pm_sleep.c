/**
 * @file    pm_sleep.c
 * @brief   休眠模式管理
 * @note    BR1601 Sleep 模式 (WFI)，仅 KEYSCAN EXTI 唤醒
 */

#include "pm_config.h"
#include "pm_sleep.h"
#include "pwr.h"
#include "exti.h"
#include "gmem.h"
#include "ble_adv.h"
#include "userhard.h"

/*===========================================================================
 * 外设关闭 / 恢复
 *===========================================================================*/

static void pm_peripherals_sleep_shutdown(void)
{
    CLRLED1;
    CLRLED2;
    USART2->CTRL1 &= (uint16_t)(~((uint16_t)USART_CTRL1_TXEN | (uint16_t)USART_CTRL1_RXEN));
}

static void pm_peripherals_sleep_restore(void)
{
    USART2->CTRL1 |= USART_CTRL1_RXEN;
}

/*===========================================================================
 * 休眠入口
 *===========================================================================*/

uint8_t pm_sleep_enter(void)
{
    PM_LOG("Sleep enter: preparing...");

    ble_adv_stop();
    bleAdvStat = 0U;
    pm_peripherals_sleep_shutdown();

    PM_LOG("Sleep enter: entering Sleep mode (WFI)...");

    /* CPU 在此处休眠，按键通过 EXTI_LINE13 唤醒 */
    PWR_EnterSLEEPMode(PWR_SLEEPENTRY_WFI);

    /* ===== CPU 已被按键唤醒 ===== */
    SETLED1;  /* 检查点1: CPU已苏醒 */

    /* 清除按键中断标志 */
    EXTI_ClrITPendBit(EXTI_LINE13);

    /* 恢复外设 */
    pm_peripherals_sleep_restore();
    SETLED2;  /* 检查点2: 外设已恢复，即将返回 */


}
