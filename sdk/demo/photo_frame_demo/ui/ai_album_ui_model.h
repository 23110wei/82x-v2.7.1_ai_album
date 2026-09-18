#ifndef AI_ALBUM_UI_MODEL_H
#define AI_ALBUM_UI_MODEL_H

#include "typesdef.h"

typedef struct {
    const char *status;
    const char *greeting;
    const char *time;
    const char *date;
    const char *lunar_date;
    const char *weather;
    const char *temperature;
    const char *weather_details;
    const char *forecast[4];
    /* 电池:字符串保留给调试日志;主页渲染用下面的进度条字段 */
    const char *battery;
    uint8 battery_percent;        /* 慢变后的显示电量 0-100 */
    uint8 battery_external_power; /* VBUS在位(真在充电/已满不可区分) */
} ai_album_home_model_t;

#endif
