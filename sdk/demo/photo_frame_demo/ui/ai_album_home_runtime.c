#include "ui/ai_album_home_runtime.h"

#include "basic_include.h"
#include "network/ai_album_weather_service.h"
#include "system/ai_album_lunar.h"
#include "system/ai_album_time_service.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_language.h"
#include "ui/pages/ai_album_home_page.h"

/*
 * home数据源:时钟(RTC+SNTP)、天气、农历。电量/充电与音量/网络/时间
 * 已归共享标题栏(ai_album_ui_common.c)各自取服务,不再进本model。
 */

typedef struct {
    ai_album_home_model_t model;
    char greeting[24];
    char time[8];
    char date[48];
    char lunar_date[48];
    char location[36];
    char weather[24];
    char temperature[16];
    char weather_details[80];
    char forecast[4][24];
    uint8 time_valid;
    lv_timer_t *timer;
} ai_album_home_runtime_t;

static ai_album_home_runtime_t g_home_runtime;

static uint8 clock_hour(const char *clock)
{
    if (clock == NULL || clock[0] < '0' || clock[0] > '9' ||
        clock[1] < '0' || clock[1] > '9') {
        return 0U;
    }
    return (uint8)((clock[0] - '0') * 10 + clock[1] - '0');
}

static void update_greeting(uint8 time_valid)
{
    uint8 hour;
    const char *greeting;

    if (!time_valid) {
        greeting = "WELCOME";
    } else {
        hour = clock_hour(g_home_runtime.time);
        if (hour < 12U) {
            greeting = "GOOD MORNING";
        } else if (hour < 18U) {
            greeting = "GOOD AFTERNOON";
        } else {
            greeting = "GOOD EVENING";
        }
    }
    os_snprintf(g_home_runtime.greeting,
                sizeof(g_home_runtime.greeting), "%s", greeting);
}

static const char *weather_condition_text(int16 code)
{
    if (code == 0) return "CLEAR SKY";
    if (code == 1) return "MAINLY CLEAR";
    if (code == 2) return "PARTLY CLOUDY";
    if (code == 3) return "OVERCAST";
    if (code == 45 || code == 48) return "FOG";
    if (code >= 95) return "THUNDERSTORM";
    if (code >= 71 && code <= 77) return "SNOW";
    if (code >= 51) return "RAIN";
    return "CLOUDY";
}

static void format_temperature(char *out, uint32 out_size, int16 value)
{
    int32 whole = value / 10;
    int32 fraction = value < 0 ? -(value % 10) : value % 10;

    os_snprintf(out, out_size, "%ld.%ld C", (long)whole, (long)fraction);
}

static void update_weather(const ai_album_weather_snapshot_t *snapshot,
                           const char *location)
{
    char feels[16];
    const char *state;

    if (!snapshot->valid) {
        if (snapshot->error) {
            state = "WEATHER UPDATE FAILED";
        } else if (snapshot->refreshing) {
            state = "UPDATING WEATHER";
        } else if (snapshot->network_ready) {
            state = "WAITING FOR WEATHER";
        } else {
            state = "WAITING FOR NETWORK";
        }
        os_snprintf(g_home_runtime.weather,
                    sizeof(g_home_runtime.weather), "WEATHER SYNCING");
        os_snprintf(g_home_runtime.temperature,
                    sizeof(g_home_runtime.temperature), "-- C");
        os_snprintf(g_home_runtime.weather_details,
                    sizeof(g_home_runtime.weather_details), "%s   %s",
                    location, ai_album_i18n_text(state));
        return;
    }
    os_snprintf(g_home_runtime.weather, sizeof(g_home_runtime.weather),
                "%s", weather_condition_text(snapshot->weather_code));
    format_temperature(g_home_runtime.temperature,
                       sizeof(g_home_runtime.temperature),
                       snapshot->temperature_c_x10);
    format_temperature(feels, sizeof(feels),
                       snapshot->apparent_temperature_c_x10);
    os_snprintf(g_home_runtime.weather_details,
                sizeof(g_home_runtime.weather_details),
                ai_album_i18n_text("%s   FEELS %s   HUMIDITY %d%%"),
                location, feels, (int)snapshot->humidity_pct);
}

