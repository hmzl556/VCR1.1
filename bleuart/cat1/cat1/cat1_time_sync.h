#ifndef CAT1_TIME_SYNC_H
#define CAT1_TIME_SYNC_H

#include <stdint.h>

/** 清除对时锚点（valid=0）；仅全量初始化调用，UDP rebuild/setup 重试不得调用 */
void cat1_time_sync_reset(void);

/**
 * @brief  从 AT+CCLK? 应答解析网络时间并建立锚点
 * @param  rsp  模组应答，如 +CCLK: "2026/07/08,10:31:35+32"
 * @return 0 成功；-1 失败
 */
int cat1_time_sync_from_cclk_rsp(const char *rsp);

/**
 * @brief  CCLK 未成功时，用 Tag11 等下行帧头 YYMMDDhhmmss BCD 作备选锚点
 * @param  bcd              6 字节 BCD 时间
 * @param  local_tz_hours   入参 BCD 相对 CCLK 基准的时区（东八区填 8）；会换算后再锚定，
 *                          使后续 get_bcd6_offset(+8) 与 CCLK 路径一致
 * @return 0 首次锚定；1 已有 CCLK 锚点跳过；2 刷新备选锚点；-1 失败
 */
int cat1_time_sync_from_bcd6_fallback(const uint8_t bcd[6], int8_t local_tz_hours);

/**
 * @brief  获取当前墙钟 YYMMDDhhmmss BCD（6 字节）
 * @return 0 成功；-1 未对时
 */
int cat1_time_get_bcd6(uint8_t out[6]);

/**
 * @brief  获取当前墙钟 BCD，并按小时偏移（跨日自动进位）
 * @param  offset_hours  小时偏移，如东八区填 8
 * @return 0 成功；-1 未对时
 */
int cat1_time_get_bcd6_offset(uint8_t out[6], int8_t offset_hours);

#endif /* CAT1_TIME_SYNC_H */
