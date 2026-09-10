#include "porting.h"
#include "atprocess.h"
#include "flash_manage.h"
#include "cfg.h"
#include "app_label_info.h"
#include "app_image_at.h"
#include "log.h"
#define ADV_DATA_FIX        "\x02\x01\x06"
#define ADV_DATA_FIX_LEN    (sizeof(ADV_DATA_FIX) - 1)


static cmd_evt_callback_t s_cmd_evt_callback = NULL;
static cmd_rsp_fmt_out_t s_cmd_rsp_fmt_out = NULL;
static cmd_pkt_proc_done_t s_cmd_pkt_done_ck = NULL; 

static bool s_cmd_uart_source = true;
char                cmd_prefix[4] = {0x41, 0x54, 0x2B, 0x00};          /*CFG_TYPE_CMD_PREFIX*/
char                cmd_suffix[4] = {0x0D, 0x0A, 0x00, 0x00};          /*CFG_TYPE_CMD_SUFFIX*/

static uint8_t countChar(const char *str, char c) 
{
    uint8_t count = 0;
    const char *tmp = str;
    
    while ((tmp = strchr(tmp, c)) != NULL) {
        count++;
        tmp++;
    }
    
    return count;
}

static uint8_t ctoi(char c)
{
    if((c >= 'A') && (c <= 'F')) {
        return (c - 'A' + 0x0A);
    }

    if((c >= 'a') && (c <= 'f')) {
        return (c - 'a' + 0x0A);
    }

    if((c >= '0') && (c <= '9')) {
        return (c - '0' + 0x00);
    }

    return 0xFF;
}

static uint16_t hex_str_2_num_array(char *str, uint16_t str_len, uint8_t *num_arr)
{
    uint16_t n = 0;
    uint16_t num = 0;
    uint16_t count = 0;

    if (str_len < 2) {
        return 0;
    }
    while (n < str_len) {
        if ((num = ctoi(str[n++])) == 0xFF) {
            return 0;
        }
        *num_arr = num << 4;
        if ((num = ctoi(str[n++])) == 0xFF) {
            return 0;
        }
        *num_arr |= num;
        num_arr++;
        count ++;
    }
    return count;
}

static uint16_t hex_str_2_rearray(const char *str, uint16_t str_len, uint8_t *num_arr)
{
    uint16_t n = 0;
    uint16_t num = 0;
    uint16_t count = 0;
    uint8_t *p_re = num_arr + (str_len / 2) - 1;

    if (p_re < num_arr || (str_len & 1)) {
        return 0;
    }
    while (n < str_len) {
        if ((num = ctoi(str[n++])) == 0xFF) {
            return 0;
        }
        *p_re = num << 4;
        if ((num = ctoi(str[n++])) == 0xFF) {
            return 0;
        }
        *p_re |= num;
        p_re--;
        count++;
    }
    return count;
}

static bool str_to_hex(uint8_t bt_addr[6], uint8_t *str)
{
	for(uint8_t i=0; i<12; i++)
	{
		if((str[i]>='0' && str[i]<='9') || (toupper(str[i])>='A' && toupper(str[i])<='F'))
		{
			if((str[i+1]>='0' && str[i+1]<='9') || (toupper(str[i+1])>='A' && toupper(str[i+1])<='F'))
			{
				bt_addr[5-i/2] = ((toupper(str[i])<='9' ? str[i]-'0':(toupper(str[i])-'A'+10))<<4) + \
										(str[i+1]<='9' ? str[i+1]-'0':(toupper(str[i+1])-'A'+10));
				i++;
				continue;
			}
		}
		return false;
	}

	return true;
}

bool check_adv_format(uint8_t type, uint8_t * data, uint8_t len) 
{
    if(len > ADV_DATA_MAX_SIZE)
        return false;
    
    if(type == 0){
        if(len < 3)
            return false;
        
        if (strncmp((const char *)data, ADV_DATA_FIX, ADV_DATA_FIX_LEN) != 0)  // 判断前6个字符是否为"020106"
            return false;
        
        data += ADV_DATA_FIX_LEN;
        len -= ADV_DATA_FIX_LEN;
    }

    while(len){   
        uint8_t adv_struct_len = *data;
        uint8_t adv_type = *(data+1);
        if(len < adv_struct_len + 1)
            return false;
        if(adv_type == 0x01 || (type == 1 && adv_type == 0x09))
            return false;
        
        len -= (adv_struct_len + 1);
        data += (adv_struct_len + 1);

        if(len == 0)
            break;
    }
    return true;
}

static int execute_cmd_evt_callback(cmd_change_evt_t evt, void* data)
{
    int ret = CMD_RUNING_FAILED;

    if(s_cmd_evt_callback != NULL){
       ret = s_cmd_evt_callback(evt,data);
    }

    return ret;
}

static int at_ver_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;

    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(CFG_TYPE_CFG_VER, &item, sizeof(item.cfg_version))){
            s_cmd_rsp_fmt_out("%s", item.cfg_version);
            ret = CMD_NO_ERR;
        }
    }
    
    return ret;
}

static int at_fw_ver_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    
	if(*cmd_line == 0 || *cmd_line == '?'){
		s_cmd_rsp_fmt_out("%s", BR_FIRMWARE_VERSION);
		ret = CMD_NO_ERR;
	}
	return ret;
}

static int at_pre_ver_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    
	if(*cmd_line == 0 || *cmd_line == '?'){
		s_cmd_rsp_fmt_out("%s", BR_PRT_VERSION_STR);
		ret = CMD_NO_ERR;
	}
	return ret;
}

static int at_baudrate_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(sizeof(uint32_t) == cfg_get(CFG_TYPE_UART_BAUD, &item, sizeof(item.uart_baud))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.uart_baud);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        if(sscanf(cmd_line, "%u", &item.uart_baud) == 1){
            switch(item.uart_baud)
			{
                case 9600:
                case 14400:
                case 19200:
                case 38400:
                case 57600:
                case 115200:
                case 230400:
                case 256000:
                case 460800:
                case 500000:
                case 921600:
                    ret = !!cfg_set(CFG_TYPE_UART_BAUD, &item.uart_baud, sizeof(item.uart_baud)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                    break;
                default:
                    ret = CMD_INVALID_PARAM;
                    break;
			}
            if(ret == CMD_NO_ERR){
                ret = execute_cmd_evt_callback(CMD_UART_BAUD, &item.uart_baud);
            }
        }
    }

	return ret;
}

static int at_flowctrl_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_UART_FC, &item, sizeof(item.uart_fc))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.uart_fc);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t flowctrl = 0;
        if(sscanf(cmd_line, "%u",  &flowctrl) == 1){
            item.uart_fc = flowctrl;
            if(item.uart_fc <= 1){
                ret = !!cfg_set(CFG_TYPE_UART_FC, &item.uart_fc, sizeof(item.uart_fc)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_UART_FLOWCTRL, &item.uart_fc);
                }
            }
            else{
                ret = CMD_INVALID_PARAM;
            }
        }
    }

	return ret;
}

static int at_silent_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    uint8_t silent = 0;

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_reserver_get(SILENT_OFF, &silent, SILENT_LEN)){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, silent);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        int32_t temp;
        if(sscanf(cmd_line, "%d",  &temp) == 1 && temp <= 1 && temp >= 0){
            silent = temp;
            ret = !!cfg_reserver_set(SILENT_OFF, &silent, SILENT_LEN) ? CMD_NO_ERR : CMD_RUNING_FAILED;
            if(ret == CMD_NO_ERR){
                ret = execute_cmd_evt_callback(CMD_SILENT, &silent);
            }
        }
        else{
            ret = CMD_INVALID_PARAM;
        }
    }

	return ret;
}

