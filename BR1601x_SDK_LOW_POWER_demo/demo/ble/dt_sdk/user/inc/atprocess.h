#ifndef  __ATPROCESS_H__
#define  __ATPROCESS_H__

#include "porting.h"
#include "app_ble_cfg.h"
#include "hogp.h"

#ifndef ADV_DATA_MAX_SIZE
#define ADV_DATA_MAX_SIZE   (31)
#endif
  
enum WAKE_SIGN_TYPE{
    WAKE_TYPE_IMPULSE = 0u, /*脉冲信号*/
    WAKE_TYPE_LEVEL,        /*电平信号*/
};

enum WHITE_LIST_INDEX_TYPE{
    INDEX_TYPE_IDX,
    INDEX_TYPE_ADDR,
};

enum CMD_ERRCODE{
    CMD_NO_ERR = 0,
    CMD_INVALID_FORMAT = 1,
    CMD_INVALID_PARAM,
    CMD_RUNING_FAILED,
    CMD_STATUS_ERR,
    CMD_UNMATCH,
};

/*=============本地信息修改事件参数对应结构体==============*/

/**
 * @brief 
 */
typedef struct{
    uint32_t baud;          /*波特率*/
}cmd_uart_baud_t;

/**
 * @brief 
 */
typedef struct{
    bool flowctrl;          /*流控状态*/
}cmd_uart_flowctrl_t;

/**
 * @brief 
 */
typedef struct{
    bool multi_conn_sts;    /*多连接设置状态*/
}cmd_multi_conn_t;

/**
 * @brief 
 */
typedef struct{
    bool command_mode;       /*命令模式设置信息*/
}cmd_command_mode_t;

/**
 * @brief 
 */
typedef struct{
    bool autounlock;        /*无感解锁功能开关*/
}cmd_autounlock_t;

/**
 * @brief 
 */
typedef struct{
    uint16_t wake_delay;    /*唤醒信号到串口发送数据的时间间隔，默认为0，单位为ms*/    
    uint16_t sleep_delay;   /*主机MCU在串口无交换进入休眠的时间，单位为ms*/
}cmd_tx_delay_t;

/**
 * @brief 
 */
typedef struct{
    uint8_t type;           /*信号类型，@see WAKE_SIGN_TYPE*/
    uint8_t level;          /*唤醒信号极性*/
    uint16_t keep_time;      /*脉冲时，脉冲保持时间*/
}cmd_wake_act_t;

typedef struct{
    uint8_t type;
    union{
        uint16_t conhdl;
        uint8_t addr[BD_ADDR_LEN];  /*添加到白名单的设备地址*/
    }info;
}cmd_white_list_add_t;

typedef struct{
    uint8_t type;                   /*删除信息的类型*/
    union{
        uint8_t index;              /*需要删除设备在列表中的偏移*/
        uint8_t addr[BD_ADDR_LEN];  /*从白名单删除设备的地址*/
    }info;
}cmd_white_list_del_t;

/*=======================蓝牙信息对应结构体=================*/
/*-----------------------------GAP-----------------------*/
/**
 * @brief 
 */
typedef struct{
    const char* name;       /*蓝牙设备名称*/
}cmd_bt_name_t;


/**
 * @brief 
 */
typedef struct {
    bool adv_enable;        /*开关广播*/
}cmd_adv_enable_t;

/**
 * @brief 
 */
typedef struct {
    uint8_t enable;
    uint8_t num;
    struct {
        uint8_t data[ADV_DATA_MAX_SIZE];
        uint8_t len;
    }adv_data[5];
}cmd_scankw_t;

/**
 * @brief 
 */
typedef struct {
    uint16_t adv_intreval_min;  /*广播间隔最小值*/
    uint16_t adv_intreval_max;  /*广播间隔最大值*/
    uint8_t adv_type;           /*广播类型*/
    uint8_t own_address_type;   /*自身地址类型*/
    uint8_t peer_address_type;  /*定向广播对端地址类型*/
    uint8_t peer_address[BD_ADDR_LEN];  /*定向广播对端地址*/
    uint8_t adv_channel_map;    /*广播通道MAP*/
    uint8_t adv_filter_policy;  /*广播过滤策略设置*/
}cmd_adv_param_t;

