#include "ui/pages/ai_album_settings_pages.h"
#include "basic_include.h"
#include "network/ai_album_weather_service.h"
#include "network/ble_settings.h"
#include "storage/ai_album_storage_info.h"
#include "ui/ai_album_ui_common.h"
#include "ui/ai_album_i18n.h"
#include "ui/pages/ai_album_settings_dialog.h"
#include "ui/pages/ai_album_settings_format_dialog.h"
#include "ui/pages/ai_album_settings_wifi_password.h"
#include "ui/pages/ai_album_settings_wifi_page.h"
#define SETTINGS_MAX_FOCUSABLES 8U
#define SETTINGS_LOCATION_SEARCH_VALUE AI_ALBUM_WEATHER_PRESET_NONE
#define SETTINGS_MAIN_LANGUAGE_FOCUS 2U
#define SETTINGS_MAIN_BRIGHTNESS_FOCUS 3U
#define SETTINGS_MAIN_BLUETOOTH_FOCUS 4U
#define SETTINGS_MAIN_FORMAT_FOCUS 5U
#define SETTINGS_MAIN_ABOUT_FOCUS 6U
#define SETTINGS_STORAGE_FORMAT "FREE %u.%u GB / %u.%u GB"
typedef struct {
    lv_obj_t *object;
    int16_t x;
    int16_t y;
    uint8_t value;
} settings_focus_item_t;
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *button_parent;
    lv_obj_t *status;
    lv_obj_t *language_button;
    lv_obj_t *brightness_button;
    lv_obj_t *bluetooth_button;
    lv_obj_t *storage_value;
    settings_focus_item_t focusables[SETTINGS_MAX_FOCUSABLES];
    ai_album_ui_route_t route;
    uint8_t focus_count;
    uint8_t focus;
} settings_page_state_t;
static settings_page_state_t g_page;
static uint32_t g_last_rendered_seq = 0U;
static uint8_t  g_scan_retried = 0U;

static void settings_storage_scan_worker(uint32_t p1, uint32_t p2, uint32_t p3)
{
    (void)p1; (void)p2; (void)p3;
    ai_album_storage_info_request();
}

static void settings_set_status(const char *text, uint32_t color);
static void settings_reset_focus(void)
{
    memset(g_page.focusables, 0, sizeof(g_page.focusables));
    g_page.focus_count = 0U;
    g_page.focus = 0U;
}
static void settings_add_focusable(lv_obj_t *object, int16_t x, int16_t y)
{
    settings_focus_item_t *item;
    if (object == NULL || g_page.focus_count >= SETTINGS_MAX_FOCUSABLES) {
        return;
    }
    item = &g_page.focusables[g_page.focus_count++];
    item->object = object;
    item->x = x;
    item->y = y;
}
static void settings_set_focus_value(lv_obj_t *object, uint8_t value)
{
    uint8_t index;
    for (index = 0U; index < g_page.focus_count; ++index) {
        if (g_page.focusables[index].object == object) {
            g_page.focusables[index].value = value;
            return;
        }
    }
}
static void settings_set_focus_position(lv_obj_t *object, int16_t x,
                                        int16_t y)
{
    uint8_t index;
    for (index = 0U; index < g_page.focus_count; ++index) {
        if (g_page.focusables[index].object == object) {
            g_page.focusables[index].x = x;
            g_page.focusables[index].y = y;
            return;
        }
    }
}
static void settings_update_focus(void)
{
    uint8_t index;
    for (index = 0U; index < g_page.focus_count; ++index) {
        ai_album_ui_common_focus(
            g_page.focusables[index].object, index == g_page.focus,
            AI_ALBUM_UI_COLOR_GREEN);
    }
}
static int32_t settings_abs(int32_t value)
{
    return value < 0 ? -value : value;
}
static uint8_t settings_is_direction_candidate(
    const settings_focus_item_t *current,
    const settings_focus_item_t *candidate,
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        return candidate->x < current->x;
    }
    if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        return candidate->x > current->x;
    }
    if (action == AI_ALBUM_UI_ACTION_UP) {
        return candidate->y < current->y;
    }
    return candidate->y > current->y;
}
static uint32_t settings_focus_distance(
    const settings_focus_item_t *current,
    const settings_focus_item_t *candidate,
    ai_album_ui_action_t action)
{
    int32_t primary;
    int32_t secondary;
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_RIGHT) {
        primary = settings_abs(candidate->x - current->x);
        secondary = settings_abs(candidate->y - current->y);
    } else {
        primary = settings_abs(candidate->y - current->y);
        secondary = settings_abs(candidate->x - current->x);
    }
    return (uint32_t)(primary * 100 + secondary);
}
static void settings_move_focus(ai_album_ui_action_t action)
{
    const settings_focus_item_t *current;
    uint32_t best_distance = 0xFFFFFFFFUL;
    uint8_t best = g_page.focus;
    uint8_t index;
    if (g_page.focus_count == 0U || g_page.focus >= g_page.focus_count) {
        return;
    }
    current = &g_page.focusables[g_page.focus];
    for (index = 0U; index < g_page.focus_count; ++index) {
        uint32_t distance;
        if (index == g_page.focus ||
            !settings_is_direction_candidate(current,
                                             &g_page.focusables[index],
                                             action)) {
            continue;
        }
        distance = settings_focus_distance(
            current, &g_page.focusables[index], action);
        if (distance < best_distance) {
            best_distance = distance;
            best = index;
        }
    }
    g_page.focus = best;
    settings_update_focus();
}

