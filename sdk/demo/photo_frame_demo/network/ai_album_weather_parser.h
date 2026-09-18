#ifndef AI_ALBUM_WEATHER_PARSER_H
#define AI_ALBUM_WEATHER_PARSER_H

#include "network/ai_album_weather_service.h"

int ai_album_weather_parse(const char *json,
                           ai_album_weather_snapshot_t *snapshot);
uint8 ai_album_weather_parse_city_search(
    const char *json, ai_album_weather_city_search_t *search);

#endif
