/**
 * @file cat1_time_sync.c
 * @brief CAT1 AT+CCLK? 网络时间锚定，按需推算 YYMMDDhhmmss BCD。
 */
#include "cat1_time_sync.h"
#include "co_printf.h"
#include "br_timer.h"

#include <string.h>

static uint8_t  s_valid;
static uint8_t  s_from_cclk; /* 1=CCLK 锚点，备选源不得覆盖 */
static uint32_t s_anchor_sec;
static uint32_t s_anchor_ms;

static uint32_t cat1_time_ms_elapsed(uint32_t since_ms)
{
	uint32_t now_ms = sdk_timer_get_current_ms();

	if (now_ms >= since_ms) {
		return now_ms - since_ms;
	}
	return (0xFFFFFFFFu - since_ms) + now_ms + 1u;
}

static uint8_t cat1_time_is_leap(uint16_t year)
{
	return ((year % 4u) == 0u) ? 1u : 0u;
}

static uint8_t cat1_time_days_in_month(uint16_t year, uint8_t month)
{
	static const uint8_t dim[12] = { 31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u };

	if (month < 1u || month > 12u) {
		return 0u;
	}
	if (month == 2u && cat1_time_is_leap(year) != 0u) {
		return 29u;
	}
	return dim[month - 1u];
}

static int cat1_time_ymd_in_range(uint16_t year, uint8_t month, uint8_t day,
                                  uint8_t hour, uint8_t min, uint8_t sec)
{
	if (year < 2020u || year > 2099u) {
		return -1;
	}
	if (month < 1u || month > 12u) {
		return -1;
	}
	if (day < 1u || day > cat1_time_days_in_month(year, month)) {
		return -1;
	}
	if (hour > 23u || min > 59u || sec > 59u) {
		return -1;
	}
	return 0;
}

static uint32_t cat1_time_ymdhms_to_sec2000(uint16_t year, uint8_t month, uint8_t day,
                                            uint8_t hour, uint8_t min, uint8_t sec)
{
	uint32_t days = 0u;
	uint16_t y;
	uint8_t m;

	for (y = 2000u; y < year; y++) {
		days += (cat1_time_is_leap(y) != 0u) ? 366u : 365u;
	}
	for (m = 1u; m < month; m++) {
		days += (uint32_t)cat1_time_days_in_month(year, m);
	}
	days += (uint32_t)(day - 1u);
	return (days * 86400u) + ((uint32_t)hour * 3600u) + ((uint32_t)min * 60u) + sec;
}

