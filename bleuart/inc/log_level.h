/**
 * @file    log_level.h
 * @brief   串口日志分级开关
 * @note    co_printf 是逐字节忙等发送（bsp/common/src/co_printf.c），
 *          115200 下每字符 86.8us，满一屏 50 字符约 4.3ms 纯 CPU 占用。
 *          打印过多会挤占 BLE 栈的调度时间，实测可在下载/联网期间造成
 *          主机侧链路异常。因此把日志统一分级，交付前只需改一个数字。
 *
 *          分级：
 *            0 = 静默：只保留启动、断连、CRC/发送失败等故障证据
 *            1 = 精简：+ 按键发码链路（每键一行，约 5ms）
 *            2 = 完整：+ 下载抓包、键值映射、RC 会话、AT 命令响应
 *
 *          注意：中断上下文（KEYSCAN_IRQHandler 等）永远不得打印——
 *          co_printf 使用全局共享的 s_dbg_tmp_buf，中断打断主循环的
 *          vsnprintf 会互相踩 buffer，导致日志错乱甚至内容被改写。
 */
#ifndef LOG_LEVEL_H
#define LOG_LEVEL_H

/** @brief 当前日志级别，量产交付前改为 0 */
#define LOG_VERBOSE     2

/** @brief 判断是否达到某级日志 */
#define LOG_ON(n)       (LOG_VERBOSE >= (n))

#endif /* LOG_LEVEL_H */