static void update_forecast(const ai_album_weather_snapshot_t *snapshot)
{
    uint8 i;
    char temperature[16];

    for (i = 0U; i < 4U; ++i) {
        if (i >= snapshot->hourly_count || !snapshot->valid) {
            os_snprintf(g_home_runtime.forecast[i],
                        sizeof(g_home_runtime.forecast[i]), "--");
            continue;
        }
        format_temperature(temperature, sizeof(temperature),
                           snapshot->hourly_temperature_c_x10[i]);
        os_snprintf(g_home_runtime.forecast[i],
                    sizeof(g_home_runtime.forecast[i]), "%s\n%s",
                    i == 0U ? ai_album_i18n_text("NOW")
                            : snapshot->hourly_time[i],
                    temperature);
    }
}

static void home_runtime_refresh(void)
{
    ai_album_weather_snapshot_t weather;
    ai_album_weather_location_t wx_location;

    /* 时间:SNTP在线对时,未同步前显示SYNCING;农历随日期计算 */
    ai_album_time_service_update();
    g_home_runtime.time_valid = (uint8_t)ai_album_time_service_format(
        g_home_runtime.date, sizeof(g_home_runtime.date),
        g_home_runtime.time, sizeof(g_home_runtime.time));
    {
        int y, m, d;

        if (ai_album_time_service_get_date(&y, &m, &d)) {
            if (ai_album_language_get() == AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED) {
                ai_album_lunar_format_chinese(
                    y, m, d, g_home_runtime.lunar_date,
                    sizeof(g_home_runtime.lunar_date));
            } else {
                ai_album_lunar_format_english(
                    y, m, d, g_home_runtime.lunar_date,
                    sizeof(g_home_runtime.lunar_date));
            }
        } else {
            os_snprintf(g_home_runtime.lunar_date,
                        sizeof(g_home_runtime.lunar_date), "--");
        }
    }
    update_greeting(g_home_runtime.time_valid);

    /* 天气:服务为自驱动任务(init一次,15分钟周期刷新),这里只取快照 */
    ai_album_weather_service_init();
    ai_album_weather_service_get_current_location(&wx_location);
    os_snprintf(g_home_runtime.location, sizeof(g_home_runtime.location),
                "%s", wx_location.name);
    memset(&weather, 0, sizeof(weather));
    ai_album_weather_service_get_snapshot(&weather);
    update_weather(&weather, g_home_runtime.location);
    update_forecast(&weather);

    ai_album_home_page_update(&g_home_runtime.model);
}

static void home_runtime_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    home_runtime_refresh();
}

const ai_album_home_model_t *ai_album_home_runtime_prepare(void)
{
    memset(&g_home_runtime, 0, sizeof(g_home_runtime));
    g_home_runtime.model.greeting      = g_home_runtime.greeting;
    g_home_runtime.model.time          = g_home_runtime.time;
    g_home_runtime.model.date          = g_home_runtime.date;
    g_home_runtime.model.lunar_date    = g_home_runtime.lunar_date;
    g_home_runtime.model.weather       = g_home_runtime.weather;
    g_home_runtime.model.temperature   = g_home_runtime.temperature;
    g_home_runtime.model.weather_details =
        g_home_runtime.weather_details;
    for (uint8 i = 0U; i < 4U; ++i) {
        g_home_runtime.model.forecast[i] = g_home_runtime.forecast[i];
    }
    home_runtime_refresh();
    return &g_home_runtime.model;
}

int ai_album_home_runtime_start(void)
{
    if (g_home_runtime.timer != NULL) {
        return RET_OK;
    }
    g_home_runtime.timer = lv_timer_create(home_runtime_timer_cb, 1000, NULL);
    if (g_home_runtime.timer == NULL) {
        return RET_ERR;
    }
    return RET_OK;
}

void ai_album_home_runtime_refresh(void)
{
    home_runtime_refresh();
}