static int at_multiconn_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_MULTI_CONN, &item, sizeof(item.multi_conn))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.multi_conn);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t multiconn = 0, command = 0;
        cfg_get(CFG_TYPE_COMMAND_MODE, &command, sizeof(item.command_mode));
        if(sscanf(cmd_line, "%u",  &multiconn) == 1){
            if(command == 1){
                item.multi_conn = multiconn;
                if(item.multi_conn <= 1){
                    ret = !!cfg_set(CFG_TYPE_MULTI_CONN, &item.multi_conn, sizeof(item.multi_conn)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                    if(ret == CMD_NO_ERR){
                        ret = execute_cmd_evt_callback(CMD_MULTI_CONN, &item.multi_conn);
                    }
                }
                else{
                    ret = CMD_INVALID_PARAM;
                }
            }
            else{
                ret = CMD_STATUS_ERR;
            }
        }
    }

	return ret;
}

static int at_command_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_COMMAND_MODE, &item, sizeof(item.command_mode))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.command_mode);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t command = 0;
        if(sscanf(cmd_line, "%u",  &command) == 1 && command <= 1){
            ret = !!cfg_get(CFG_TYPE_MULTI_CONN, &item.multi_conn, sizeof(item.multi_conn)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
            if(ret == CMD_NO_ERR){
                if(item.multi_conn == 1 && command == 0)
                    ret = CMD_INVALID_PARAM;
                else {
                    item.command_mode = command;
                    ret = !!cfg_set(CFG_TYPE_COMMAND_MODE, &item.command_mode, sizeof(item.command_mode)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                    if(ret == CMD_NO_ERR){
                        ret = execute_cmd_evt_callback(CMD_COMMAND_MODE,&item.command_mode);
                    }
               	}
            } 
       	}
       	else
           	ret = CMD_INVALID_PARAM;
    }

	return ret;
}

static int at_autounlock_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_AUTOUNLOCK, &item, sizeof(item.auto_unlock))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.auto_unlock);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t autounlock = 0;
        if(sscanf(cmd_line, "%u",  &autounlock) == 1){
            item.auto_unlock = autounlock;
            if(item.auto_unlock <= 1){
                ret = !!cfg_set(CFG_TYPE_AUTOUNLOCK, &item.auto_unlock, sizeof(item.auto_unlock)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_AUTOUNLOCK, &item.auto_unlock);
                }
            }
            else{
                ret = CMD_INVALID_PARAM;
            }
        }
    }

	return ret;
}

static int at_txdelay_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_TX_DELAY, &item, sizeof(item.tx_delay))){
    		s_cmd_rsp_fmt_out("+%s:%d,%d", match_string, item.tx_delay.wake_delay, item.tx_delay.sleep_delay);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t wake_delay = 0, sleep_delay = 0;
        if(sscanf(cmd_line, "%u,%u",  &wake_delay, &sleep_delay) == 2){
            item.tx_delay.wake_delay = wake_delay;
            item.tx_delay.sleep_delay = sleep_delay;
            ret = !!cfg_set(CFG_TYPE_TX_DELAY, &item.tx_delay, sizeof(item.tx_delay)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
            if(ret == CMD_NO_ERR){
                ret = execute_cmd_evt_callback(CMD_TX_DELAY, &item.tx_delay);
            }
        }
    }

	return ret;
}

static int at_wakeact_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_WAKEACT, &item, sizeof(item.wake_act))){
    		s_cmd_rsp_fmt_out("+%s:%d,%d,%d", match_string, item.wake_act.type, item.wake_act.level, item.wake_act.keep_time);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t type = 0, level = 0, keep_time = 0;
        if(sscanf(cmd_line, "%u,%u,%u", &type, &level, &keep_time) == 3){
            item.wake_act.type = type;
            item.wake_act.level = level;
            item.wake_act.keep_time = keep_time;
            ret = !!cfg_set(CFG_TYPE_WAKEACT, &item.wake_act, sizeof(item.wake_act)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
            if(ret == CMD_NO_ERR){
                ret = execute_cmd_evt_callback(CMD_WAKEACT, &item.wake_act);
            }
        }
    }

	return ret;
}

static int at_whitelist_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	
    if(*cmd_line == 0 || *cmd_line == '?'){
        cmd_whitelist_t wl_dev = {0};
        ret = execute_cmd_evt_callback(CMD_WHITE_LIST_GET, &wl_dev);
        if(ret == CMD_NO_ERR){
            int len = 0;
            char str[256];
            len += sprintf(str+len,"+%s:",match_string);
            for(uint8_t i=0; i<wl_dev.dev_n; i++)
                len += sprintf(str+len, "%d,%02x%02x%02x%02x%02x%02x;", i, wl_dev.dev[i].addr[5],
                                                                            wl_dev.dev[i].addr[4],
                                                                            wl_dev.dev[i].addr[3],
                                                                            wl_dev.dev[i].addr[2],
                                                                            wl_dev.dev[i].addr[1],
                                                                            wl_dev.dev[i].addr[0]); 
            str[len-1] = 0;
            s_cmd_rsp_fmt_out("%s", str);
        }
    }
    else if(*cmd_line++ == '=') {
    	cmd_white_list_add_t wl;
        uint16_t param_len = strlen(cmd_line);
        if(param_len == 12){
            wl.type = INDEX_TYPE_ADDR;
            ret = !str_to_hex(wl.info.addr, (uint8_t *)(cmd_line));
        }
        else if(param_len == 4){
            wl.type = INDEX_TYPE_IDX;
            int32_t conhdl = 0;
            sscanf(cmd_line, "%d", &conhdl);
            if(conhdl <= BLE_MAX_CONN && conhdl >= 0){
                wl.info.conhdl = conhdl;
                ret = CMD_NO_ERR;
            }
        }
        if(ret == CMD_NO_ERR){
            ret = execute_cmd_evt_callback(CMD_WHITE_LIST_ADD, &wl);
        }
    }
    
    return ret;
}

static int at_del_whitelist_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cmd_white_list_del_t del_whitelist;
	
    if(*cmd_line++ == '='){
        uint16_t param_len = strlen(cmd_line);
        if(param_len == 12){
            del_whitelist.type = INDEX_TYPE_ADDR;
		    ret = !str_to_hex(del_whitelist.info.addr, (uint8_t *)(cmd_line));
        }
        else if(param_len == 1){
            del_whitelist.type = INDEX_TYPE_IDX;
            int32_t index = 0;
            sscanf(cmd_line, "%d", &index);
            if(index < MAX_WHTIELIST_NUM && index >= 0){
                del_whitelist.info.index = index;
                ret = CMD_NO_ERR;
            }
        }
        if(ret == CMD_NO_ERR){    
		    ret = execute_cmd_evt_callback(CMD_WHITE_LIST_DEL, &del_whitelist);
        }
    }
    
    return ret;
}

static int at_reset_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if(*cmd_line == 0) {
        ret = execute_cmd_evt_callback(CMD_RESET, NULL);
    }

    return ret;
}

static int at_name_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;

    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(CFG_TYPE_NAME, &item, sizeof(item.name))){
            s_cmd_rsp_fmt_out("+%s:%s", match_string, item.name);
            ret = CMD_NO_ERR;
        }
    }
    else if(*cmd_line == '=' && ( strlen(cmd_line + 1) <= 29) ){ ///APP_DEVICE_NAME_MAX_LEN
        ret = !!cfg_set(CFG_TYPE_NAME, cmd_line + 1, strlen(cmd_line + 1)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
		if(ret == CMD_NO_ERR){
            ret = execute_cmd_evt_callback(CMD_BT_NAME, (uint8_t *)cmd_line + 1);
		}
    }
    
    return ret;
}

