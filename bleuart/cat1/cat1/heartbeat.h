/**
 * @file    heartbeat.h
 * @brief   服务器心跳 — 周期唤醒 MCU 并通过 CAT1 向服务器报活
 *          (遥控器基本信息 / 电量 / SIM 卡号)
 *
 * 计时方式: app_now_ms() 单调毫秒计数, 无需 RTC 闹钟:
 *   - 工作态: 主循环每圈 heartbeat_poll() 检查;
 *   - 待机态: br_sleep 深睡期间 SDK 对毫秒计数做睡眠补偿(90s 超时已验证),
 *     每个连接事件唤醒(~5s)时检查, 到期切回工作态触发;
 *   - 休眠态: br_sleep 循环的噪音唤醒(~1-2s 一次)时检查, 到期退出循环,
 *     即"定时唤醒 MCU", 精度 ±2s 对周期无影响。
 *
 * 时间戳存 RAM: 复位后重新计时(可接受)。32 位毫秒约 49.7 天回绕,
 * 到期判断用有符号差值, 回绕安全。
 */

#ifndef _HEARTBEAT_H_
#define _HEARTBEAT_H_

#include <stdint.h>

/**
 * 出厂默认心跳周期: 0=关闭(出厂状态, 等服务器下发间隔后才开始心跳);
 * 当前测试值 1800s(30min, 与规范默认一致); ★正式出厂烧录前改回 0。
 */
#define HEARTBEAT_DEFAULT_PERIOD_MS    (1800u * 1000u)

/** 上电初始化: 按出厂默认周期排程 (0=关闭) */
void heartbeat_init(void);

/**
 * 服务器下发心跳间隔 (响应数据ID 6, 单位秒; 0=关闭):
 * 值变化时更新周期并重新排程; 同值(所有上行响应都会带该字段的
 * 常规回声)不重排, 避免倒计时被任意上报反复清零。
 */
void heartbeat_set_period_s(uint16_t period_s);

/** 是否到期 (周期为 0 时永不到期; 32 位回绕安全) */
uint8_t heartbeat_due(void);

/** 心跳是否开启 (周期非 0; 休眠保底唤醒定时器据此布防) */
uint8_t heartbeat_enabled(void);

/** 是否有待发的心跳业务 (待机分支据此切回工作态执行) */
uint8_t heartbeat_pending(void);

/**
 * 主循环轮询:
 * - 到期 → 周期节拍照常排程下一次, 本次标为待发;
 * - 待发且 CAT1/OTA 忙 → 挂起等待, 业务结束立刻补发 (不丢本次心跳);
 * - 待发且空闲、UDP 已就绪 → 直接组帧入队发送;
 * - 待发且空闲、UDP 未就绪 → CAT1 上电建链, 就绪后由
 *   heartbeat_on_udp_ready() 补发 (提前入队会被 tx 步骤在建链期
 *   误发丢失)。
 */
void heartbeat_poll(void);

/** UDP 就绪回调 (rc_cat1_on_setup_ok 调用): 补发挂起的心跳帧 */
void heartbeat_on_udp_ready(void);

/** 心跳待发期间需强制推进 CAT1 poll (绕过 BLE 活动门控); 1=需要 */
uint8_t heartbeat_needs_cat1_poll(void);

#endif /* _HEARTBEAT_H_ */