/**
 * @brief 广播数据事件对应参数结构。 @see CMD_BT_ADV_DATA
 */
typedef struct{
    uint8_t adv_data_len;       /*广播数据长度*/
    uint8_t adv_data[ADV_DATA_MAX_SIZE];    /*广播数据payload*/
}cmd_adv_data_t;

/**
 * @brief 扫描响应数据事件对应参数结构 @see CMD_BT_ADV_SRDATA
 */
typedef struct{
    uint8_t sr_data_len;        /*扫描响应数据长度*/
    uint8_t sr_data[ADV_DATA_MAX_SIZE];     /*扫描响应数据payload*/
}cmd_sr_data_t;

/**
 * @brief 扫描参数事件对应参数结构。 @see CMD_BT_SCAN_PARAMS
 */
typedef struct{
    uint16_t scan_interval;     /*扫描间隔设置*/
    uint16_t scan_window;       /*扫描窗口设置*/
    uint8_t scan_type;          /*扫描类型*/
}cmd_scan_param_t;

/**
 * @brief 发起连接事件对应参数结构。 @see CMD_BT_CONNECT_DEV
 */
typedef struct{
    uint8_t addr_type;          /*对端设备地址类型*/
    uint8_t addr[BD_ADDR_LEN];  /*对端设备地址*/
}cmd_conn_req_t;

/**
 * @brief 连接参数事件对应参数结构。 @see CMD_BT_CONN_PARAMS
 */
typedef struct{
    uint16_t conn_interval_min; /*连接参数最小值*/
    uint16_t conn_interval_max; /*连接参数最大值*/
    uint16_t conn_latency;      /*连接允许从机最多忽略连接事件数*/
    uint16_t sup_timemout;      /*连接丢失超时时间*/
}cmd_conn_param_t;

/**
 * @brief 断开连接指令事件参数。 @see CMD_BT_DISCONN
 */
typedef struct{
    uint16_t conn_id;            /*断连连接通道ID*/
}cmd_disconn_t;

/*--------------------------------SMP--------------------------*/
typedef struct{
    bool pair_req_confirm;      /*开关paring requset 确认功能*/
}cmd_pair_req_confirm_t;

/**
 * @brief 设置IOCAP命令事件对应参数， @see CMD_PAIR_IOCAP
 */
typedef struct{
    uint8_t io_cap;             /*设置设备的IOCAP*/
}cmd_pair_iocap_t;

/**
 * @brief 设置固定passkey命令事件对应参数 , @see CMD_PAIR_PSK_SET
 */
typedef struct{
    uint32_t passkey;           /*设置固定的PASSKEY*/
}cmd_passkey_t;

/**
 * @brief 响应配对请求事件对应参数 , @see CMD_PAIR_REQ_REPLY
 */
typedef struct{
    uint16_t conhdl;
    bool accept;                /*是否同意*/
}cmd_pair_req_reply_t;

/**
 * @brief 响应passkey entry事件对应参数 , @see CMD_PAIR_PSK_ENTRY
 */
 typedef struct{
    uint16_t conhdl;
    uint32_t passkey;           /*用户输入的passkey*/
 }cmd_pair_passkey_entry_t;

/**
 * @brief 响应数字比对结果事件对应参数 , @see CMD_PAIR_NUMCF_RPL
 */
typedef struct{
    uint16_t conhdl;
    bool result;                /*比对结果是否通过*/
}cmd_pair_numcf_reply_t;

/**
 * @brief 设置GATT 默认MTU参数, @see CMD_GATT_MTU
 */
typedef struct{
    uint16_t mtu;               /*MTU值*/
}cmd_gatt_mtu_t;

typedef struct{
    uint8_t tx_phys;
    uint8_t rx_phys;
}cmd_phy_pref_t;

/**
 * @brief UUID相关事件的参数结构,对应所有UUID设置
 */
typedef gatt_uuid_t cmd_gatt_uuid_t;

/**
 * @brief 需要蓝牙发送数据到对端设备结构。 @see CMD_DATA_SEND
 */