static int at_addr_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;
	
    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(CFG_TYPE_ADDR, &item, sizeof(item.addr))){
            s_cmd_rsp_fmt_out("+%s:%02X%02X%02X%02X%02X%02X", match_string, item.addr[5], item.addr[4], \
                item.addr[3], item.addr[2], item.addr[1], item.addr[0]);
            ret = CMD_NO_ERR;
        }
    }
    else if(*cmd_line == '=' && (strlen(cmd_line + 1)  == 12) ){
		if(str_to_hex(item.addr, (uint8_t *)(cmd_line+1)) == true){
    		ret = !!cfg_set(CFG_TYPE_ADDR, item.addr, sizeof(item.addr)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
    		if(ret == CMD_NO_ERR){
    			ret = execute_cmd_evt_callback(CMD_BT_ADDR, item.addr);
    		}
        }
        else
            ret = CMD_INVALID_PARAM;
    }
    
    return ret;
}

static int at_adv_param_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_ADV_PARAMS, &item, sizeof(item.adv_params))){
            s_cmd_rsp_fmt_out("+%s:%d", match_string, item.adv_params.adv_intreval_max);
            ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        char addr[20] = {0};
        uint32_t adv_intreval_min = 0, adv_intreval_max = 0, adv_type = 0, own_address_type = 0, peer_address_type = 0, adv_channel_map = 0, adv_filter_policy = 0;
        if(sscanf(cmd_line, "%u,%u,%u,%u,%u,%[^,],%u,%u",  &adv_intreval_min, &adv_intreval_max, &adv_type,&own_address_type, &peer_address_type, addr, \
            &adv_channel_map, &adv_filter_policy) == 8){
            item.adv_params.adv_intreval_min = adv_intreval_min;
            item.adv_params.adv_intreval_max = adv_intreval_max;
            item.adv_params.adv_type = adv_type;
            item.adv_params.own_address_type = own_address_type;
            item.adv_params.peer_address_type = peer_address_type;
            item.adv_params.adv_channel_map = adv_channel_map;
            item.adv_params.adv_filter_policy = adv_filter_policy;
            if(strlen(addr) == 12){
                str_to_hex(item.adv_params.peer_address, (uint8_t *)(addr));
                ret = !!cfg_set(CFG_TYPE_ADV_PARAMS, &item.adv_params, sizeof(item.adv_params)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_BT_ADV_PARAMS, &item.adv_params);
                }
            }
        } 
        else if(sscanf(cmd_line, "%u,%u,%u,%u,%u,%[^,],%u,%u",  &adv_intreval_min, &adv_intreval_max, &adv_type,&own_address_type, &peer_address_type, addr, \
            &adv_channel_map, &adv_filter_policy) == 1){
            if(adv_intreval_min < 0x0020 || adv_intreval_min > 0x4000) //#define ADV_INTERVAL_MIN    0x0020 //(20 ms) #define ADV_INTERVAL_MAX    0x4000 //(10.24 sec)
                ret = CMD_INVALID_PARAM;
            else if(cfg_get(CFG_TYPE_ADV_PARAMS, &item, sizeof(item.adv_params))){
                item.adv_params.adv_intreval_min = adv_intreval_min;
                item.adv_params.adv_intreval_max = adv_intreval_min;
                ret = !!cfg_set(CFG_TYPE_ADV_PARAMS, &item.adv_params, sizeof(item.adv_params)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_BT_ADV_PARAMS, &item.adv_params);
                }
            }
        } 
    }

	return ret;
}

static int at_adv_data_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	
    if(*cmd_line == 0 || *cmd_line == '?'){
        cmd_adv_data_t adv_data = {0};
        uint8_t adv_data_str[64] = {0};
        ret = execute_cmd_evt_callback(CMD_BT_ADVDATA_GET, &adv_data);
		if(ret == CMD_NO_ERR){
        	hlp_numayyr_to_hexstr(adv_data_str, (uint8_t *)adv_data.adv_data, adv_data.adv_data_len);
        	s_cmd_rsp_fmt_out("+%s:%s", match_string, adv_data_str);
		}
    }
    else if(*cmd_line == '=' && (strlen(cmd_line + 1) % 2 == 0)){
        cmd_adv_data_t adv_data = {0};
        adv_data.adv_data_len = strlen(cmd_line + 1) >> 1;
        if(adv_data.adv_data_len <= ADV_DATA_MAX_SIZE){ 
		    if(hex_str_2_num_array((char *)(cmd_line+1), strlen(cmd_line+1), (uint8_t *)adv_data.adv_data) == adv_data.adv_data_len &&
              check_adv_format(0x00, adv_data.adv_data, adv_data.adv_data_len) == true)
		        ret = execute_cmd_evt_callback(CMD_BT_ADV_DATA, &adv_data);
            else
                ret = CMD_INVALID_PARAM;
        }
        else
            ret = CMD_INVALID_PARAM;
    }
    
    return ret;
}

static int at_adv_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if(*cmd_line == 0 || *cmd_line == '?'){
        uint32_t  adv_set;    
        ret = execute_cmd_evt_callback(CMD_BT_ADV_GET, (void *)&adv_set);
		if(ret == CMD_NO_ERR){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, adv_set);
		}
    }
    else if(*cmd_line++ == '='){
		uint32_t  adv_set;
        if(sscanf(cmd_line, "%u", &adv_set) == 1 && adv_set <= 1){
            ret = execute_cmd_evt_callback(CMD_BT_ADV_ENABLE, (void *)&adv_set);
        } 
    }
    
    return ret;
}

static int at_scan_rsp_data_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	
    if(*cmd_line == 0 || *cmd_line == '?'){
        cmd_sr_data_t sr_data = {0};
        uint8_t sr_data_str[64] = {0};
        ret = execute_cmd_evt_callback(CMD_BT_SRDATA_GET, &sr_data);
		if(ret == CMD_NO_ERR){
        	hlp_numayyr_to_hexstr(sr_data_str, sr_data.sr_data, sr_data.sr_data_len);
        	s_cmd_rsp_fmt_out("+%s:%s", match_string, sr_data_str);
       	}
    }
    else if(*cmd_line == '=' && (strlen(cmd_line + 1) % 2 == 0)){
        cmd_sr_data_t sr_data = {0};
        sr_data.sr_data_len = strlen(cmd_line + 1) >> 1;
		if(hex_str_2_num_array((char *)(cmd_line+1), strlen(cmd_line+1), (uint8_t *)sr_data.sr_data) == sr_data.sr_data_len &&
          check_adv_format(0x01, sr_data.sr_data, sr_data.sr_data_len) == true)
		    ret = execute_cmd_evt_callback(CMD_BT_ADV_SRDATA, &sr_data);
    }
    
    return ret;
}

static int at_scan_param_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_SCAN_PARAMS, &item, sizeof(item.scan_params))){
    		s_cmd_rsp_fmt_out("+%s:%d,%d,%d", match_string, item.scan_params.scan_interval, item.scan_params.scan_window, item.scan_params.scan_type);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t scan_interval, scan_window,scan_type;
        if(sscanf(cmd_line, "%u,%u,%u",  &scan_interval, &scan_window, &scan_type) == 3){
            if(scan_interval >= scan_window && scan_window >= 0x0004 && scan_interval <= 0x4000 && scan_type <= 1) { 
                item.scan_params.scan_interval = scan_interval;
                item.scan_params.scan_window = scan_window;
                item.scan_params.scan_type = scan_type;
                ret = !!cfg_set(CFG_TYPE_SCAN_PARAMS, &item.scan_params, sizeof(item.scan_params)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_BT_SCAN_PARAMS, &item.scan_params);
                }
            }
        }
    }

	return ret;
}

