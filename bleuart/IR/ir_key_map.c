/**
 * @file    ir_key_map.c
 * @brief   红外用户码管理与发送封装
 * @note    发送决策：学习值优先，否则"当前用户码 + 默认键值"。
 *          用户码切换由组合键触发：STB 0x3A↔0x3C，TV 0x51↔0x53。
 *          当前用户码存 RAM，掉电不保存（标准未强制持久化）。
 */

#include "ir_key_map.h"
#include "ir_learn_store.h"
#include "irc.h"

/*===========================================================================
 * 一、当前用户码状态（RAM）
 *===========================================================================*/

/** @brief 当前机顶盒用户码（默认 0x3A，备用 0x3C） */
static uint8_t s_ir_user_stb = IRC_USER_STB;

/** @brief 当前电视机用户码（默认 0x51，备用 0x53） */
static uint8_t s_ir_user_tv  = IRC_USER_TV;

/*===========================================================================
 * 二、公共 API
 *===========================================================================*/

/**
 * @brief  切换机顶盒用户码（0x3A ↔ 0x3C）
 * @return true = 切换后为默认码 0x3A，false = 切换后为备用码 0x3C
 */
bool ir_user_switch_stb(void)
{
    s_ir_user_stb = (s_ir_user_stb == IRC_USER_STB) ? IRC_USER_STB_ALT : IRC_USER_STB;
    return (s_ir_user_stb == IRC_USER_STB);
}

/**
 * @brief  切换电视机用户码（0x51 ↔ 0x53）
 * @return true = 切换后为默认码 0x51，false = 切换后为备用码 0x53
 */
bool ir_user_switch_tv(void)
{
    s_ir_user_tv = (s_ir_user_tv == IRC_USER_TV) ? IRC_USER_TV_ALT : IRC_USER_TV;
    return (s_ir_user_tv == IRC_USER_TV);
}

/**
 * @brief  按物理键发送红外（学习值优先，否则当前用户码 + 默认键值）
 * @param  key 物理按键号（KEYSCAN_KEY1..17）
 * @note   未定义键（默认 cmd=0）不发送。
 *         电视机业务键（KEY3）用 TV 用户码，其余键用 STB 用户码。
 */
void ir_send_key(uint8_t key)
{
    uint8_t user;
    uint8_t cmd;

    if (ir_learn_store_get_learned(key, &user, &cmd)) {
        /* 学习值优先：user/cmd 直接采用学习结果 */
    } else {
        cmd = ir_default_cmd(key);
        if (cmd == 0u) {
            return;  /* 未定义键 */
        }
        /* 电视机业务键用 TV 用户码，其余用 STB 用户码（机顶盒遥控器） */
        user = (key == IR_KEY_TV_BUSINESS) ? s_ir_user_tv : s_ir_user_stb;
    }
    irc_send(user, cmd);
}

/**
 * @brief  发送配对请求红外码 0x7D（用当前机顶盒用户码）
 * @note   机顶盒收到 0x7D 后进入配对模式（GY/T 407-2024 §7.2）。
 */
void ir_send_pair_code(void)
{
    irc_send(s_ir_user_stb, IRC_CMD_PAIR);
}