typedef struct{
    uint16_t cid;               /*连接通道*/
    uint16_t length;            /*待发送数据长度*/
    const uint8_t* data;        /*指向待发送数据的缓存区*/
}cmd_data_send_t;

typedef struct{
    uint16_t ch;
    uint16_t time;
}cmd_test_freq_t;

typedef struct{
    struct {
        uint16_t mtu;
        uint16_t conhdl;
    }mtu[BLE_MAX_CONN];
    uint8_t conn_n;
}cmd_conn_mtu_t;

typedef struct{
    struct {
        uint8_t addr[BD_ADDR_LEN];
        uint16_t conhdl;
    }dev[BLE_MAX_CONN];
    uint8_t conn_n;
}cmd_conn_dev_t;

typedef struct{
    uint8_t dev_n;
    struct {
        uint8_t addr[BD_ADDR_LEN];
    }dev[MAX_WHTIELIST_NUM];
}cmd_whitelist_t;

typedef struct{
    uint16_t conhdl;
    hid_rpt_t in_rpt;
}cmd_hogp_rpt_t;

typedef struct {
    bool enable;
    uint16_t period;
}advled_t;

/**
 * @brief 通知业务逻辑模块的指令修改信息的类型枚举。 
 */
typedef enum{
    /*本地设备信息修改*/
    CMD_UART_BAUD = 0,      /*修改波特率,参数 @see cmd_uart_baud_t*/
    CMD_UART_FLOWCTRL,      /*流控状态, 参数 @see cmd_uart_flowctrl_t*/
    CMD_SILENT,             /*静默模式,*/
    CMD_MULTI_CONN,         /*多连接设置，参数 @see cmd_multi_conn_t*/
    CMD_COMMAND_MODE,       /*PDU模式*/
    CMD_AUTOUNLOCK,         /*无感解锁功能开关*/
    CMD_BT_MODE,            /*蓝牙工作模式*/
    CMD_TX_DELAY,           /*唤醒时间设置*/
    CMD_WAKEACT,            /*唤醒方式*/  
    CMD_WHITE_LIST_GET,     /*查询白名单*/
    CMD_WHITE_LIST_ADD,     /*添加设备到白名单*/
    CMD_WHITE_LIST_DEL,     /*将设备从白名单删除*/
    CMD_RESET,              /*复位模块*/
    CMD_FACTORYRESET,
    CMD_CLEARLINKKEY,       /*删除配对记录*/
    CMD_ADVLED_GET,       
    CMD_ADVLED_SET,
    CMD_LABEL_INFO,
    /*-------------*/
    /*蓝牙信息修改*/
    /*----GAP----*/
    CMD_BT_NAME = 0x100,    /*蓝牙名称*/
    CMD_BT_ADDR,            /*蓝牙设备地址*/
    CMD_BT_ADV_ENABLE,      /*开关广播*/
    CMD_BT_ADV_PARAMS,      /*广播参数*/
    CMD_BT_ADV_DATA,        /*广播数据，参数 @see cmd_adv_data_t*/
    CMD_BT_ADV_SRDATA,      /*扫描响应数据，参数 @see cmd_sr_data_t*/
    CMD_BT_SCAN_KW,         /*扫描过滤关键字,参数 @see cmd_scankw_t*/
    CMD_BT_SCAN_ENABLE,     /*开关扫描功能，参数 @see cmd_adv_enable_t*/
    CMD_BT_SCAN_PARAMS,     /*扫描参数参数， @see cmd_scan_param_t*/
    CMD_BT_CONNECT_DEV,      /*发起连接参数， @see cmd_conn_req_t*/
    CMD_BT_CONN_PARAMS,     /*设置连接参数参数， @see cmd_conn_param_t*/
    CMD_BT_DISCONN,         /*断开指定的设备连接参数， @see cmd_disconn_t*/
    CMD_BT_CONNMTU,			/*获取连接的mtu*/
    CMD_BT_CONNDEV,			/*获取连接设备的地址和句柄*/
    CMD_BT_PHYPREF,

    CMD_BT_ADV_GET,         /*获取广播状态*/
    CMD_BT_ADVDATA_GET,     /*获取广播数据**/
    CMD_BT_SRDATA_GET,      /*获取广播响应数据*/
    CMD_BT_SCAN_GET,        /*获取扫描状态*/
    CMD_BT_RSSI_GET,        /*获取对端设备rssi*/
    /*----SMP----*/
    /*settings*/
    CMD_PAIR_REQ_CFM_FEA,   /*配对请求确认功能设置*/
    CMD_PAIR_IOCAP,         /*设备的IOCAP设置*/
    CMD_PAIR_LESC,			/*配对feature_le_sec*/
    CMD_PARI_PSK_SET,       /*设置固定的PASSKEY*/
    /*reply*/
    CMD_PAIR_REQ_REPLY,     /*响应配对确认请求*/
    CMD_PAIR_PSK_ENTRY,     /*输入PASSKEY响应*/  
    CMD_PAIR_NUMCF_RPL,     /*数字比较结果*/

    /*----GATT----*/
    CMD_GATT_MTU,            /*设置GATT通信默认MTU*/

    /*----HOGP----*/
    CMD_HOGP_KB_RPT,
    CMD_HOGP_FUNC_RPT,
    /*====Server====*/
    CMD_GATTS_SVC,          /*GATT Server注册服务UUID*/
    CMD_GATTS_WRTCHAT,       /*用于接受Clinet端数据的特征值UUID*/
    CMD_GATTS_NTFCHAT,       /*用于发送数据给Client端特征值UUID*/
    CMD_GATTS_FCCHAT,
    
    /*====Client====*/
    CMD_GATTC_SVC,          /*GATT Client通信目标服务*/
    CMD_GATTC_WRTCHAT,      /*发送数据给Server端使用的特征值UUID*/
    CMD_GATTC_NTFCHAT,      /*接受Server端数据特征UUID*/
    CMD_GATTC_FCCHAT,

    /*Format*/
    CMD_IND_PREFIX,         /*指示前缀*/
    CMD_IND_SUFFIX,         /*指示后缀*/
    CMD_CMD_PREFIX,         /*指令前缀*/
    CMD_CMD_SUFFIX,         /*指令后缀*/

    /*数据交互*/
    CMD_DATA_SEND = 0x200,   /*数据发送*/

    CMD_TEST_TXPOWER = 0x300,
    CMD_TEST_FREQ,
}cmd_change_evt_t;

