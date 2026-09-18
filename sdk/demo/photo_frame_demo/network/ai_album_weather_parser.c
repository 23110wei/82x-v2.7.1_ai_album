#include "network/ai_album_weather_parser.h"

#include <string.h>

static const char *skip_space(const char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    return text;
}

static int parse_x10(const char *text, int16 *value, const char **end)
{
    const char *cursor = skip_space(text);
    int32 whole = 0;
    int32 fraction = 0;
    int32 scaled;
    uint8 negative = 0;

    if (*cursor == '-') {
        negative = 1;
        cursor++;
    }
    if (*cursor < '0' || *cursor > '9') {
        return 0;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        whole = whole * 10 + (*cursor++ - '0');
        if (whole > 4000) {
            return 0;
        }
    }
    if (*cursor == '.') {
        cursor++;
        if (*cursor >= '0' && *cursor <= '9') {
            fraction = *cursor++ - '0';
        }
        if (*cursor >= '5' && *cursor <= '9') {
            fraction++;
        }
    }
    scaled = whole * 10 + fraction;
    if (negative) {
        scaled = -scaled;
    }
    if (scaled < -32768 || scaled > 32767) {
        return 0;
    }
    *value = (int16)scaled;
    if (end != NULL) {
        *end = cursor;
    }
    return 1;
}

static int json_number(const char *object, const char *key, int16 *value)
{
    const char *cursor = strstr(object, key);

    if (cursor == NULL || (cursor = strchr(cursor, ':')) == NULL) {
        return 0;
    }
    return parse_x10(cursor + 1, value, NULL);
}

static int parse_e4(const char *text, int32 *value)
{
    const char *cursor = skip_space(text);
    int32 whole = 0;
    int32 fraction = 0;
    uint8 digits = 0U;
    uint8 negative = 0U;

    if (*cursor == '-') {
        negative = 1U;
        cursor++;
    }
    if (*cursor < '0' || *cursor > '9') {
        return 0;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        whole = whole * 10 + (*cursor++ - '0');
        if (whole > 180) {
            return 0;
        }
    }
    if (*cursor++ == '.') {
        while (digits < 4U && *cursor >= '0' && *cursor <= '9') {
            fraction = fraction * 10 + (*cursor++ - '0');
            digits++;
        }
    }
    while (digits++ < 4U) {
        fraction *= 10;
    }
    *value = whole * 10000 + fraction;
    if (negative) {
        *value = -*value;
    }
    return 1;
}

static const char *find_key(const char *object, const char *object_end,
                            const char *key)
{
    const char *cursor = strstr(object, key);

    return cursor != NULL && cursor < object_end ? cursor : NULL;
}

static int json_e4(const char *object, const char *object_end,
                   const char *key, int32 *value)
{
    const char *cursor = find_key(object, object_end, key);

    if (cursor == NULL || (cursor = strchr(cursor, ':')) == NULL ||
        cursor >= object_end) {
        return 0;
    }
    return parse_e4(cursor + 1, value);
}

static int json_string(const char *object, const char *object_end,
                       const char *key, char *out, uint32 out_size)
{
    const char *cursor = find_key(object, object_end, key);
    uint32 length = 0U;

    if (out == NULL || out_size == 0U || cursor == NULL ||
        (cursor = strchr(cursor, ':')) == NULL || cursor >= object_end) {
        return 0;
    }
    cursor = skip_space(cursor + 1);
    if (*cursor++ != '"') {
        return 0;
    }
    while (cursor < object_end && *cursor != '"' && length + 1U < out_size) {
        if (*cursor == '\\' && cursor + 1 < object_end) {
            cursor++;
        }
        out[length++] = *cursor++;
    }
    if (cursor >= object_end || *cursor != '"') {
        return 0;
    }
    out[length] = '\0';
    return length > 0U;
}

static uint8 json_number_array(const char *object, const char *key,
                               int16 *values, uint8 capacity)
{
    const char *cursor = strstr(object, key);
    uint8 count = 0;

    if (cursor == NULL || (cursor = strchr(cursor, '[')) == NULL) {
        return 0;
    }
    cursor++;
    while (count < capacity) {
        cursor = skip_space(cursor);
        if (*cursor == ']') {
            break;
        }
        if (!parse_x10(cursor, &values[count], &cursor)) {
            return 0;
        }
        count++;
        cursor = skip_space(cursor);
        if (*cursor == ',') {
            cursor++;
        } else if (*cursor != ']') {
            return 0;
        }
    }
    return count;
}

