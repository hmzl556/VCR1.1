1. 功能描述
    1. TIM1输出3路互补波形以及1路CH4波形

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
            HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM1时钟=64M
        2. 端口配置：
                    PB8 配置为TIM1 CH1输出
                    PB9 配置为TIM1 CH2输出
                    PB10配置为TIM1 CH3输出
                    PB11配置为TIM1 CH1N输出
                    PB12配置为TIM1 CH2N输出
                    PB13配置为TIM1 CH3N输出
                    PA4 配置为TIM1 CH4输出

        3. 定时器配置：
            TIM1六通道互补输出，开启CH4输出

    操作步骤：
        1. 编译后打开调试模式，使用示波器或者逻辑分析仪观测TIM1输出波形
        2. TIM1输出3路互补波形外加1路CH4波形

4. 注意事项
    PA4引脚复用为TIM3_CH3N和SWDCLK。上电后需要延时2秒，再将PA4配置为TIM3_CH3N功能。