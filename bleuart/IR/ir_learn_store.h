/**
 * @file    ir_learn_store.h
 * @brief   红外学习键值持久化 — 学习值优先、默认值兜底
 * @note    每个物理按键（KEYSCAN_KEY1..17）可学到一个 (user, cmd) 对，
 *          存入 Flash 独立 4KB 扇区 @ 0x0106B000。
 *          存储结构：Magic + Version + Count + Checksum + entries[]。
 */
#ifndef _IR_LEARN_STORE_H_
#define _IR_LEARN_STORE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  加载学习键值到 RAM 缓存
 * @return true = 有效数据已恢复，false = 空/校验失败（全部回默认值）
 * @note   须在 Qflash_Init() 之后调用。
 */
bool ir_learn_store_load(void);

/**
 * @brief  保存某个按键的学习值
 * @param  key  物理按键号（KEYSCAN_KEY1..17）
 * @param  user 学习到的 NEC 用户码
 * @param  cmd  学习到的 NEC 数据码
 * @return true = 保存成功，false = 参数非法或 Flash 操作失败
 */
bool ir_learn_store_save(uint8_t key, uint8_t user, uint8_t cmd);

/**
 * @brief  查询按键是否已学习（仅返回学习值，不兜底默认值）
 * @param  key  物理按键号（KEYSCAN_KEY1..17）
 * @param  user 输出学习到的 NEC 用户码
 * @param  cmd  输出学习到的 NEC 数据码
 * @return true = 已学习（user/cmd 有效），false = 未学习
 */
bool ir_learn_store_get_learned(uint8_t key, uint8_t *user, uint8_t *cmd);

/**
 * @brief  查询按键的最终发送码（学习值优先，否则默认值）
 * @param  key  物理按键号（KEYSCAN_KEY1..17）
 * @param  user 输出用户码
 * @param  cmd  输出数据码
 * @return true = 有效，false = 按键号非法
 */
bool ir_learn_store_lookup(uint8_t key, uint8_t *user, uint8_t *cmd);

/**
 * @brief  清除全部学习值（恢复默认键值）
 * @note   用于工厂复位联动：擦除整个学习扇区并回退默认值。
 */
void ir_learn_store_clear_all(void);

#endif /* _IR_LEARN_STORE_H_ */
