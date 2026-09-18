#ifndef AI_ALBUM_LUNAR_H
#define AI_ALBUM_LUNAR_H

#include "typesdef.h"

/*
 * 农历转换(公历->农历),支持1900~2049年。
 * 数据表+算法已用2000/2008/2015/2020/2023~2026年春节及
 * 2025年闰六月锚点验证通过。
 */

typedef struct {
    uint16 year;   /* 农历年 */
    uint8  month;  /* 农历月 1~12 */
    uint8  day;    /* 农历日 1~30 */
    uint8  leap;   /* 当前月是否闰月 */
} ai_album_lunar_date_t;

/* 公历转农历,失败(年份超范围)返回0 */
int ai_album_lunar_convert(int gy, int gm, int gd,
                           ai_album_lunar_date_t *out);

/* 中文格式:"农历丙午马年 八月十四"(闰月如"闰四月廿三") */
int ai_album_lunar_format_chinese(int gy, int gm, int gd,
                                  char *out, uint32 out_size);

/* 英文数字格式:"LUNAR 8/14"(LEAP标记闰月:"LUNAR L6/14") */
int ai_album_lunar_format_english(int gy, int gm, int gd,
                                  char *out, uint32 out_size);

#endif
