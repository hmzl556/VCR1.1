#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      (64000000UL)
#define configTICK_RATE_HZ                      (125U)
#define configMAX_PRIORITIES                    (4U)
#define configMINIMAL_STACK_SIZE                (128U)
#define configTOTAL_HEAP_SIZE                   (768U)
#define configMAX_TASK_NAME_LEN                 (12U)
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_QUEUE_SETS                    0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 1

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskIncrementTick              1

#define configKERNEL_INTERRUPT_PRIORITY         (255U)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (255U)

#define configASSERT(x) \
    do { if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) { } } } while (0)

#endif /* FREERTOS_CONFIG_H */