static void settings_move_main_focus(ai_album_ui_action_t action)
{
    if (g_page.focus_count == 0U || g_page.focus >= g_page.focus_count ||
        action == AI_ALBUM_UI_ACTION_UP ||
        action == AI_ALBUM_UI_ACTION_DOWN) {
        return;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        if (g_page.focus == 0U) {
            return;
        }
        g_page.focus--;
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        if (g_page.focus + 1U >= g_page.focus_count) {
            return;
        }
        g_page.focus++;
    } else {
        return;
    }
    settings_update_focus();
}
static void settings_set_status(const char *text, uint32_t color)
{
    if (g_page.status == NULL || text == NULL) {
        return;
    }
    ai_album_ui_common_set_label_text(g_page.status, text);
    lv_obj_set_style_text_color(g_page.status, lv_color_hex(color),
                                LV_PART_MAIN);
}
static void settings_create_heading(const char *title, const char *subtitle)
{
    lv_obj_t *label;
    label = ai_album_ui_common_label(
        g_page.screen, title, &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(label, 40, 72);
    label = ai_album_ui_common_label(
        g_page.screen, subtitle, &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 40, 108);
}
static lv_obj_t *settings_create_button(const char *title,
                                        const char *subtitle,
                                        int32_t x, int32_t y)
{
    lv_obj_t *parent = g_page.button_parent != NULL ?
                           g_page.button_parent : g_page.screen;
    lv_obj_t *button = ai_album_ui_common_button(parent, title, subtitle);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 450, 70);
    settings_add_focusable(button, (int16_t)(x + 225),
                           (int16_t)(y + 35));
    return button;
}

static lv_obj_t *settings_create_main_button(const char *title,
                                             const char *subtitle,
                                             int32_t x, int32_t y)
{
    lv_obj_t *button = settings_create_button(title, subtitle, x, y);
    lv_obj_t *title_label;
    lv_obj_t *value_label;

    if (button == NULL || lv_obj_get_child_count(button) < 2U) {
        return button;
    }
    title_label = lv_obj_get_child(button, 0);
    value_label = lv_obj_get_child(button, 1);
    lv_obj_set_pos(title_label, 18, 25);
    lv_obj_set_width(title_label, 160);
    lv_obj_set_pos(value_label, 185, 27);
    lv_obj_set_width(value_label, 245);
    return button;
}
static void settings_update_button_subtitle(lv_obj_t *button,
                                            const char *subtitle)
{
    lv_obj_t *label;
    if (button == NULL || subtitle == NULL ||
        lv_obj_get_child_count(button) < 2U) {
        return;
    }
    label = lv_obj_get_child(button, 1);
    if (label != NULL && lv_obj_is_valid(label)) {
        ai_album_ui_common_set_label_text(label, subtitle);
    }
}

static lv_obj_t *settings_create_storage_card(const char *title,
                                              const char *subtitle,
                                              int32_t x, int32_t y)
{
    lv_obj_t *parent = g_page.button_parent != NULL ?
                           g_page.button_parent : g_page.screen;
    lv_obj_t *card = ai_album_ui_common_button(parent, title, subtitle);
    lv_obj_t *title_label;
    lv_obj_t *value_label;

    if (card == NULL || lv_obj_get_child_count(card) < 2U) {
        return card;
    }
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, 450, 70);
    title_label = lv_obj_get_child(card, 0);
    value_label = lv_obj_get_child(card, 1);
    lv_obj_set_pos(title_label, 18, 25);
    lv_obj_set_width(title_label, 160);
    lv_obj_set_pos(value_label, 185, 27);
    lv_obj_set_width(value_label, 245);
    settings_add_focusable(card, (int16_t)(x + 225),
                           (int16_t)(y + 35));
    g_page.storage_value = value_label;
    return card;
}

