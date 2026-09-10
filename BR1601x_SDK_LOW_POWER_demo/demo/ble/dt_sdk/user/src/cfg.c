#include "porting.h"
#include "cfg.h"
#include "flash_manage.h"
#include "task.h"
#include "app_ble_cfg.h"
#include "log.h"

#define DELAY_STORE_TIME_MS         500

#define  HEAD_FLAG              "BRNV"
#define  BR_CFG_AREA_SIZE       0x1000
#define  FLASH_BASE_ADDR        0
#define  BR_CFG_ADDR            0
#define  BR_CFG_BACKUP_ADDR     0
#define  CONFIG_AREA            0
#define  CONFIG_BACK_AREA       1

#define  CFG_LEN_OFFSET         0x0008
#define  CFG_CNT_OFFSET         0x000C    
#define  CRC_OFFSET             0x0010
#define  DATA_OFFSET            0x0012
#define  __MAX(a, b)            ((a) > (b) ? (a) : (b))

#define  MINOR_CFG_HEAD_FLAG        "NVDS"
#define  MINOR_CFG_HEAD_SIZE        (sizeof(MINOR_CFG_HEAD_FLAG) - 1)
#define  MINOR_CFG_HEAD_OFFSET      (0)
#define  MINOR_CFG_DATA_OFFSET      (MINOR_CFG_HEAD_SIZE)
#define  MINOR_CFG_INVALID_TYPE     (0xFF)
#define  MINOR_CFG_INVALID_STATE    (0xFF)

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t state;
    uint8_t checksum;
    uint8_t data[];
}minor_cfg_item_t;

typedef struct {
    const void* ob_pointer;
    uint16_t offset;
    uint16_t len;
    uint8_t is_revisable;
}item_info_t;

static const uint8_t br_firm_vision[] __attribute__((at(0x01004000+0x1000)))= BR_DEFAULT_CONFIG_VERSION; 

static uint32_t g_cfg = 0;

static uint32_t g_cur_cfg_cnt = 0;

static const item_info_t s_item_tab[] = {
    {BR_DEFAULT_CONFIG_VERSION,  offsetof(struct cfg_strc, cfg_version),  32,                              false},
    {NULL,              offsetof(struct cfg_strc, uart_baud),             sizeof(uint32_t),                true},
	{NULL,              offsetof(struct cfg_strc, uart_fc),               sizeof(uint8_t),                 true},
	{NULL,              offsetof(struct cfg_strc, multi_conn),            sizeof(uint8_t),                 true},
	{NULL,              offsetof(struct cfg_strc, command_mode),          sizeof(uint8_t),                 true},
	{NULL,              offsetof(struct cfg_strc, auto_unlock),           sizeof(uint8_t),                 true},
	{NULL,				offsetof(struct cfg_strc, btmode),				  sizeof(uint8_t),				   true},
	{NULL,              offsetof(struct cfg_strc, tx_delay),              sizeof(cmd_tx_delay_t),          true},
	{NULL,              offsetof(struct cfg_strc, wake_act),              sizeof(cmd_wake_act_t),          true},
	{NULL,              offsetof(struct cfg_strc, name),                  32,                              true},
	{NULL,              offsetof(struct cfg_strc, addr),                  6,                               true},
	{NULL,              offsetof(struct cfg_strc, adv_params),            sizeof(cmd_adv_param_t),         true},
	{NULL,              offsetof(struct cfg_strc, scan_params),           sizeof(cmd_scan_param_t),        true},
	{NULL,              offsetof(struct cfg_strc, conn_params),           sizeof(cmd_conn_param_t),        true},
	{NULL,              offsetof(struct cfg_strc, pair_req_cfm),          sizeof(uint8_t),                 true},
	{NULL,              offsetof(struct cfg_strc, iocap),                 sizeof(uint8_t),                 true},
	{NULL,              offsetof(struct cfg_strc, passkey),               sizeof(uint32_t),                true},
	{NULL,              offsetof(struct cfg_strc, mtu),                   sizeof(uint16_t),                true},
	{NULL,              offsetof(struct cfg_strc, gatts_svcuuid),         sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gatts_wrtchar_uuid),    sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gatts_ntfchar_uuid),    sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gatts_fcchar_uuid),     sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gattc_svcuuid),         sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gattc_wrtchar_uuid),    sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gattc_ntfchar_uuid),    sizeof(cmd_gatt_uuid_t),         true},
	{NULL,              offsetof(struct cfg_strc, gattc_fcchar_uuid),     sizeof(cmd_gatt_uuid_t),         true},
    {NULL,              offsetof(struct cfg_strc, ind_prefix_type),       sizeof(uint8_t),                 true},
    {NULL,              offsetof(struct cfg_strc, ind_suffix_type),       sizeof(uint8_t),                 true},
    {NULL,              offsetof(struct cfg_strc, cmd_prefix_type),       sizeof(uint8_t),                 true},
    {NULL,              offsetof(struct cfg_strc, cmd_suffix_type),       sizeof(uint8_t),                 true},
    {NULL,              offsetof(struct cfg_strc, txpower),               sizeof(uint8_t),                 true},
    {NULL,              offsetof(struct cfg_strc, reserve),               256,                             true},

};
 
