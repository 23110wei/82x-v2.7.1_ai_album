#include "ui/ai_album_ui_router.h"

#include "album/ai_album_album_perf.h"
#include "basic_include.h"
#include "ui/ai_album_home_runtime.h"
#include "ui/ai_album_language.h"
#include "ui/ai_album_power_dialog.h"
#include "ui/ai_album_ui_route.h"
#include "ui/pages/ai_album_ai_chat_page.h"
#include "ui/pages/ai_album_album_pages.h"
#include "ui/pages/ai_album_home_page.h"
#include "ui/pages/ai_album_location_search_page.h"
#include "ui/pages/ai_album_practice_page.h"
#include "ui/pages/ai_album_settings_pages.h"
#include "ui/pages/ai_album_settings_wifi_password.h"
#include "ui/pages/ai_album_translate_page.h"

typedef struct {
    lv_display_t *display;
    const ai_album_home_model_t *home_model;
    ai_album_ui_route_t current_route;
    ai_album_ui_route_t cached_route;
    uint32_t language_revision;
    uint8_t initialized;
} ai_album_ui_router_state_t;

static ai_album_ui_router_state_t g_router;

static const char *ui_route_name(ai_album_ui_route_t route)
{
    static const char *const names[AI_ALBUM_UI_ROUTE_COUNT] = {
        "HOME", "ALBUM", "GALLERY", "IMAGE AI", "LIVE TRANSLATE",
        "AI CHAT", "SPEAKING PRACTICE", "SETTINGS", "SETTINGS WI-FI",
        "SETTINGS LOCATION", "SETTINGS LOCATION SEARCH",
        "SETTINGS WIFI PASSWORD",
    };

    if (route >= AI_ALBUM_UI_ROUTE_COUNT) {
        return "UNKNOWN";
    }
    return names[route];
}

static uint8_t ui_route_is_album(ai_album_ui_route_t route)
{
    return route == AI_ALBUM_UI_ROUTE_ALBUM ||
           route == AI_ALBUM_UI_ROUTE_GALLERY ||
           route == AI_ALBUM_UI_ROUTE_IMAGE_AI;
}

static uint8_t ui_route_is_settings(ai_album_ui_route_t route)
{
    return route >= AI_ALBUM_UI_ROUTE_SETTINGS &&
           route <= AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD &&
           route != AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH;
}

static int ui_page_create(ai_album_ui_route_t route)
{
    if (route == AI_ALBUM_UI_ROUTE_HOME) {
        return ai_album_home_page_create(g_router.display,
                                         g_router.home_model);
    }
    if (ui_route_is_album(route)) {
        return ai_album_album_pages_create(g_router.display, route);
    }
    if (route == AI_ALBUM_UI_ROUTE_TRANSLATE) {
        return ai_album_translate_page_create(g_router.display);
    }
    if (route == AI_ALBUM_UI_ROUTE_AI_CHAT) {
        return ai_album_ai_chat_page_create(g_router.display);
    }
    if (route == AI_ALBUM_UI_ROUTE_PRACTICE) {
        return ai_album_practice_page_create(g_router.display);
    }
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH) {
        return ai_album_location_search_page_create(g_router.display);
    }
    if (ui_route_is_settings(route)) {
        return ai_album_settings_pages_create(g_router.display, route);
    }
    return RET_ERR;
}

static void ui_page_destroy(ai_album_ui_route_t route)
{
    if (route == AI_ALBUM_UI_ROUTE_HOME) {
        ai_album_home_page_destroy();
    } else if (ui_route_is_album(route)) {
        ai_album_album_pages_destroy();
    } else if (route == AI_ALBUM_UI_ROUTE_TRANSLATE) {
        ai_album_translate_page_destroy();
    } else if (route == AI_ALBUM_UI_ROUTE_AI_CHAT) {
        ai_album_ai_chat_page_destroy();
    } else if (route == AI_ALBUM_UI_ROUTE_PRACTICE) {
        ai_album_practice_page_destroy();
    } else if (route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH) {
        ai_album_location_search_page_destroy();
    } else if (ui_route_is_settings(route)) {
        ai_album_settings_pages_destroy();
    }
}

static int ui_page_show(ai_album_ui_route_t route)
{
    if (route == AI_ALBUM_UI_ROUTE_HOME) {
        return ai_album_home_page_show();
    }
    if (ui_route_is_album(route)) {
        return ai_album_album_pages_show();
    }
    if (route == AI_ALBUM_UI_ROUTE_TRANSLATE) {
        return ai_album_translate_page_show();
    }
    if (route == AI_ALBUM_UI_ROUTE_AI_CHAT) {
        return ai_album_ai_chat_page_show();
    }
    if (route == AI_ALBUM_UI_ROUTE_PRACTICE) {
        return ai_album_practice_page_show();
    }
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH) {
        return ai_album_location_search_page_show();
    }
    if (ui_route_is_settings(route)) {
        return ai_album_settings_pages_show();
    }
    return RET_ERR;
}

