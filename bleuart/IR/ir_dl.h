/**
 * @file ir_dl.h
 * @brief 红外「下载层」：使用 APP 下发的 ETV 键值发码（GY/T 407-2024）
 *
 * 三层查表里的第 2 层：
 *   1) 学习值      Flash 波形区（ir_wave_store）
 *   2) 下载值      ETV 覆盖层（本模块，数据来自 upg_store）
 *   3) 出厂默认    ROM 表（ir_key_map 的 NEC/RC6）
 *
 * 电视机只走红外、不建立任何蓝牙连接，因此电视机键查 irtv、机顶盒键查 irstb。
 * 本模块不做缓存：每次发码按需从 Flash 读结构，零静态 RAM。
 */
#ifndef _IR_DL_H_
#define _IR_DL_H_

#include <stdint.h>
#include <stdbool.h>

/** @brief 该按键是否有可用的下载键值；tv=true 查 irtv，false 查 irstb */
bool ir_dl_available(uint8_t key, bool tv);

/** @brief 该层（irtv/irstb）的提交序号；0 表示没有下载数据 */
uint16_t ir_dl_seq(bool tv);

/**
 * @brief 用下载键值发一帧
 * @return true 已启动发送；false 没有该键 / 数据非法 / 发送器忙或失败
 */
bool ir_dl_send(uint8_t key, bool tv);

#endif /* _IR_DL_H_ */
