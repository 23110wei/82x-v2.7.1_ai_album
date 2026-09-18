#include "system/ai_album_lunar.h"

#include "basic_include.h"

/*
 * 农历数据表(1900~2049),每项编码一年:
 *   bit[3:0]   闰月月份(0=无闰月)
 *   bit[16]    闰月大小(1=30天)
 *   bit[15:4]  正月至十二月大小月(1=30天,0x8000=正月)
 * 春节锚点验证:1900-01-31为1900年正月初一。
 */

#define AI_ALBUM_LUNAR_MIN_YEAR 1900
#define AI_ALBUM_LUNAR_MAX_YEAR 2049

static const uint32 lunar_table[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
};

static uint32 lunar_leap_month(uint32 year)
{
    return lunar_table[year - AI_ALBUM_LUNAR_MIN_YEAR] & 0xf;
}

static uint32 lunar_leap_days(uint32 year)
{
    uint32 data = lunar_table[year - AI_ALBUM_LUNAR_MIN_YEAR];

    if (!(data & 0xf)) {
        return 0;
    }
    return (data & 0x10000) ? 30 : 29;
}

static uint32 lunar_month_days(uint32 year, uint32 month)
{
    uint32 data = lunar_table[year - AI_ALBUM_LUNAR_MIN_YEAR];
    return (data & (0x8000 >> (month - 1))) ? 30 : 29;
}

static uint32 lunar_year_days(uint32 year)
{
    uint32 data = lunar_table[year - AI_ALBUM_LUNAR_MIN_YEAR];
    uint32 days = 348; /* 12 x 29 */
    uint32 bit;

    for (bit = 0x8000; bit > 0x8; bit >>= 1) {
        days += (data & bit) ? 1 : 0;
    }
    return days + lunar_leap_days(year);
}

/* 基于儒略日数的公历日期->天数(Howard Hinnant算法,1900+年恒正) */
static int32 days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int32 era = y / 400;
    int32 yoe = y - era * 400;                               /* [0, 399] */
    int32 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int ai_album_lunar_convert(int gy, int gm, int gd,
                           ai_album_lunar_date_t *out)
{
    ai_album_lunar_date_t lunar;
    int32 offset;
    uint32 year;
    uint32 month;
    uint32 leap;
    uint8 is_leap;
    uint32 days;

    if (out == NULL || gm < 1 || gm > 12 || gd < 1 || gd > 31) {
        return 0;
    }
    offset = days_from_civil(gy, gm, gd) - days_from_civil(1900, 1, 31);
    if (offset < 0) {
        return 0;
    }

    year = AI_ALBUM_LUNAR_MIN_YEAR;
    for (;;) {
        days = lunar_year_days(year);
        if (offset < (int32)days || year >= AI_ALBUM_LUNAR_MAX_YEAR) {
            break;
        }
        offset -= days;
        year++;
    }
    if (offset >= (int32)days) {
        return 0; /* 超出2049年 */
    }

    leap = lunar_leap_month(year);
    month = 1;
    is_leap = 0;
    for (;;) {
        days = is_leap ? lunar_leap_days(year)
                       : lunar_month_days(year, month);
        if (offset < (int32)days) {
            break;
        }
        offset -= days;
        if (leap && month == leap && !is_leap) {
            is_leap = 1;
        } else {
            month++;
            is_leap = 0;
        }
    }

    lunar.year = (uint16)year;
    lunar.month = (uint8)month;
    lunar.day = (uint8)(offset + 1);
    lunar.leap = is_leap;
    *out = lunar;
    return 1;
}

static const char *const lunar_stems[] = {
    "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸",
};
static const char *const lunar_branches[] = {
    "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥",
};
static const char *const lunar_zodiac[] = {
    "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪",
};
static const char *const lunar_month_names[] = {
    "正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊",
};

static const char *lunar_day_name(uint32 day)
{
    static const char *const names[] = {
        "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
        "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
        "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十",
    };
    if (day < 1 || day > 30) {
        return "";
    }
    return names[day - 1];
}

int ai_album_lunar_format_chinese(int gy, int gm, int gd,
                                  char *out, uint32 out_size)
{
    ai_album_lunar_date_t lunar;
    uint32 stem;
    uint32 branch;

    if (out == NULL || out_size == 0) {
        return 0;
    }
    if (!ai_album_lunar_convert(gy, gm, gd, &lunar)) {
        os_snprintf(out, out_size, "--");
        return 0;
    }
    stem = (lunar.year - 4) % 10;
    branch = (lunar.year - 4) % 12;
    os_snprintf(out, out_size, "农历%s%s%s年 %s%s月%s",
                lunar_stems[stem], lunar_branches[branch],
                lunar_zodiac[branch],
                lunar.leap ? "闰" : "",
                lunar_month_names[lunar.month - 1],
                lunar_day_name(lunar.day));
    return 1;
}

int ai_album_lunar_format_english(int gy, int gm, int gd,
                                  char *out, uint32 out_size)
{
    ai_album_lunar_date_t lunar;

    if (out == NULL || out_size == 0) {
        return 0;
    }
    if (!ai_album_lunar_convert(gy, gm, gd, &lunar)) {
        os_snprintf(out, out_size, "--");
        return 0;
    }
    os_snprintf(out, out_size, "LUNAR %s%u/%u",
                lunar.leap ? "L" : "",
                (unsigned)lunar.month, (unsigned)lunar.day);
    return 1;
}
