/**
 * @file ble_key_file.h
 * @brief 蓝牙遥控基础信息文件（E.25）解析：GY/T 407-2024 表 E.25
 *
 * 只处理 `blestb`（机顶盒蓝牙键值）。电视机走红外，`bletv` 按产品范围拒绝，
 * 见 docs/待客户确认事项.md 的 D1。
 *
 * 数据按需从覆盖层 Flash 读取（upg_store_read_at），**不整块载入 RAM**，
 * 因此本模块零静态 RAM；状态结构由调用方放在栈上。
 *
 * ---- 文件结构（用真实样本 753 字节逐字节确认过） ----
 *   0     name(8)        "blestb\0\0"
 *   8     version(8)     "ble1\0\0\0\0"
 *   16    size(4)        721（= 文件长度 - 32）
 *   20    crc32(4)
 *   24    reserved(8)
 *   32    device_nums(4)
 *   36    version2(8)    "bt01\0\0\0\0"
 *   44    设备索引表 × device_nums，每条 16 字节：
 *           device_type(4) device_id(4) detail_off(4) detail_size(4)
 *   detail_off（相对文件起点）：
 *           +0    广播参数 148 字节（配对/回连/开机三套 adv + MAC + scanrsp）
 *           +148  fc2hid_entry_count(1)
 *           +149  fc2hid 表
 *           ...   fc2no_entry_count(1)
 *           ...   fc2no 表
 *           ...   VID(4) PID(4)
 *
 * ---- 两个容易踩的坑 ----
 * 1) **fc2hid 表是固定 48 槽 × 8 字节 = 384 字节**，不是按 entry_count 变长。
 *    规范只写「上限 48」，但实际实现按最大槽位预分配，未用的条目补 0。
 *    按 entry_count 变长解析会错位 64 字节，fc2no 全部读错。
 * 2) **fc2hid 的 usage 字段解释随 page 不同**：
 *      page = 0x07（键盘）  -> 该字段是两个独立单字节键码（key1, key2）
 *      page = 0x0C（消费类）-> 该字段是一个 2 字节大端 usage
 *
 * fc2hid 条目 8 字节：key_fcode(2) page(1) mod(1) usage(2) reserved(2)
 * fc2no  条目 3 字节：key_fcode2(2) key_no(1)
 *
 * 查询链路：物理键 → 表 B.1 序号(key_no) → fc2no 查 fcode → fc2hid 查 page/mod/usage
 */
#ifndef _BLE_KEY_FILE_H_
#define _BLE_KEY_FILE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * 联调期日志开关：把每次按键解析出的 HID 键值、以及 blestb 提交后的整份
 * 映射表打到串口，便于与原始 753 字节逐条核对。
 * **量产必须改回 0**（与其它调试开关一起登记在 暂时改动文档.md）。
 */
#ifndef BLEDL_DEBUG_LOG
#define BLEDL_DEBUG_LOG 1u
#endif

/** 一条「表 B.1 功能序号 → HID 键值」映射 */
typedef struct {
    uint8_t  page;      /**< 0x07 键盘页 / 0x0C 消费类页 */
    uint8_t  mod;       /**< 修饰键位图（键盘页有效，消费类页忽略） */
    uint16_t usage;     /**< HID usage id */
} ble_hid_ent_t;

/** @brief 是否已下载过 blestb（且文件头有效） */
bool ble_key_file_available(void);

/** @brief blestb 记录序号；0 表示无记录（用于三层查表比新旧） */
uint16_t ble_key_file_seq(void);

/**
 * @brief 按表 B.1 功能序号查 HID 键值
 * @param key_no 表 B.1 序号（1~30）
 * @param e      输出映射
 * @return false：无 blestb / 文件结构异常 / 该序号没有映射
 */
bool ble_key_file_lookup(uint8_t key_no, ble_hid_ent_t *e);

/**
 * @brief 打印整份 blestb 映射到串口（联调核对用）
 * @note  仅在 BLEDL_DEBUG_LOG 打开时调用有意义；量产应移除调用点。
 */
void ble_key_file_dump(void);

#endif /* _BLE_KEY_FILE_H_ */