static int at_scankw_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    char *p_cmd_end = (char *)cmd_line + strlen(cmd_line);
    if(*cmd_line++ == '='){
        cmd_scankw_t scankw = {0};
        uint8_t comma_cnt = countChar(cmd_line, ',');
        uint32_t enable = 0, num = 0;
        do {
            if(sscanf(cmd_line, "%d,%d", &enable, &num) == 0) {
                break;
            }
            if(enable > 1 || num > 5) {
                ret = CMD_INVALID_PARAM;
                break;
            }
            if((num !=0 && comma_cnt != num + 1) || (enable == 1 && num == 0)) {
                ret = CMD_INVALID_PARAM;
                break;
            }
            uint8_t exception = false;
            char *comma = strchr(cmd_line, ',');///Find the first comma
            comma = strchr(comma+1, ',')+1;///Find the second comma
            for(uint8_t i=0; i<num; i++) {
                ///If it is the last parameter, then point to 0x00.
                char *next_comma = (i == num - 1) ? p_cmd_end : strchr(comma, ',');
                uint32_t hexstr_len = (uint32_t)next_comma - (uint32_t)comma;
                scankw.adv_data[i].len = hexstr_len >> 1;
                if(next_comma == NULL || hexstr_len == 0 || (hexstr_len%2)!=0 || (hexstr_len > ADV_DATA_MAX_SIZE*2) ||
                   (hex_str_2_num_array(comma, hexstr_len, scankw.adv_data[i].data) != scankw.adv_data[i].len))
                {
                    exception = true;
                    ret = CMD_INVALID_PARAM;
                    break;
                }
                comma = next_comma+1;
            }
            if(exception == true) {
                break;
            }
            scankw.enable = enable;
            scankw.num = num;
            ret = execute_cmd_evt_callback(CMD_BT_SCAN_KW, &scankw);
        }while(0);
    }

    return ret;
}

static int at_scan_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if(*cmd_line == 0 || *cmd_line == '?'){
        uint8_t  scan_enable;
        ret = execute_cmd_evt_callback(CMD_BT_SCAN_GET, (void *)&scan_enable);
    	s_cmd_rsp_fmt_out("+%s:%d", match_string, scan_enable);
    }
    else if(*cmd_line++ == '='){
        uint8_t scan_enable = 0;
        uint32_t temp;
        if(sscanf(cmd_line, "%d", &temp) == 1 && temp <= ENABLE) {
            scan_enable = temp & 0xFF;
            ret = execute_cmd_evt_callback(CMD_BT_SCAN_ENABLE, (void *)&scan_enable);
        }
        else {
            ret = CMD_INVALID_PARAM;
        }
    }
    
    return ret;
}

static int at_connect_dev_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	

	if(*cmd_line++ == '='){
		cmd_conn_req_t conn_addr;
		char* ptr = strchr(cmd_line, ',');

		if(ptr == cmd_line + 1){
			if((*cmd_line == '1' || *cmd_line == '0')) {
                ret = CMD_NO_ERR;
	            conn_addr.addr_type = *cmd_line - '0';
            }

			if(ret == CMD_NO_ERR ) {
				ptr++;
				if(strlen(ptr) == BD_ADDR_LEN * 2 && str_to_hex(conn_addr.addr, (uint8_t *)ptr)) {
	                ret = execute_cmd_evt_callback(CMD_BT_CONNECT_DEV, &conn_addr);
				}
				else {
					ret = CMD_INVALID_PARAM;
				}
			}
		}
	}
	else {
        ret = CMD_INVALID_PARAM;
    }

	return ret;
}

static int at_connect_param_handler(const char* match_string, const char* cmd_line)
{
	#define CONN_INTERVAL_MIN   0x06
	#define CONN_INTERVAL_MAX   0x0C80
	#define CONN_LATENCY_MAX    0x01FC 
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_CONN_PARAMS, &item, sizeof(item.conn_params))){
    		s_cmd_rsp_fmt_out("+%s:%d,%d,%d,%d", match_string, item.conn_params.conn_interval_min, item.conn_params.conn_interval_max,\
                item.conn_params.conn_latency, item.conn_params.sup_timemout);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t conn_interval_min = 0, conn_interval_max = 0, conn_latency = 0, sup_timemout = 0;
        if(sscanf(cmd_line, "%u,%u,%u,%u",  &conn_interval_min, &conn_interval_max, &conn_latency, &sup_timemout) == 4){
            ///不同芯片可支持的参数可能不同
            if(conn_interval_min < CONN_INTERVAL_MIN || conn_interval_max > CONN_INTERVAL_MAX ||
               conn_interval_min > conn_interval_max || conn_latency > CONN_LATENCY_MAX ||
               sup_timemout > 1000 || (conn_interval_max * (conn_latency + 1) * 3) > sup_timemout * 8)
                ret = CMD_INVALID_PARAM;
            else {
                item.conn_params.conn_interval_min = conn_interval_min;
                item.conn_params.conn_interval_max = conn_interval_max;
                item.conn_params.conn_latency = conn_latency;
                item.conn_params.sup_timemout = sup_timemout;
                ret = !!cfg_set(CFG_TYPE_CONN_PARAMS, &item.conn_params, sizeof(item.conn_params)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_BT_CONN_PARAMS, &item.conn_params);
                }
           }
        }
    }

	return ret;
}

static int at_disconnect_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    uint32_t conn_id;
	cmd_disconn_t disc_cid;
	if(*cmd_line++ == '='){
        if(strlen(cmd_line) == 4 && sscanf(cmd_line, "%u",  &conn_id) == 1 ){
            disc_cid.conn_id = conn_id;
            ret = execute_cmd_evt_callback(CMD_BT_DISCONN, &disc_cid);
        }
        else{
            ret = CMD_INVALID_PARAM;
        }
    }

	return ret;
}

static int at_conn_mtu_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cmd_conn_mtu_t conn_mtu = {0};
	if(*cmd_line == 0 || *cmd_line == '?'){
        ret = execute_cmd_evt_callback(CMD_BT_CONNMTU, &conn_mtu);
        if(ret == CMD_NO_ERR && conn_mtu.conn_n){
            int len = 0;
            char *str = NEW(64);
            if(str != NULL){
                len += sprintf(str+len,"+%s:",match_string);
                for(uint8_t i=0;i<conn_mtu.conn_n;i++)
                    len += sprintf(str+len, "%04x,%d;", conn_mtu.mtu[i].conhdl, conn_mtu.mtu[i].mtu); 
                str[len-1] = 0;
                s_cmd_rsp_fmt_out("%s", str);
                FREE(str);
            }
        }
        else 
            ret = CMD_STATUS_ERR;
    }
    
	return ret;
}



static int at_conn_dev_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cmd_conn_dev_t conn_dev = {0};
	if(*cmd_line == 0 || *cmd_line == '?'){
        ret = execute_cmd_evt_callback(CMD_BT_CONNDEV, &conn_dev);
        if(ret == CMD_NO_ERR && conn_dev.conn_n){
            int len = 0;
            char str[128];
            len += sprintf(str+len,"+%s:",match_string);
            for(uint8_t i=0;i<conn_dev.conn_n;i++)
                len += sprintf(str+len, "%04x,%02x%02x%02x%02x%02x%02x;", conn_dev.dev[i].conhdl, 
                                                                          conn_dev.dev[i].addr[5],
                                                                          conn_dev.dev[i].addr[4],
                                                                          conn_dev.dev[i].addr[3],
                                                                          conn_dev.dev[i].addr[2],
                                                                          conn_dev.dev[i].addr[1],
                                                                          conn_dev.dev[i].addr[0]); 
            str[len-1] = 0;
            s_cmd_rsp_fmt_out("%s", str);
        }
        else 
            ret = CMD_STATUS_ERR;
    }
    return ret;
}

