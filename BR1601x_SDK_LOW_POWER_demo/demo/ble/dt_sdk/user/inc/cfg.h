
#ifndef  __CFG_H__
#define  __CFG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "atprocess.h"
#include "porting.h"

#ifdef i2616e_u1
#define BR_DEFAULT_CONFIG_VERSION	"i2616e-u1_00_260305_r5800"
#define BR_PRT_VERSION_STR          "i2616e-u1_00_260305_r5800"
#elif  i2616e
#define BR_DEFAULT_CONFIG_VERSION	"i2616e_00_260511_r6103"
#define BR_PRT_VERSION_STR          "i2616e_00_260511_r6103"
#endif
#define BR_FIRMWARE_VERSION         BR_DEFAULT_CONFIG_VERSION


/**
 * @brief 配置项枚举。
 *  
 */
typedef enum{
    /*本地信息配置*/
    CFG_TYPE_CFG_VER = 0,   /*配置文件版本信息*/
    CFG_TYPE_UART_BAUD,     /*串口波特率*/
    CFG_TYPE_UART_FC,       /*串口硬件流控*/
    CFG_TYPE_MULTI_CONN,    /*多连接功能*/
    CFG_TYPE_COMMAND_MODE,  /*PDU模式*/
    CFG_TYPE_AUTOUNLOCK,    /*无感解锁功能*/
	CFG_TYPE_BTMODE,        /*BLE工作模式*/
    CFG_TYPE_TX_DELAY,      /*主机唤醒信号延时信息*/
    CFG_TYPE_WAKEACT,       /*唤醒信号定义*/

    /*BT*/
    /*GAP*/
    CFG_TYPE_NAME,          /*蓝牙设备名称*/
    CFG_TYPE_ADDR,          /*蓝牙设备地址*/
    CFG_TYPE_ADV_PARAMS,    /*广播参数*/
    CFG_TYPE_SCAN_PARAMS,   /*扫描参数*/
    CFG_TYPE_CONN_PARAMS,   /*连接参数*/
    /*SMP*/
    CFG_TYPE_PAIR_REQ_CFM,  /*配对请求确认功能*/
    CFG_TYPE_IOCAP,         /*设备输入输出能力*/
    CFG_TYPE_PSK,           /*固定本地display passkey内容*/

    /*GATT*/
    CFG_TYPE_GATT_MTU,      /*GATT 默认MTU大小*/
    /*GATT server*/
    CFG_TYPE_GATTS_SVCUUID,         /*GATT Server 注册透传服务UUID*/
    CFG_TYPE_GATTS_WRTCHAR_UUID,    /*GATT Server 透传服务接受Client发送数据的特征UUID*/
    CFG_TYPE_GATTS_NTFCHAR_UUID,    /*GATT Server 透传服务向Client发送数据的特征值UUID*/
    CFG_TYPE_GATTS_FCCHAR_UUID,    /*GATT Server 透传服务向Client发送数据的特征值UUID*/
    /*GATT client*/
    CFG_TYPE_GATTC_SVCUUID,         /*GATT Clinet 查找的目标通信服务UUID*/
    CFG_TYPE_GATTC_WRTCHAR_UUID,    /*GATT Client 发送数据给Server的特征值UUID*/
    CFG_TYPE_GATTC_NTFCHAR_UUID,    /*GATT Clinet 监听Server发送数据的特征值UUID*/
    CFG_TYPE_GATTC_FCCHAR_UUID,    /*GATT Clinet 监听Server发送数据的特征值UUID*/

    /*Format*/
    CFG_TYPE_IND_PREFIX,         /*设置指示的前缀*/
    CFG_TYPE_IND_SUFFIX,         /*设置指示的后缀*/
    CFG_TYPE_CMD_PREFIX,         /*设置命令的前缀*/
    CFG_TYPE_CMD_SUFFIX,         /*设置命令的后缀*/

    CFG_TYPE_TXPOWER,			 /*设置发射功率*/
    CFG_TYPE_RESERVE,            /*设置保留区参数,保留*/
    CFG_TYPE_MAX,
}cfg_type_t;

typedef enum {
    AUTH_LEN = sizeof(uint8_t),
    SILENT_LEN = sizeof(uint8_t),
    LESC_LEN = sizeof(uint8_t),
    PHYPREF_LEN = sizeof(cmd_phy_pref_t), ///2*sizeof(uint8_t)
    LABEL_INFO_LEN = 13,
}reserver_info_len;

