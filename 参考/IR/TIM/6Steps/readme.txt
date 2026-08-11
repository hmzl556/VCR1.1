1. 功能描述
    1. SysTick每100ms触发TIM1，输出六步换相波形

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
                    HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM1时钟=64M
        2. 中断：
                    开启TIM1 COM事件中断，优先级1
                    SysTick 100ms中断，优先级0
        3. 端口配置：
                    PB8 配置为TIM1 CH1输出
                    PB9 配置为TIM1 CH2输出
                    PB10配置为TIM1 CH3输出
                    PB12配置为TIM1 CH1N输出
                    PB13配置为TIM1 CH2N输出
                    PA4 配置为TIM1 CH3N输出
                    PA6 配置为TIM1刹车输入
        4. 定时器配置：
                    TIM1 六通道互补冻结输出模式，关闭刹车，开启COM中断

    操作步骤：
        1. 编译后打开调试模式，使用示波器或逻辑分析仪观测TIM1输出波形
        2. SysTick每100ms触发COM中断，在TIM的COM中断内部输出AB、AC、BC、BA、CA、CB六步换相波形

4. 注意事项
       PA4复用为TIM3_CH3N与SWDCLK。上电后需要延时2秒，再将PA4切换为TIM3_CH3N功能。