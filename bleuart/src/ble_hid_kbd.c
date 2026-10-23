#include "app_gatt_safe.h"
#include <string.h>
#include "gap.h"
#include "gatt.h"
#include "ble_gap.h"
#include "ble_hid_kbd.h"
#include "co_printf.h"
#include "pm_state.h"       /* 电源管理 — 对端数据活动通知 */
#include "br_timer.h"    /* sdk_timer_get_current_ms */


// HID Report Map（从参考工程原封不动复制，当前阶段不要改动）。
// 该描述符包含：
// 1) Report ID=1：标准键盘输入（8 字节：修饰键+保留+6个按键）
// 2) Report ID=2：Consumer 控制输入（16-bit Usage）
// 3) Report ID=3：厂商自定义输入（长度由 Report Count 定义）
static const uint8_t hid_report_map[] =
{
    // ===== Report ID 1: Keyboard =====
    0x05, 0x01, /* Usage Page (Generic Desktop Controls 0x01) - 声明设备属于通用桌面类（用于后续声明 Keyboard） */
    0x09, 0x06, /* Usage (Keyboard) - 指定这是一个键盘 */

    0xA1, 0x01, /* Collection (Application) - 应用集合，HID 设备的顶层集合 */
    0x85, 0x01, /* Report ID (1) - 报告 ID = 1，便于区分多个报告 */

    0x05, 0x07, /* Usage Page (Keyboard/Keypad 0x07) - 键盘/键盘页面，后续使用该页的 Usage 定义键值 */
    0x19, 0xE0, /* Usage Minimum (0xE0) - 修饰键范围起始 (Left Control) */
    0x29, 0xE7, /* Usage Maximum (0xE7) - 修饰键范围结束 (Right GUI) */
    0x15, 0x00, /* Logical Minimum (0) - 逻辑最小值 */
    0x25, 0x01, /* Logical Maximum (1) - 逻辑最大值（二值：按下/未按下） */
    0x75, 0x01, /* Report Size (1) - 每个修饰键占 1 bit */
    0x95, 0x08, /* Report Count (8) - 共 8 个修饰键位，构成第 0 字节（modifier byte） */

    0x81, 0x02, /* Input (Data,Var,Abs) - 输入字段：8 位修饰键（Data, Variable, Absolute） */

    0x95, 0x01, /* Report Count (1) - 下一个字段为 1 个 */
    0x75, 0x08, /* Report Size (8) - 大小 8 位（保留字节）*/
    0x81, 0x01, /* Input (Constant) - 保留字节（主机忽略）；类型 Constant 表示固定占位 */
    0x95, 0x05, /* Report Count (5) - LEDs 字段只用 5 位定义（NumLock..Kana 等） */
    0x75, 0x01, /* Report Size (1) - 每个 LED 1 bit */

    0x05, 0x08, /* Usage Page (LEDs 0x08) - 指示灯页面（NumLock, CapsLock, ScrollLock...） */
    0x19, 0x01, /* Usage Minimum (1) - Num Lock */
    0x29, 0x05, /* Usage Maximum (5) - Kana (视实现而定) */
    0x91, 0x02, /* Output (Data,Var,Abs) - 输出字段（主机 -> 设备），用于控制 LED */
    0x95, 0x01, /* Report Count (1) - 填充位数（以下为填充） */
    0x75, 0x03, /* Report Size (3) - 填充 3 bit 以对齐到 1 字节 */
    0x91, 0x01, /* Output (Constant) - 填充位为 Constant */

    0x95, 0x06, /* Report Count (6) - 普通键按键数组包含 6 个按键位置 */
    0x75, 0x08, /* Report Size (8) - 每个键码占 8 bit（一个字节） */
    0x15, 0x00, /* Logical Minimum (0) - 键码逻辑最小值 0 */
    0x25, 0xFF, /* Logical Maximum (255) - 键码逻辑最大值 255 */
    0x05, 0x07, /* Usage Page (Keyboard/Keypad) - 回到键盘页面，键码来自该页面 */
    0x19, 0x00, /* Usage Minimum (0) - Usage 数组最小值（0 表示无按键） */
    0x29, 0xFF, /* Usage Maximum (255) - Usage 最大（覆盖 0-255 范围） */
    0x81, 0x00, /* Input (Data,Array,Abs) - 输入：6 字节的键码数组，Array 表示索引型（按键使用 Usage 值） */

    0xC0,       /* End Collection - 结束 Keyboard 的 Application 集合 */

    0x05, 0x0C, /* Usage Page (Consumer) */
    0x09, 0x01, /* Usage (Consumer Control) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, 0x02, /* Report ID (2) */

    // 3-bit Variable: Mute / Volume+ / Volume-
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x01, /* Logical Maximum (1) */
    0x75, 0x01, /* Report Size (1) */
    0x95, 0x03, /* Report Count (3) */
    0x09, 0xE2, /* Usage (Mute) */
    0x09, 0xE9, /* Usage (Volume Increment) */
    0x09, 0xEA, /* Usage (Volume Decrement) */
    0x81, 0x02, /* Input (Data,Var,Abs) */

    // Padding to next byte
    0x75, 0x05, /* Report Size (5) */
    0x95, 0x01, /* Report Count (1) */
    0x81, 0x01, /* Input (Constant) */

    0xC0,       /* End Collection */

0x06,0x00,0xFF,     		//   	GLOBAL_USAGE_PAGE(Undefined)	
0x09,0x00,          		//   	LOCAL_USAGE()	
0xA1,0x01,          		//   	MAIN_COLLECTION(Applicatior)	
0xA1,0x02,          		//  	MAIN_COLLECTION(Logical)	
0x85,0x03,          		//  	GLOBAL_REPORT_ID(3)	
0x09,0x00,          		//  	LOCAL_USAGE()	
0x15,0x00,          		//  	GLOBAL_LOGICAL_MINIMUM(-128)	
0x25,0xFF,          		//  	GLOBAL_LOCAL_MAXIMUM(127)	
0x75,0x08,          		//  	GLOBAL_REPORT_SIZE(8)	
0x95,0xE8,          		//  	GLOBAL_REPORT_COUNT(20)	
0x81,0x00,          		//  	MAIN_INPUT(data var absolute NoWrap linear NoPreferred NoNullPosition NonVolatile )	Input 80.0
0xC0,               		//  	MAIN_COLLECTION_END	
0xC0,               		//  	MAIN_COLLECTION_END
};

