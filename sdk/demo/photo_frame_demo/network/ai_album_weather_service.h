#ifndef AI_ALBUM_WEATHER_SERVICE_H
#define AI_ALBUM_WEATHER_SERVICE_H

#include "typesdef.h"

#define AI_ALBUM_WEATHER_HOURLY_COUNT 4U
#define AI_ALBUM_WEATHER_LOCATION_NAME_LEN 32U
#define AI_ALBUM_WEATHER_CITY_RESULT_COUNT 5U
#define AI_ALBUM_WEATHER_PRESET_NONE 0xFFU

typedef struct {
    char name[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
    int32 latitude_e4;
    int32 longitude_e4;
} ai_album_weather_location_t;

typedef struct {
    int16 temperature_c_x10;
    int16 apparent_temperature_c_x10;
    int16 humidity_pct;
    int16 weather_code;
    int16 hourly_temperature_c_x10[AI_ALBUM_WEATHER_HOURLY_COUNT];
    int16 hourly_weather_code[AI_ALBUM_WEATHER_HOURLY_COUNT];
    char hourly_time[AI_ALBUM_WEATHER_HOURLY_COUNT][6];
    uint32 update_sequence;
    uint8 valid;
    uint8 refreshing;
    uint8 network_ready;
    uint8 error;
    uint8 hourly_count;
} ai_album_weather_snapshot_t;

typedef struct {
    ai_album_weather_location_t location;
    char admin1[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
    char country[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
} ai_album_weather_city_result_t;

typedef enum {
    AI_ALBUM_WEATHER_SEARCH_ERROR_NONE = 0,
    AI_ALBUM_WEATHER_SEARCH_ERROR_INVALID_QUERY,
    AI_ALBUM_WEATHER_SEARCH_ERROR_OFFLINE,
    AI_ALBUM_WEATHER_SEARCH_ERROR_REQUEST,
    AI_ALBUM_WEATHER_SEARCH_ERROR_NO_RESULTS,
} ai_album_weather_search_error_t;

typedef struct {
    ai_album_weather_city_result_t cities[AI_ALBUM_WEATHER_CITY_RESULT_COUNT];
    uint32 sequence;
    uint8 count;
    uint8 searching;
    uint8 error;
} ai_album_weather_city_search_t;

void ai_album_weather_service_init(void);
void ai_album_weather_service_get_snapshot(ai_album_weather_snapshot_t *out);
uint8 ai_album_weather_service_location_count(void);
int ai_album_weather_service_get_location(
    uint8 index, ai_album_weather_location_t *out);
uint8 ai_album_weather_service_get_selected_location(void);
void ai_album_weather_service_get_current_location(
    ai_album_weather_location_t *out);
int ai_album_weather_service_select_location(uint8 index);
int ai_album_weather_service_search_city(const char *query);
void ai_album_weather_service_get_city_search(
    ai_album_weather_city_search_t *out);
int ai_album_weather_service_select_city_result(uint8 index);

#endif