static ai_album_ui_route_t ui_page_handle_action(ai_album_ui_action_t action)
{
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_HOME) {
        return ai_album_home_page_handle_action(action);
    }
    if (ui_route_is_album(g_router.current_route)) {
        return ai_album_album_pages_handle_action(action);
    }
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_TRANSLATE) {
        return ai_album_translate_page_handle_action(action);
    }
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_AI_CHAT) {
        return ai_album_ai_chat_page_handle_action(action);
    }
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_PRACTICE) {
        return ai_album_practice_page_handle_action(action);
    }
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH) {
        return ai_album_location_search_page_handle_action(action);
    }
    if (ui_route_is_settings(g_router.current_route)) {
        return ai_album_settings_pages_handle_action(action);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

static int ui_router_show_home(void)
{
    ai_album_home_runtime_refresh();
    if (ui_page_show(AI_ALBUM_UI_ROUTE_HOME) != RET_OK) {
        return RET_ERR;
    }
    g_router.current_route = AI_ALBUM_UI_ROUTE_HOME;
    os_printf("ai_album: route -> %s\r\n",
              ui_route_name(g_router.current_route));
    return RET_OK;
}

static int ui_router_replace_business(ai_album_ui_route_t route,
                                      uint32_t *create_ms,
                                      uint32_t *show_ms)
{
    uint64 start_ms;

    if (g_router.current_route != AI_ALBUM_UI_ROUTE_HOME &&
        ui_router_show_home() != RET_OK) {
        return RET_ERR;
    }
    if (g_router.cached_route != AI_ALBUM_UI_ROUTE_NONE) {
        os_printf("ai_album: UI cache evict %s\r\n",
                  ui_route_name(g_router.cached_route));
        ui_page_destroy(g_router.cached_route);
        g_router.cached_route = AI_ALBUM_UI_ROUTE_NONE;
    }

    start_ms = os_mseconds();
    if (ui_page_create(route) != RET_OK) {
        ui_page_destroy(route);
        return RET_ERR;
    }
    *create_ms = (uint32_t)(os_mseconds() - start_ms);
    start_ms = os_mseconds();
    if (ui_page_show(route) != RET_OK) {
        ui_page_destroy(route);
        return RET_ERR;
    }
    *show_ms = (uint32_t)(os_mseconds() - start_ms);
    g_router.cached_route = route;
    g_router.current_route = route;
    os_printf("ai_album: route -> %s\r\n", ui_route_name(route));
    return RET_OK;
}

static void ui_router_fallback_home(ai_album_ui_route_t target)
{
    os_printf("ai_album: UI route open failed target=%s\r\n",
              ui_route_name(target));
    if (ui_router_show_home() == RET_OK) {
        return;
    }
    g_router.initialized = 0U;
    os_printf("ai_album: UI fallback HOME show failed\r\n");
}

static void ui_router_begin_album_perf(ai_album_ui_route_t previous,
                                       ai_album_ui_route_t target)
{
    if (target == AI_ALBUM_UI_ROUTE_ALBUM) {
        ai_album_album_perf_begin(AI_ALBUM_ALBUM_PERF_OPEN_ALBUM);
    } else if (target == AI_ALBUM_UI_ROUTE_GALLERY) {
        ai_album_album_perf_begin(AI_ALBUM_ALBUM_PERF_OPEN_GALLERY);
    } else if (target == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        ai_album_album_perf_begin(AI_ALBUM_ALBUM_PERF_OPEN_IMAGE_AI);
    } else if (target == AI_ALBUM_UI_ROUTE_HOME &&
               ui_route_is_album(previous)) {
        ai_album_album_perf_begin(AI_ALBUM_ALBUM_PERF_RETURN_HOME);
    }
}

static void ui_router_navigate(ai_album_ui_route_t route)
{
    ai_album_ui_route_t previous = g_router.current_route;
    uint64 start_ms = os_mseconds();
    uint32_t create_ms = 0U;
    uint32_t show_ms;

    if (route == AI_ALBUM_UI_ROUTE_NONE || route >= AI_ALBUM_UI_ROUTE_COUNT ||
        route == previous) {
        return;
    }
    ui_router_begin_album_perf(previous, route);

    if (ui_route_is_album(previous) && ui_route_is_album(route)) {
        if (ai_album_album_pages_switch(route) != RET_OK) {
            os_printf("ai_album: album mode switch failed %s -> %s\r\n",
                      ui_route_name(previous), ui_route_name(route));
            return;
        }
        g_router.current_route = route;
        g_router.cached_route = route;
        show_ms = (uint32_t)(os_mseconds() - start_ms);
        ai_album_album_perf_record_route(0U, show_ms);
        os_printf("ai_album: UI route %s -> %s cache=resident show=%ums\r\n",
                  ui_route_name(previous), ui_route_name(route),
                  (unsigned)show_ms);
        return;
    }

    if (route == AI_ALBUM_UI_ROUTE_HOME) {
        if (ui_router_show_home() != RET_OK) {
            ui_router_fallback_home(route);
            return;
        }
        show_ms = (uint32_t)(os_mseconds() - start_ms);
        ai_album_album_perf_record_route(0U, show_ms);
        os_printf("ai_album: UI route %s -> HOME cache=hit show=%ums\r\n",
                  ui_route_name(previous), (unsigned)show_ms);
        return;
    }

    if (g_router.cached_route == route) {
        if (ui_page_show(route) != RET_OK) {
            ui_page_destroy(route);
            g_router.cached_route = AI_ALBUM_UI_ROUTE_NONE;
            ui_router_fallback_home(route);
            return;
        }
        g_router.current_route = route;
        show_ms = (uint32_t)(os_mseconds() - start_ms);
        ai_album_album_perf_record_route(0U, show_ms);
        os_printf("ai_album: UI route %s -> %s cache=hit show=%ums\r\n",
                  ui_route_name(previous), ui_route_name(route),
                  (unsigned)show_ms);
        return;
    }

    if (ui_router_replace_business(route, &create_ms, &show_ms) != RET_OK) {
        ui_router_fallback_home(route);
        return;
    }
    ai_album_album_perf_record_route(create_ms, show_ms);
    os_printf("ai_album: UI route %s -> %s cache=miss create=%ums "
              "show=%ums total=%ums\r\n",
              ui_route_name(previous), ui_route_name(route),
              (unsigned)create_ms, (unsigned)show_ms,
              (unsigned)(os_mseconds() - start_ms));
}

static int ui_router_rebuild_language(void)
{
    ai_album_ui_route_t target = g_router.current_route;
    uint32_t create_ms = 0U;
    uint32_t show_ms = 0U;

    if (target != AI_ALBUM_UI_ROUTE_HOME &&
        ui_router_show_home() != RET_OK) {
        return RET_ERR;
    }
    if (g_router.cached_route != AI_ALBUM_UI_ROUTE_NONE) {
        ui_page_destroy(g_router.cached_route);
        g_router.cached_route = AI_ALBUM_UI_ROUTE_NONE;
    }
    ui_page_destroy(AI_ALBUM_UI_ROUTE_HOME);
    if (ui_page_create(AI_ALBUM_UI_ROUTE_HOME) != RET_OK ||
        ui_page_show(AI_ALBUM_UI_ROUTE_HOME) != RET_OK) {
        return RET_ERR;
    }
    g_router.current_route = AI_ALBUM_UI_ROUTE_HOME;
    if (target != AI_ALBUM_UI_ROUTE_HOME &&
        ui_router_replace_business(target, &create_ms, &show_ms) != RET_OK) {
        return RET_ERR;
    }
    g_router.language_revision = ai_album_language_revision();
    os_printf("ai_album: UI language rebuilt route=%s revision=%u\r\n",
              ui_route_name(target),
              (unsigned)g_router.language_revision);
    return RET_OK;
}

int ai_album_ui_router_init(lv_display_t *display,
                            const ai_album_home_model_t *home_model)
{
    if (display == NULL || home_model == NULL || g_router.initialized) {
        return RET_ERR;
    }
    memset(&g_router, 0, sizeof(g_router));
    (void)ai_album_language_init();
    g_router.display = display;
    g_router.home_model = home_model;
    g_router.current_route = AI_ALBUM_UI_ROUTE_NONE;
    g_router.cached_route = AI_ALBUM_UI_ROUTE_NONE;
    g_router.language_revision = ai_album_language_revision();
    if (ui_page_create(AI_ALBUM_UI_ROUTE_HOME) != RET_OK) {
        memset(&g_router, 0, sizeof(g_router));
        return RET_ERR;
    }
    if (ui_page_show(AI_ALBUM_UI_ROUTE_HOME) != RET_OK) {
        ui_page_destroy(AI_ALBUM_UI_ROUTE_HOME);
        memset(&g_router, 0, sizeof(g_router));
        return RET_ERR;
    }
    g_router.current_route = AI_ALBUM_UI_ROUTE_HOME;
    g_router.initialized = 1U;
    return RET_OK;
}

void ai_album_ui_router_handle(ai_album_ui_action_t action)
{
    ai_album_ui_route_t requested_route;

    if (!g_router.initialized) {
        return;
    }
    if (ai_album_power_dialog_is_open()) {
        /* The dialog is modal: it consumes every action until closed. */
        ai_album_power_dialog_handle_action(action);
        return;
    }
    if (action == AI_ALBUM_UI_ACTION_POWER_OFF) {
        (void)ai_album_power_dialog_open();
        return;
    }
    requested_route = ui_page_handle_action(action);
    if (g_router.language_revision != ai_album_language_revision()) {
        if (ui_router_rebuild_language() != RET_OK) {
            g_router.initialized = 0U;
            os_printf("ai_album: UI language rebuild failed\r\n");
        }
        return;
    }
    ui_router_navigate(requested_route);
}

void ai_album_ui_router_poll(void)
{
    ai_album_ui_route_t requested_route;

    if (!g_router.initialized) {
        return;
    }
    if (g_router.current_route == AI_ALBUM_UI_ROUTE_SETTINGS) {
        ai_album_settings_pages_poll();
        return;
    }
    if (g_router.current_route != AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        return;
    }
    requested_route = ai_album_settings_wifi_password_poll();
    ui_router_navigate(requested_route);
}
