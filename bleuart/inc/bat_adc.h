#ifndef _BAT_ADC_H_
#define _BAT_ADC_H_

#include <stdint.h>

/**
 * @brief  读取电池电量百分比（0–100）
 * @note   LTE_EN 为低时不上采样，直接返回 100；
 *         LTE_EN 为高时 PB6/CH5 采 8 次平均，按 3.3V=0%、4.2V=100% 线性换算。
 *         语音 ADC 占用时跳过采样，返回上次缓存值。
 */
uint8_t bat_get_level_percent(void);

/** @brief 返回最近一次有效电量缓存（未采样过则为 100） */
uint8_t bat_get_cached_level(void);


#endif /* _BAT_ADC_H_ */
