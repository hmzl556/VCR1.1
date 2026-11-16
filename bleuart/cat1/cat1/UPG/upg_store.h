/**
 * @file upg_store.h
 * @brief 键值下载覆盖层：原始文件 A/B 双扇区存储（GY/T 407-2024 E.3.4）
 *
 * 只存原始文件字节流，不做解析（解析属于 P3）。读取接口按数据类型取
 * seq 最大的那条记录，供解析层与 usr1 版本回读使用。
 *
 * 记录布局：每条记录起始 4 字节对齐
 *   [upg_rec_hdr_t 12B][payload lenB][crc16 2B]
 * 扇区布局：
 *   [upg_sec_hdr_t 16B][记录区]
 * 扇区头 magic 最后写，写成功才算该份有效。
 */
#ifndef _UPG_STORE_H_
#define _UPG_STORE_H_

#include <stdint.h>
#include <stdbool.h>

/* 数据类型（对应启动读写请求的 8 字节 ASCII 字段） */
typedef enum {
    UPG_ST_FW01 = 0,
    UPG_ST_REL0,
    UPG_ST_USR1,
    UPG_ST_IRTV,
    UPG_ST_IRSTB,
    UPG_ST_BLETV,
    UPG_ST_BLESTB,
    UPG_ST_NUM,
    UPG_ST_NONE = 0xFF
} upg_store_type_t;

#define UPG_STORE_SEC_SIZE    0x1000u   /**< 一个扇区 4KB */
#define UPG_STORE_DATA_MAX    3000u     /**< 单条记录 payload 上限（留压实余量） */

/** 写入过程态：由调用方提供（放在借来的会话工作区），本模块不占静态 RAM */
typedef struct {
    uint32_t sec;        /**< 目标扇区基址 */
    uint32_t pay_addr;   /**< payload 起始地址 */
    uint16_t written;    /**< 已写入长度 */
    uint16_t seq;        /**< 本次提交序号 */
    uint16_t total;      /**< 本次数据总长 */
    uint8_t  busy;       /**< 写入进行中 */
} upg_store_wr_t;

/**
 * @brief 把 8 字节 ASCII 类型字段解析为 upg_store_type_t
 * @return UPG_ST_NONE 表示不认识
 */
upg_store_type_t upg_store_type_of(const uint8_t *field8);

/** @brief 该类型是否允许经本通道写入（fw01 走 UDP OTA，不在此列） */
bool upg_store_type_writable(upg_store_type_t t);

/** @brief 下一个提交序号（两份中所有记录的最大 seq + 1） */
uint16_t upg_store_seq_next(void);

/**
 * @brief 开始一次写入：擦非生效份 → 压实其余记录 → 写本次记录头
 * @param w        写入过程态（调用方提供）
 * @param type      数据类型
 * @param len       payload 长度
 * @param addr_out  输出本次 payload 在 Flash 中的起始地址
 * @return true 成功
 */
bool upg_store_begin(upg_store_wr_t *w, upg_store_type_t type, uint16_t len, uint32_t *addr_out);

/** @brief 追加 payload 数据（按不跨页切块写入） */
bool upg_store_write(upg_store_wr_t *w, const uint8_t *data, uint16_t len);

/** @brief 数据收完：写记录尾 CRC + 扇区头，提交生效 */
bool upg_store_commit(upg_store_wr_t *w, uint16_t crc16);

/** @brief 放弃本次写入（擦掉目标份，旧份不受影响） */
void upg_store_abort(upg_store_wr_t *w);

/**
 * @brief 读取某类型记录
 * @param buf/cap  输出缓冲
 * @return payload 长度；0 表示无记录或校验失败
 */
uint16_t upg_store_load(upg_store_type_t type, uint8_t *buf, uint16_t cap);

/**
 * @brief 从记录里按偏移读一段（不整块载入，供流式解析用）
 * @param type  数据类型
 * @param off   payload 内的偏移
 * @param buf   输出缓冲
 * @param len   读取长度
 * @return true 成功且完全在记录范围内
 */
bool upg_store_read_at(upg_store_type_t type, uint16_t off, uint8_t *buf, uint16_t len);

/** @brief 记录 payload 长度；0 表示无记录 */
uint16_t upg_store_size(upg_store_type_t type);

/** @brief 该类型当前生效记录的提交序号；0 表示无记录（用于判断"谁更新"） */
uint16_t upg_store_seq_of(upg_store_type_t type);

/** @brief 该类型记录是否存在且 CRC 正确 */
bool upg_store_has(upg_store_type_t type);

/** @brief 恢复出厂：擦除两份覆盖层 */
bool upg_store_erase_all(void);

/* ---- 升级标志持久化（表 E.12～E.15）：独立扇区，不碰 OTA 配置区 ---- */

/** @brief 读取升级标志；无有效记录时返回 UPG_FLAG_NONE */
uint32_t upg_flag_load(void);

/** @brief 写入升级标志（先擦后写，掉电保持） */
bool upg_flag_save(uint32_t flag);

#endif /* _UPG_STORE_H_ */
