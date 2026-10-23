/**
 * @file    ir_key_map.h
 * @brief   红外按键路由：歌华RC6机顶盒、NEC电视机及学习值
 * @note    物理按键号沿用 KEYSCAN_KEY1..18（见 app_keyscan.h）：
 *            KEY1 电视机电源 / KEY2 静音 / KEY3 电视 / KEY4 确认 / KEY5 左 /
 *            KEY6 右 / KEY7 下 / KEY8 主页 / KEY9 返回 / KEY10 菜单 /
 *            KEY11 频道+ / KEY12 音量+ / KEY13 频道- / KEY14 音量- /
 *            KEY15 语音 / KEY16 上 / KEY17 回看 / KEY18 机顶盒电源
 *
 *          机顶盒默认使用歌华RC6；电视机电源及学习值支持原始波形、歌华RC6及NEC。
 *          学习值优先，歌华未定义键不发送；下方NEC表保留用于兼容查询。
 */
#ifndef _IR_KEY_MAP_H_
#define _IR_KEY_MAP_H_

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * 一、常量定义
 *===========================================================================*/

/** @brief 物理按键数量（KEYSCAN_KEY1..18，索引 0 未用） */
#define IR_KEY_COUNT            18u
#define IR_KEY_TV_POWER         1u  /* right, original KEY1 position */
#define IR_KEY_STB_POWER        18u /* left, PB9--PA6 */

/** @brief 电视机业务键号（无默认RC6码，未学习时仅保留BLE业务） */
#define IR_KEY_TV_BUSINESS      3u

/** @brief 默认用户码（机顶盒 STB 0x3A，GY/T 407-2024 §6.1.3） */
#define IR_DEFAULT_USER_CODE    0x3Au

/*===========================================================================
 * 二、兼容NEC键值表（GY/T 407-2024 表 B.1；不是当前机顶盒路由表）
 *
 *  按 KEYSCAN_KEY 索引（0 未用），与 app_keyscan.h 的物理键一一对应。
 *===========================================================================*/

static const uint8_t ir_default_cmd_table[IR_KEY_COUNT + 1u] =
{
    0x00u,  /* [0]  未用                          */
    0xDCu,  /* [1]  KEY1  电视机电源                     */
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
    0x37u,  /* [17] KEY17 回看                     */
    0xDCu,  /* [18] KEY18 STB POWER, PB9--PA6      */
};

/*===========================================================================
 * 三、查询接口
 *===========================================================================*/

/**
 * @brief  查询物理按键的默认红外数据码
 * @param  key 物理按键号（KEYSCAN_KEY1..18）
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
 * @brief  兼容接口：切换旧NEC机顶盒用户码（0x3A ↔ 0x3C）
 * @note   UI组合键已停用；此接口不影响固定歌华RC6系统码0x26。
 * @return true = 切换后为默认码 0x3A，false = 切换后为备用码 0x3C
 */
bool ir_user_switch_stb(void);

/**
 * @brief  切换电视机用户码（0x51 ↔ 0x53）
 * @return true = 切换后为默认码 0x51，false = 切换后为备用码 0x53
 */
bool ir_user_switch_tv(void);

/**
 * @brief  按物理键发送红外（学习波形优先，电视电源NEC，其余已定义键歌华RC6）
 * @param  key 物理按键号（KEYSCAN_KEY1..18）
 * @note   歌华未定义键不发送；已学习的NEC命令0x00仍有效；只有KEY1/2/3/17允许学习。
 */
bool ir_send_key(uint8_t key);
/** Service pending frames in BLE task; repeat only a successfully sent held key. */
void ir_tx_process(void);
void ir_tx_repeat(uint8_t key);
void ir_tx_release(void);
/* OTA owns Flash timing: stop current IR, discard queued frames. */
void ir_tx_cancel(void);
bool ir_tx_pending(void);
uint8_t ir_user_for_key(uint8_t key);
void ir_user_reset(void);
void ir_send_pair_code_target(bool television);

/**
 * @brief  发送机顶盒配对请求：歌华RC6系统码0x26、命令0xAD
 */
void ir_send_pair_code(void);

#endif /* _IR_KEY_MAP_H_ */