// HID 服务（0x1812）运行时句柄与状态。
static gatt_svc_t g_hid_boot_svc = NULL;
static gatt_chr_t g_hid_boot_in_chr = NULL;
static gatt_chr_t g_hid_report_in_chr = NULL;   // Report ID=1: Keyboard Input
static gatt_chr_t g_hid_report_in_consumer_chr = NULL;
static peer_t g_hid_boot_peer = NULL;
static peer_t g_hid_report_peer = NULL;
static peer_t g_hid_report_consumer_peer = NULL;
static volatile bool g_kbd_ntf_enabled_boot = false;
static volatile bool g_kbd_ntf_enabled_report = false;
static volatile bool g_kbd_ntf_enabled_report_consumer = false;

// Boot 键盘输入/输出缓存。
static uint8_t g_hid_boot_in_report[HID_KBD_REPORT_LEN] = {0};
static uint8_t g_hid_boot_out_report[1] = {0};
static uint8_t g_hid_report_in_buf[HID_KBD_REPORT_LEN] = {0};
static uint8_t g_hid_report_in_consumer_buf[HID_CONSUMER_REPORT_LEN] = {0};
static const uint8_t g_hid_report_in_ref[] = {
    HID_KBD_RPT_ID_KEYBOARD,
    HID_RPT_TYPE_INPUT
};
static const uint8_t g_hid_report_in_consumer_ref[] = {
    HID_KBD_RPT_ID_CONSUMER,
    HID_RPT_TYPE_INPUT
};
static const uint8_t g_hid_boot_in_ref[] = {
    HID_KBD_RPT_ID_KEYBOARD,
    HID_RPT_TYPE_BOOT_KB_IN
};

// HID Information: bcdHID=0x0100, bCountryCode=0x00, Flags=0x00
static const uint8_t g_hid_info_value[] = {0x00, 0x01, 0x00, 0x00};
static uint8_t g_hid_protocol_mode = 0x01; // 0=Boot, 1=Report

static bool hid_kbd_ntf_is_enabled(uint8_t rpt_id, uint8_t rpt_type)
{
    if (rpt_type == HID_RPT_TYPE_BOOT_KB_IN) {
        return g_kbd_ntf_enabled_boot;
    }
    if (rpt_type == HID_RPT_TYPE_INPUT) {
        if (rpt_id == HID_KBD_RPT_ID_CONSUMER) {
            return g_kbd_ntf_enabled_report_consumer;
        }
        return g_kbd_ntf_enabled_report;
    }
    return false;
}

// HID 服务事件回调：根据事件类型分发到不同的处理函数。
static void hid_boot_svc_cbk(gatt_event_t event, void *context, void *param)
{
    (void)context;
    switch (event) {
        // 服务启动完成事件：打印结果，后续可根据需要添加错误处理或状态更新逻辑。
        case GATTS_EV_SERVICE_START: // 服务启动完成事件
        { 
            gatt_svc_start_t *in = (gatt_svc_start_t *)param; // 事件参数包含服务启动结果
            co_printf("[HID  ] svc start = %d\r\n", in ? in->result : -1); // 打印服务启动结果，成功为 0，失败为其他值
            break;
        }
        default:
            break;
    }
}