static void settings_storage_gb_parts(uint32_t megabytes,
                                      uint32_t *whole,
                                      uint32_t *tenths)
{
    uint32_t fraction = megabytes % 1024U;

    *whole = megabytes / 1024U;
    *tenths = (fraction * 10U + 512U) / 1024U;
    if (*tenths >= 10U) {
        *whole += 1U;
        *tenths = 0U;
    }
}

static void settings_render_storage(
    const ai_album_storage_snapshot_t *snapshot)
{
    char text[64];
    uint32_t free_whole;
    uint32_t free_tenths;
    uint32_t total_whole;
    uint32_t total_tenths;
    const char *source;

    if (snapshot == NULL || g_page.storage_value == NULL ||
        !lv_obj_is_valid(g_page.storage_value)) {
        return;
    }
    if (snapshot->status == AI_ALBUM_STORAGE_STATUS_READY) {
        settings_storage_gb_parts(snapshot->free_mb, &free_whole,
                                  &free_tenths);
        settings_storage_gb_parts(snapshot->total_mb, &total_whole,
                                  &total_tenths);
        source = ai_album_i18n_text(SETTINGS_STORAGE_FORMAT);
        os_snprintf(text, sizeof(text), source,
                    (unsigned)free_whole, (unsigned)free_tenths,
                    (unsigned)total_whole, (unsigned)total_tenths);
        ai_album_ui_common_set_label_raw(g_page.storage_value, text);
        lv_obj_set_style_text_color(g_page.storage_value,
                                    lv_color_hex(AI_ALBUM_UI_COLOR_GREEN),
                                    LV_PART_MAIN);
        return;
    }
    source = snapshot->status == AI_ALBUM_STORAGE_STATUS_NO_MEDIA ?
                 "SD NOT READY  ·  CHECK CARD" :
                 (snapshot->status == AI_ALBUM_STORAGE_STATUS_ERROR ?
                      "NOT AVAILABLE" : "SCANNING...");
    ai_album_ui_common_set_label_text(g_page.storage_value, source);
    lv_obj_set_style_text_color(
        g_page.storage_value,
        lv_color_hex(snapshot->status == AI_ALBUM_STORAGE_STATUS_ERROR ?
                         AI_ALBUM_UI_COLOR_RED : AI_ALBUM_UI_COLOR_MUTED),
        LV_PART_MAIN);
}
static void settings_refresh_main_values(void)
{
    settings_update_button_subtitle(
        g_page.language_button,
        ai_album_settings_dialog_language_name());
    settings_update_button_subtitle(
        g_page.brightness_button,
        ai_album_settings_dialog_brightness_name());
    settings_update_button_subtitle(
        g_page.bluetooth_button,
        ble_settings_is_enabled() ? "ON" : "OFF");
}
static int settings_build_main(lv_display_t *display)
{
    g_page.screen = ai_album_ui_common_prepare(
        display, "SETTINGS", 0xEAF1EFU);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    g_page.button_parent = g_page.screen;
    settings_create_heading("FRAME SETTINGS", "DEVICE PREFERENCES");
    settings_create_main_button("WI-FI", "NETWORK AND CONNECTION", 40, 145);
    settings_create_main_button("LOCATION", "WEATHER CITY", 534, 145);
    g_page.language_button = settings_create_main_button(
        "LANGUAGE", ai_album_settings_dialog_language_name(), 40, 245);
    g_page.brightness_button = settings_create_main_button(
        "BRIGHTNESS", ai_album_settings_dialog_brightness_name(), 534, 245);
    g_page.bluetooth_button = settings_create_main_button(
        "BLUETOOTH", ble_settings_is_enabled() ? "ON" : "OFF", 40, 345);
    settings_create_main_button("STORAGE RESET", "KEEP SYSTEM FONTS", 534, 345);
    settings_create_main_button("ABOUT", "MODEL AND VERSION", 40, 445);
    settings_create_storage_card("SD STORAGE", "SCANNING...", 534, 445);
    ai_album_storage_info_init();
    g_last_rendered_seq = 0U;
    g_scan_retried = 0U;
    os_run_func(settings_storage_scan_worker, 0, 0, 0);
    ai_album_ui_common_footer(
        g_page.screen, "POWER BACK   ARROWS SELECT   OK OPEN / TOGGLE");
    return RET_OK;
}
static int settings_build_location(lv_display_t *display)
{
    ai_album_weather_location_t current;
    ai_album_weather_location_t location;
    uint8_t selected = ai_album_weather_service_get_selected_location();
    uint8_t count = ai_album_weather_service_location_count();
    uint8_t visible_count = 0U;
    uint8_t index;
    uint8_t selected_focus = SETTINGS_LOCATION_SEARCH_VALUE;
    lv_obj_t *button;
    int32_t search_x;
    g_page.screen = ai_album_ui_common_prepare(
        display, "SETTINGS / LOCATION", 0xEAF1EFU);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    g_page.button_parent = g_page.screen;
    settings_create_heading("WEATHER LOCATION", "SELECT A PRESET OR SEARCH");

    /* 7个预设城市(受页面焦点上限约束),SEARCH与最后一个城市同行的右列;
     * 不足7个时SEARCH紧跟其后。城市多于7个的部分用SEARCH到达 */
    for (index = 0U; index < count &&
                    visible_count < SETTINGS_MAX_FOCUSABLES - 1U; ++index) {
        if (ai_album_weather_service_get_location(index, &location) != RET_OK) {
            continue;
        }
        button = settings_create_button(
            location.name, index == selected ? "CURRENT" : "AVAILABLE",
            40 + (visible_count % 2U) * 494,
            145 + (visible_count / 2U) * 82);
        settings_set_focus_value(button, index);
        if (index == selected) {
            selected_focus = visible_count;
        }
        visible_count++;
    }
    search_x = 40 + (visible_count % 2U) * 494;
    button = settings_create_button(
        "SEARCH CITY", NULL, search_x,
        145 + (visible_count / 2U) * 82);
    settings_set_focus_value(button, SETTINGS_LOCATION_SEARCH_VALUE);
    settings_set_focus_position(
        button, (int16_t)(search_x + 225),
        (int16_t)(145 + (visible_count / 2U) * 82 + 35));
    if (selected_focus == SETTINGS_LOCATION_SEARCH_VALUE) {
        g_page.focus = g_page.focus_count > 0U ?
                           (uint8_t)(g_page.focus_count - 1U) : 0U;
    } else {
        g_page.focus = selected_focus;
    }
    memset(&current, 0, sizeof(current));
    ai_album_weather_service_get_current_location(&current);
    g_page.status = ai_album_ui_common_label(
        g_page.screen, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_GREEN);
    lv_label_set_text_fmt(g_page.status, ai_album_i18n_text("CURRENT: %s"),
                          current.name);
    lv_obj_set_pos(g_page.status, 40, 480);
    ai_album_ui_common_footer(
        g_page.screen, "POWER BACK   ARROWS SELECT   OK APPLY / SEARCH");
    return RET_OK;
}
static int settings_build_page(lv_display_t *display,
                               ai_album_ui_route_t route)
{
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS) {
        return settings_build_main(display);
    }
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION) {
        return settings_build_location(display);
    }
    return RET_ERR;
}
static ai_album_ui_route_t settings_back_route(void)
{
    return g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS ?
               AI_ALBUM_UI_ROUTE_HOME : AI_ALBUM_UI_ROUTE_SETTINGS;
}
static ai_album_ui_route_t settings_activate_main(void)
{
    ai_album_settings_dialog_type_t dialog_type;
    if (g_page.focus == 0U) return AI_ALBUM_UI_ROUTE_SETTINGS_WIFI;
    if (g_page.focus == 1U) return AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION;
    if (g_page.focus == SETTINGS_MAIN_BLUETOOTH_FOCUS) {
        ble_settings_set_enabled((uint8_t)!ble_settings_is_enabled());
        settings_refresh_main_values();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.focus == SETTINGS_MAIN_FORMAT_FOCUS) {
        if (ai_album_settings_format_dialog_open(g_page.screen) != RET_OK) {
            os_printf("ai_album: format dialog open failed\r\n");
        }
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.focus == SETTINGS_MAIN_LANGUAGE_FOCUS) {
        dialog_type = AI_ALBUM_SETTINGS_DIALOG_LANGUAGE;
    } else if (g_page.focus == SETTINGS_MAIN_BRIGHTNESS_FOCUS) {
        dialog_type = AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS;
    } else if (g_page.focus == SETTINGS_MAIN_ABOUT_FOCUS) {
        dialog_type = AI_ALBUM_SETTINGS_DIALOG_ABOUT;
    } else {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (ai_album_settings_dialog_open(g_page.screen, dialog_type) != RET_OK) {
        os_printf("ai_album: settings dialog open failed (%u)\r\n",
                  (unsigned)dialog_type);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
static ai_album_ui_route_t settings_activate_location(void)
{
    uint8_t value = g_page.focusables[g_page.focus].value;
    if (value == SETTINGS_LOCATION_SEARCH_VALUE) {
        return AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH;
    }
    if (ai_album_weather_service_select_location(value) != RET_OK) {
        settings_set_status("LOCATION SAVE FAILED", AI_ALBUM_UI_COLOR_RED);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    settings_set_status("LOCATION APPLIED", AI_ALBUM_UI_COLOR_GREEN);
    return AI_ALBUM_UI_ROUTE_NONE;
}
static ai_album_ui_route_t settings_activate(void)
{
    if (g_page.focus_count == 0U || g_page.focus >= g_page.focus_count) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS) {
        return settings_activate_main();
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION) {
        return settings_activate_location();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
int ai_album_settings_pages_create(lv_display_t *display,
                                   ai_album_ui_route_t route)
{
    int result;
    if (display == NULL) {
        return RET_ERR;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI ||
        g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        return RET_ERR;
    }
    if (g_page.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_page, 0, sizeof(g_page));
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI) {
        if (ai_album_settings_wifi_page_create(display) != RET_OK) {
            return RET_ERR;
        }
        g_page.route = route;
        return RET_OK;
    }
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        if (ai_album_settings_wifi_password_create(
                display,
                ai_album_settings_wifi_page_selected_ssid()) != RET_OK) {
            return RET_ERR;
        }
        g_page.route = route;
        return RET_OK;
    }
    g_page.route = route;
    result = settings_build_page(display, route);
    if (result != RET_OK) {
        ai_album_settings_pages_destroy();
        return RET_ERR;
    }
    settings_update_focus();
    return RET_OK;
}
int ai_album_settings_pages_show(void)
{
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI) {
        return ai_album_settings_wifi_page_show();
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        return ai_album_settings_wifi_password_show();
    }
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_page.screen);
    settings_update_focus();
    return RET_OK;
}
void ai_album_settings_pages_destroy(void)
{
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI) {
        ai_album_settings_wifi_page_destroy();
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        ai_album_settings_wifi_password_destroy();
    }
    ai_album_settings_dialog_destroy();
    ai_album_settings_format_dialog_destroy();
    if (g_page.screen != NULL) {
        lv_obj_delete(g_page.screen);
    }
    memset(&g_page, 0, sizeof(g_page));
}