static uint8 json_hour_array(const char *hourly, char hours[][6],
                             uint8 capacity)
{
    const char *cursor = strstr(hourly, "\"time\"");
    uint8 count = 0;

    if (cursor == NULL || (cursor = strchr(cursor, '[')) == NULL) {
        return 0;
    }
    cursor++;
    while (count < capacity) {
        cursor = skip_space(cursor);
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '"' || strlen(cursor) < 18U ||
            cursor[11] != 'T' || cursor[14] != ':') {
            return 0;
        }
        hours[count][0] = cursor[12];
        hours[count][1] = cursor[13];
        hours[count][2] = ':';
        hours[count][3] = cursor[15];
        hours[count][4] = cursor[16];
        hours[count][5] = '\0';
        count++;
        cursor = strchr(cursor + 1, '"');
        if (cursor == NULL) {
            return 0;
        }
        cursor = skip_space(cursor + 1);
        if (*cursor == ',') {
            cursor++;
        } else if (*cursor != ']') {
            return 0;
        }
    }
    return count;
}

static int parse_hourly(const char *hourly,
                        ai_album_weather_snapshot_t *snapshot)
{
    uint8 count = AI_ALBUM_WEATHER_HOURLY_COUNT;
    uint8 i;

    if (hourly == NULL ||
        json_hour_array(hourly, snapshot->hourly_time, count) != count ||
        json_number_array(hourly, "\"temperature_2m\"",
                          snapshot->hourly_temperature_c_x10, count) != count ||
        json_number_array(hourly, "\"weather_code\"",
                          snapshot->hourly_weather_code, count) != count) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        snapshot->hourly_weather_code[i] =
            (int16)((snapshot->hourly_weather_code[i] + 5) / 10);
    }
    snapshot->hourly_count = count;
    return 1;
}

int ai_album_weather_parse(const char *json,
                           ai_album_weather_snapshot_t *snapshot)
{
    const char *current;
    const char *hourly;
    int16 humidity_x10;

    if (json == NULL || snapshot == NULL) {
        return 0;
    }
    current = strstr(json, "\"current\"");
    hourly = strstr(json, "\"hourly\"");
    if (current == NULL ||
        !json_number(current, "\"temperature_2m\"",
                     &snapshot->temperature_c_x10) ||
        !json_number(current, "\"apparent_temperature\"",
                     &snapshot->apparent_temperature_c_x10) ||
        !json_number(current, "\"relative_humidity_2m\"", &humidity_x10) ||
        !json_number(current, "\"weather_code\"", &snapshot->weather_code)) {
        return 0;
    }
    snapshot->humidity_pct = (int16)((humidity_x10 + 5) / 10);
    snapshot->weather_code = (int16)((snapshot->weather_code + 5) / 10);
    return parse_hourly(hourly, snapshot);
}

uint8 ai_album_weather_parse_city_search(
    const char *json, ai_album_weather_city_search_t *search)
{
    const char *cursor;
    uint8 count = 0U;

    if (json == NULL || search == NULL ||
        (cursor = strstr(json, "\"results\"")) == NULL ||
        (cursor = strchr(cursor, '[')) == NULL) {
        return 0U;
    }
    memset(search->cities, 0, sizeof(search->cities));
    cursor++;
    while (count < AI_ALBUM_WEATHER_CITY_RESULT_COUNT) {
        const char *object = strchr(cursor, '{');
        const char *object_end;
        ai_album_weather_city_result_t *city;

        if (object == NULL || (object_end = strchr(object, '}')) == NULL) {
            break;
        }
        city = &search->cities[count];
        if (json_string(object, object_end, "\"name\"",
                        city->location.name, sizeof(city->location.name)) &&
            json_e4(object, object_end, "\"latitude\"",
                    &city->location.latitude_e4) &&
            json_e4(object, object_end, "\"longitude\"",
                    &city->location.longitude_e4)) {
            (void)json_string(object, object_end, "\"admin1\"",
                              city->admin1, sizeof(city->admin1));
            (void)json_string(object, object_end, "\"country\"",
                              city->country, sizeof(city->country));
            count++;
        }
        cursor = object_end + 1;
    }
    search->count = count;
    return count;
}