static void hid_boot_in_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[HID  ] hid_boot_in_cbk event=%d\n", event);
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   g_hid_boot_in_report, sizeof(g_hid_boot_in_report));
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            // Boot Keyboard Input 不允许写，返回写不允许。
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_RES_WRITE_NOT_PERMITTED);
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START: {
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            g_kbd_ntf_enabled_boot = (in->type == GATT_SEND_VAL_NOTIFICATION);
            g_hid_boot_peer = g_kbd_ntf_enabled_boot ? in->peer : NULL;
            co_printf("[HID  ] HID Boot IN CCCD=%s\n", g_kbd_ntf_enabled_boot ? "EN" : "DIS");
            break;
        }
        default:
            break;
    }
}

static void hid_boot_out_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[HID  ] hid_boot_out_cbk event=%d\n", event);
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   g_hid_boot_out_report, sizeof(g_hid_boot_out_report));
            break;
        }
        // Boot Keyboard Output 写请求事件：处理对端写入的输出报告数据。
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            uint16_t copy_len = (in->val_len > sizeof(g_hid_boot_out_report))
                                    ? sizeof(g_hid_boot_out_report)
                                    : in->val_len;
            if (copy_len > 0) {
                memcpy(g_hid_boot_out_report, in->val, copy_len);
            }
            co_printf("[HID  ] HID Boot OUT write (len=%d)\n", in->val_len);
            /* 通知PM：对端(STB)下发数据 → 重置空闲计时器/退出待机态 */
            pm_activity_notify(PM_ACTIVITY_PEER_DATA);
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        // 处理 Boot Keyboard Output 的写命令事件（无需响应，直接处理数据）。
        case GATTS_EV_CHR_VALUE_WRITE: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            uint16_t copy_len = (in->val_len > sizeof(g_hid_boot_out_report))
                                    ? sizeof(g_hid_boot_out_report)
                                    : in->val_len;
            if (copy_len > 0) {
                memcpy(g_hid_boot_out_report, in->val, copy_len);
            }
            co_printf("[HID  ] HID Boot OUT write cmd (len=%d)\n", in->val_len);
            /* 通知PM：对端(STB)下发数据 → 重置空闲计时器/退出待机态 */
            pm_activity_notify(PM_ACTIVITY_PEER_DATA);
            break;
        }
    }
}

static void hid_info_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[HID  ] hid_info_cbk event=%d\n", event);
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            co_printf("[HID  ] HID Information read req\n");
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   (uint8_t *)g_hid_info_value, sizeof(g_hid_info_value));
            break;
        }
        default:
            break;
    }
}

static void hid_report_map_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[HID  ] hid_report_map_cbk event=%d\n", event);
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            uint16_t map_len = sizeof(hid_report_map);
            uint16_t off = in->val_off;
            if (off < map_len) {
                gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                       (uint8_t *)hid_report_map + off, map_len - off);
            } else {
                gatts_chr_val_read_rsp(in->peer, in->chr, GATT_RES_INVALID_OFFSET, NULL, 0);
            }
            break;
        }
        default:
            break;
    }
}

static void hid_protocol_mode_cbk(gatt_event_t event, void *context, void *param)
{
    co_printf("[HID  ] hid_protocol_mode_cbk event=%d\n", event);
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   &g_hid_protocol_mode, sizeof(g_hid_protocol_mode));
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            co_printf("[HID  ] HID Protocol Mode write req\n");
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            if (in->val_len >= 1) {
                uint8_t requested = in->val[0];
                g_hid_protocol_mode = requested;
                co_printf("[HID  ] HID Protocol Mode req=%u, set=%u\n", requested, g_hid_protocol_mode);
            }
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        case GATTS_EV_CHR_VALUE_WRITE: {
            co_printf("[HID  ] HID Protocol Mode write cmd\n");
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            if (in->val_len >= 1) {
                uint8_t requested = in->val[0];
                g_hid_protocol_mode = requested;
                co_printf("[HID  ] HID Protocol Mode cmd=%u, set=%u\n", requested, g_hid_protocol_mode);
            }
            break;
        }
        default:
            break;
    }
}

static void hid_control_point_cbk(gatt_event_t event, void *context, void *param)
{
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_WRITE_REQ: {
            gatt_chr_val_write_req_t *in = (gatt_chr_val_write_req_t *)param;
            co_printf("[HID  ] HID Control Point write len=%u\n", in->val_len);
            gatts_chr_val_write_rsp(in->peer, in->chr, GATT_OK);
            break;
        }
        default:
            break;
    }
}