void ai_album_settings_pages_poll(void)
{
    ai_album_storage_snapshot_t snap;

    if (g_page.route != AI_ALBUM_UI_ROUTE_SETTINGS ||
        g_page.storage_value == NULL) {
        return;
    }
    ai_album_storage_info_get_snapshot(&snap);
    if (snap.sequence != g_last_rendered_seq) {
        settings_render_storage(&snap);
        g_last_rendered_seq = snap.sequence;
        g_scan_retried = 0U;
    } else if (snap.status == AI_ALBUM_STORAGE_STATUS_CHECKING &&
               g_scan_retried < 1U) {
        os_run_func(settings_storage_scan_worker, 0, 0, 0);
        g_scan_retried = 1U;
    }
}

ai_album_ui_route_t ai_album_settings_pages_handle_action(
    ai_album_ui_action_t action)
{
    if (ai_album_settings_dialog_is_open()) {
        (void)ai_album_settings_dialog_handle_action(action);
        settings_refresh_main_values();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (ai_album_settings_format_dialog_is_open()) {
        (void)ai_album_settings_format_dialog_handle_action(action);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI) {
        return ai_album_settings_wifi_page_handle_action(action);
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD) {
        return ai_album_settings_wifi_password_handle_action(action);
    }
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        return settings_back_route();
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_RIGHT ||
        action == AI_ALBUM_UI_ACTION_UP ||
        action == AI_ALBUM_UI_ACTION_DOWN) {
        if (g_page.route == AI_ALBUM_UI_ROUTE_SETTINGS) {
            settings_move_main_focus(action);
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        settings_move_focus(action);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_OK) {
        return settings_activate();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