typedef enum {
    AUTH_OFF = 0,
    SILENT_OFF = AUTH_OFF + AUTH_LEN,    
    LESC_OFF = SILENT_OFF + SILENT_LEN,
    PHYPREF_OFF = LESC_OFF +LESC_LEN,
    LABEL_INFO_OFF = 16,
}reserver_info_off;


/*通用的配置项结构体*/
typedef union{
    char                cfg_version[32];         /*CFG_TYPE_CFG_VER*/
    uint32_t            uart_baud;               /*CFG_TYPE_UART_BAUD*/
    uint8_t             uart_fc;                 /*CFG_TYPE_UART_FC*/
    uint8_t             multi_conn;              /*CFG_TYPE_MULTI_CONN*/
    uint8_t             command_mode;            /*CFG_TYPE_COMMAND_MODE*/
    uint8_t             auto_unlock;             /*CFG_TYPE_AUTOUNLOCK*/
    uint8_t 			btmode;					 /*CFG_TYPE_BTMODE*/
    cmd_tx_delay_t      tx_delay;                /*CFG_TYPE_TX_DELAY*/
    cmd_wake_act_t      wake_act;                /*CFG_TYPE_WAKEACT*/

    char                name[32];                /*CFG_TYPE_NAME*/
    uint8_t             addr[BD_ADDR_LEN];       /*CFG_TYPE_ADDR*/
    cmd_adv_param_t 	adv_params;	             /*CFG_TYPE_ADV_PARAMS*/
    cmd_scan_param_t 	scan_params;             /*CFG_TYPE_SCAN_PARAMS*/
    cmd_conn_param_t    conn_params;             /*CFG_TYPE_CONN_PARAMS*/
    /*SMP*/ 
    uint8_t             pair_req_cfm;            /*CFG_TYPE_PAIR_REQ_CFM*/
    uint8_t 	        iocap;                   /*CFG_TYPE_IOCAP*/
    uint32_t            passkey;                 /*CFG_TYPE_PSK*/
    /* GATT */
    uint16_t            mtu;                     /*CFG_TYPE_GATT_MTU*/
    /*GATT server */
    cmd_gatt_uuid_t     gatts_svcuuid;           /*CFG_TYPE_GATTS_SVCUUID*/
    cmd_gatt_uuid_t     gatts_wrtchar_uuid;      /*CFG_TYPE_GATTS_WRTCHAR_UUID*/
    cmd_gatt_uuid_t     gatts_ntfchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    cmd_gatt_uuid_t     gatts_fcchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    /*GATT Client*/
    cmd_gatt_uuid_t     gattc_svcuuid;           /*CFG_TYPE_GATTC_SVCUUID*/
    cmd_gatt_uuid_t     gattc_wrtchar_uuid;      /*CFG_TYPE_GATTC_WRTCHAR_UUID*/
    cmd_gatt_uuid_t     gattc_ntfchar_uuid;      /*CFG_TYPE_GATTC_NTFCHAR_UUID*/
    cmd_gatt_uuid_t     gattc_fcchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    /*Format*/
    uint8_t             ind_prefix_type;          /*CFG_TYPE_IND_PREFIX*/
    uint8_t             ind_suffix_type;          /*CFG_TYPE_IND_SUFFIX*/
    uint8_t             cmd_prefix_type;          /*CFG_TYPE_CMD_PREFIX*/
    uint8_t             cmd_suffix_type;          /*CFG_TYPE_CMD_SUFFIX*/

    uint8_t             txpower;				  /*CFG_TYPE_CMD_TXPOWER*/
}cfg_general_t;