static void hid_report_in_cbk(gatt_event_t event, void *context, void *param)
{
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   g_hid_report_in_buf, sizeof(g_hid_report_in_buf));
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START: {
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            g_kbd_ntf_enabled_report = (in->type == GATT_SEND_VAL_NOTIFICATION);
            g_hid_report_peer = g_kbd_ntf_enabled_report ? in->peer : NULL;
            co_printf("[HID  ] HID Report IN CCCD=%s\n", g_kbd_ntf_enabled_report ? "EN" : "DIS");
            break;
        }
        default:
            break;
    }
}

static void hid_report_in_consumer_cbk(gatt_event_t event, void *context, void *param)
{
    (void)context;
    switch (event) {
        case GATTS_EV_CHR_VALUE_READ_REQ: {
            gatt_chr_val_read_req_t *in = (gatt_chr_val_read_req_t *)param;
            gatts_chr_val_read_rsp(in->peer, in->chr, GATT_OK,
                                   g_hid_report_in_consumer_buf, sizeof(g_hid_report_in_consumer_buf));
            break;
        }
        case GATTS_EV_CHR_VALUE_SEND_START: {
            gatt_chr_val_send_start_t *in = (gatt_chr_val_send_start_t *)param;
            g_kbd_ntf_enabled_report_consumer = (in->type == GATT_SEND_VAL_NOTIFICATION);
            g_hid_report_consumer_peer = g_kbd_ntf_enabled_report_consumer ? in->peer : NULL;
            co_printf("[HID  ] HID Report IN (Consumer) CCCD=%s\n", g_kbd_ntf_enabled_report_consumer ? "EN" : "DIS");
            break;
        }
        default:
            break;
    }
}

