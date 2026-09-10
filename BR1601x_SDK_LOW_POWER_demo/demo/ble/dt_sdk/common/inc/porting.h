/**
 * @file porting.h
 * @author Barrot Firmware Team
 * @brief 
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */
#ifndef __PORTING__H__
#define __PORTING__H__

/**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include "br1601x.h"
#include "datatype.h"
#include "gap.h"
#include "gatt.h"

/*
 * DEFINE
 ****************************************************************************************
 */
/// Critical action
#define _ENTER_CRITICAL()               do{uint32_t __irq_save = __get_PRIMASK(); __set_PRIMASK(1);
#define _LEAVE_CRITICAL()               __set_PRIMASK(__irq_save);} while(0)
#define ASSERT_ERR(cond)
#define FUNC_RAM                        __attribute__((section("IRAM1")))
/// Maximum transmitting power class 10(8dbm)
#define TX_POWER_LEVEL_MAX  			10
/// att head size
#define ATT_HEAD_SIZE   				3

/// br malloc and br free
extern void *brt_malloc(uint32_t size);
extern void brt_free(void *ptr);

#ifndef NEW
#define NEW(_s)                         brt_malloc(_s)
#endif
#ifndef FREE
#define FREE(_p)                        brt_free(_p)
#endif

#define DECODE2BYTE_LITTLE(ptr) 	(UINT16)(*(UINT8 *)(ptr) | (*(UINT8 *)((ptr) + 1) << 8))
#define DECODE4BYTE_LITTLE(ptr)\
	(UINT32)(*(UINT8 *)(ptr) | (*(UINT8 *)((ptr) + 1) << 8) \
	| (*(UINT8 *)((ptr) + 2) << 16) | (*(UINT8 *)((ptr) + 3) << 24))
#define ENCODE2BYTE_LITTLE(ptr, data)\
	*(UINT8 *)((ptr) + 1) = (UINT8)((data) >> 8);\
	*(UINT8 *)(ptr) = (UINT8)(data);
#define ENCODE4BYTE_LITTLE(ptr, data)\
	*(UINT8 *)((ptr) + 3) = (UINT8)((data) >> 24);\
	*(UINT8 *)((ptr) + 2) = (UINT8)((data) >> 16);\
	*(UINT8 *)((ptr) + 1) = (UINT8)((data) >> 8);\
	*(UINT8 *)(ptr) = (UINT8)(data);

#define DECODE2BYTE_BIG(ptr) 	(UINT16)(*((UINT8 *)(ptr) + 1) | (*(UINT8 *)(ptr) << 8))
#define DECODE4BYTE_BIG(ptr)\
	(UINT32)(*(UINT8 *)((ptr) + 3) | (*(UINT8 *)((ptr) + 2) << 8) \
	| (*(UINT8 *)((ptr) + 1) << 16) | (*(UINT8 *)(ptr) << 24))
#define ENCODE2BYTE_BIG(ptr, data)\
	*(UINT8 *)(ptr) = (UINT8)((data) >> 8);\
	*(UINT8 *)((ptr) + 1) = (UINT8)(data);
#define ENCODE4BYTE_BIG(ptr, data)\
	*(UINT8 *)(ptr) = (UINT8)((data) >> 24);\
	*(UINT8 *)((ptr) + 1) = (UINT8)((data) >> 16);\
	*(UINT8 *)((ptr) + 2) = (UINT8)((data) >> 8);\
	*(UINT8 *)((ptr) + 3) = (UINT8)(data);

/*
 * ENUM
 ****************************************************************************************
 */
enum
{
    LSCI_32000HZ = 0,
    LSCI_32768HZ,
    LSCI_28800HZ,
    LSCE_32768HZ
};

enum {
    PHY_1MBPS = 1,
    PHY_2MBPS = 2,
    PHY_CODED = 3,
    PHY_CODED_128KBPS = 3,
    PHY_CODED_500KBPS = 4,
};

/*
 * TYPEDEFS
 ****************************************************************************************
 */
typedef struct {
#define GATT_UUID_LEN_16 2
#define GATT_UUID_LEN_32 4
#define GATT_UUID_LEN_128 16

#ifdef CONFIG_LITTLE_ENDIAN
    #define GATT_UUID16_INITVAL(u) {(uint8_t)(u), (uint8_t)((u) >> 8)}
    #define GATT_UUID32_INITVAL(u) {(uint8_t)(u), (uint8_t)((u) >> 8), \
                                        (uint8_t)((u) >> 16), (uint8_t)((u) >> 24)}
#else
    #define GATT_UUID16_INITVAL(u) {(uint8_t)((u) >> 8), (uint8_t)(u)}
    #define GATT_UUID32_INITVAL(u) {(uint8_t)((u) >> 24), (uint8_t)((u) >> 16), \
                                        (uint8_t)((u) >> 8), (uint8_t)(u)}
#endif

    uint32_t len;

    union {
        uint8_t uuid128[GATT_UUID_LEN_128];
        uint32_t uuid32;
        uint16_t uuid16;
    } u;
} gatt_uuid_t;

struct HCI_AddressStru {
    UINT8 bd[BD_ADDR_LEN];						/* 6octets, 48bits */
	UINT8 atype; 								/* HCI_OWN_ADDRESS_TYPE_PUBLIC_DEVICE_ADDRESS */
};

struct SMP_KeysStru{

	/* Static, remote identity address */
	struct HCI_AddressStru Identity_Address;

	uint32_t owner_p2hdl;/* Obj.PairProc or Obj.PairSync */

	/* DHK = d1(IR, 3, 0); Y = dm(DHK, Rand); EDIV = Y xor DIV; DIV = Y xor EDIV */