static tid_t s_update_id = NULL;
static app_cfg_strc_t s_cur_cfg = {0};
static const app_cfg_strc_t default_cfg = {
    .cfg_version = BR_DEFAULT_CONFIG_VERSION,
    .uart_baud = 115200,
    .uart_fc = 0,
    .multi_conn = 0,
    .command_mode = 0,
    .auto_unlock = 0,
    .btmode = 0x02,
    
    .tx_delay.wake_delay = 5,
    .tx_delay.sleep_delay = 500,

    .wake_act.type = 1,
    .wake_act.level = 1,
    .wake_act.keep_time = 0,

    .name = BLE_NAME_DEFAULT,
    .addr = {0x00, 0x00, 0x00, 0x0e, 0x7f, 0x04},

    .adv_params.adv_intreval_min = 320,
    .adv_params.adv_intreval_max = 320,
    .adv_params.adv_type = 0,
    .adv_params.own_address_type = 0,
    .adv_params.peer_address_type = 0,
    .adv_params.peer_address = {0},
    .adv_params.adv_channel_map = 39,
    .adv_params.adv_filter_policy = 0,

    .scan_params.scan_type = 1,
    .scan_params.scan_interval = 0x80,
    .scan_params.scan_window = 0x40,

    /* Customer BLE connection profile:
     * 50 ms interval, slave latency 49, supervision timeout 10 s.
     * BLE units are 1.25 ms for interval and 10 ms for timeout. */
    .conn_params.conn_interval_min = 40,
    .conn_params.conn_interval_max = 40,
    .conn_params.conn_latency = 49,
    .conn_params.sup_timemout = 1000,

    /*SMP*/ 
    .pair_req_cfm = 0,
    .iocap = 0,
    .passkey = 123456,

    /* GATT */
    .mtu = 515,

    /*GATT server */
    .gatts_svcuuid.len = 16,
    .gatts_svcuuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00},
    .gatts_wrtchar_uuid.len = 16,
    .gatts_wrtchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00},
    .gatts_ntfchar_uuid.len = 16,
    .gatts_ntfchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00},
    .gatts_fcchar_uuid.len = 16,
    .gatts_fcchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00},

    /*GATT Client*/
    .gattc_svcuuid.len = 16,
    .gattc_svcuuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00},
    .gattc_wrtchar_uuid.len = 16,
    .gattc_wrtchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00},
    .gattc_ntfchar_uuid.len = 16,
    .gattc_ntfchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00},
    .gattc_fcchar_uuid.len = 16,
    .gattc_fcchar_uuid.u.uuid128 = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00},
    
    /*Format*/
    .ind_prefix_type = 1,
    .ind_suffix_type = 1,
    .cmd_prefix_type = 0xFF,
    .cmd_suffix_type = 0xFF,

    .txpower = 9,
    .reserve = {0, ///auth level
                0, ///silent mode
                0, ///lesc
                0, 0, ///tx_phy, rx_phy
                0},
};

static bool flash_cfg_read(bool area, uint32_t addr, uint8_t *buf, uint32_t len)
{
    bool ret = 0;
    uint32_t (*flash_read_p)(uint32_t addr, uint8_t *buf, uint32_t len) = NULL;

    if(area == CONFIG_AREA) {
        flash_read_p = &flash_config_read;
    }
    else if(area == CONFIG_BACK_AREA){
        flash_read_p = &flash_config_back_read;
    }
    else {
        //invalid 
    }
    if(flash_read_p != NULL)
       ret = flash_read_p(addr, buf, len);

    return ret;
}

