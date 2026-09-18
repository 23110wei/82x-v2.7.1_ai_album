#include "system/ai_album_time_service.h"

#include "basic_include.h"
#include "lib/common/timezone.h"
#include "lib/net/utils.h"
#include "network/wifi_provision.h"

#include <time.h>

/*
 * 时间服务真实现:WiFi在线后启动vendor的SNTP客户端(周期对时,写系统时钟),
 * 读取用 time()+localtime_tz(时区预设东八区)。对时完成前显示SYNCING。
 * 注意:不要同时打开SYS_APP_SNTP宏,否则main.c会再起一份SNTP。
 */

#define AI_ALBUM_TIME_SERVER "ntp.aliyun.com"
#define AI_ALBUM_TIME_UPDATE_HOURS 2U
#define AI_ALBUM_TIME_VALID_EPOCH 1577836800UL /* 2020-01-01 */

static uint8_t g_sntp_started;

void ai_album_time_service_update(void)
{
    wifi_provision_status_t wifi;

    if (g_sntp_started) {
        return;
    }
    wifi_provision_get_status(&wifi);
    if (wifi.state != WIFI_PROVISION_STATE_ONLINE) {
        return;
    }
    timezone_set_preset(TZ_CST);
    if (sntp_client_init(AI_ALBUM_TIME_SERVER,
                         AI_ALBUM_TIME_UPDATE_HOURS) == RET_OK) {
        g_sntp_started = 1U;
        os_printf("ai_album: SNTP started server=%s interval=%uh\r\n",
                  AI_ALBUM_TIME_SERVER, (unsigned)AI_ALBUM_TIME_UPDATE_HOURS);
    }
}

int ai_album_time_service_format(char *date, uint32 date_size,
                                 char *clock, uint32 clock_size)
{
    static const char *const weekdays[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT",
    };
    struct tm t;
    time_t now;

    if (date == NULL || date_size == 0U || clock == NULL ||
        clock_size == 0U) {
        return 0;
    }
    now = time(NULL);
    if ((uint32)now < AI_ALBUM_TIME_VALID_EPOCH) {
        os_snprintf(date, date_size, "SYNCING DATE");
        os_snprintf(clock, clock_size, "--:--");
        return 0;
    }
    localtime_tz(now, &t);
    /* 注意:vendor的localtime_tz为非标准实现,返回值tm_year已是完整年份
     * (如2026)、tm_mon已是1~12,不要再加1900/加1;tm_wday为标准0~6 */
    os_snprintf(date, date_size, "%04d-%02d-%02d %s",
                t.tm_year, t.tm_mon, t.tm_mday,
                weekdays[t.tm_wday % 7]);
    os_snprintf(clock, clock_size, "%02d:%02d",
                t.tm_hour, t.tm_min);
    return 1;
}

int ai_album_time_service_get_date(int *year, int *month, int *day)
{
    struct tm t;
    time_t now;

    if (year == NULL || month == NULL || day == NULL) {
        return 0;
    }
    now = time(NULL);
    if ((uint32)now < AI_ALBUM_TIME_VALID_EPOCH) {
        return 0;
    }
    localtime_tz(now, &t);
    /* 同上:tm_year/tm_mon为非标准返回(完整年/1~12月) */
    *year = t.tm_year;
    *month = t.tm_mon;
    *day = t.tm_mday;
    return 1;
}