static int at_get_rssi_hanlder(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    uint32_t rssi_cid;
    if(*cmd_line++ == '='){
        if(strlen(cmd_line) == 4 && sscanf(cmd_line, "000%u",  &rssi_cid) == 1 ){
            ret = execute_cmd_evt_callback(CMD_BT_RSSI_GET, &rssi_cid);
        }
        else{
            ret = CMD_INVALID_PARAM;
        }
    }
    
    return ret;
}

static int at_phypref_handler(const char* match_string, const char* cmd_line)
{
    #define PHY_MASK_ALL      0x07
    int ret = CMD_INVALID_FORMAT;
    cmd_phy_pref_t phy;
    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_reserver_get(PHYPREF_OFF, &phy, PHYPREF_LEN)){
            s_cmd_rsp_fmt_out("+%s:%d,%d", match_string, phy.tx_phys, phy.rx_phys);
            ret = CMD_NO_ERR;
        }
    }
    else if(*cmd_line++ == '='){
        uint32_t tx_phys, rx_phys;
        if(sscanf(cmd_line, "%u,%u", &tx_phys, &rx_phys) == 2){
            if(tx_phys > PHY_MASK_ALL || rx_phys > PHY_MASK_ALL) {
                ret = CMD_INVALID_PARAM;
            }
            else{
                phy.tx_phys = tx_phys & PHY_MASK_ALL;
                phy.rx_phys = rx_phys & PHY_MASK_ALL;
                ret = !!cfg_reserver_set(PHYPREF_OFF, &phy, PHYPREF_LEN) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_BT_PHYPREF, &phy);
                }
            }
        }
    }
    
    return ret;
}

static int at_pair_confirm_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_PAIR_REQ_CFM, &item, sizeof(item.pair_req_cfm))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.uart_fc);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t uart_fc = 0;
        if(sscanf(cmd_line, "%u",  &uart_fc) == 1){
            item.uart_fc = uart_fc;
            if(item.uart_fc <= 1){
                ret = !!cfg_set(CFG_TYPE_PAIR_REQ_CFM, &item.uart_fc, sizeof(item.uart_fc)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_PAIR_REQ_CFM_FEA, &item.uart_fc);
                }
            }
            else{
                ret = CMD_INVALID_PARAM;
            }
        }
    }

	return ret;
}

static int at_pair_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cmd_pair_req_reply_t req_replt;
    if(*cmd_line++ == '='){
        uint32_t conhdl, accept;
        if(sscanf(cmd_line, "%u,%u", &conhdl, &accept) == 2 && (accept < 2)){
            req_replt.conhdl = conhdl & 0xFFFF;
            req_replt.accept = accept & 0xFF;
            ret = execute_cmd_evt_callback(CMD_PAIR_REQ_REPLY, &req_replt);
        }
    }
    return ret;
}

static int at_iocap_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_IOCAP, &item, sizeof(item.iocap))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.iocap);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t iocap = 0;
        if(sscanf(cmd_line, "%u",  &iocap) == 1){
            item.iocap = iocap;
            if(item.iocap <= 4){
                ret = !!cfg_set(CFG_TYPE_IOCAP, &item.iocap, sizeof(item.iocap)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_PAIR_IOCAP, &item.iocap);
                }
            }
            else{
                ret = CMD_INVALID_PARAM;
            }
        }
    }

	return ret;
}

static int at_lesc_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	uint8_t le_sec = 0;

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_reserver_get(LESC_OFF, &le_sec, LESC_LEN)){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, le_sec);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t temp = 0;
        if(sscanf(cmd_line++, "%d", &temp) == 1 && *cmd_line == 0x00){
            if(temp <= TRUE){
                le_sec = temp & 0xFF;
                ret = !!cfg_reserver_set(LESC_OFF, &le_sec, LESC_LEN) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_PAIR_LESC, &le_sec);
                }
            }
            else{
                ret = CMD_INVALID_PARAM;
            }
        }
    }

	return ret;
}

static int at_passkey_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_PSK, &item, sizeof(item.passkey))){
    		s_cmd_rsp_fmt_out("+%s:%06d", match_string, item.passkey);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '=' && strlen(cmd_line) == 6){
        if(sscanf(cmd_line, "%u",  &item.passkey) == 1){
            if(item.passkey<=999999){
                ret = !!cfg_set(CFG_TYPE_PSK, &item.passkey, sizeof(item.passkey)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_PARI_PSK_SET, &item.passkey);
                }
            }
        }
    }

	return ret;
}

static int at_psk_entry_hanlder(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cmd_pair_passkey_entry_t psk_entry;
    if(*cmd_line++ == '='){
        uint32_t conhdl, passkey;
        if(sscanf(cmd_line, "%u,%u", &conhdl, &passkey) == 2 && (passkey <= 999999)){
            psk_entry.conhdl = conhdl & 0xFFFF;
            psk_entry.passkey = passkey;
            ret = execute_cmd_evt_callback(CMD_PAIR_PSK_ENTRY, &psk_entry);
        }
    }
    return ret;
}

static int at_number_confirm_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cmd_pair_numcf_reply_t numcf_reply;
    if(*cmd_line++ == '='){
        uint32_t conhdl, result;
        if(sscanf(cmd_line, "%u,%u", &conhdl, &result) == 2 && (result <= 1)){
            numcf_reply.conhdl = conhdl & 0xFFFF;
            numcf_reply.result = !!result;
            ret = execute_cmd_evt_callback(CMD_PAIR_NUMCF_RPL, &numcf_reply);
        }
    }
    return ret;
}

static int at_gmtu_handler(const char* match_string, const char* cmd_line)
{
    #define GMTU_MAX            515
    #define GMTU_MIN            23
    int ret = CMD_INVALID_FORMAT;
	cfg_general_t item = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_get(CFG_TYPE_GATT_MTU, &item, sizeof(item.mtu))){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, item.mtu);
			ret  = CMD_NO_ERR;
		}
    }
    else if(*cmd_line++ == '='){
        uint32_t mtu = 0;
        if(sscanf(cmd_line, "%u",  &mtu) == 1){
            if(mtu > GMTU_MAX || mtu < GMTU_MIN)
                ret = CMD_INVALID_PARAM;
            else {
                item.mtu = mtu;
                ret = !!cfg_set(CFG_TYPE_GATT_MTU, &item.mtu, sizeof(item.mtu)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(CMD_GATT_MTU, &item.mtu);
                }
            }
        }
    }

	return ret;
}