static bool flash_cfg_write(bool area, uint32_t addr, uint8_t *buf, uint32_t len)
{
    bool ret = 0;
    
    uint32_t (*flash_write_p)(uint32_t addr, uint8_t *buf, uint32_t len) = NULL;

    if(area == CONFIG_AREA) {
        flash_write_p = &flash_config_write;
    }
    else if(area == CONFIG_BACK_AREA){
        flash_write_p = &flash_config_back_write;
    }
    else {
        //invalid 
    }
    if(flash_write_p != NULL)
        ret = flash_write_p(addr, buf, len);

    return ret;
}

static bool flash_cfg_erase(bool area, uint32_t addr)
{
    bool ret = 0;
    
    uint32_t (*flash_erase_p)(uint32_t addr) = NULL;

    if(area == CONFIG_AREA) {
        flash_erase_p = &flash_config_erase;
    }
    else if(area == CONFIG_BACK_AREA){
        flash_erase_p = &flash_config_back_erase;
    }
    else {
        //invalid 
    }
    if(flash_erase_p != NULL)
        ret = flash_erase_p(addr);

    return ret;    
}

static uint8_t minor_data_checksum(uint8_t *data, uint32_t len)
{
    uint8_t check_sum = 0;
    for(uint32_t i=0; i<len; i++)
    {
        check_sum = (check_sum + data[i])&0xff;
    }
    return check_sum;
}

void minor_cfg_load(app_cfg_strc_t *p_cfg)
{
    uint8_t head[MINOR_CFG_HEAD_SIZE];
    flash_minor_cfg_read(MINOR_CFG_HEAD_OFFSET, head, MINOR_CFG_HEAD_SIZE);
    if(memcmp(head, MINOR_CFG_HEAD_FLAG, MINOR_CFG_HEAD_SIZE))  return;
    /// load default cfg
    uint32_t cur_off = MINOR_CFG_DATA_OFFSET;
    minor_cfg_item_t cfg_item;
    uint8_t data[256] = {0};
    while(cur_off + sizeof(minor_cfg_item_t) < FLASH_MINOR_CFG_SIZE){
        if(flash_minor_cfg_read(cur_off, (uint8_t *)&cfg_item, sizeof(minor_cfg_item_t))) break;
        BR_LOG_DEBUG_HEX("item head",&cfg_item, sizeof(minor_cfg_item_t));
        cur_off += sizeof(minor_cfg_item_t);
        if(cfg_item.type == MINOR_CFG_INVALID_TYPE) break;
        if(cfg_item.len + cur_off > FLASH_MINOR_CFG_SIZE || cfg_item.len == 0) break;
        if(flash_minor_cfg_read(cur_off, data, cfg_item.len))  break;
        BR_LOG_DEBUG_HEX("item data", data, cfg_item.len);
        cur_off += cfg_item.len;
        if(cfg_item.state == MINOR_CFG_INVALID_STATE) continue;
        BR_LOG_DEBUG("item checksum: %x\r\n", minor_data_checksum(data, cfg_item.len));
        if(cfg_item.checksum != minor_data_checksum(data, cfg_item.len)) continue;
        if(cfg_item.type >= CFG_TYPE_MAX) continue;
        const item_info_t* item = &s_item_tab[cfg_item.type];
        BR_LOG_DEBUG("cfg offset:%x, cfg len:%d\r\n", item->offset, item->len);
        memcpy((uint8_t *)p_cfg + item->offset, data, co_min(item->len, cfg_item.len));
    }
}

static bool flash_write_verify(bool area, uint32_t addr, uint8_t *data, uint32_t size)
{
    int write_cnt = 0;
    bool ret = true;

    while(write_cnt < 2) { /// may rewrite it up to 2.
        flash_cfg_erase(area, addr);
        flash_cfg_write(area, addr, data, size);
        ///verify 
        uint8_t verify_buf[256];
        uint32_t offset = 0;
        ret = true; 
        while(offset < size) { 
            uint32_t once_size = co_min(size - offset, 256);
            flash_cfg_read(area, addr + offset, verify_buf, once_size);
            if(memcmp(verify_buf, data + offset, once_size) != 0) {
                ret = false;
                break;
            }
            offset += once_size;
        }
        if(ret == true) break;
        else   write_cnt++;
    }
    return ret;
}

