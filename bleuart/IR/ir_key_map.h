/**
 * @file    ir_key_map.h
 * @brief   红外按键路由：歌华RC6机顶盒、NEC电视机及学习值
 * @note    物理按键号采用 SDK 44 键编号 KEY_n（见 app_keyscan.h，2026-11-17
 *           飞线枚举确认，与新版原理图线对一致）：
 *            1 电视机电源 / 2 机顶盒电源 / 3 确定 / 4 频道- / 5 上 / 6 下 /
 *            7 音量- / 8 左 / 9 右 / 10 音量+ / 11 回看 / 12 直播业务 /
 *            13 主页 / 14 点播(暂借设置码) / 15~29 数字与语音等 /
 *            16 语音 / 17 频道+ / 18 菜单 / 19 返回 / 23 电视机业务
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

/** @brief 物理按键数量（SDK KEY_n 1..29，索引 0 未用） */
#define IR_KEY_COUNT            29u
#define IR_KEY_TV_POWER         1u  /* 电视机电源(右) */
#define IR_KEY_STB_POWER        2u  /* 机顶盒电源(左) */

/** @brief 电视机业务键号（无默认RC6码，未学习时仅保留BLE业务） */
#define IR_KEY_TV_BUSINESS      23u

/** @brief 默认用户码（机顶盒 STB 0x3A，GY/T 407-2024 §6.1.3） */
#define IR_DEFAULT_USER_CODE    0x3Au

/*===========================================================================
 * 二、兼容NEC键值表（GY/T 407-2024 表 B.1；不是当前机顶盒路由表）
 *
 *  按 SDK KEY_n 索引（0 未用），与 app_keyscan.h 的物理键一一对应。
 *  点播键暂借设置键码 0x8D（甲方确认）。
 *===========================================================================*/

static const uint8_t ir_default_cmd_table[IR_KEY_COUNT + 1u] =
{
    0x00u,  /* [0]  未用                          */
    0xDCu,  /* [1]  电视机电源                     */
    0xDCu,  /* [2]  机顶盒电源                     */
    0xCEu,  /* [3]  确定                           */
    0x86u,  /* [4]  频道-                          */
    0xCAu,  /* [5]  上                             */
    0xD2u,  /* [6]  下                             */
    0x81u,  /* [7]  音量-                          */
    0x99u,  /* [8]  左                             */
    0xC1u,  /* [9]  右                             */
    0x80u,  /* [10] 音量+                          */
    0x37u,  /* [11] 回看                           */
    0x3Du,  /* [12] 直播业务                       */
    0x88u,  /* [13] 主页                           */
    0x8Du,  /* [14] 点播（暂借设置码）             */
    0x92u,  /* [15] 数字1                          */
    0xE5u,  /* [16] 语音                           */
    0x85u,  /* [17] 频道+                          */
    0x82u,  /* [18] 菜单                           */
    0x95u,  /* [19] 返回                           */
    0x93u,  /* [20] 数字2                          */
    0xCCu,  /* [21] 数字3                          */
    0x8Eu,  /* [22] 数字4                          */
    0x88u,  /* [23] 电视机业务                     */
    0x8Fu,  /* [24] 数字5                          */
    0xC8u,  /* [25] 数字6                          */
    0x8Au,  /* [26] 数字7                          */
    0x8Bu,  /* [27] 数字8                          */
    0xC4u,  /* [28] 数字9                          */
    0x87u,  /* [29] 数字0                          */
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
/**
 * @brief  按物理键向电视机发送红外（学习波形优先，否则电视 NEC 表）
 * @param  key 物理按键号（SDK KEY_n 1..29）
 * @note   电视模式路由与电视机业务/电视电源共用; 语音键除外
 */
bool ir_send_key_tv(uint8_t key);
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