static int at_uuid_hanlder(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cmd_gatt_uuid_t gatt_uuid = {0};
    cfg_type_t type;
    cmd_change_evt_t evt;
    const char *str[8] = {"SSVCUUID","SWRTUUID","SNTFUUID","SFCUUID","CSVCUUID","CWRTUUID","CNTFUUID","CFCUUID"};

    for(uint8_t i=0;i<8;i++) {
        if(!memcmp(str[i], match_string, strlen(match_string))) {
            type = (cfg_type_t)(CFG_TYPE_GATTS_SVCUUID + i);
            evt = (cmd_change_evt_t)(CMD_GATTS_SVC + i);
            break;
        }
    }

    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(type, &gatt_uuid, sizeof(cmd_gatt_uuid_t))){
            if(2 == gatt_uuid.len || 4 == gatt_uuid.len || 16 == gatt_uuid.len){
                ret = CMD_NO_ERR;
                char uuid_str[33];
                int off = 0;
                for(int i = 0; i < gatt_uuid.len; ++i){
                    off += snprintf(uuid_str + off, 33 - off, "%02X", gatt_uuid.u.uuid128[gatt_uuid.len - 1 - i]);
                }
                s_cmd_rsp_fmt_out("+%s:%s", match_string, uuid_str);
            }
        }
    }
    else if(*cmd_line++ == '='){
        int uuid_str_len = strlen(cmd_line);
        if(4 == uuid_str_len || 8 == uuid_str_len || 32 == uuid_str_len){
            gatt_uuid.len = hex_str_2_rearray((char *)cmd_line, uuid_str_len, gatt_uuid.u.uuid128) & 0xFF;
            if(2 == gatt_uuid.len || 4 == gatt_uuid.len || 16 == gatt_uuid.len){
                ret = !!cfg_set(type, &gatt_uuid, sizeof(cmd_gatt_uuid_t)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
                    ret = execute_cmd_evt_callback(evt, &gatt_uuid);
                }
            }
        }
    }
    
    return ret;
}

static int at_authentication_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
	uint8_t  auth_level = {0};

	if(*cmd_line == 0 || *cmd_line == '?'){
		if(cfg_reserver_get(AUTH_OFF, &auth_level, AUTH_LEN)){
    		s_cmd_rsp_fmt_out("+%s:%d", match_string, auth_level);
			ret  = CMD_NO_ERR;
        }
    }
    else if(*cmd_line++ == '='){
        int32_t temp = 0;
        if(sscanf(cmd_line, "%d", &temp) == 1){
            if(temp <= 1 && temp >= 0 ){ //只保留安全等级0/1,
                auth_level = temp;
                ret = !!cfg_reserver_set(AUTH_OFF, &auth_level, AUTH_LEN) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if (ret == CMD_NO_ERR)
                {
                    ret = execute_cmd_evt_callback(CMD_LABEL_INFO, NULL);
                }
            }
            else
                ret = CMD_INVALID_PARAM;
        }
    }
    return ret;
}

static int at_label_info_handler(const char *match_string, const char *cmd_line)
{
    app_label_info_t info;
    int hardware_version;
    int device_code;
    int wake_period;
    int group;
    int temperature;
    int custom;
    int consumed = 0;

    if ((*cmd_line == 0) || ((*cmd_line == '?') && (cmd_line[1] == 0)))
    {
        app_label_info_get(&info);
        s_cmd_rsp_fmt_out("+%s:%u,%u,%u,%u,%d,%u", match_string,
            info.hardware_version, info.device_code, info.wake_period,
            info.group, info.temperature, info.custom);
        return CMD_NO_ERR;
    }
    if ((*cmd_line++ != '=') ||
        (sscanf(cmd_line, "%i,%i,%i,%i,%i,%i%n", &hardware_version,
                &device_code, &wake_period, &group, &temperature,
                &custom, &consumed) != 6) ||
        (cmd_line[consumed] != 0))
    {
        return CMD_INVALID_FORMAT;
    }
    if ((hardware_version < 0) || (hardware_version > 0xFF) ||
        (device_code < 0) || (device_code > 0xFFFF) ||
        (wake_period < 0) || (wake_period > 0xFF) ||
        (group < 0) || (group > 0xFFFF) ||
        (temperature < -127) || (temperature > 127) ||
        (custom < 0) || (custom > 0xFF))
    {
        return CMD_INVALID_PARAM;
    }

    info.hardware_version = (uint8_t)hardware_version;
    info.device_code = (uint16_t)device_code;
    info.wake_period = (uint8_t)wake_period;
    info.group = (uint16_t)group;
    info.temperature = (int8_t)temperature;
    info.custom = (uint8_t)custom;
    if (!app_label_info_set(&info))
    {
        return CMD_RUNING_FAILED;
    }
    return execute_cmd_evt_callback(CMD_LABEL_INFO, &info);
}

static int at_image_handler(const char *match_string, const char *cmd_line)
{
    char response[APP_IMAGE_AT_RESPONSE_SIZE];
    app_image_at_result_t result;

    result = app_image_at_execute(s_cmd_uart_source, match_string, cmd_line,
                                  response, sizeof(response));
    if (result == APP_IMAGE_AT_OK)
    {
        s_cmd_rsp_fmt_out("%s", response);
    }
    return (int)result;
}

static int at_cmd_factory_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if(*cmd_line == 0 ){
        ret = execute_cmd_evt_callback(CMD_FACTORYRESET, NULL);
    }
    return ret;
}

static int at_cmd_clear_linkkey_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if(*cmd_line == 0){
        ret = execute_cmd_evt_callback(CMD_CLEARLINKKEY, NULL);
    }
    return ret;
}

static int at_cmd_kb_report_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    if(*cmd_line++ == '='){
        uint16_t conhdl = 0xFF;
        
        char *token;
        char *rest = (char *)cmd_line; // 剩余字符串指针
        uint8_t count = 0;
        uint8_t keydata[8] = {0};
        // 1. 解析第一个参数（十进制）
        token = strtok(rest, ",");
        if (!token) return ret;
        conhdl = atoi(token);
        if(conhdl > BLE_MAX_CONN) return ret;
        // 2. 解析后续参数（十六进制）   
        while ((token = strtok(NULL, ",")) != NULL) {
            // 检查十六进制字符串长度（1-2字符）
            size_t len = strlen(token);
            if (len == 0 || len > 2) return ret;

            // 转换为十六进制字节
            char *endptr;
            int val = strtol(token, &endptr, 16);
            if (*endptr != '\0' || val < 0 || val > 0xFF) {
                return ret; // 非法字符或超限
            }
            keydata[count++] = (uint8_t)val;
        }
		if (count >= 7 && count == 0){
            return ret; // 参数个数异常
        } 
        memmove(&keydata[2],&keydata[1],sizeof(uint8_t)*6);//调整KEYBOARD REPORT格式
        keydata[1] = 0;

        cmd_hogp_rpt_t cmd_in_rpt= {0};
        cmd_in_rpt.conhdl = conhdl;
        cmd_in_rpt.in_rpt.rpt_id = 1;//APP_HID_KEYBOARD_REPORT_ID
        cmd_in_rpt.in_rpt.rpt_sz = 8;
        cmd_in_rpt.in_rpt.rpt = keydata;
        ret = execute_cmd_evt_callback(CMD_HOGP_KB_RPT, &cmd_in_rpt);
    }
    return ret;
}

static int at_cmd_func_report_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    if(*cmd_line++ == '='){
        int conhdl = 0xFF;
        uint32_t consumer_key;
		if(sscanf(cmd_line, "%d,%x", &conhdl,&consumer_key) == 2) {//fix me
            if(conhdl <= BLE_MAX_CONN && conhdl >= 0){
                uint8_t keydata[2] = {0};
                cmd_hogp_rpt_t cmd_in_rpt= {0};
                cmd_in_rpt.conhdl = conhdl;
                cmd_in_rpt.in_rpt.rpt_id = 2;//APP_HID_CONSUMER_REPORT_ID
                cmd_in_rpt.in_rpt.rpt_sz = 2;
                keydata[1] = (consumer_key >> 8) & 0xFF;
                keydata[0] = consumer_key & 0xFF;
                cmd_in_rpt.in_rpt.rpt = keydata;

                ret = execute_cmd_evt_callback(CMD_HOGP_FUNC_RPT, &cmd_in_rpt);
              }
              else{
                ret = CMD_INVALID_PARAM;
              }
        }
    }
    return ret;
}

