1. 功能描述
     1. TIM3 CH2上升沿触发CH3输出单脉冲

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
            HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM3时钟=64M
         2. 端口配置：
                    PB6 配置为TIM3 CH3输出
                    PB5 配置为TIM3 CH2输入
                    PA6 配置为IO输出
         3. 定时器配置：
                     TIM3配置为CH2上升沿触发CH3输出单脉冲

     操作步骤：
         1. 编译后打开调试模式，将PA6连接至PB5，使用示波器或逻辑分析仪观测TIM3 CH3波形
         2. 将变量gSendTrigEn添加到观察窗口，默认gSendTrigEn=0；每次手动将gSendTrigEn置1，TIM3 CH3引脚将输出一个单脉冲

4. 注意事项
        开发板PB6、PB7默认跳线帽连接DAPLink虚拟串口。若项目中PB6、PB7不作为串口，用作其他功能，必须拔掉串口跳线帽。