	/* Keys - Local distributed to remote */
	uint16_t DIV;/* Legacy only, exist at SMP54_KEYFEA_LTK_SEND or SMP54_KEYFEA_CSRK_ENCODE */

	/* Keys - Remote distributed to local */
	uint16_t EDIV;/* Legacy only */
	uint8_t Rand[8];/* Legacy only */
	uint8_t LTK[16];/* Legacy only, LTK = d1(ER, DIV, 0) */
	uint8_t IRK[16];/* LESC + Legacy, IRK = d1(IR, 1, 0) */
	uint8_t CSRK[16];/* LESC + Legacy, CSRK = d1(ER, DIV, 1) */

	/* Security Properties */
	uint8_t Distributed_Key_Sizes;
	uint8_t kmask;/* etc, SMP54_KEYFEA_LTK_SEND */
};


struct KeyStoreStru {			/* Keys that indexes by addr */
	struct HCI_AddressStru addr;	/* Peer link address, FIXME, is dule-mode? */
	struct SMP_KeysStru k;		/* Keys */
};

typedef struct user_cfg_stru {
    uint8_t scan_en;
    uint8_t lsc_type;
} user_cfg;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 * @brief 获取空闲时间
 * 
 * @return int32_t 单位ms
 */
int32_t br_idle_time_get(void);

/**
 * @brief 获取当前时间
 * 
 * @return uint32_t 当前时间，单位ms
 */
static inline uint32_t br_get_curr_time(void)
{
    extern unsigned long GetCurrTime(void);
    return GetCurrTime();
}

/**
 * @brief crc计算
 * 
 * @param pack 需要校验的数据
 * @param len 数据的长度
 * @return uint16_t 校验后的结果
 */
uint16_t br_calculate_crc(uint8_t *pack, uint32_t len);

/**
 * @brief 获取芯片唯一的蓝牙地址
 * 
 * @param bd 地址缓存区指针
 * @return true 获取成功
 * @return false 获取异常
 */
bool br_addr_get(uint8_t *bd);

/**
 * @brief 设置蓝牙频偏，立即生效
 * 
 * @param trim 频偏值
 * @return true 保存成功
 * @return false 保存失败
 */
bool br_radio_freq_offset_set(uint32_t trim);

/**
 * @brief 获取蓝牙频偏值
 * 
 * @return uint32_t 频偏值
 */
uint32_t br_radio_freq_offset_get(void);

/**
 * @brief 功率设定
 * 
 */
bool br_tx_power_set(uint8_t level);

/**
 * @brief 大小端转换
 * @param src 需要转换的缓冲区指针
 * @param len 需要转化的长度
 * 
 */
void hlp_invert_arr(uint8_t *src, uint32_t len);

/**
 * @brief 将hex数组转换成hex字符串
 * @param dst 存储字符串的缓冲区指针
 * @param arr 需要转换的数组指针
 * @param len 数组的长度
 * 
 */
uint8_t *hlp_numayyr_to_hexstr(uint8_t *dst, uint8_t *arr, uint16_t len);

/**
 * @brief 启动定频
 * @param ch 定频频率,范围[2402-2480]
 * 
 */
void br_freq_fix_start(uint32_t freq);

/**
 * @brief 结束定频
 * 
 */
void br_freq_fix_end(void);


bool hlp_uuid_comp(uuid_t *uuid_a, uuid_t *uuid_b);

/**
 ****************************************************************************************
 * @brief Function to initialize the random seed.
 * @param[in] seed The seed number to use to generate the random sequence.
 ****************************************************************************************
 */
__INLINE void co_random_init(uint32_t seed)
{
    srand(seed);
}

/**
 ****************************************************************************************
 * @brief Function to get an 8 bit random number.
 * @return Random byte value.
 ****************************************************************************************
 */
__INLINE uint8_t co_rand_byte(void)
{
    return (uint8_t)(rand() & 0xFF);
}

/**
 ****************************************************************************************
 * @brief Function to get an 16 bit random number.
 * @return Random half word value.
 ****************************************************************************************
 */
__INLINE uint16_t co_rand_hword(void)
{
    return (uint16_t)(rand() & 0xFFFF);
}

/**
 ****************************************************************************************
 * @brief Function to get an 32 bit random number.
 * @return Random word value.
 ****************************************************************************************
 */
__INLINE uint32_t co_rand_word(void)
{
    return (uint32_t)(rand() & 0xFFFFFFFF);
}

/**
 ****************************************************************************************
 * @brief Function to return the smallest of 2 unsigned 32 bits words.
 * @return The smallest value.
 ****************************************************************************************
 */
__forceinline uint32_t co_min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

/**
 ****************************************************************************************
 * @brief Function to return the smallest of 2 signed 32 bits words.
 * @return The smallest value.
 ****************************************************************************************
 */
static __INLINE int32_t co_min_s(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

/**
 ****************************************************************************************
 * @brief Function to return the greatest of 2 unsigned 32 bits words.
 * @return The greatest value.
 ****************************************************************************************
 */
__INLINE uint32_t co_max(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

/**
 ****************************************************************************************
 * @brief Function to return the absolute value of a signed integer.
 * @return The absolute value.
 ****************************************************************************************
 */
__INLINE int co_abs(int val)
{
    return val < 0 ? val*(-1) : val;
}

#if defined(BR1601A01) || defined(BR1601A02)
void br_modem_set(uint8_t phy);
void br_scan_en_set(bool scan_en);
void radio_freq_cal_set(uint32_t f_cal);
void br_scan_link_clear(void);
#endif

#endif