static int at_cmd_btmode_hanlder(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;

    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(CFG_TYPE_BTMODE, &item, sizeof(item.btmode))){
            s_cmd_rsp_fmt_out("+%s:%d", match_string, item.btmode);
            ret = CMD_NO_ERR;
        }
    }
    else if(*cmd_line++ == '='){
        uint32_t btmode;
		if(sscanf(cmd_line ,"%u",  &btmode) == 1) //fix me
			item.btmode = btmode & 0xFF;
            if(item.btmode == 6) ///dtm
                ret = execute_cmd_evt_callback(CMD_BT_MODE, &item.btmode);  
            else if(btmode <= 2){ /// s/m/ms
            	ret = !!cfg_set(CFG_TYPE_BTMODE, &item.btmode, sizeof(&item.btmode)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
            }
            else
                ret = CMD_INVALID_PARAM;
    }
    
    return ret;
}

#ifdef CFG_ADV_LED
static int at_cmd_advled_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    advled_t advled_par = {0};
    if(*cmd_line == 0 || *cmd_line == '?'){
        ret = execute_cmd_evt_callback(CMD_ADVLED_GET, &advled_par);
        s_cmd_rsp_fmt_out("+%s:%d,%d", match_string, advled_par.enable, advled_par.period);
    }
    else if(*cmd_line++ == '='){
        uint32_t enable,period;
        if(sscanf(cmd_line, "%u,%u", &enable,&period) == 2) {
            if((enable == 1 && period >= 100 && period <=1000) || enable == 0)
            {
                advled_par.enable = enable;
                advled_par.period = period;
                ret = execute_cmd_evt_callback(CMD_ADVLED_SET, &advled_par);
            }
            else
            {
                ret = CMD_INVALID_PARAM;
            }
        }
    }
    return ret;
}
#endif

static int at_cmd_trim_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    if(*cmd_line == 0 || *cmd_line == '?'){
        s_cmd_rsp_fmt_out("+%s:%d", match_string, br_radio_freq_offset_get());
        ret = CMD_NO_ERR;
    }      
    else if(*cmd_line++ == '='){
        uint32_t trim = 0;
		if(sscanf(cmd_line, "%u", &trim) == 1) {//fix me
            if(br_radio_freq_offset_set(trim) == true){
                ret = CMD_NO_ERR;
            }
        }
    }
    return ret;
}

static int at_cmd_txpower_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;
    
    if(*cmd_line == 0 || *cmd_line == '?'){
        if(cfg_get(CFG_TYPE_TXPOWER, &item.txpower, sizeof(item.txpower))){
            s_cmd_rsp_fmt_out("+%s:%d", match_string, item.txpower);
            ret = CMD_NO_ERR;
         }
    }      
    else if ((*(cmd_line++) == '=') && strlen(cmd_line)) {
        int32_t tx_power = 0;
		if(sscanf(cmd_line, "%d", &tx_power) == 1) {//fix me
		    if(tx_power > TX_POWER_LEVEL_MAX || tx_power < 0)
                ret = CMD_INVALID_PARAM;
            else {
    			item.txpower = tx_power & 0xFF;
                ret = !!cfg_set(CFG_TYPE_TXPOWER, &item.txpower, sizeof(item.txpower)) ? CMD_NO_ERR : CMD_RUNING_FAILED;
                if(ret == CMD_NO_ERR){
    	            ret = execute_cmd_evt_callback(CMD_TEST_TXPOWER, &item.txpower);
    			}
            }
        }
    }
    return ret;
  
}


static int at_cmd_freq_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    if (*cmd_line++ == '=') {
        uint32_t ch,time;
        if(sscanf(cmd_line, "%u,%u", &ch, &time) == 2 && (ch <= 2480) && (ch >= 2402) && (ch % 2 == 0) && (time > 0)){
            cmd_test_freq_t freq;
            freq.ch = ch;
            freq.time = time;
            ret = execute_cmd_evt_callback(CMD_TEST_FREQ, &freq);
       }
    }
    return ret;
}

static int at_dtm_handle(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    cfg_general_t item;
    if (*cmd_line == 0) {
        item.btmode = 6;
        ret = execute_cmd_evt_callback(CMD_BT_MODE, &item.btmode);
    }
    return ret;
}

static int at_burn_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;

    if (*cmd_line++ == '=') {
        uint32_t len;
        if(sscanf(cmd_line, "%u", &len) == 1 && (len %4 == 0) && len <= 256*1024){
            uint32_t crc = br_calculate_crc((uint8_t *)0x1000000, len);
            s_cmd_rsp_fmt_out("+%s:%x", match_string, crc);
            ret = CMD_NO_ERR;
        }
    }
    return ret;
}

static int at_getram_handler(const char* match_string, const char* cmd_line)
{
    int ret = CMD_INVALID_FORMAT;
    
    if (*cmd_line++ == '=') {
        uint32_t data_addr,data_len;
        if(sscanf(cmd_line, "%x,%d", &data_addr, &data_len) == 2 && data_len > 0 && data_len <= 64){
            if((data_addr >= FLASH_START_ADDR && data_addr+data_len <= FLASH_END_ADDR) 
                || (data_addr >= 0x20000000 && data_addr+data_len <= 0x2000C000)) {
                uint8_t str[256] = {0};
                uint8_t buf[64] = {0};
                if(data_addr >= FLASH_START_ADDR && data_addr+data_len <= FLASH_END_ADDR) {
                    hal_flash_read(data_addr, buf, data_len);
                }
                else {
                    memcpy(buf, (uint8_t *)data_addr, data_len);
                }
                uint16_t str_len = 0;
                str_len += sprintf((char *)str+str_len, "+%s:0x%08x=", match_string, data_addr); 
                for(uint8_t i =0;i<data_len;i++) {
                    str_len += sprintf((char *)str+str_len, "%02X,", buf[i]); 
                }
                str[--str_len] = 0;
                s_cmd_rsp_fmt_out("%s", str);
                ret = CMD_NO_ERR;
            }
        }
    }
    return ret;    
}

static int at_cmd_send_hanlder(uint8_t cid, const uint8_t *buf, uint16_t len)
{
	cmd_data_send_t item;
	
	item.cid = cid;
	item.data = buf;
	item.length = len;
	
	return execute_cmd_evt_callback(CMD_DATA_SEND, &item);
}

void cmd_callback_set(cmd_evt_callback_t fn)
{
    s_cmd_evt_callback = fn;
}

void cmd_rsp_out_set(cmd_rsp_fmt_out_t fn)
{
    s_cmd_rsp_fmt_out = fn;
}

void cmd_pkt_done_set(cmd_pkt_proc_done_t fn)
{
    s_cmd_pkt_done_ck = fn;
}

