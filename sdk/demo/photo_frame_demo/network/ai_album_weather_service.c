#include "network/ai_album_weather_service.h"

#include "network/ai_album_weather_parser.h"
#include "network/wifi_sta.h"

#include "basic_include.h"
#include "syscfg.h"
#include "lwip/apps/http_client.h"
#include "lwip/tcpip.h"
#include "osal/sleep.h"
#include "osal/task.h"

#include <string.h>

#define WEATHER_HOST "api.open-meteo.com"
#define WEATHER_GEOCODING_HOST "geocoding-api.open-meteo.com"
#define WEATHER_HTTP_PORT 80
#define WEATHER_RESPONSE_CAPACITY 3072U
#define WEATHER_URI_CAPACITY 512U
#define WEATHER_QUERY_CAPACITY (AI_ALBUM_WEATHER_LOCATION_NAME_LEN * 3U + 1U)
#define WEATHER_QUERY_STORAGE_CAPACITY ((WEATHER_QUERY_CAPACITY + 3U) & ~3U)
#define WEATHER_REFRESH_INTERVAL_S (15U * 60U)

typedef enum {
    WEATHER_REQUEST_FORECAST = 0,
    WEATHER_REQUEST_CITY_SEARCH,
} weather_request_kind_t;

/*
 * 预设城市表(经纬度x1e4)。设置页位置列表最多显示
 * SETTINGS_MAX_FOCUSABLES-1=7个(一线/广东省内优先),其余城市用SEARCH。
 */
static const ai_album_weather_location_t g_locations[] = {
    { "SHENZHEN", 225431, 1140579 },
    { "GUANGZHOU", 231291, 1132644 },
    { "BEIJING", 399042, 1164074 },
    { "SHANGHAI", 312304, 1214737 },
    { "HANGZHOU", 302741, 1201551 },
    { "CHENGDU", 305728, 1040668 },
    { "WUHAN", 306168, 1143055 },
    { "DONGGUAN", 230209, 1137518 },
    { "FOSHAN", 230219, 1131228 },
    { "HUIZHOU", 231115, 1144162 },
    { "ZHONGSHAN", 225170, 1133926 },
    { "ZHUHAI", 222719, 1135764 },
    { "SHANTOU", 233549, 1166820 },
    { "SUZHOU", 313046, 1205853 },
    { "NANJING", 320409, 1187968 },
    { "CHONGQING", 295648, 1065527 },
    { "XIAN", 343416, 1089398 },
    { "CHANGSHA", 282286, 1129388 },
    { "ZHENGZHOU", 347465, 1136253 },
    { "JINAN", 366513, 1169887 },
    { "QINGDAO", 360674, 1203826 },
    { "XIAMEN", 244798, 1180894 },
    { "FUZHOU", 260747, 1192965 },
    { "KUNMING", 250388, 1027103 },
    { "HARBIN", 458034, 1265330 },
    { "SHENYANG", 418055, 1234298 },
    { "TIANJIN", 390843, 1172005 },
    { "HAIKOU", 200324, 1103189 },
    { "SANYA", 182527, 1095119 },
    { "HONGKONG", 223195, 1141694 },
    { "MACAU", 221984, 1135411 },
    { "TAIPEI", 250478, 1215654 },
};

