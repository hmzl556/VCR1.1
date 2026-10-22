/**
 * @file    irc.h
 * @brief   IRC 红外控制器驱动 — 类型定义与公共 API
 * @note    基于 BR1601x 硬件 IRC 外设，实现 GY/T 407-2024 §6.1 的
 *          NEC 协议红外发送。硬件自动处理载波、位时序、重复码。
 *
 *          引脚：PB4=IRTX(AF2)，须配置 GPIO 复用并使能 IRC。
 *          时序基准：AHB = HSI = 64MHz。
 */
#ifndef _IRC_H_
#define _IRC_H_

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * 一、IRC 时序参数（AHB=64MHz）
 *===========================================================================*/

/** @brief 载波 38kHz 1/3 占空比：64M/(561+1123)=38005Hz */
#define IRC_CARR_ON         561u
#define IRC_CARR_OFF        1123u

/** @brief NEC 逻辑位时序（载波周期 ≈26.3μs） */
#define IRC_LO_MARK         21u     /* 逻辑0 MARK  560μs */
#define IRC_LO_SPACE        21u     /* 逻辑0 SPACE 560μs */
#define IRC_HI_MARK         21u     /* 逻辑1 MARK  560μs */
#define IRC_HI_SPACE        64u     /* 逻辑1 SPACE 1680μs */

/** @brief Paint message 时长（载波周期数） */
#define IRC_T9MS            342u    /* 引导码 Mark   9ms */
#define IRC_T4P5MS          171u    /* 引导码 Space  4.5ms */
#define IRC_T2P25MS         86u     /* 重复码 Space  2.25ms */
#define IRC_T600US          23u     /* 停止位 Mark   600μs */

/*===========================================================================
 * 二、NEC 协议常量（GY/T 407-2024 §6.1.3 / §7.2）
 *===========================================================================*/

#define IRC_USER_STB        0x3Au   /* 机顶盒默认用户码 */
#define IRC_USER_STB_ALT    0x3Cu   /* 机顶盒备用用户码 */
#define IRC_USER_TV         0x51u   /* 电视机默认用户码 */
#define IRC_USER_TV_ALT     0x53u   /* 电视机备用用户码 */
#define IRC_CMD_PAIR        0x7Du   /* 配对请求红外码 */

/*===========================================================================
 * 三、公共 API
 *===========================================================================*/

/**
 * @brief  初始化 IRC 外设
 * @note   配置 38kHz 载波与 NEC 位时序，使能 IRC，使用 BUSY 轮询而非中断。
 *         配置 PB4 为 AF2 推挽输出，再使能 IRC 选择 IRTX。
 */
void irc_init(void);

/**
 * @brief  发送 NEC 红外帧
 * @param  user_code  NEC 用户码（STB: 0x3A/0x3C, TV: 0x51/0x53）
 * @param  cmd_code   NEC 数据码（参见 GY/T 407-2024 表 B.1）
 * @return true 发送已启动，false IRC 忙或未初始化
 * @note   帧 = 引导码(9ms+4.5ms) + 用户码+反码 + 数据码+反码 + 停止位(600μs)
 */
bool irc_send(uint8_t user_code, uint8_t cmd_code);

/**
 * @brief  发送 NEC 重复码
 * @note   按键保持时调用，波形 = 9ms + 2.25ms + 600μs。
 *         108ms 发送间隔由上层软件定时器管理。
 */
void irc_send_repeat(void);

/**
 * @brief  停止 IRC 发送
 * @note   清 TX_START，硬件自动清空 FIFO。
 */
void irc_stop(void);

/**
 * @brief  查询 IRC 是否正在发送
 * @return true = 发送中，false = 空闲
 */
bool irc_is_busy(void);

#endif /* _IRC_H_ */