/**
 * @brief 指令执行接口定义。
 */
typedef int (*cmd_processor_t)(const char* match_string, const char* cmd_line);

/**
 * @brief 指令结构体。 
 */
typedef struct {
    const char* cmd_string;
    cmd_processor_t cmd_proccesor;
}cmd_info_t;

/**
 * @brief 立即生效命令执行回调函数接口定义。
 */
typedef int (*cmd_evt_callback_t)(cmd_change_evt_t, void*);

/**
 * @brief 命令响应输出接口定义。
 */
typedef int (*cmd_rsp_fmt_out_t)(const char* fmt, ...);

/**
 * @brief 数据包处理完成接口定义。
 */
typedef int (*cmd_pkt_proc_done_t)(void);

/**
 * @brief 设置立即生效命令执行回调函数。
 *
 * @param[in] fn    设置函数指针。
 */
void cmd_callback_set(cmd_evt_callback_t fn);

/**
 * @brief 设置命令响应输出接口。
 *
 * @param[in] fn    设置函数指针。
 */
void cmd_rsp_out_set(cmd_rsp_fmt_out_t fn);


/**
 * @brief 设置数据包处理处理完成回调。
 *
 * @param[in] fn    设置函数指针。
 */
void cmd_pkt_done_set(cmd_pkt_proc_done_t fn);


/**
 * @brief 解析命令数据并执行命令，并响应命令。
 *
 * @param[in] buf   输入数据缓存区。
 * @param[in] len   输入数据的长度，单位为字节。
 * @param[out] 接受的字节数。
 */
uint32_t cmd_parse(const uint8_t* buf, uint32_t len);
uint32_t cmd_parse_ble(const uint8_t *buf, uint32_t len);

#endif