void hid_kbd_init(void)
{
    //co_printf("hid_kbd_init enter\n");
    (void)hid_report_map; // 当前手动建表，仅保留用于对照说明。

    /*
     * 手动创建 HID Service (0x1812) 的核心特征（按规范顺序组织）：
     * 1) HID Information (0x2A4A)
     * 2) HID Report Map (0x2A4B)
     * 3) HID Protocol Mode (0x2A4E)
     * 4) HID Control Point (0x2A4C)
     * 5) Report (Input, 0x2A4D) + CCCD (0x2902)
     *    说明：Report Reference (0x2908) 需要描述符支持，当前 SDK 仅提供 CCCD 添加接口。
     *
     * Boot 模式特征：
     * a) Boot Keyboard Input Report (0x2A22)
     *    - 属性: READ + NOTIFY
     *    - 必须挂载 CCCD (0x2902)
     * b) Boot Keyboard Output Report (0x2A32)
     *    - 属性: READ + WRITE + WRITE WITHOUT RSP
     *    - 不需要描述符
     */
    uuid_t svc_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x1812) };
    uuid_t in_uuid  = { UUID_LEN_16, UUID16_VAL_SET(0x2A22) };
    uuid_t out_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A32) };
    uuid_t info_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A4A) };
    uuid_t report_map_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A4B) };
    uuid_t protocol_mode_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A4E) };
    uuid_t control_point_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A4C) };
    uuid_t report_in_uuid = { UUID_LEN_16, UUID16_VAL_SET(0x2A4D) };

    g_hid_boot_svc = gatts_svc_create(&svc_uuid, hid_boot_svc_cbk, NULL);

    (void)gatts_chr_add(g_hid_boot_svc, &info_uuid,
                        GATT_CHR_PROP_READ,
                        ATT_PERM_READ,
                        hid_info_cbk, NULL);

    (void)gatts_chr_add(g_hid_boot_svc, &report_map_uuid,
                        GATT_CHR_PROP_READ,
                        ATT_PERM_READ,
                        hid_report_map_cbk, NULL);

    (void)gatts_chr_add(g_hid_boot_svc, &protocol_mode_uuid,
                        GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE,
                        ATT_PERM_READ | ATT_PERM_WRITE,
                        hid_protocol_mode_cbk, NULL);

    (void)gatts_chr_add(g_hid_boot_svc, &control_point_uuid,
                        GATT_CHR_PROP_WRITE,
                        ATT_PERM_WRITE,
                        hid_control_point_cbk, NULL);

    g_hid_report_in_chr = gatts_chr_add(g_hid_boot_svc, &report_in_uuid,
                                        GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY,
                                        ATT_PERM_READ,
                                        hid_report_in_cbk, NULL);

    //co_printf("HID Report IN chr add result: %p\n", g_hid_report_in_chr);

    // 添加 Report Input 的 CCCD 和 Report Reference 描述符（当前 SDK 复用 CCCD 添加接口来添加 Report Reference 描述符，实际使用时需要区分这两者）。
    gatts_dsc_ccc_add(g_hid_report_in_chr, 
                      GATT_CHR_PROP_NOTIFY,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_ref_dsc_ccc_add(g_hid_report_in_chr, // 注意：当前 SDK 没有专门的 Report Reference 描述符添加接口，这里复用 CCCD 添加接口来添加 Report Reference 描述符，实际使用时需要区分这两者。
                          GATT_CHR_PROP_NOTIFY,
                          ATT_PERM_READ | ATT_PERM_WRITE,
                          sizeof(g_hid_report_in_ref),
                          (uint8_t *)g_hid_report_in_ref);

    g_hid_report_in_consumer_chr = gatts_chr_add(g_hid_boot_svc, &report_in_uuid,
                                                 GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY,
                                                 ATT_PERM_READ,
                                                 hid_report_in_consumer_cbk, NULL);

    gatts_dsc_ccc_add(g_hid_report_in_consumer_chr,
                      GATT_CHR_PROP_NOTIFY,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_ref_dsc_ccc_add(g_hid_report_in_consumer_chr,
                          GATT_CHR_PROP_NOTIFY,
                          ATT_PERM_READ | ATT_PERM_WRITE,
                          sizeof(g_hid_report_in_consumer_ref),
                          (uint8_t *)g_hid_report_in_consumer_ref);

    g_hid_boot_in_chr = gatts_chr_add(g_hid_boot_svc, &in_uuid,
                                      GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY,
                                      ATT_PERM_READ,
                                      hid_boot_in_cbk, NULL);

    gatts_dsc_ccc_add(g_hid_boot_in_chr,
                      GATT_CHR_PROP_NOTIFY,
                      ATT_PERM_READ | ATT_PERM_WRITE);

    gatts_ref_dsc_ccc_add(g_hid_boot_in_chr,
                          GATT_CHR_PROP_NOTIFY,
                          ATT_PERM_READ | ATT_PERM_WRITE,
                          sizeof(g_hid_boot_in_ref),
                          (uint8_t *)g_hid_boot_in_ref);

    (void)gatts_chr_add(g_hid_boot_svc, &out_uuid,
                        GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE |
                            GATT_CHR_PROP_WRITE_WITHOUT_RSP,
                        ATT_PERM_READ | ATT_PERM_WRITE,
                        hid_boot_out_cbk, NULL);

    gatts_svc_start(g_hid_boot_svc);

    //co_printf("hid_kbd_init exit\n");
}

void hid_kbd_on_disconnect(void)
{
    g_hid_boot_peer = NULL;
    g_hid_report_peer = NULL;
    g_hid_report_consumer_peer = NULL;
    g_kbd_ntf_enabled_boot = false;
    g_kbd_ntf_enabled_report = false;
    g_kbd_ntf_enabled_report_consumer = false;
    memset(g_hid_boot_in_report, 0, sizeof(g_hid_boot_in_report));
    memset(g_hid_report_in_buf, 0, sizeof(g_hid_report_in_buf));
    memset(g_hid_report_in_consumer_buf, 0, sizeof(g_hid_report_in_consumer_buf));
}

bool hid_kbd_can_send_current(void)
{
    // 当前实现仅检查是否已连接。后续可叠加 CCCD 订阅检查等策略。
    peer_t peer = ble_gap_get_peer();
    static uint16_t warn_throttle = 0;
    if (peer == NULL) {
        if (++warn_throttle >= 200) { // 约 2s 打一次提示（主循环 10ms）
            warn_throttle = 0;
            co_printf("[HID  ] HID send blocked: peer NULL\n");
        }
        return false;
    }

    warn_throttle = 0;
    return true;
}

void hid_kbd_send_report(peer_t peer, uint8_t rpt_id, uint8_t rpt_type, const uint8_t *data, uint16_t len)
{
    // 发送 HID 报告的薄封装：优先通过 Report Input (0x2A4D)，次选 Boot Input (0x2A22)。
    if (data == NULL || len == 0) {
        return;
    }
    if (!hid_kbd_ntf_is_enabled(rpt_id, rpt_type)) {
        //co_printf("HID send blocked: notify disabled\n");
        return;
    }

    if (peer == NULL) {
        if (rpt_type == HID_RPT_TYPE_INPUT && rpt_id == HID_KBD_RPT_ID_CONSUMER) {
            peer = g_hid_report_consumer_peer;
        } else {
            peer = (g_hid_report_peer != NULL) ? g_hid_report_peer : g_hid_boot_peer;
        }
        if (peer == NULL) {
            peer = ble_gap_get_peer();
        }
    }
    if (peer == NULL) {
        return;
    }

    if (rpt_type == HID_RPT_TYPE_INPUT && rpt_id == HID_KBD_RPT_ID_CONSUMER) {
        if (!g_kbd_ntf_enabled_report_consumer || g_hid_report_in_consumer_chr == NULL) {
            return;
        }
        if (len > sizeof(g_hid_report_in_consumer_buf)) {
            len = sizeof(g_hid_report_in_consumer_buf);
        }
        memcpy(g_hid_report_in_consumer_buf, data, len);
        gatt_rst_t ret = app_gatt_notify(peer, g_hid_report_in_consumer_chr,
                                             g_hid_report_in_consumer_buf, len);
        co_printf("[HID  ] HID Report IN (Consumer) notify ret=%d\n", ret);
        return;
    }

    if (g_kbd_ntf_enabled_report && g_hid_report_in_chr != NULL) {
        memset(g_hid_report_in_buf, 0, sizeof(g_hid_report_in_buf));
        if (len > sizeof(g_hid_report_in_buf)) {
            len = sizeof(g_hid_report_in_buf);
        }
        memcpy(g_hid_report_in_buf, data, len);
        gatt_rst_t ret = app_gatt_notify(peer, g_hid_report_in_chr, g_hid_report_in_buf, len);
        co_printf("[HID  ] HID Report IN notify ret=%d\n", ret);
        {
            extern volatile uint32_t g_last_ble_ms;
            g_last_ble_ms = sdk_timer_get_current_ms();
        }
        return;
    }

    if (g_kbd_ntf_enabled_boot && g_hid_boot_in_chr != NULL) {
        memset(g_hid_boot_in_report, 0, sizeof(g_hid_boot_in_report));
        if (len > sizeof(g_hid_boot_in_report)) {
            len = sizeof(g_hid_boot_in_report);
        }
        memcpy(g_hid_boot_in_report, data, len);
        gatt_rst_t ret = app_gatt_notify(peer, g_hid_boot_in_chr, g_hid_boot_in_report,
                                             sizeof(g_hid_boot_in_report));
        co_printf("[HID  ] HID Boot IN notify ret=%d\n", ret);
    }

    
}



void hid_kbd_send_report_current(uint8_t rpt_id, uint8_t rpt_type, const uint8_t *data, uint16_t len)
{
    // 使用当前连接对端句柄发送，便于上层简化调用。
    peer_t peer = ble_gap_get_peer();
    if (peer == NULL) {
        return;
    }
    //co_printf("Sending HID report to current peer, rpt_id=%d, rpt_type=%d\n", rpt_id, rpt_type);
    hid_kbd_send_report(peer, rpt_id, rpt_type, data, len);
}

void hid_kbd_send_test_key_current(void)
{
    // 测试接口：用于验证 HID 通路是否正常。
    if (!hid_kbd_can_send_current()) {
        return;
    }

    // 简单测试：发送键盘 'a'（0x04）按下，再发送全 0 表示抬起。
    // 使用静态缓冲并做节流，避免发送队列堆积导致 malloc 失败。
    static uint8_t report[HID_KBD_REPORT_LEN];
    static uint16_t throttle = 0;
    static bool key_down = false;

    if (++throttle < 50) { // 约 50 次调用发一次，主循环 10ms 时约 500ms
        return;
    }
    throttle = 0;

    memset(report, 0, sizeof(report));
    if (!key_down) {
        report[2] = 0x04; // HID usage: 'a'
        key_down = true;
    } 
    else {
        key_down = false;
    }

    // 若主机启用 Boot 模式，只接受 Boot Keyboard Input（无 Report ID）。
    if (hid_kbd_ntf_is_enabled(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT)) {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT, report, sizeof(report));
    } else {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_BOOT_KB_IN, report, sizeof(report));
    }
    co_printf("[HID  ] HID send done\n");
}

