1. Function description
    1. ADC对PB8引脚的模拟电压进行采样转换
    2. ADC转换结果通过DMA_CH1通道读取到变量ADCConvertedValue中

2. Use environment
    软件开发环境：KEIL MDK-ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. Instructions for use
    系统配置：
        1. 时钟源：
                    HSE=32M，HSI=64M，AHB=64M，APB1=32M，APB2=64M，ADC时钟=HSE/8，DMA时钟=64M
        2. 端口配置：
                    PB8设为模拟功能，ADC转换通道3
        3. DMA：
                    DMA1_CH1通道配置为循环模式，将半字ADC转换结果传输至ADCConvertedValue变量
        4. ADC：
                    ADC配置：连续转换，软件触发，10位数据，转换通道3为PB8的模拟电压数据
        5. 日志打印：演示板PB6（发送端），波特率115200，8位数据位，1位停止位，无校验位，无硬件流控
    使用方法：
        1. 编译后下载程序，复位运行，在日志中查看转换结果。
        2. 改变PB8引脚电压，可观察到转换结果变量同步变化。

4. Matters needing attention
    请确保PB8连接IO口，而非晶振。