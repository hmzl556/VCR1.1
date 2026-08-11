1. 功能描述
    1. TIM3 CH2捕获引脚，通过CH1下降沿与CH2上升沿计算占空比与频率

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
            HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM1时钟=64M，TIM3时钟=64M
         2. 中断：
                    开启TIM3 CC2比较中断，优先级1
         3. 端口配置：
                    PB5 配置为TIM3 CH2输入
                    PA6 配置为IO输出
         4. 定时器配置：
                    TIM3 CH1捕获CH2信号的下降沿，TIM3 CH2捕获CH2信号的上升沿。
     操作步骤：
         1. 编译后打开调试模式，将PB5与PA6短接，把TIM3Freq、gOnePulsEn变量添加到观察窗口
         2. 默认gOnePulsEn=0；每次手动将gOnePulsEn置1，即可在DutyCycle、Frequency变量中读取捕获得到的占空比和频率数值。

4. 注意事项
    无