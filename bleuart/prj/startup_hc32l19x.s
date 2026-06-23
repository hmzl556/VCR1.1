; Stack Configuration
; Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>

Stack_Size      EQU     0x00000200

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp


; Heap Configuration
;  Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>

Heap_Size       EQU     0x00000200

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit


                PRESERVE8
                THUMB


; Vector Table Mapped to Address 0 at Reset

                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors                       
                DCD     __initial_sp              ; Top of Stack
                DCD     Reset_Handler             ; Reset        
                DCD     NMI_Handler               ; NMI
                DCD     HardFault_Handler         ; Hard Fault
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     SVC_Handler               ; SVCall
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     PendSV_Handler            ; PendSV
                DCD     SysTick_Handler           ; SysTick

                DCD     PORTA_IRQHandler
                DCD     PORTB_IRQHandler
                DCD     PORTC_E_IRQHandler
                DCD     PORTD_F_IRQHandler
                DCD     DMAC_IRQHandler
                DCD     TIM3_IRQHandler
                DCD     UART0_2_IRQHandler
                DCD     UART1_3_IRQHandler
                DCD     LPUART0_IRQHandler
                DCD     LPUART1_IRQHandler
                DCD     SPI0_IRQHandler
                DCD     SPI1_IRQHandler
                DCD     I2C0_IRQHandler
                DCD     I2C1_IRQHandler
                DCD     TIM0_IRQHandler
                DCD     TIM1_IRQHandler
                DCD     TIM2_IRQHandler
                DCD     LPTIM0_1_IRQHandler
                DCD     TIM4_IRQHandler
                DCD     TIM5_IRQHandler
                DCD     TIM6_IRQHandler
                DCD     PCA_IRQHandler
                DCD     WDT_IRQHandler
                DCD     RTC_IRQHandler
                DCD     ADC_DAC_IRQHandler
                DCD     PCNT_IRQHandler
                DCD     VC0_IRQHandler
                DCD     VC1_2_IRQHandler
                DCD     LVD_IRQHandler
                DCD     LCD_IRQHandler
                DCD     FLASH_RAM_IRQHandler
                DCD     CLKTRIM_IRQHandler

                

__Vectors_End

__Vectors_Size 	EQU     __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY


; Reset Handler

Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  SystemInit
                IMPORT  __main

               ;reset NVIC if in rom debug
                LDR     R0, =0x20000000
                LDR     R2, =0x0
                MOVS    R1, #0                 ; for warning, 
                ADD     R1, PC,#0              ; for A1609W, 
                CMP     R1, R0
                BLS     RAMCODE

              ; ram code base address. 
                ADD     R2, R0,R2
RAMCODE
              ; reset Vector table address.
                LDR     R0, =0xE000ED08 
                STR     R2, [R0]

                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP


; Dummy Exception Handlers (infinite loops which can be modified)

NMI_Handler     PROC
                EXPORT  NMI_Handler               [WEAK]
                B       .
                ENDP
                
                
HardFault_Handler\
                PROC
                EXPORT  HardFault_Handler         [WEAK]
                B       .
                ENDP
