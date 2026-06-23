#include "FreeRTOS.h"
#include "task.h"

/* BLE 栈使用 SysTick 做协议计时，FreeRTOS tick 由 SDK 8ms 软定时器驱动 */
void vPortSetupTimerInterrupt(void)
{
    volatile uint32_t *pulSysTickCtrl = (volatile uint32_t *)0xE000E010UL;
    *pulSysTickCtrl = 0UL;
}

static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &s_idle_tcb;
    *ppxIdleTaskStackBuffer = s_idle_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}