#pragma pack(4)
typedef struct cfg_strc{
    char                cfg_version[32];         /*CFG_TYPE_CFG_VER*/
    uint32_t            uart_baud;               /*CFG_TYPE_UART_BAUD*/
    uint8_t             uart_fc;                 /*CFG_TYPE_UART_FC*/
    uint8_t             multi_conn;              /*CFG_TYPE_MULTI_CONN*/
    uint8_t             command_mode;            /*CFG_TYPE_COMMAND_MODE*/
    uint8_t             auto_unlock;             /*CFG_TYPE_AUTOUNLOCK*/
    uint8_t 			btmode;				     /*CFG_TYPE_BTMODE*/
    cmd_tx_delay_t      tx_delay;                /*CFG_TYPE_TX_DELAY*/
    cmd_wake_act_t      wake_act;                /*CFG_TYPE_WAKEACT*/

    char                name[32];                /*CFG_TYPE_NAME*/
    uint8_t             addr[BD_ADDR_LEN];       /*CFG_TYPE_ADDR*/
    cmd_adv_param_t 	adv_params;	             /*CFG_TYPE_ADV_PARAMS*/
    cmd_scan_param_t 	scan_params;             /*CFG_TYPE_SCAN_PARAMS*/
    cmd_conn_param_t    conn_params;             /*CFG_TYPE_CONN_PARAMS*/
    /*SMP*/ 
    uint8_t             pair_req_cfm;            /*CFG_TYPE_PAIR_REQ_CFM*/
    uint8_t 	        iocap;                   /*CFG_TYPE_IOCAP*/
    uint32_t            passkey;                 /*CFG_TYPE_PSK*/
    /* GATT */
    uint16_t            mtu;                     /*CFG_TYPE_GATT_MTU*/
    /*GATT server */
    cmd_gatt_uuid_t     gatts_svcuuid;           /*CFG_TYPE_GATTS_SVCUUID*/
    cmd_gatt_uuid_t     gatts_wrtchar_uuid;      /*CFG_TYPE_GATTS_WRTCHAR_UUID*/
    cmd_gatt_uuid_t     gatts_ntfchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    cmd_gatt_uuid_t     gatts_fcchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    /*GATT Client*/
    cmd_gatt_uuid_t     gattc_svcuuid;           /*CFG_TYPE_GATTC_SVCUUID*/
    cmd_gatt_uuid_t     gattc_wrtchar_uuid;      /*CFG_TYPE_GATTC_WRTCHAR_UUID*/
    cmd_gatt_uuid_t     gattc_ntfchar_uuid;      /*CFG_TYPE_GATTC_NTFCHAR_UUID*/
    cmd_gatt_uuid_t     gattc_fcchar_uuid;      /*CFG_TYPE_GATTS_NTFCHAR_UUID*/
    /*Format*/
    char                ind_prefix_type;          /*CFG_TYPE_IND_PREFIX*/
    char                ind_suffix_type;          /*CFG_TYPE_IND_SUFFIX*/
    char                cmd_prefix_type;          /*CFG_TYPE_CMD_PREFIX*/
    char                cmd_suffix_type;          /*CFG_TYPE_CMD_SUFFIX*/

    uint8_t             txpower;
	/*reserve*/
    uint8_t             reserve[256];
}app_cfg_strc_t;
#pragma pack()


/**
 * @brief 初始化配置管理模块。
 * 
 */
void cfg_init(void);

/**
 * @brief 获取指定配置项信息。
 * 
 * @param[in] cfg_type  配置项ID。
 * @param[out] buf      用于保存配置项信息的缓存区。
 * @param[in] len       配置缓存区的大小，单位为Bytes。
 * 
 * @return 实际获取到数据的长度，如果传入的缓存区为NULL，则返回值为获取配置项所有信息所需缓
 *         存区大小。
 */ 
uint16_t cfg_get(cfg_type_t cfg_type, void* buf, uint16_t len);

/**
 * @brief 设置指定配置项信息。
 * 
 * @param[in] cfg_type   配置项ID。
 * @param[in] buf        存储待修改内容的缓存区。
 * @param[in] len        待修改内容的长度，单位为字节。
 * 
 * @return 实际写入到配置信息区的长度。
 */ 
uint16_t cfg_set(cfg_type_t cfg_type,const void* buf, uint16_t len);

/**
 * @brief 恢复配置区。
 * 
 */
void cfg_factory(void);

/**
 * @brief 获取指定位置的保留信息区信息。
 * 
 * @param[in] off       保留信息区的偏移量。
 * @param[out] data     用于保存保留信息区信息的缓存区
 * @param[in] len       保留信息区信息的缓存区大小
 * 
 * @return 实际获取到数据的长度。
 */ 
uint8_t cfg_reserver_get(uint8_t off, void* data, uint8_t len);

/**
 * @brief 修改保留区指定位置的信息。
 * 
 * @param[in] off       保留信息区的偏移量。
 * @param[in] data      待修改信息的缓存区。
 * @param[in] len       待修改信息的长度，单位为字节。
 * 
 * @return 实际写入到配置信息区的长度。
 */ 
uint8_t cfg_reserver_set(uint8_t off, const void* data, uint8_t len);

/**
 * @brief 立即保存配置信息。
 * 
 */ 
void cfg_store(void);


#ifdef __cplusplus
}
#endif

#endif