typedef struct {
    ai_album_weather_snapshot_t snapshot;
    ai_album_weather_city_search_t city_search;
    ai_album_weather_location_t location;
    ai_album_weather_location_t request_location;
    char search_query[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
    char request_search_query[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
    char *network_buffer;
    void *task_handle;
    uint32 location_sequence;
    uint32 request_location_sequence;
    uint32 search_generation;
    uint32 request_search_generation;
    uint16 response_length;
    uint16 seconds_since_refresh;
    weather_request_kind_t request_kind;
    uint8 initialized;
    uint8 request_pending;
    uint8 request_in_flight;
    uint8 search_pending;
    uint8 response_overflow;
    uint8 preset_index;
} ai_album_weather_service_ctx_t;

static ai_album_weather_service_ctx_t g_weather;

static uint8 weather_location_count(void)
{
    return (uint8)(sizeof(g_locations) / sizeof(g_locations[0]));
}

static uint8 weather_find_preset(const ai_album_weather_location_t *location)
{
    uint8 index;

    for (index = 0U; index < weather_location_count(); ++index) {
        if (location->latitude_e4 == g_locations[index].latitude_e4 &&
            location->longitude_e4 == g_locations[index].longitude_e4) {
            return index;
        }
    }
    return AI_ALBUM_WEATHER_PRESET_NONE;
}

static void weather_location_load(void)
{
    /* 移植适配:位置持久化存"syscfg"主记录追加字段(sys_cfgs.album_wx_*),
     * vendor syscfg库不给新命名记录分配槽位(见WORKLOG持久化方案定型) */
    if (sys_cfgs.album_wx_lat == 0xFFFFFFFF ||
        sys_cfgs.album_wx_city[0] == '\0') {
        os_printf("ai_album: weather location default=%s\r\n",
                  g_weather.location.name);
        return;
    }
    g_weather.location.latitude_e4 = (int32)sys_cfgs.album_wx_lat;
    g_weather.location.longitude_e4 = (int32)sys_cfgs.album_wx_lon;
    strncpy(g_weather.location.name, sys_cfgs.album_wx_city,
            sizeof(g_weather.location.name) - 1U);
    g_weather.location.name[sizeof(g_weather.location.name) - 1U] = '\0';
    g_weather.preset_index = weather_find_preset(&g_weather.location);
    os_printf("ai_album: weather location restored=%s\r\n",
              g_weather.location.name);
}

static int weather_location_save(const ai_album_weather_location_t *location)
{
    uint32 old_lat = sys_cfgs.album_wx_lat;
    uint32 old_lon = sys_cfgs.album_wx_lon;
    char old_city[sizeof(sys_cfgs.album_wx_city)];

    memcpy(old_city, sys_cfgs.album_wx_city, sizeof(old_city));
    sys_cfgs.album_wx_lat = (uint32)location->latitude_e4;
    sys_cfgs.album_wx_lon = (uint32)location->longitude_e4;
    strncpy(sys_cfgs.album_wx_city, location->name,
            sizeof(sys_cfgs.album_wx_city) - 1U);
    sys_cfgs.album_wx_city[sizeof(sys_cfgs.album_wx_city) - 1U] = '\0';
    if (syscfg_save() == RET_OK) {
        os_printf("ai_album: weather location saved=%s\r\n", location->name);
        return RET_OK;
    }
    sys_cfgs.album_wx_lat = old_lat;
    sys_cfgs.album_wx_lon = old_lon;
    memcpy(sys_cfgs.album_wx_city, old_city, sizeof(old_city));
    os_printf("ai_album: weather location save failed\r\n");
    return RET_ERR;
}

static char *weather_response_buffer(void)
{
    return g_weather.network_buffer;
}

static char *weather_uri_buffer(void)
{
    return g_weather.network_buffer + WEATHER_RESPONSE_CAPACITY;
}

static char *weather_query_buffer(void)
{
    return weather_uri_buffer() + WEATHER_URI_CAPACITY;
}

static ai_album_weather_city_search_t *weather_search_buffer(void)
{
    return (ai_album_weather_city_search_t *)(void *)(
        weather_query_buffer() + WEATHER_QUERY_STORAGE_CAPACITY);
}

static uint32 weather_network_buffer_size(void)
{
    return WEATHER_RESPONSE_CAPACITY + WEATHER_URI_CAPACITY +
           WEATHER_QUERY_STORAGE_CAPACITY +
           sizeof(ai_album_weather_city_search_t);
}

static int weather_network_ready(void)
{
    char status[40];

    wifi_sta_get_status(status, sizeof(status));
    return wifi_sta_is_connected() && wifi_sta_get_ip()[0] != '\0';
}

static void weather_commit(const ai_album_weather_snapshot_t *snapshot)
{
    os_sched_disable();
    g_weather.snapshot = *snapshot;
    os_sched_enbale();
}

static void weather_set_transport(uint8 refreshing, uint8 network_ready,
                                  uint8 error)
{
    os_sched_disable();
    g_weather.snapshot.refreshing = refreshing;
    g_weather.snapshot.network_ready = network_ready;
    g_weather.snapshot.error = error;
    os_sched_enbale();
}

static void weather_search_fail(uint32 generation,
                                ai_album_weather_search_error_t error)
{
    os_sched_disable();
    if (generation == g_weather.search_generation) {
        g_weather.city_search.count = 0U;
        g_weather.city_search.searching = 0U;
        g_weather.city_search.error = (uint8)error;
        g_weather.city_search.sequence++;
    }
    os_sched_enbale();
}

static void weather_search_result(httpc_result_t result, uint32 status_code)
{
    ai_album_weather_city_search_t *search = weather_search_buffer();
    char *response = weather_response_buffer();

    if (g_weather.request_search_generation != g_weather.search_generation) {
        g_weather.search_pending = 1U;
        return;
    }
    if (result != HTTPC_RESULT_OK || status_code != 200U ||
        response == NULL || g_weather.response_overflow) {
        weather_search_fail(g_weather.request_search_generation,
                            AI_ALBUM_WEATHER_SEARCH_ERROR_REQUEST);
        return;
    }
    response[g_weather.response_length] = '\0';
    memset(search, 0, sizeof(*search));
    ai_album_weather_parse_city_search(response, search);
    search->error = search->count == 0U ?
        AI_ALBUM_WEATHER_SEARCH_ERROR_NO_RESULTS :
        AI_ALBUM_WEATHER_SEARCH_ERROR_NONE;
    os_sched_disable();
    search->sequence = g_weather.city_search.sequence + 1U;
    g_weather.city_search = *search;
    os_sched_enbale();
    os_printf("ai_album: city search results=%u error=%u\r\n",
              (unsigned)search->count, (unsigned)search->error);
}

static void weather_forecast_result(httpc_result_t result,
                                    uint32 status_code)
{
    ai_album_weather_snapshot_t snapshot;
    char *response = weather_response_buffer();

    if (g_weather.request_location_sequence != g_weather.location_sequence) {
        g_weather.request_pending = 1U;
        return;
    }
    if (result != HTTPC_RESULT_OK || status_code != 200U ||
        response == NULL || g_weather.response_overflow) {
        weather_set_transport(0U, (uint8)weather_network_ready(), 1U);
        os_printf("ai_album: weather HTTP failed result=%d status=%u\r\n",
                  (int)result, (unsigned)status_code);
        return;
    }
    response[g_weather.response_length] = '\0';
    memset(&snapshot, 0, sizeof(snapshot));
    if (!ai_album_weather_parse(response, &snapshot)) {
        weather_set_transport(0U, (uint8)weather_network_ready(), 1U);
        os_printf("ai_album: weather parse failed len=%u\r\n",
                  (unsigned)g_weather.response_length);
        return;
    }
    snapshot.valid = 1U;
    snapshot.network_ready = 1U;
    snapshot.update_sequence = g_weather.snapshot.update_sequence + 1U;
    weather_commit(&snapshot);
    g_weather.seconds_since_refresh = 0U;
    os_printf("ai_album: weather updated seq=%lu temp=%d.%dC code=%d\r\n",
              (unsigned long)snapshot.update_sequence,
              (int)(snapshot.temperature_c_x10 / 10),
              (int)(snapshot.temperature_c_x10 < 0 ?
                    -(snapshot.temperature_c_x10 % 10) :
                    snapshot.temperature_c_x10 % 10),
              (int)snapshot.weather_code);
}

static void weather_http_result(void *arg, httpc_result_t result,
                                u32_t content_length, u32_t status_code,
                                err_t error)
{
    weather_request_kind_t request_kind = g_weather.request_kind;

    (void)arg;
    (void)content_length;
    (void)error;
    g_weather.request_in_flight = 0U;
    if (request_kind == WEATHER_REQUEST_CITY_SEARCH) {
        weather_search_result(result, status_code);
    } else {
        weather_forecast_result(result, status_code);
    }
}

static err_t weather_http_receive(void *arg, struct altcp_pcb *pcb,
                                  struct pbuf *packet, err_t error)
{
    char *response = weather_response_buffer();
    u16_t copied;

    (void)arg;
    (void)error;
    if (packet == NULL) {
        return ERR_OK;
    }
    if (response == NULL ||
        packet->tot_len > (u16_t)(WEATHER_RESPONSE_CAPACITY - 1U -
                                  g_weather.response_length)) {
        g_weather.response_overflow = 1U;
    } else {
        copied = pbuf_copy_partial(packet,
                                   response + g_weather.response_length,
                                   packet->tot_len, 0U);
        if (copied != packet->tot_len) {
            g_weather.response_overflow = 1U;
        } else {
            g_weather.response_length =
                (uint16)(g_weather.response_length + copied);
        }
    }
    altcp_recved(pcb, packet->tot_len);
    pbuf_free(packet);
    return ERR_OK;
}

static const httpc_connection_t g_http_settings = {
    .result_fn = weather_http_result,
    .headers_done_fn = NULL,
};

static void weather_url_encode(const char *source, char *out, uint32 out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32 used = 0U;

    while (source != NULL && *source != '\0' && used + 1U < out_size) {
        uint8 value = (uint8)*source++;

        if ((value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '.') {
            out[used++] = (char)value;
        } else if (used + 3U < out_size) {
            out[used++] = '%';
            out[used++] = hex[(value >> 4) & 0x0FU];
            out[used++] = hex[value & 0x0FU];
        } else {
            break;
        }
    }
    out[used] = '\0';
}

static err_t weather_start_city_search(char *uri)
{
    char *encoded_query = weather_query_buffer();

    weather_url_encode(g_weather.request_search_query, encoded_query,
                       WEATHER_QUERY_CAPACITY);
    os_snprintf(uri, WEATHER_URI_CAPACITY,
                "/v1/search?name=%s&count=%u&language=en&format=json",
                encoded_query, (unsigned)AI_ALBUM_WEATHER_CITY_RESULT_COUNT);
    return httpc_get_file_dns(WEATHER_GEOCODING_HOST, WEATHER_HTTP_PORT, uri,
                              &g_http_settings, weather_http_receive,
                              NULL, NULL);
}

static err_t weather_start_forecast(char *uri)
{
    const ai_album_weather_location_t *location = &g_weather.request_location;

    os_snprintf(uri, WEATHER_URI_CAPACITY,
                "/v1/forecast?latitude=%ld.%04ld&longitude=%ld.%04ld"
                "&current=temperature_2m,apparent_temperature,"
                "relative_humidity_2m,weather_code"
                "&hourly=temperature_2m,weather_code"
                "&timezone=Asia%%2FShanghai&forecast_hours=4",
                (long)(location->latitude_e4 / 10000),
                (long)(location->latitude_e4 < 0 ?
                       -location->latitude_e4 % 10000 :
                       location->latitude_e4 % 10000),
                (long)(location->longitude_e4 / 10000),
                (long)(location->longitude_e4 < 0 ?
                       -location->longitude_e4 % 10000 :
                       location->longitude_e4 % 10000));
    return httpc_get_file_dns(WEATHER_HOST, WEATHER_HTTP_PORT, uri,
                              &g_http_settings, weather_http_receive,
                              NULL, NULL);
}

static void weather_request_start_failed(err_t result)
{
    g_weather.request_in_flight = 0U;
    if (g_weather.request_kind == WEATHER_REQUEST_CITY_SEARCH) {
        weather_search_fail(g_weather.request_search_generation,
                            AI_ALBUM_WEATHER_SEARCH_ERROR_REQUEST);
    } else {
        weather_set_transport(0U, 1U, 1U);
    }
    os_printf("ai_album: weather request start failed kind=%u err=%d\r\n",
              (unsigned)g_weather.request_kind, (int)result);
}

static void weather_http_start(void *arg)
{
    char *uri;
    err_t result;

    (void)arg;
    if (!g_weather.request_in_flight) {
        return;
    }
    if (g_weather.network_buffer == NULL) {
        g_weather.network_buffer = os_malloc_psram(weather_network_buffer_size());
    }
    if (g_weather.network_buffer == NULL) {
        weather_request_start_failed(ERR_MEM);
        return;
    }
    g_weather.response_length = 0U;
    g_weather.response_overflow = 0U;
    uri = weather_uri_buffer();
    result = g_weather.request_kind == WEATHER_REQUEST_CITY_SEARCH ?
                 weather_start_city_search(uri) : weather_start_forecast(uri);
    if (result != ERR_OK) {
        weather_request_start_failed(result);
    }
}

static uint8 weather_forecast_due(void)
{
    return g_weather.request_pending || !g_weather.snapshot.valid ||
           g_weather.seconds_since_refresh >= WEATHER_REFRESH_INTERVAL_S;
}

static void weather_prepare_search_request(void)
{
    g_weather.search_pending = 0U;
    g_weather.request_kind = WEATHER_REQUEST_CITY_SEARCH;
    g_weather.request_search_generation = g_weather.search_generation;
    strncpy(g_weather.request_search_query, g_weather.search_query,
            sizeof(g_weather.request_search_query) - 1U);
    g_weather.request_search_query[
        sizeof(g_weather.request_search_query) - 1U] = '\0';
}

static void weather_prepare_forecast_request(void)
{
    g_weather.request_pending = 0U;
    g_weather.request_kind = WEATHER_REQUEST_FORECAST;
    g_weather.request_location = g_weather.location;
    g_weather.request_location_sequence = g_weather.location_sequence;
    weather_set_transport(1U, 1U, 0U);
}

static void weather_dispatch_request(void)
{
    if (g_weather.request_in_flight) {
        return;
    }
    if (g_weather.search_pending) {
        weather_prepare_search_request();
    } else if (weather_forecast_due()) {
        weather_prepare_forecast_request();
    } else {
        return;
    }
    g_weather.request_in_flight = 1U;
    if (tcpip_callback(weather_http_start, NULL) != ERR_OK) {
        weather_request_start_failed(ERR_IF);
    }
}

static void weather_handle_offline(void)
{
    weather_set_transport(0U, 0U, 0U);
    if (!g_weather.request_in_flight && g_weather.search_pending) {
        g_weather.search_pending = 0U;
        weather_search_fail(g_weather.search_generation,
                            AI_ALBUM_WEATHER_SEARCH_ERROR_OFFLINE);
    }
}

static void weather_service_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (weather_network_ready()) {
            weather_dispatch_request();
        } else {
            weather_handle_offline();
        }
        if (g_weather.seconds_since_refresh < WEATHER_REFRESH_INTERVAL_S) {
            g_weather.seconds_since_refresh++;
        }
        os_sleep(1U);
    }
}

void ai_album_weather_service_init(void)
{
    if (g_weather.initialized) {
        return;
    }
    memset(&g_weather, 0, sizeof(g_weather));
    g_weather.initialized = 1U;
    g_weather.request_pending = 1U;
    g_weather.location = g_locations[0];
    g_weather.preset_index = 0U;
    weather_location_load();
    g_weather.task_handle = os_task_create(
        "album_weather", weather_service_task, NULL,
        OS_TASK_PRIORITY_BELOW_NORMAL, 0U, NULL, 1024U);
    if (g_weather.task_handle == NULL) {
        g_weather.snapshot.error = 1U;
        os_printf("ai_album: weather task creation failed\r\n");
    }
}

void ai_album_weather_service_get_snapshot(ai_album_weather_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    os_sched_disable();
    *out = g_weather.snapshot;
    os_sched_enbale();
}

uint8 ai_album_weather_service_location_count(void)
{
    return weather_location_count();
}

int ai_album_weather_service_get_location(
    uint8 index, ai_album_weather_location_t *out)
{
    if (out == NULL || index >= weather_location_count()) {
        return RET_ERR;
    }
    *out = g_locations[index];
    return RET_OK;
}

uint8 ai_album_weather_service_get_selected_location(void)
{
    uint8 index;

    os_sched_disable();
    index = g_weather.preset_index;
    os_sched_enbale();
    return index;
}

void ai_album_weather_service_get_current_location(
    ai_album_weather_location_t *out)
{
    if (out == NULL) {
        return;
    }
    os_sched_disable();
    *out = g_weather.location;
    os_sched_enbale();
}

static int weather_apply_location(const ai_album_weather_location_t *location,
                                  uint8 preset_index)
{
    os_sched_disable();
    if (g_weather.location.latitude_e4 == location->latitude_e4 &&
        g_weather.location.longitude_e4 == location->longitude_e4 &&
        g_weather.preset_index == preset_index &&
        strcmp(g_weather.location.name, location->name) == 0) {
        os_sched_enbale();
        return RET_OK;
    }
    g_weather.location = *location;
    g_weather.preset_index = preset_index;
    g_weather.location_sequence++;
    g_weather.request_pending = 1U;
    g_weather.snapshot.valid = 0U;
    g_weather.snapshot.refreshing = 1U;
    g_weather.snapshot.error = 0U;
    g_weather.snapshot.update_sequence++;
    os_sched_enbale();
    if (weather_location_save(location) != RET_OK) {
        os_printf("ai_album: weather location save failed name=%s\r\n",
                  location->name);
        return RET_ERR;
    }
    os_printf("ai_album: weather location selected=%s\r\n", location->name);
    return RET_OK;
}

int ai_album_weather_service_select_location(uint8 index)
{
    if (index >= weather_location_count()) {
        return RET_ERR;
    }
    return weather_apply_location(&g_locations[index], index);
}

static uint8 weather_copy_query(const char *query, char *out)
{
    const char *start = query;
    const char *end;
    uint32 length;

    while (*start == ' ') {
        start++;
    }
    end = start + strlen(start);
    while (end > start && end[-1] == ' ') {
        end--;
    }
    length = (uint32)(end - start);
    if (length < 2U || length > AI_ALBUM_WEATHER_LOCATION_NAME_LEN) {
        return 0U;
    }
    memcpy(out, start, length);
    out[length] = '\0';
    return 1U;
}

int ai_album_weather_service_search_city(const char *query)
{
    char normalized[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];

    if (query == NULL || !weather_copy_query(query, normalized)) {
        weather_search_fail(g_weather.search_generation,
                            AI_ALBUM_WEATHER_SEARCH_ERROR_INVALID_QUERY);
        return RET_ERR;
    }
    if (!g_weather.initialized) {
        ai_album_weather_service_init();
    }
    os_sched_disable();
    strncpy(g_weather.search_query, normalized,
            sizeof(g_weather.search_query) - 1U);
    g_weather.search_generation++;
    g_weather.search_pending = 1U;
    g_weather.city_search.count = 0U;
    g_weather.city_search.searching = 1U;
    g_weather.city_search.error = AI_ALBUM_WEATHER_SEARCH_ERROR_NONE;
    g_weather.city_search.sequence++;
    os_sched_enbale();
    os_printf("ai_album: city search query=%s\r\n", normalized);
    return RET_OK;
}

void ai_album_weather_service_get_city_search(
    ai_album_weather_city_search_t *out)
{
    if (out == NULL) {
        return;
    }
    os_sched_disable();
    *out = g_weather.city_search;
    os_sched_enbale();
}

int ai_album_weather_service_select_city_result(uint8 index)
{
    ai_album_weather_location_t location;

    os_sched_disable();
    if (index >= g_weather.city_search.count) {
        os_sched_enbale();
        return RET_ERR;
    }
    location = g_weather.city_search.cities[index].location;
    os_sched_enbale();
    return weather_apply_location(&location, AI_ALBUM_WEATHER_PRESET_NONE);
}
