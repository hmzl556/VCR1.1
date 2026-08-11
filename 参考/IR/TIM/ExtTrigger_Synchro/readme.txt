1. 功能描述
     1. TIM1 CH2对TIM1 CH1以及TIM3进行门控

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
            HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM1时钟=64M，TIM3时钟=64M
        2. 端口配置：
                    PB8 配置为TIM1 CH1输出
                    PB9 配置为TIM1 CH2输入
                    PB4 配置为TIM3 CH1输出
        3. 定时器配置：
            TIM1 CH2门控TIM1 CH1与TIM3
     操作步骤：
        1. 编译后打开调试模式，使用示波器或逻辑分析仪观测TIM1 CH1、TIM3 CH1波形
        2. TIM1 CH2为高电平时定时器开始计数，低电平时停止计数

4. 注意事项
    无