const cmd_info_t cmd_table[] = {
    /*本地设备信息修改*/
    {"GVER",        at_ver_handler},
    {"GFWVER",      at_fw_ver_handler},
    {"GPRTVER",     at_pre_ver_handler},
    {"BAUD",        at_baudrate_handler},
    {"FLOWCTRL",    at_flowctrl_handler},
    {"SILENT",      at_silent_handler},
    {"MULTICONN",   at_multiconn_handler},
    {"COMMAND",     at_command_handler},
    {"AUTOUNLOCK",  at_autounlock_handler},
    {"TXDELAY",     at_txdelay_handler},
    {"WAKEACT",     at_wakeact_handler},
    {"WLST",        at_whitelist_handler},
    {"DWLST",       at_del_whitelist_handler},
    {"RESET",       at_reset_handler},
    {"BTMODE",		at_cmd_btmode_hanlder}, 
#ifdef CFG_ADV_LED
    {"ADVLED",       at_cmd_advled_handler},
#endif
    /*蓝牙信息修改*/
    /*----GAP----*/
    {"NAME",        at_name_handler},
    {"LBDADDR",     at_addr_handler},
    {"ADVPARAM",    at_adv_param_handler},
    {"ADVDATA",     at_adv_data_handler},
    {"ADV",         at_adv_handler},
    {"SRDATA",      at_scan_rsp_data_handler},
    {"SCANPARAM",   at_scan_param_handler}, 
    {"SCANKW",      at_scankw_handler},
    {"SCAN",        at_scan_handler},
    {"CONNECT",     at_connect_dev_handler},
    {"CONNPARAM",   at_connect_param_handler},
    {"DISCONN",     at_disconnect_handler},
    {"CONNMTU",     at_conn_mtu_handler},
    {"CONNDEV",     at_conn_dev_handler},
    {"GRSSI",       at_get_rssi_hanlder},
    {"PHYPREF",     at_phypref_handler},
    /*----SMP----*/
    {"PAIRCONFIRM", at_pair_confirm_handler},
    {"PAIR",        at_pair_handler},
    {"IOCAP",       at_iocap_handler},
    {"LESC",        at_lesc_handler},
    {"PASSKEY",     at_passkey_handler},
    {"PSKENTRY",    at_psk_entry_hanlder},
    {"NUMCF",       at_number_confirm_handler},
    /*----GATT----*/
    {"GMTU",        at_gmtu_handler},
    {"SSVCUUID",    at_uuid_hanlder},
    {"SWRTUUID",    at_uuid_hanlder},
    {"SNTFUUID",    at_uuid_hanlder},
    {"SFCUUID",     at_uuid_hanlder},
    {"CSVCUUID",    at_uuid_hanlder},
    {"CWRTUUID",    at_uuid_hanlder},
    {"CNTFUUID",    at_uuid_hanlder},
    {"CFCUUID",     at_uuid_hanlder},
    {"AUTH",        at_authentication_handler},///fixme
    {"LABELINFO",   at_label_info_handler},
    {"FACTORYRESET",at_cmd_factory_handler},
    {"CLEARLINKKEY",at_cmd_clear_linkkey_handler},
    {"IMGINFO",     at_image_handler},
    {"IMGREAD",     at_image_handler},
    /*----HOGP----*/
    {"KBRPT",       at_cmd_kb_report_handle},
    {"FUNCRPT",     at_cmd_func_report_handle},
	/*----Production test----*/
    {"TRIM",        at_cmd_trim_handle},
    {"TXPOWER",     at_cmd_txpower_handle},
    {"FREQ",        at_cmd_freq_handle},
    {"BOARD",       at_ver_handler},
    {"DTM",         at_dtm_handle},
    {"BURN",        at_burn_handler},
    {"GETRAM",      at_getram_handler},
};

typedef enum{
    SEND_UNMATCH = 0,
    CMD_WAIT_CID,
    CMD_WAIT_LEN,
    CMD_WAIT_PAYLOAD
}send_cmd_status_t;

static struct{
    uint16_t off;
    uint8_t *data;
}m_send_buf;

uint32_t cmd_parse(const uint8_t* buf, uint32_t len)
{
    #define MAX_UART_DATA_LEN    530 //AT+SEND=000x,512,xxxx\r 18+512
    static char data_array[MAX_UART_DATA_LEN];
    static uint16_t index = 0;
    static send_cmd_status_t send_cmd = SEND_UNMATCH;
    static int send_cid, send_len;
    uint32_t data_len = 0;
	uint32_t at_send_flag = 0;

    for (; data_len < len; data_len++) {
        char cCurr = buf[data_len];
        if (index == 1 && cCurr != cmd_prefix[1]) {  
            index = 0;
        }
        if (index == 0 && cCurr != cmd_prefix[0]) {
			continue;
		}
        if(send_cmd == CMD_WAIT_PAYLOAD)
		{
			m_send_buf.data[m_send_buf.off++] = cCurr;
			if(m_send_buf.off == send_len + 1 && cCurr == '\r')
			{
                if(at_cmd_send_hanlder(send_cid, m_send_buf.data, send_len) == CMD_NO_ERR){
					at_send_flag = 1;
                    s_cmd_rsp_fmt_out("OK");
                }
                else{
                    s_cmd_rsp_fmt_out("ERROR:%u", CMD_STATUS_ERR);
                }
				index = 0;
				send_cmd = SEND_UNMATCH;
				m_send_buf.off = 0;
                break;
			}
			else if(m_send_buf.off > send_len)
			{
				index = 0;
				send_cmd = SEND_UNMATCH;
				m_send_buf.off = 0;
				s_cmd_rsp_fmt_out("ERROR:%u", CMD_INVALID_FORMAT);
			}
			continue;
		}
        else if ( index < MAX_UART_DATA_LEN)
		{
			data_array[index ++] = cCurr;
			if(send_cmd == SEND_UNMATCH && index == 7 && memcmp(data_array, "AT+SEND", 7) == 0)
			{
				send_cmd = CMD_WAIT_CID;
			}
			else if(send_cmd == CMD_WAIT_CID && cCurr == ',')
			{
				send_cmd = CMD_WAIT_LEN;
			}
			else if(send_cmd == CMD_WAIT_LEN && cCurr == ',')
			{
			    ///fixme，Handle range 0000-0003
				if(sscanf(data_array, "AT+SEND=000%1d,%d,", &send_cid, &send_len) == 2 && send_len <= 512) 
				{
					m_send_buf.off = 0;
                    m_send_buf.data = (uint8_t *)&data_array[index];
					send_cmd = CMD_WAIT_PAYLOAD;
				}
				else
				{
					send_cmd = SEND_UNMATCH;
					index = 0;
                    s_cmd_rsp_fmt_out("ERROR:%u", CMD_INVALID_FORMAT);
				}
			}
		}
		else
		{
			memset(data_array,0,MAX_UART_DATA_LEN);
			index = 0;
			continue;
		}
		if(send_cmd == SEND_UNMATCH && cCurr == '\r')
		{
			data_array[index-1] = 0;
			int ret = CMD_UNMATCH;
            if(memcmp(data_array, cmd_prefix, strlen(cmd_prefix)) == 0){
                for(int i = 0; i < (sizeof(cmd_table) / sizeof(cmd_info_t)); ++i){
                    uint32_t at_len = strlen(cmd_table[i].cmd_string);
                    if(memcmp(cmd_table[i].cmd_string, data_array + 3, at_len) == 0)
                    {
                        ret = cmd_table[i].cmd_proccesor(cmd_table[i].cmd_string, (const char*)data_array + 3 + at_len);
                        break;
                    }
                }
            }
            memset(data_array, 0, index);
			index = 0;
			if(ret == CMD_NO_ERR)
            {
                s_cmd_rsp_fmt_out("OK");
            }
            else
            {
                s_cmd_rsp_fmt_out("ERROR:%u", ret);
            }
		}
    }
    if(s_cmd_pkt_done_ck && !at_send_flag) ///没有发送at+send
        s_cmd_pkt_done_ck();

    if(data_len >= len)
        return len;
    else 
       return data_len+ 1; 
}

uint32_t cmd_parse_ble(const uint8_t *buf, uint32_t len)
{
    uint32_t consumed;

    s_cmd_uart_source = false;
    consumed = cmd_parse(buf, len);
    s_cmd_uart_source = true;
    return consumed;
}
