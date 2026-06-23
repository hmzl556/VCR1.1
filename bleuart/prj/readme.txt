1. Demo description
    Bluetooth BLE Data Transfer Service (DTS) slave routine, using a 128-bit UUID.
    After connecting the slave device, the data received by Bluetooth will be transparently transmitted to USART1,The data received by USART1 is transparently transmitted to the peer device.
    In addition, the serial port can also send a two-byte 0xfa 0x55.
    Note that since the serial port can only take effect after connecting.The default value is slave mode.

2. Demo environment
    IDE: KEIL MDK-ARM V5.30.0.0
    Hardware: BR1601A01 DK

3. Instructions
    System configuration;
        1. Clock source: HSI=64M, AHB=64M, APB1=32M, APB2=64M
        2. Log print: DEMO board PB1(TX), baud rate: 115200, 8 data bits, 1 stop bit, no parity bit, no hardware flow control
        3. Bluetooth BLE name:BR1601Axx
        4. BLE Data: USART1 PB6(TX) and PB7(RX), baud rate: 115200, 8 data bits, 1 stop bit, no parity bit, no hardware flow control
        5. This example can test with ble_dtsc.

        How to use:
        1. Compile and burn to the development board.
        2. Power on the board and check the logs.
        3. Set up another board as the master connection.
        4. Send data to each other via USART1.

4. Note
    Because the program will enter sleep mode, SWD cannot be used.
    Press the RESET button to perform the program burning step within one second.