static void app_update_cfg_task(uint8_t ev, void *arg)
{
    g_cur_cfg_cnt++;
    app_cfg_strc_t* app = &s_cur_cfg;
    uint8_t temp_data[sizeof(app_cfg_strc_t) + DATA_OFFSET];
    memcpy(temp_data, HEAD_FLAG, 4);
    *((uint32_t*)(&temp_data[CFG_LEN_OFFSET])) = sizeof(app_cfg_strc_t);
    *((uint32_t*)(&temp_data[CFG_CNT_OFFSET])) = g_cur_cfg_cnt;
    *((uint16_t*)(&temp_data[CRC_OFFSET])) = br_calculate_crc((uint8_t *)app, sizeof(app_cfg_strc_t)); 
    memcpy(temp_data + DATA_OFFSET, app, sizeof(app_cfg_strc_t));
    //debug_log("update cnofiguration to NV\n");
    if(flash_write_verify(CONFIG_BACK_AREA, BR_CFG_BACKUP_ADDR, temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET))
    {
        if(!flash_write_verify(CONFIG_AREA, BR_CFG_ADDR,  temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET)){
            //debug_log("store cfg failed\n");
        }
    }
    else{
        //debug_log("store cfg backup failed\n");
    }
}

static void app_update_cfg(app_cfg_strc_t* app)
{
    memcpy(&s_cur_cfg, app, sizeof(app_cfg_strc_t));
    if(s_update_id == NULL)
        TIMER_CREATE_ONESHOT(&s_update_id, DELAY_STORE_TIME_MS, NULL, app_update_cfg_task);
    else
        timer_restart(&s_update_id);
}

static bool cfg_valid_check(bool area, uint32_t base_addr, uint32_t* cnt)
{
    uint8_t data[sizeof(app_cfg_strc_t) + DATA_OFFSET];
    bool ret = false;
    flash_cfg_read(area, base_addr, data, sizeof(app_cfg_strc_t) + DATA_OFFSET);

    *cnt = 0;
    if(memcmp(data, HEAD_FLAG, 4) == 0){
        uint32_t len = *(uint32_t*)(data + CFG_LEN_OFFSET);

        if(sizeof(app_cfg_strc_t) == len){
            uint16_t new_crc = br_calculate_crc(data + DATA_OFFSET,len);
            if(new_crc == *(uint16_t*)(data + CRC_OFFSET)){//check crc from dataDATA_OFFSET to data{
                *cnt = *(uint32_t*)(data + CFG_CNT_OFFSET);
                //debug_log("cnt [%d]", *cnt);
                ret = true;
            }
        }
    }

    return ret;
}

static void cfg_ref(uint32_t msk)
{
    uint8_t temp_data[sizeof(app_cfg_strc_t) + DATA_OFFSET];
    if(msk & 1)
        flash_cfg_read(CONFIG_BACK_AREA, g_cfg - DATA_OFFSET, temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET);
    else
        flash_cfg_read(CONFIG_AREA, g_cfg - DATA_OFFSET, temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET);

    if(!(msk & 1)){
        //debug_log("backup ref");
        flash_write_verify(CONFIG_BACK_AREA, BR_CFG_BACKUP_ADDR, temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET);
    }

    if(!(msk & 2)){
        //debug_log("cfg ref");
        flash_write_verify(CONFIG_AREA, BR_CFG_ADDR, temp_data, sizeof(app_cfg_strc_t) + DATA_OFFSET);
    }
}

void __weak sdk_default_cfg(app_cfg_strc_t *cfg)
{
}

