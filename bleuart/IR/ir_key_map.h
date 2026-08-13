/**
 * @file    ir_key_map.h
 * @brief   红外按键映射 — 物理按键 → GY/T 407-2024 默认红外键值
 * @note    物理按键号沿用 KEYSCAN_KEY1..17（见 app_keyscan.h）：
 *            KEY1 电源 / KEY2 静音 / KEY3 电视 / KEY4 确认 / KEY5 左 /
 *            KEY6 右 / KEY7 下 / KEY8 主页 / KEY9 返回 / KEY10 菜单 /
 *            KEY11 频道+ / KEY12 音量+ / KEY13 频道- / KEY14 音量- /
 *            KEY15 语音 / KEY16 上 / KEY17 未定义
 *
 *          默认键值取 GY/T 407-2024 表 B.1（NEC 数据码）。学习值由
 *          ir_learn_store 管理，发送时"学习值优先、默认值兜底"。
 */
#ifndef _IR_KEY_MAP_H_
#define _IR_KEY_MAP_H_

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * 一、常量定义
 *===========================================================================*/

/** @brief 物理按键数量（KEYSCAN_KEY1..17，索引 0 未用） */
#define IR_KEY_COUNT            17u

/** @brief 电视机业务键号（控制对象仅电视机，用 TV 用户码） */
#define IR_KEY_TV_BUSINESS      3u

/** @brief 默认用户码（机顶盒 STB 0x3A，GY/T 407-2024 §6.1.3） */
#define IR_DEFAULT_USER_CODE    0x3Au

/*===========================================================================
 * 二、默认键值表（GY/T 407-2024 表 B.1）
 *
 *  按 KEYSCAN_KEY 索引（0 未用），与 app_keyscan.h 的物理键一一对应。
 *===========================================================================*/

static const uint8_t ir_default_cmd_table[IR_KEY_COUNT + 1u] =
{
    0x00u,  /* [0]  未用                          */
    0xDCu,  /* [1]  KEY1  电源                     */
    0x9Cu,  /* [2]  KEY2  静音                     */
    0x88u,  /* [3]  KEY3  电视                     */
    0xCEu,  /* [4]  KEY4  确认                     */
    0x99u,  /* [5]  KEY5  左                       */
    0xC1u,  /* [6]  KEY6  右                       */
    0xD2u,  /* [7]  KEY7  下                       */
    0x88u,  /* [8]  KEY8  主页                     */
    0x95u,  /* [9]  KEY9  返回                     */
    0x82u,  /* [10] KEY10 菜单                     */
    0x85u,  /* [11] KEY11 频道+                    */
    0x80u,  /* [12] KEY12 音量+                    */
    0x86u,  /* [13] KEY13 频道-                    */
    0x81u,  /* [14] KEY14 音量-                    */
    0xE5u,  /* [15] KEY15 语音                     */
    0xCAu,  /* [16] KEY16 上                       */
    0x00u,  /* [17] KEY17 未定义                   */
};

/*===========================================================================
 * 三、查询接口
 *===========================================================================*/

/**
 * @brief  查询物理按键的默认红外数据码
 * @param  key 物理按键号（KEYSCAN_KEY1..17）
 * @return 默认数据码；按键号越界返回 0
 */
static inline uint8_t ir_default_cmd(uint8_t key)
{
    if (key == 0u || key > IR_KEY_COUNT) {
        return 0x00u;
    }
    return ir_default_cmd_table[key];
}

/**
 * @brief  查询默认红外用户码
 * @return 默认用户码（STB 0x3A）
 */
static inline uint8_t ir_default_user(void)
{
    return IR_DEFAULT_USER_CODE;
}

/*===========================================================================
 * 四、用户码管理与发送封装（实现见 ir_key_map.c）
 *===========================================================================*/

/**
 * @brief  切换机顶盒用户码（0x3A ↔ 0x3C）
 * @return true = 切换后为默认码 0x3A，false = 切换后为备用码 0x3C
 */
bool ir_user_switch_stb(void);

/**
 * @brief  切换电视机用户码（0x51 ↔ 0x53）
 * @return true = 切换后为默认码 0x51，false = 切换后为备用码 0x53
 */
bool ir_user_switch_tv(void);

/**
 * @brief  按物理键发送红外（学习值优先，否则当前用户码 + 默认键值）
 * @param  key 物理按键号（KEYSCAN_KEY1..17）
 * @note   未定义键（默认 cmd=0）不发送。
 */
void ir_send_key(uint8_t key);

/**
 * @brief  发送配对请求红外码 0x7D（用当前机顶盒用户码）
 */
void ir_send_pair_code(void);

#endif /* _IR_KEY_MAP_H_ */
