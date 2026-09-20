# 07 FreeRTOS 原理（面试备用）— 贴项目实际用法的通俗讲解

> 项目里的 FreeRTOS 是"薄用"：单任务 + 静态分配 + 队列解耦 + 临界区，没有用延时/互斥锁/信号量 API。这份笔记先把通用原理讲清，再落到本项目怎么用的。

## 一、为什么要有 RTOS：前后台 vs 多任务

**前后台（裸机）**：一个大 while(1) 主循环（前台中断 + 后台循环）。问题：任何一处任务耗时久，其他事全被拖住；各模块的时间要求互相打架。

**RTOS**：把程序拆成多个"任务"（task），每个任务有自己的栈、看似同时运行，由**调度器**决定谁此刻用 CPU。

**本项目的事实**：其实只有 **1 个应用任务**（task_ble）。那为什么还用 FreeRTOS？
- 任务/队列框架 + 中断到任务的解耦通道（ISR 只置标志/入队，任务里慢慢处理）；
- 项目是"**全事件驱动 + 无阻塞**"风格：单任务循环里轮询各模块状态机（pm/keyscan/voice/cat1 全是状态机），FreeRTOS 提供任务隔离和队列，但不依赖它的并发能力；
- 面试好讲点：**够用就好**——单核小资源 MCU 上，单任务 + 状态机轮询是最可控的结构，RTOS 只当"带队列的前后台"用。

## 二、核心概念（面试常问清单）

| 概念 | 通俗解释 | 本项目对应 |
|---|---|---|
| **任务 Task** | 独立栈 + 优先级 + 入口函数的执行单元 | task_ble（512 字栈、优先级 3）；空闲任务 idle（FreeRTOS 自动创建） |
| **调度器 Scheduler** | 决定哪个任务上 CPU | vTaskStartScheduler() 后接管，main 的死循环不再执行 |
| **抢占 Preemption** | 高优先级就绪立刻抢低优先级的 CPU | configUSE_PREEMPTION=1，本项目 4 个优先级档 |
| **Tick** | 系统心跳，时间片/延时的时钟基准 | **125Hz = 8ms**，由 SDK 定时器回调 `xTaskIncrementTick()` 注入 |
| **上下文切换** | 保存当前任务现场、恢复新任务现场（M0 上经 PendSV 异常） | 每次 tick/队列操作/taskYIELD 可能触发 |
| **阻塞 Blocking** | 任务等事件时让出 CPU（vTaskDelay/队列等待） | 本项目**刻意不用**——队列收发全部 timeout=0 |
| **队列 Queue** | 任务间/中断与任务间传数据的 FIFO 管道 | voice_ctrl 命令队列（4×1B）；cat1→ble 事件队列（4×evt） |
| **临界区** | 关中断保护共享变量的一小段代码 | rc_key_input 里 taskENTER_CRITICAL 包 s_pending_tag10 |
| **栈溢出检测** | 换上下文时检查栈顶哨兵字节 | configCHECK_FOR_STACK_OVERFLOW=2 |

## 三、本项目怎么用的（贴代码讲）

1. **静态创建一切**（configSUPPORT_STATIC_ALLOCATION=1）：
   - `xTaskCreateStatic(task_ble_entry, "ble", 512, NULL, 3, s_ble_stack, &s_ble_tcb)`——任务栈和控制块都是编译期定死的数组，**不依赖 malloc**，内存布局 100% 可预期（嵌入式小 RAM 的常见选择）；
   - `xQueueCreateStatic(4, ..., s_voice_cmd_queue_storage, &s_voice_cmd_queue_struct)`——队列存储同样静态；
2. **Tick 由 SDK 定时器注入**（configOVERRIDE_DEFAULT_TICK_CONFIGURATION=1）：main.c 的 `callBakSdkTimer` 每 8ms 被硬件定时器回调，里面调 `xTaskIncrementTick()`——把"系统时钟节拍"和芯片 SDK 的定时器绑在一起；
3. **队列解耦 ISR 与任务**：GATT 回调（协议栈上下文）里 `xQueueSend(..., 0)` 投递语音 START/STOP 命令，主循环 `xQueueReceive(..., 0)` 消费——**回调里绝不做重活**；
4. **临界区只包最小范围**：`taskENTER_CRITICAL()` 关中断 → 读写 s_pending_tag10 → `taskEXIT_CRITICAL()` 立即开中断；
5. **tickless 思想的实战**（衔接低功耗笔记）：待机态删掉 8ms tick 定时器让 br_sleep 深睡——这正是 FreeRTOS 官方 tickless idle 想解决的问题：**空闲时停 tick、唤醒时补偿**；
6. 没有互斥锁/信号量（configUSE_MUTEXES=0）：单任务架构没有跨任务资源竞争，优先级反转问题不存在——这是**配置上的取舍**，面试可主动讲。

## 四、面试自测题

1. RTOS 和裸机前后台的本质区别？什么时候该上 RTOS？
2. 任务切换发生在哪些时机？PendSV 为什么用来做上下文切换？
3. tick 是什么？tick 停了会怎样？（延时不算数/时间片不轮转——本项目待机态就是主动停 tick）
4. 队列满了 xQueueSend 返回什么？本项目怎么处理？（pdFAIL → 延后重试，Tag10 有 qfull 日志）
5. 临界区 vs 互斥锁的区别？为什么本项目用临界区就够？
6. 静态分配 vs 动态分配？（内存可预期/无碎片/嵌入式首选）
7. 栈溢出怎么检测？（canary 哨兵字节；configCHECK_FOR_STACK_OVERFLOW=2）
8. 优先级反转是什么？为什么本项目的单任务架构天然规避？
9. 中断里能调用哪些 FreeRTOS API？（带 FromISR 后缀的；本项目 ISR 里没直接调用，只置标志）
10. 如果面试官问"你的项目里 FreeRTOS 到底做了什么"，怎么答？（见第一节末段——单任务+队列解耦+状态机轮询，够用就好）
