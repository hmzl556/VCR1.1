/**
 * @file    ir_learn_store.c
 * @brief   红外学习键值持久化实现
 * @note    存储布局（Flash @ 0x0106B000，4KB 扇区）：
 *
 *          ┌────────────┬────────────┬────────────┬────────────┬──────────────┐
 *          │ magic(4B)  │ version(2B)│ count(2B)  │ checksum(4B)│ entries[18] │
 *          └────────────┴────────────┴────────────┴────────────┴──────────────┘
 *
 *          entry = { user_code(1B), cmd_code(1B), valid(1B), reserved(1B) }。
 *          校验方式：全字段异或（与工程 ble_gap/rc_cat1_bridge 一致）。
 */

#include "ir_learn_store.h"
#include "ir_key_map.h"
#include "qflash.h"
#include "co_printf.h"

/*===========================================================================
 * 一、存储常量与结构
 *===========================================================================*/

/** @brief 学习键值存储地址（4KB 独立扇区，紧接云策略之后） */
#define IR_LEARN_STORE_ADDR      0x0106B000u

/** @brief Magic 标记 'IRLN' */
#define IR_LEARN_STORE_MAGIC     0x49524C4Eu

/** @brief 存储格式版本 */
#define IR_LEARN_STORE_VER       1u

/** @brief 单个按键学习项（4 字节，按 KEYSCAN_KEY 索引） */
typedef struct
{
    uint8_t user_code;  /* 学习到的 NEC 用户码 */
    uint8_t cmd_code;   /* 学习到的 NEC 数据码 */
    uint8_t valid;      /* 1 = 已学习 */
    uint8_t reserved;   /* 保留对齐 */
} ir_learn_entry_t;

/** @brief Flash 存储整体结构 */
typedef struct
{
    uint32_t         magic;      /* 有效标记 */
    uint16_t         version;    /* 格式版本 */
    uint16_t         count;      /* 槽位数 = IR_KEY_COUNT */
    uint32_t         checksum;   /* 全字段异或校验 */
    ir_learn_entry_t entries[IR_KEY_COUNT + 1u]; /* 索引 0 未用 */
} ir_learn_store_t;

/*===========================================================================
 * 二、内部静态变量
 *===========================================================================*/

/** @brief RAM 缓存，所有查询/保存先在内存完成再落 Flash */
static ir_learn_store_t s_store;

/*===========================================================================
 * 三、内部辅助函数
 *===========================================================================*/

/**
 * @brief  计算存储结构校验和（全字段异或）
 * @param  p 存储结构指针
 * @return 异或校验值
 */
static uint32_t ir_learn_store_checksum(const ir_learn_store_t *p)
{
    uint32_t sum = 0u;
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;

    for (i = 0u; i < (uint32_t)sizeof(ir_learn_store_t); i++) {
        sum ^= (uint32_t)b[i];
    }
    return sum;
}

/**
 * @brief  校验 Flash 数据是否有效
 * @param  p 存储结构指针
 * @return true = 有效，false = magic/version/校验不符
 */
static bool ir_learn_store_valid(const ir_learn_store_t *p)
{
    if (p->magic != IR_LEARN_STORE_MAGIC) {
        return false;
    }
    if (p->version != IR_LEARN_STORE_VER) {
        return false;
    }
    if (p->count != IR_KEY_COUNT) {
        return false;
    }
    if (p->checksum != ir_learn_store_checksum(p)) {
        return false;
    }
    return true;
}

/**
 * @brief  将 RAM 缓存重置为默认状态（全部未学习）
 */
static void ir_learn_store_set_default(ir_learn_store_t *p)
{
    uint8_t i;

    p->magic    = IR_LEARN_STORE_MAGIC;
    p->version  = IR_LEARN_STORE_VER;
    p->count    = IR_KEY_COUNT;
    for (i = 0u; i <= IR_KEY_COUNT; i++) {
        p->entries[i].user_code = 0u;
        p->entries[i].cmd_code  = 0u;
        p->entries[i].valid     = 0u;
        p->entries[i].reserved  = 0u;
    }
    p->checksum = ir_learn_store_checksum(p);
}

/*===========================================================================
 * 四、公共 API
 *===========================================================================*/

/**
 * @brief  加载学习键值到 RAM 缓存
 * @return true = 有效数据已恢复，false = 空/校验失败（回默认值）
 */
bool ir_learn_store_load(void)
{
    ir_learn_store_t tmp;

    if (Qflash_Read(IR_LEARN_STORE_ADDR, (uint8_t *)&tmp,
                    (uint32_t)sizeof(tmp)) != FlashOperationSuccess) {
        co_printf("[IR   ] learn store flash read fail\r\n");
        ir_learn_store_set_default(&s_store);
        return false;
    }
    if (!ir_learn_store_valid(&tmp)) {
        co_printf("[IR   ] learn store empty/invalid, use default\r\n");
        ir_learn_store_set_default(&s_store);
        return false;
    }

    s_store = tmp;
    co_printf("[IR   ] learn store restored\r\n");
    return true;
}

/**
 * @brief  保存某个按键的学习值
 * @param  key  物理按键号（KEYSCAN_KEY1..17）
 * @param  user 学习到的 NEC 用户码
 * @param  cmd  学习到的 NEC 数据码
 * @return true = 保存成功，false = 参数非法或 Flash 操作失败
 */
bool ir_learn_store_save(uint8_t key, uint8_t user, uint8_t cmd)
{
    ir_learn_store_t neu;

    if (key == 0u || key > IR_KEY_COUNT) {
        return false;
    }

    neu = s_store;
    neu.entries[key].user_code = user;
    neu.entries[key].cmd_code  = cmd;
    neu.entries[key].valid     = 1u;
    neu.checksum = ir_learn_store_checksum(&neu);

    if (Qflash_Erase_Sector(IR_LEARN_STORE_ADDR) != FlashOperationSuccess) {
        co_printf("[IR   ] learn store flash erase fail\r\n");
        return false;
    }
    if (Qflash_Write(IR_LEARN_STORE_ADDR, (uint8_t *)&neu,
                     (uint32_t)sizeof(neu)) != FlashOperationSuccess) {
        co_printf("[IR   ] learn store flash write fail\r\n");
        return false;
    }

    s_store = neu;
    co_printf("[IR   ] learn store saved key=%u user=0x%02X cmd=0x%02X\r\n",
              (unsigned)key, (unsigned)user, (unsigned)cmd);
    return true;
}

/**
 * @brief  查询按键的最终发送码（学习值优先，否则默认值）
 * @param  key  物理按键号（KEYSCAN_KEY1..17）
 * @param  user 输出用户码
 * @param  cmd  输出数据码
 * @return true = 有效，false = 按键号非法
 */
bool ir_learn_store_lookup(uint8_t key, uint8_t *user, uint8_t *cmd)
{
    if (key == 0u || key > IR_KEY_COUNT) {
        return false;
    }

    if (s_store.entries[key].valid == 1u) {
        *user = s_store.entries[key].user_code;
        *cmd  = s_store.entries[key].cmd_code;
    } else {
        *user = ir_default_user();
        *cmd  = ir_default_cmd(key);
    }
    return true;
}

/**
 * @brief  清除全部学习值（恢复默认键值）
 * @note   擦除整个学习扇区并重置 RAM 缓存。
 */
void ir_learn_store_clear_all(void)
{
    (void)Qflash_Erase_Sector(IR_LEARN_STORE_ADDR);
    ir_learn_store_set_default(&s_store);
    co_printf("[IR   ] learn store cleared, back to default\r\n");
}