void hid_kbd_send_keycode_current(uint8_t usage_id)
{
    // 单键发送：先按下，再抬起，用于后续按键映射对接。
    // usage_id 来自 HID Keyboard Page（0x07）。
    uint8_t report[HID_KBD_REPORT_LEN] = {0};

    //co_printf("HID send keycode: %02X\n", usage_id);
    if (!hid_kbd_can_send_current()) {
        return;
    }
   
    // 报告格式：byte0=修饰键，byte1=保留，byte2~7=按键数组
    report[2] = usage_id;

    if (hid_kbd_ntf_is_enabled(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT)) {
        //co_printf("HID send as Report Input\n");
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT, report, sizeof(report));
    } else {
        //co_printf("HID send as Boot Keyboard Input\n");
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_BOOT_KB_IN, report, sizeof(report));
    }
}

void hid_kbd_send_keycode_with_mod_current(uint8_t modifier, uint8_t usage_id)
{
    // 修饰键 + 单键：先按下，再抬起。
    // modifier 为位掩码（HID_MOD_*），usage_id 为键盘 Usage。
    uint8_t report[HID_KBD_REPORT_LEN] = {0};

    if (!hid_kbd_can_send_current()) {
        return;
    }

    // 报告格式：byte0=修饰键，byte1=保留，byte2~7=按键数组
    report[0] = modifier;
    report[2] = usage_id;

    if (hid_kbd_ntf_is_enabled(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT)) {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT, report, sizeof(report));
    } else {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_BOOT_KB_IN, report, sizeof(report));
    }
}

