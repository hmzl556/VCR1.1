/**
 * @file upg_session.h
 * @brief 键值下载的独占会话：复用 OTA 会话互斥并借用 Flash 写缓冲作为工作区
 *
 * 设计依据见 docs/GYT407键值下载-RAM与存储约束定稿.md：
 * 芯片 SRAM 48KB 已用满（余 84B），因此本功能**不新增静态 RAM**——
 * 所有会话态（状态机、摘要、序号、对端句柄、升级标志）都放在借来的缓冲里，
 * 因为独占锁在整个会话期间持有，缓冲不会被别人改写。
 */
#ifndef _UPG_SESSION_H_
#define _UPG_SESSION_H_

#include <stdint.h>
#include <stdbool.h>

#include "upg_store.h"   /* upg_store_wr_t：写过程态借用会话工作区 */

#define UPG_IO_MAX          140u   /**< 收包/组包：MTU 139 - 3 = 136，按 140 预留 */
#define UPG_SCRATCH_MAX     256u   /**< 读：文本生成临时区；写：整页对齐余量 */
#define UPG_WS_NEED         508u   /**< 总需求，留 4 字节做 4 字节对齐余量 */

/**
 * 联调期放宽会话空闲超时：手机手动敲帧/换数据需要时间，30 秒容易超时。
 * **量产必须改回 0**（会与「暂时改动文档」一起检查）。
 * 注意：会话期间不广播、不发红外、不跑 CAT1 上报，放宽等于把这几项一起挂起这么久；
 * 一旦 BLE 断开仍会立即收尾，所以风险仅限"连着但一直不操作"。
 */
#ifndef UPG_DEBUG_LONG_IDLE
#define UPG_DEBUG_LONG_IDLE 1u
#endif

#if UPG_DEBUG_LONG_IDLE
#define UPG_SESSION_IDLE_MS  600000u   /**< 10 分钟（联调） */
#else
#define UPG_SESSION_IDLE_MS   30000u   /**< 30 秒（设计值） */
#endif

/** 会话态：生命周期与借来的缓冲一致，不占静态 RAM */
typedef struct {
    uint32_t last_ms;          /**< 最近一次交互时刻（会话超时判据） */
    uint32_t flag;             /**< 升级标志 0xF5F5F500 有效 */
    uint32_t rng;              /**< 认证随机数种子 */
    uint16_t rd_len;           /**< 待上行发送总长 */
    uint16_t rd_off;           /**< 已发送长度 */
    uint8_t  hash1[16];        /**< 本端计算的 rand1 摘要 */
    uint8_t  reading;          /**< 正在分包发送读结果 */
    uint8_t  pack_len;         /**< 请求的每包长度 16/32/64/128 */
    uint8_t  seq;              /**< 上行包序号 */
    uint8_t  cfg_target;       /**< 写指令配置：控制目标（P5 生效） */
    uint8_t  cfg_has;          /**< 是否收到过 0x22 */
    uint8_t  auth_state;       /**< upg_auth_state_t */
    uint8_t  key_index;        /**< 认证密钥索引 */
    uint8_t  entered;          /**< 独占资源已获取（在任务上下文完成） */
    uint8_t  auth_pending;     /**< 待发起认证（由任务发送，避免在 CCCD 回调中发通知） */
    /* ---- 键值写入（0x12/0x14/0x13）：回调只置标志，Flash 操作全部在任务上下文 ---- */
    uint8_t  writing;          /**< 写会话进行中 */
    uint8_t  wr_type;          /**< upg_type_t */
    uint8_t  wr_expect;        /**< 期望的下一个包序号 */
    uint8_t  wr_pkt_ready;     /**< 已收到一包待落盘（数据在 io 里） */
    uint8_t  wr_start;         /**< 已收到启动写请求，待擦扇区/压实 */
    uint8_t  wr_abort;         /**< 已收到终止读写请求，待提交或回滚 */
    uint8_t  flag_pending;     /**< 待写入升级标志（擦扇区在任务上下文） */
    uint16_t wr_len;           /**< 本次数据总长 */
    upg_store_wr_t wo;         /**< 存储层写过程态（不占静态 RAM） */
    uint16_t wr_crc;           /**< 本端累计 CRC2 */
    uint16_t wr_crc1;          /**< 主机下发的 CRC1 */
} upg_state_t;

typedef struct {
    uint8_t     io[UPG_IO_MAX];
    upg_state_t st;
    uint8_t     scratch[UPG_SCRATCH_MAX];
} upg_ws_t;

/**
 * @brief  进入独占会话：借出工作区，并置忙标志
 * @return true 成功；false 已在 OTA 会话中或缓冲不可借
 * @note   幂等。**可在 GATT 回调中调用**——这里只做指针赋值和清零，
 *         停广播/停语音等协议栈相关动作延后到任务上下文的 upg_session_poll()。
 */
bool upg_session_begin(void);

/** @brief 退出会话，归还原缓冲并恢复广播 */
void upg_session_end(void);

/** @brief 会话是否进行中 */
bool upg_session_busy(void);

/** @brief 刷新会话空闲计时（收到任何合法帧时调用） */
void upg_session_touch(void);

/** @brief 空闲超时检查，需周期调用 */
void upg_session_poll(void);

/** @brief 取工作区指针；会话未激活时返回 NULL */
upg_ws_t *upg_ws(void);

#endif /* _UPG_SESSION_H_ */
