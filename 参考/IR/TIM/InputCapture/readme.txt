1. 功能描述
    1. TIM3 CH2通过上升沿捕获计算输入信号频率

2. 使用环境
    软件开发环境：KEIL MDK‑ARM V5.30.0.0
    硬件环境：基于BR1601 DK开发板开发

3. 使用说明
    系统配置：
        1. 时钟源：
            HSI=64M，AHB=64M，APB1=32M，APB2=64M，TIM3时钟=64M
        2. 中断：
            开启TIM3 CH2上升沿中断，优先级1
        3. 端口配置：
                     PB5 配置为TIM3 CH2输入
                     PA6 配置为普通IO输出
        4. 定时器配置：
            开启TIM3 CH2上升沿捕获中断

操作步骤：
    1. 编译后打开调试模式，将PB5与PA6短接，把TIM3Freq、gOnePulsEn变量添加到观察窗口
    2. 默认gOnePulsEn=0；每次手动将gOnePulsEn置1，即可读取TIM3Freq中计算得到的频率数值

4. 注意事项
    无