void hid_kbd_send_consumer_current(uint16_t usage_id)
{
    // Consumer 报告：1 字节位图，释放由上层统一路径处理。
    // bit0=Mute(0xE2), bit1=Vol+(0xE9), bit2=Vol-(0xEA)
    uint8_t report[HID_CONSUMER_REPORT_LEN] = {0};

    if (!hid_kbd_can_send_current()) {
        return;
    }
    co_printf("[HID  ] HID send consumer usage: %04X\n", usage_id);
    switch (usage_id) {
        case HID_CONSUMER_MUTE:
            report[0] = 0x01u;
            break;
        case HID_CONSUMER_VOL_UP:
            report[0] = 0x02u;
            break;
        case HID_CONSUMER_VOL_DOWN:
            report[0] = 0x04u;
            break;
        default:
            return;
    }
    hid_kbd_send_report_current(HID_KBD_RPT_ID_CONSUMER, HID_RPT_TYPE_INPUT, report, sizeof(report));
}

void hid_kbd_send_consumer_release_current(void)
{
    // Consumer 报告释放：发送 16-bit 全 0。
    uint8_t report[HID_CONSUMER_REPORT_LEN] = {0};

    if (!hid_kbd_can_send_current()) {
        return;
    }

    hid_kbd_send_report_current(HID_KBD_RPT_ID_CONSUMER, HID_RPT_TYPE_INPUT, report, sizeof(report));
}

void hid_kbd_send_release_current(void)
{
    // 释放报告：发送全 0。
    uint8_t report[HID_KBD_REPORT_LEN] = {0};

    if (!hid_kbd_can_send_current()) {
        return;
    }

    if (hid_kbd_ntf_is_enabled(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT)) {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_INPUT, report, sizeof(report));
    } else {
        hid_kbd_send_report_current(HID_KBD_RPT_ID_KEYBOARD, HID_RPT_TYPE_BOOT_KB_IN, report, sizeof(report));
    }
}