void cfg_init(void)
{
    uint32_t cfg_area_cnt, backup_area_cnt/*, backup_area2_cnt*/;
    uint32_t msk = 0;

    msk |= cfg_valid_check(CONFIG_BACK_AREA, BR_CFG_BACKUP_ADDR, &backup_area_cnt) ? 1 : 0;
    g_cur_cfg_cnt = backup_area_cnt;
    msk |= cfg_valid_check(CONFIG_AREA, BR_CFG_ADDR, &cfg_area_cnt) && cfg_area_cnt >= g_cur_cfg_cnt ? 2 : 0;
    g_cur_cfg_cnt = __MAX(cfg_area_cnt, g_cur_cfg_cnt);

    g_cfg = (msk & 0x01) ? BR_CFG_BACKUP_ADDR + DATA_OFFSET : \
            (msk & 0x02) ? BR_CFG_ADDR + DATA_OFFSET : 0;

    if(g_cfg && msk != 3){
        cfg_ref(msk);
    }

    if(0 == msk){  /// cfg and backup area are bad,laod default configuration.
        //const item_info_t *start = s_item_tab, *end = start + sizeof(s_item_tab) / sizeof(item_info_t);
        app_cfg_strc_t temp;
        //debug_log("load default data\n");
        memcpy(&temp, &default_cfg, sizeof(app_cfg_strc_t));
		//update for address
		if(!br_addr_get(temp.addr)) {
            BR_LOG_WARNING("MAC ERR\r\n");///产测信息丢失  log
        }
        //Check the secondary configuration and load it.
        minor_cfg_load(&temp);
        //BR_LOG_DEBUG_HEX("temp", &temp, sizeof(app_cfg_strc_t));
        g_cur_cfg_cnt = 0;
        memcpy(&s_cur_cfg, &temp, sizeof(app_cfg_strc_t));
        /// Run an update task at initialization
        app_update_cfg_task(NULL,NULL);      
        g_cfg = BR_CFG_ADDR + DATA_OFFSET;
    }
    /// Obtain all configuration information
    flash_cfg_read(CONFIG_AREA, g_cfg, (void *)&s_cur_cfg, sizeof(app_cfg_strc_t));
}

void cfg_factory(void)
{
    flash_cfg_erase(CONFIG_AREA, BR_CFG_ADDR);
    flash_cfg_erase(CONFIG_BACK_AREA, BR_CFG_BACKUP_ADDR);
}

uint16_t cfg_get(cfg_type_t cfg_type, void* buf, uint16_t len)
{
    uint16_t ret = 0;
    const item_info_t* item = &s_item_tab[cfg_type];
    const uint8_t* data_info = item->ob_pointer;
    uint8_t temp_buf[sizeof(cfg_general_t)];

    if(g_cfg){
        memcpy(temp_buf, (uint8_t *)&s_cur_cfg + item->offset, item->len);
        data_info = temp_buf;
    }
    if(data_info){
        // Calculate the minimum buffer required.
		ret = item->len;
		
        /// Will return minimum buffer required,if 'buf' is a null pointer.
        if(NULL != buf){
			if(len < ret){
                ret = len;
            }
            memcpy(buf, data_info, ret);
        }
    }

    return ret;
}

uint16_t cfg_set(cfg_type_t cfg_type,const void* buf, uint16_t len)
{
    uint16_t ret = 0;
    const item_info_t* item = &s_item_tab[cfg_type];
    if(g_cfg && NULL != buf && item->is_revisable){
        app_cfg_strc_t app_temp;
        //sf_enable(HS_SF, 0);
        memcpy(&app_temp, &s_cur_cfg, sizeof(app_cfg_strc_t));
        
        uint8_t* data_info = ((uint8_t*)&app_temp) + item->offset;
        
        if(len > item->len){
            len = item->len;
        }
        ret = len;
        if(memcmp(data_info, buf, item->len) != 0){
            memset(data_info,0,item->len);
            memcpy(data_info, buf, len);
            app_update_cfg(&app_temp);
        }
    }
    
    return ret;
}

uint8_t cfg_reserver_get(uint8_t off, void* data, uint8_t len)
{
    if(g_cfg && 256 - len > off){
        memcpy(data, (uint8_t *)&s_cur_cfg + offsetof(struct cfg_strc, reserve) + off, len);
        return len;
    }

    return 0;
}

uint8_t cfg_reserver_set(uint8_t off, const void* data, uint8_t len)
{
    if(g_cfg && 256 - len > off){
        app_cfg_strc_t app_temp;
        //sf_enable(HS_SF, 0);
        memcpy(&app_temp, &s_cur_cfg, sizeof(app_cfg_strc_t));

        if(memcmp(&app_temp.reserve[0] + off, data, len) != 0){
            memcpy(&app_temp.reserve[0] + off, data, len);
            app_update_cfg(&app_temp);
        }

        return len;
    }

    return 0;
}

void cfg_store(void)
{
    if(s_update_id != NULL){
        timer_delete(&s_update_id);
        app_update_cfg_task(0,NULL);
    }
}