SVC_Handler     PROC
                EXPORT  SVC_Handler               [WEAK]
				;xPSR PC LR R12 R3 R2 R1 R0 R11 R10 R9 R8 R7 R6 R5 R4 R0->R4
				IMPORT Os_readNowSp
				IMPORT Os_saveNowSp
				IMPORT Os_findActTask	
				MRS     R0,MSP
                LDR     R1,[R0,#24]
				SUBS	R1,#2
				LDR		R0,[R1]
				LDR		R1,=0x00000001
				ANDS	R0,R1
                BEQ     Lab_OsSw
				BL		Os_readNowSp		;调用该函数后 R0 = addr R4
				LDR		R4,[R0,#16]			;把任务的R4 - R14从堆栈中恢复出来
				LDR		R5,[R0,#20]
				LDR		R6,[R0,#24]
				LDR		R7,[R0,#28]
				MOV		R8,R4
				MOV		R9,R5
				MOV		R10,R6
				MOV		R11,R7
				LDR		R4,[R0]
				LDR		R5,[R0,#4]
				LDR		R6,[R0,#8]
				LDR		R7,[R0,#12]			
				MSR		MSP,R0
				ADD		SP,#32
				LDR		R0,=0xfffffff9
				push	{R0}
				pop		{PC}
				
Lab_OsSw		MRS		R0,MSP				;R0 ->R0位置
				SUBS	R0,#32				;(R0-32)->R4位置
				STR		R4,[R0]
				STR		R5,[R0,#4]
				STR		R6,[R0,#8]
				STR		R7,[R0,#12]
				MOV		R4,R8
				MOV		R5,R9
				MOV		R6,R10
				MOV		R7,R11
				STR		R4,[R0,#16]
				STR		R5,[R0,#20]
				STR		R6,[R0,#24]
				STR		R7,[R0,#28]			;R0 ->R4位置	
				BL		Os_saveNowSp		;R0是任务栈顶
				BL		Os_findActTask		;得到新任务的栈顶R0 ->新任务R4
				LDR		R4,[R0,#16]
				LDR		R5,[R0,#20]
				LDR		R6,[R0,#24]
				LDR		R7,[R0,#28]
				MOV		R8,R4
				MOV		R9,R5
				MOV		R10,R6
				MOV		R11,R7
				LDR		R4,[R0]
				LDR		R5,[R0,#4]
				LDR		R6,[R0,#8]
				LDR		R7,[R0,#12]			;R0->R4 + 32后指向R0
				MSR		MSP,R0				;SP->R0
				ADD		SP,#32
				LDR		R0,=0xfffffff9
				PUSH	{R0}
				POP		{PC}
				
				ENDP

PendSV_Handler  PROC
                EXPORT  PendSV_Handler            [WEAK]
                B       .
                ENDP
SysTick_Handler PROC
                EXPORT  SysTick_Handler           [WEAK]
                B       .
                ENDP

Default_Handler PROC
                EXPORT  PORTA_IRQHandler                [WEAK]
                EXPORT  PORTB_IRQHandler                [WEAK]
                EXPORT  PORTC_E_IRQHandler              [WEAK]
                EXPORT  PORTD_F_IRQHandler              [WEAK]
                EXPORT  DMAC_IRQHandler                 [WEAK]
                EXPORT  TIM3_IRQHandler                 [WEAK]
                EXPORT  UART0_2_IRQHandler              [WEAK]
                EXPORT  UART1_3_IRQHandler              [WEAK]
                EXPORT  LPUART0_IRQHandler              [WEAK]
                EXPORT  LPUART1_IRQHandler              [WEAK]
                EXPORT  SPI0_IRQHandler                 [WEAK]
                EXPORT  SPI1_IRQHandler                 [WEAK]
                EXPORT  I2C0_IRQHandler                 [WEAK]
                EXPORT  I2C1_IRQHandler                 [WEAK]
                EXPORT  TIM0_IRQHandler                 [WEAK]
                EXPORT  TIM1_IRQHandler                 [WEAK]
                EXPORT  TIM2_IRQHandler                 [WEAK]
                EXPORT  LPTIM0_1_IRQHandler             [WEAK]
                EXPORT  TIM4_IRQHandler                 [WEAK]
                EXPORT  TIM5_IRQHandler                 [WEAK]
                EXPORT  TIM6_IRQHandler                 [WEAK]
                EXPORT  PCA_IRQHandler                  [WEAK]
                EXPORT  WDT_IRQHandler                  [WEAK]
                EXPORT  RTC_IRQHandler                  [WEAK]
                EXPORT  ADC_DAC_IRQHandler              [WEAK]
                EXPORT  PCNT_IRQHandler                 [WEAK]
                EXPORT  VC0_IRQHandler                  [WEAK]
                EXPORT  VC1_2_IRQHandler                [WEAK]
                EXPORT  LVD_IRQHandler                  [WEAK]
                EXPORT  LCD_IRQHandler                  [WEAK]
                EXPORT  FLASH_RAM_IRQHandler            [WEAK]
                EXPORT  CLKTRIM_IRQHandler              [WEAK]
PORTA_IRQHandler
PORTB_IRQHandler
PORTC_E_IRQHandler
PORTD_F_IRQHandler
DMAC_IRQHandler
TIM3_IRQHandler
UART0_2_IRQHandler
UART1_3_IRQHandler
LPUART0_IRQHandler
LPUART1_IRQHandler
SPI0_IRQHandler
SPI1_IRQHandler
I2C0_IRQHandler
I2C1_IRQHandler
TIM0_IRQHandler
TIM1_IRQHandler
TIM2_IRQHandler
LPTIM0_1_IRQHandler
TIM4_IRQHandler
TIM5_IRQHandler
TIM6_IRQHandler
PCA_IRQHandler
WDT_IRQHandler
RTC_IRQHandler
ADC_DAC_IRQHandler
PCNT_IRQHandler
VC0_IRQHandler
VC1_2_IRQHandler
LVD_IRQHandler
LCD_IRQHandler
FLASH_RAM_IRQHandler
CLKTRIM_IRQHandler

                
               B .

                ENDP


                ALIGN


; User Initial Stack & Heap

                IF      :DEF:__MICROLIB

                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                ELSE

                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap
__user_initial_stackheap

                LDR     R0, =  Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, = (Heap_Mem +  Heap_Size)
                LDR     R3, = Stack_Mem
                BX      LR

                ALIGN

                ENDIF


                END