bool hid_kbd_map_key_to_usage(uint16_t key_id, uint16_t *usage_id, bool *is_consumer)
{
    // 按键映射表：将本地按键编号 (key_id) 映射为标准 HID 键盘 usage ID。
    // 本示例采用标准 QWERTY 布局，44 键矩阵分配如下：
    // KEY1-KEY10:  数字键 1-9, 0
    // KEY11-KEY20: 字母键 Q W E R T Y U I O P
    // KEY21-KEY30: 字母键 A S D F G H J K L BackSpace
    // KEY31-KEY40: 字母键 Z X C V B N M , . /
    // KEY41:       Enter
    // KEY42:       Space
    // KEY43:       Tab
    // KEY44:       Escape
    
    if (!usage_id || !is_consumer) {
        return false;
    }
    //co_printf("Mapping key_id %d to usage_id...\n", key_id);
    *is_consumer = false; // 默认为标准键盘报告（非 Consumer）
    
    switch (key_id) {
        // 数字行 1-0
        case 1:  *usage_id = 0x66; break; // '1' HID 0x1E 电源
        case 2:  *usage_id = 0xE2; *is_consumer = true; break; // '2' HID 0x1F 静音
        case 3:  *usage_id = 0x55; break; // '3' HID Gehua 电视
        //case 3:  *usage_id = 0x3D; break; // '3' HID 规范书 电视
        case 4:  *usage_id = 0x28; break; // '4' HID 0x21 确认
        case 5:  *usage_id = 0x50; break; // '5' HID 0x22 左
        case 6:  *usage_id = 0x4F; break; // '6' HID 0x23 右
        case 7:  *usage_id = 0x51; break; // '7' HID 0x24 下
        case 8:  *usage_id = 0x4A; break; // '8' HID 0x25 主页
        case 9:  *usage_id = 0x29; break; // '9' HID 0x26 返回
        case 10: *usage_id = 0x65; break; // '0' HID 0x27 菜单
        
        // 第二行 Q-P
        case 11: *usage_id = 0x4B; break; // 'Q' HID 0x14 频道+
        case 12: *usage_id = 0xE9; *is_consumer = true; break; // 'W' HID 0x1A 音量+
        case 13: *usage_id = 0x4E; break; // 'E' HID 0x08 频道-
        case 14: *usage_id = 0xEA; *is_consumer = true; break; // 'R' HID 0x15 音量-
        case 15: *usage_id = 0x3E; break; // 'T' HID 0x17 语音
        case 16: *usage_id = 0x52; break; // 'Y' HID 0x1C 上
        case 17: *usage_id = 0x41; break; // 'U' HID 0x18 回看 Gehua
        //case 17: *usage_id = 0x91; break; // 'U' HID 回看 规范书
        case 18: *usage_id = 0x0C; break; // 'I' HID 0x0C
        case 19: *usage_id = 0x12; break; // 'O' HID 0x12
        case 20: *usage_id = 0x13; break; // 'P' HID 0x13
        
        // 第三行 A-L + BackSpace
        case 21: *usage_id = 0x04; break; // 'A' HID 0x04
        case 22: *usage_id = 0x16; break; // 'S' HID 0x16
        case 23: *usage_id = 0x07; break; // 'D' HID 0x07
        case 24: *usage_id = 0x09; break; // 'F' HID 0x09
        case 25: *usage_id = 0x0A; break; // 'G' HID 0x0A
        case 26: *usage_id = 0x0B; break; // 'H' HID 0x0B
        case 27: *usage_id = 0x0D; break; // 'J' HID 0x0D
        case 28: *usage_id = 0x0E; break; // 'K' HID 0x0E
        case 29: *usage_id = 0x0F; break; // 'L' HID 0x0F
        case 30: *usage_id = 0x2A; break; // BackSpace HID 0x2A
        
        // 第四行 Z-/ 
        case 31: *usage_id = 0x1D; break; // 'Z' HID 0x1D
        case 32: *usage_id = 0x1B; break; // 'X' HID 0x1B
        case 33: *usage_id = 0x06; break; // 'C' HID 0x06
        case 34: *usage_id = 0x19; break; // 'V' HID 0x19
        case 35: *usage_id = 0x05; break; // 'B' HID 0x05
        case 36: *usage_id = 0x11; break; // 'N' HID 0x11
        case 37: *usage_id = 0x10; break; // 'M' HID 0x10
        case 38: *usage_id = 0x36; break; // ',' HID 0x36
        case 39: *usage_id = 0x37; break; // '.' HID 0x37
        case 40: *usage_id = 0x38; break; // '/' HID 0x38
        
        // 特殊键
        case 41: *usage_id = 0x28; break; // Enter HID 0x28
        case 42: *usage_id = 0x2C; break; // Space HID 0x2C
        case 43: *usage_id = 0x2B; break; // Tab HID 0x2B
        case 44: *usage_id = 0x29; break; // Escape HID 0x29
        
        default:
            return false; // 未知按键编号，返回失败
    }
    
    return true; // 映射成功
}

void hid_kbd_send_key_id_current(uint16_t key_id)
{
    // 业务层入口：根据本地按键编码查表映射并发送。
    uint16_t usage_id = 0;
    bool is_consumer = false;
    //co_printf("Mapping key_id %d to usage_id...\n", key_id);
    if (!hid_kbd_map_key_to_usage(key_id, &usage_id, &is_consumer))
    {
        return;
    }
    //co_printf("Mapped key_id %d to usage_id 0x%04X (consumer=%d)\n", key_id, usage_id, is_consumer);
    if (is_consumer) 
    {
        hid_kbd_send_consumer_current(usage_id);
    }   
    else 
    {
        //co_printf("Sending keyboard usage_id 0x%02X\n", usage_id);
        hid_kbd_send_keycode_current((uint8_t)usage_id);
    }
}

// 强制使能所有 HID CCCD，绕过 STB 不写 CCCD 的问题
void hid_kbd_force_cccd_enable(peer_t peer)
{
    if (peer == NULL) return;

    // Boot Keyboard Input
    g_kbd_ntf_enabled_boot = true;
    g_hid_boot_peer = peer;

    // Report Input (keyboard)
    g_kbd_ntf_enabled_report = true;
    g_hid_report_peer = peer;

    // Report Input (consumer)
    g_kbd_ntf_enabled_report_consumer = true;
    g_hid_report_consumer_peer = peer;

    co_printf("HID CCCD force-enabled for bonded peer %08X\r\n", (uint32_t)peer);
}

bool hid_kbd_is_any_cccd_enabled(void)
{
	return g_kbd_ntf_enabled_boot || g_kbd_ntf_enabled_report || g_kbd_ntf_enabled_report_consumer;
}