static void cat1_time_byte_to_bcd(uint8_t value, uint8_t *out)
{
	*out = (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static void cat1_time_sec2000_to_bcd6(uint32_t sec, uint8_t out[6])
{
	uint32_t days = sec / 86400u;
	uint32_t rem = sec % 86400u;
	uint8_t hour = (uint8_t)(rem / 3600u);
	uint8_t min;
	uint8_t second;
	uint16_t year = 2000u;
	uint8_t month = 1u;
	uint32_t year_days;
	uint8_t dim;

	rem %= 3600u;
	min = (uint8_t)(rem / 60u);
	second = (uint8_t)(rem % 60u);

	while (year <= 2099u) {
		year_days = (cat1_time_is_leap(year) != 0u) ? 366u : 365u;
		if (days < year_days) {
			break;
		}
		days -= year_days;
		year++;
	}

	while (month <= 12u) {
		dim = cat1_time_days_in_month(year, month);
		if (days < dim) {
			break;
		}
		days -= dim;
		month++;
	}

	cat1_time_byte_to_bcd((uint8_t)(year % 100u), &out[0]);
	cat1_time_byte_to_bcd(month, &out[1]);
	cat1_time_byte_to_bcd((uint8_t)(days + 1u), &out[2]);
	cat1_time_byte_to_bcd(hour, &out[3]);
	cat1_time_byte_to_bcd(min, &out[4]);
	cat1_time_byte_to_bcd(second, &out[5]);
}

static int cat1_time_parse_digit(const char **p)
{
	if (p == NULL || *p == NULL || **p < '0' || **p > '9') {
		return -1;
	}
	return (int)(*(*p)++ - '0');
}

static int cat1_time_parse_2digits(const char **p)
{
	int hi;
	int lo;

	hi = cat1_time_parse_digit(p);
	if (hi < 0) {
		return -1;
	}
	lo = cat1_time_parse_digit(p);
	if (lo < 0) {
		return -1;
	}
	return hi * 10 + lo;
}

static int cat1_time_parse_4digits(const char **p)
{
	int v;
	int d;

	v = 0;
	for (d = 0; d < 4; d++) {
		int digit = cat1_time_parse_digit(p);
		if (digit < 0) {
			return -1;
		}
		v = v * 10 + digit;
	}
	return v;
}

static int cat1_time_expect_char(const char **p, char ch)
{
	if (p == NULL || *p == NULL || **p != ch) {
		return -1;
	}
	(*p)++;
	return 0;
}

void cat1_time_sync_reset(void)
{
	s_valid = 0u;
	s_from_cclk = 0u;
	s_anchor_sec = 0u;
	s_anchor_ms = 0u;
}

static int cat1_time_bcd_byte(uint8_t b)
{
	uint8_t hi = (uint8_t)(b >> 4);
	uint8_t lo = (uint8_t)(b & 0x0Fu);

	if (hi > 9u || lo > 9u) {
		return -1;
	}
	return (int)(hi * 10u + lo);
}

int cat1_time_sync_from_cclk_rsp(const char *rsp)
{
	const char *p;
	uint16_t year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
	uint32_t sec2000;

	if (rsp == NULL) {
		return -1;
	}

	p = strstr(rsp, "+CCLK:");
	if (p == NULL) {
		return -1;
	}
	p = strchr(p, '"');
	if (p == NULL) {
		return -1;
	}
	p++;

	year = (uint16_t)cat1_time_parse_4digits(&p);
	if (cat1_time_expect_char(&p, '/') != 0) {
		return -1;
	}
	month = cat1_time_parse_2digits(&p);
	if (cat1_time_expect_char(&p, '/') != 0) {
		return -1;
	}
	day = cat1_time_parse_2digits(&p);
	if (cat1_time_expect_char(&p, ',') != 0) {
		return -1;
	}
	hour = cat1_time_parse_2digits(&p);
	if (cat1_time_expect_char(&p, ':') != 0) {
		return -1;
	}
	min = cat1_time_parse_2digits(&p);
	if (cat1_time_expect_char(&p, ':') != 0) {
		return -1;
	}
	sec = cat1_time_parse_2digits(&p);

	if (month < 0 || day < 0 || hour < 0 || min < 0 || sec < 0) {
		return -1;
	}
	if (cat1_time_ymd_in_range(year, (uint8_t)month, (uint8_t)day,
	                           (uint8_t)hour, (uint8_t)min, (uint8_t)sec) != 0) {
		return -1;
	}

	sec2000 = cat1_time_ymdhms_to_sec2000(year, (uint8_t)month, (uint8_t)day,
	                                      (uint8_t)hour, (uint8_t)min, (uint8_t)sec);
	s_anchor_sec = sec2000;
	s_anchor_ms = sdk_timer_get_current_ms();
	s_valid = 1u;
	s_from_cclk = 1u;
	return 0;
}

int cat1_time_sync_from_bcd6_fallback(const uint8_t bcd[6], int8_t local_tz_hours)
{
	int yy;
	int month;
	int day;
	int hour;
	int min;
	int sec;
	uint16_t year;
	uint32_t sec2000;
	int32_t tz_sec;

	if (bcd == NULL) {
		return -1;
	}
	if (s_from_cclk != 0u) {
		return 1;
	}

	yy = cat1_time_bcd_byte(bcd[0]);
	month = cat1_time_bcd_byte(bcd[1]);
	day = cat1_time_bcd_byte(bcd[2]);
	hour = cat1_time_bcd_byte(bcd[3]);
	min = cat1_time_bcd_byte(bcd[4]);
	sec = cat1_time_bcd_byte(bcd[5]);
	if (yy < 0 || month < 0 || day < 0 || hour < 0 || min < 0 || sec < 0) {
		return -1;
	}

	year = (uint16_t)(2000u + (uint16_t)yy);
	if (cat1_time_ymd_in_range(year, (uint8_t)month, (uint8_t)day,
	                           (uint8_t)hour, (uint8_t)min, (uint8_t)sec) != 0) {
		return -1;
	}

	sec2000 = cat1_time_ymdhms_to_sec2000(year, (uint8_t)month, (uint8_t)day,
	                                      (uint8_t)hour, (uint8_t)min, (uint8_t)sec);
	/* Tag11 等帧头为当地时间：换算为与 CCLK 同基准后再锚定 */
	tz_sec = (int32_t)local_tz_hours * 3600;
	if (tz_sec > 0) {
		uint32_t sub = (uint32_t)tz_sec;

		if (sec2000 < sub) {
			return -1;
		}
		sec2000 -= sub;
	} else if (tz_sec < 0) {
		sec2000 += (uint32_t)(-tz_sec);
	}

	s_anchor_sec = sec2000;
	s_anchor_ms = sdk_timer_get_current_ms();
	{
		uint8_t first = (s_valid == 0u);

		s_valid = 1u;
		s_from_cclk = 0u;
		return (first != 0u) ? 0 : 2;
	}
}

int cat1_time_get_bcd6(uint8_t out[6])
{
	uint32_t now_sec;

	if (out == NULL || s_valid == 0u) {
		return -1;
	}

	now_sec = s_anchor_sec + (cat1_time_ms_elapsed(s_anchor_ms) / 1000u);
	cat1_time_sec2000_to_bcd6(now_sec, out);
	return 0;
}

int cat1_time_get_bcd6_offset(uint8_t out[6], int8_t offset_hours)
{
	uint32_t now_sec;
	int32_t offset_sec;

	if (out == NULL || s_valid == 0u) {
		/* 锚点无效(CCLK 未同步成功): 调用方回退占位时间 */
		return -1;
	}

	now_sec = s_anchor_sec + (cat1_time_ms_elapsed(s_anchor_ms) / 1000u);
	offset_sec = (int32_t)offset_hours * 3600;
	if (offset_sec >= 0) {
		now_sec += (uint32_t)offset_sec;
	} else {
		uint32_t sub = (uint32_t)(-offset_sec);

		if (now_sec < sub) {
			now_sec = 0u;
		} else {
			now_sec -= sub;
		}
	}
	cat1_time_sec2000_to_bcd6(now_sec, out);
	return 0;
}
