/**
 * @file etv.h
 * @brief 红外遥控基础信息文件（ETV）解析：GY/T 407-2024 表 E.24
 *
 * 数据直接从覆盖层 Flash 按需读取（upg_store_read_at），**不整块载入 RAM**，
 * 因此本模块零静态 RAM；状态结构由调用方放在栈上。
 *
 * 文件结构：
 *   文件头 28B → 协议表 × table_num → 按键 × key_num → [映射表] → [扩展区]
 *
 * 协议表每条大小随 col_num 变化：
 *   8 + 16(b0..b3) + 4(header) + 4(header2) + col_num × 7(bit_num/ref_col/has_header/burst)
 *
 * 按键每条 11 字节：key_id(2) table_index(1) key_value[8](8)
 */
#ifndef _ETV_H_
#define _ETV_H_

#include <stdint.h>
#include <stdbool.h>
#include "upg_store.h"

#define ETV_HDR_LEN        28u
#define ETV_TABLE_FIXED    32u   /**< enc..col_num(8) + b0..b3(16) + header(4) + header2(4) */
#define ETV_COL_LEN         7u   /**< bit_num(1) + ref_col_index(1) + has_header(1) + burst(4) */
#define ETV_KEY_LEN        11u   /**< key_id(2) + table_index(1) + key_value(8) */
#define ETV_HEADER_FILE_LEN 9u   /**< 文件头 + 首个协议表（常见单表文件） */

/** 文件级信息（栈上，约 14 字节） */
typedef struct {
    upg_store_type_t type;
    uint16_t len;          /**< 记录总长 */
    uint16_t table_off;    /**< 第一个协议表偏移 */
    uint16_t key_off;      /**< 按键表偏移 */
    uint16_t key_num;
    uint8_t  table_num;
    uint8_t  ratio_freq;   /**< 高 4bit 载波、低 4bit 占空比 */
    uint8_t  scale;
    uint8_t  repeat_mode;
} etv_file_t;

/** 单个协议表的参数（栈上，约 40 字节） */
typedef struct {
    uint16_t unit;
    uint16_t b0[2];
    uint16_t b1[2];
    uint16_t b2[2];
    uint16_t b3[2];
    uint16_t header[2];
    uint16_t header2[2];
    uint16_t burst[2];
    uint8_t  enc;            /**< 1=PW 脉宽编码，2=BP 双相编码 */
    uint8_t  has_start;
    uint8_t  has_toggle_bit;
    uint8_t  data_bit_num;
    uint8_t  header2_pos;
    uint8_t  col_num;
    uint8_t  bit_num;
    uint8_t  ref_col_index;
    uint8_t  has_header;
} etv_table_t;

/** @brief 一次按键查询的结果（栈上，约 12 字节） */
typedef struct {
    uint8_t  table_index;
    uint8_t  value[8];       /**< key_value，低字节在前（b2/b3 为 0 时按位展开） */
} etv_key_t;

/**
 * @brief 打开并校验文件头，算出协议表与按键表的偏移
 * @return false：无记录 / tag 不是 'ETV\0' / 结构越界
 */
bool etv_open(etv_file_t *f, upg_store_type_t type);

/**
 * @brief 读第 index 个协议表
 * @note  当前只支持 col_num == 1（多列留待接入真实样本后确认语义）
 */
bool etv_read_table(const etv_file_t *f, uint8_t index, etv_table_t *t);

/**
 * @brief 按 key_id 查按键
 * @return false：文件里没有该 key_id
 */
bool etv_find_key(const etv_file_t *f, uint16_t key_id, etv_key_t *k);

/** @brief ratio_freq 里的载波编码 → 频率 Hz（表 E.24 第 6 项），0 表示未知 */
uint32_t etv_carrier_hz(uint8_t ratio_freq);

/** @brief ratio_freq 里的占空比编码 → 百分比（1/2、1/3、1/4、1/5） */
uint8_t etv_duty_percent(uint8_t ratio_freq);

/** @brief 载波频率 → 载波周期（定时器 tick，MCU 定时器 64 MHz） */
uint16_t etv_period_ticks(uint32_t carrier_hz);

/**
 * @brief 把一条按键编码成 mark/space 交替的时长序列（单位 µs）
 * @param t      协议表参数
 * @param k      按键（含 key_value）
 * @param scale  文件级 scale（所有时长乘该系数）
 * @param w      输出缓冲
 * @param cap    w 的容量（条数）
 * @param count  输出条数；**保证为偶数**（ir_raw_t 要求偶数）
 * @return false：缓冲不足 / 参数不支持
 * @note  PW(enc=1)：b2、b3 全 0 时 1 bit/符号（0→b0，1→b1）；否则 2 bit/符号（00→b0…11→b3）
 *        BP(enc=2)：1→1P1S，0→0S0P；has_start 在引导码后补一个 unit 高电平；
 *                    has_toggle_bit 时第 4 位（0 基）时长翻倍；相邻同电平自动合并
 */
bool etv_encode(const etv_table_t *t, const etv_key_t *k, uint8_t scale,
                uint16_t *w, uint16_t cap, uint16_t *count);

#endif /* _ETV_H_ */
