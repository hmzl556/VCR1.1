#ifndef _CMB_CFG_H_
#define _CMB_CFG_H_
/* print line, must config by user */
#define cmb_println(format, ...)               do{BR_LOG_INFO(format,  ## __VA_ARGS__); BR_LOG_INFO("\n");}while(0)/* e.g., printf(__VA_ARGS__);printf("\r\n")  or  SEGGER_RTT_printf(0, __VA_ARGS__);SEGGER_RTT_WriteString(0, "\r\n")  */
/* enable bare metal(no OS) platform */
#define CMB_USING_BARE_METAL_PLATFORM
/* enable OS platform */
/* #define CMB_USING_OS_PLATFORM */
/* OS platform type, must config when CMB_USING_OS_PLATFORM is enable */
// #define CMB_OS_PLATFORM_TYPE           CMB_OS_PLATFORM_RTT or CMB_OS_PLATFORM_UCOSII or CMB_OS_PLATFORM_UCOSIII or CMB_OS_PLATFORM_FREERTOS or CMB_OS_PLATFORM_RTX5 */
/* cpu platform type, must config by user */
#define CMB_CPU_PLATFORM_TYPE          CMB_CPU_ARM_CORTEX_M3/* CMB_CPU_ARM_CORTEX_M0 or CMB_CPU_ARM_CORTEX_M3 or CMB_CPU_ARM_CORTEX_M4 or CMB_CPU_ARM_CORTEX_M7 or CMB_CPU_ARM_CORTEX_M33 */
/* enable dump stack information */
#define CMB_USING_DUMP_STACK_INFO
/* language of print information */
#define CMB_PRINT_LANGUAGE             CMB_PRINT_LANGUAGE_ENGLISH//CMB_PRINT_LANGUAGE_ENGLISH//(default) or CMB_PRINT_LANGUAGE_CHINESE or CMB_PRINT_LANGUAGE_CHINESE_UTF8 */
#endif /* _CMB_CFG_H_ */
