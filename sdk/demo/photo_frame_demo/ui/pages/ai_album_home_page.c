#include "ui/pages/ai_album_home_page.h"

#include "basic_include.h"
#include "ui/ai_album_i18n.h"

#define AI_ALBUM_HOME_MENU_COUNT 5U

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *clock_block;
    lv_obj_t *forecast;
    lv_obj_t *menu;
    lv_obj_t *menu_hint;
    lv_obj_t *menu_buttons[AI_ALBUM_HOME_MENU_COUNT];
    lv_obj_t *volume_icon_label;
    lv_obj_t *battery_bar;
    lv_obj_t *battery_bolt_label;
    lv_obj_t *status_label;
    lv_obj_t *greeting_label;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *lunar_label;
    lv_obj_t *weather_label;
    lv_obj_t *temperature_label;
    lv_obj_t *weather_details_label;
    lv_obj_t *forecast_labels[4];
    const ai_album_home_model_t *model;
    uint8_t menu_open;
    uint8_t menu_focus;
    uint8_t update_pending;
} ai_album_home_page_state_t;

static ai_album_home_page_state_t g_home;

static lv_obj_t *home_plain_object(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *home_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    ai_album_i18n_set_label_text(label, text, font);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

static lv_obj_t *home_raw_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_text(label, text == NULL ? "" : text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

static void home_style_box(lv_obj_t *obj, uint32_t color, lv_opa_t opacity,
                           int32_t radius)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, opacity, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

static void home_create_topbar(void)
{
    lv_obj_t *bar = home_plain_object(g_home.screen);
    lv_obj_t *label;

    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 1024, 48);
    home_style_box(bar, 0x152327U, LV_OPA_COVER, 0);

    label = home_raw_label(
        bar, "AI FRAME", &lv_font_montserrat_14, 0xEDF4F1U);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 22, 0);
    label = home_label(bar, "WEATHER HOME", &lv_font_montserrat_16, 0xFFFFFFU);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    g_home.volume_icon_label = home_raw_label(
        bar, "", &lv_font_montserrat_14, 0x8DE4C8U);
    lv_obj_set_width(g_home.volume_icon_label, 20);
    lv_obj_set_style_text_align(g_home.volume_icon_label,
                                LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(g_home.volume_icon_label, LV_ALIGN_RIGHT_MID, -230, 0);
    /* 电池:进度条+外部供电⚡。PA10只能测VBUS在位(TP4056的CHRG脚
     * 未接SoC),"真在充电/已充满"无法区分——⚡=接着外部电源,进度条
     * 缓慢上爬即充电中。电量数值不上屏(显示值经慢变限速) */
    g_home.battery_bolt_label = home_raw_label(
        bar, LV_SYMBOL_CHARGE, &lv_font_montserrat_14, 0x27B58BU);
    lv_obj_align(g_home.battery_bolt_label, LV_ALIGN_RIGHT_MID, -254, 0);
    g_home.battery_bar = lv_bar_create(bar);
    lv_obj_set_size(g_home.battery_bar, 56, 16);
    lv_obj_align(g_home.battery_bar, LV_ALIGN_RIGHT_MID, -274, 0);
    lv_bar_set_range(g_home.battery_bar, 0, 100);
    lv_obj_set_style_bg_color(g_home.battery_bar, lv_color_hex(0x2F4A44U),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_home.battery_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_home.battery_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(g_home.battery_bar, 4, LV_PART_INDICATOR);
    lv_obj_add_flag(g_home.battery_bolt_label, LV_OBJ_FLAG_HIDDEN);
    g_home.status_label =
        home_label(bar, "", &lv_font_montserrat_14, 0x8DE4C8U);
    lv_obj_set_width(g_home.status_label, 200);
    lv_obj_set_style_text_align(g_home.status_label,
                                LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(g_home.status_label, LV_ALIGN_RIGHT_MID, -22, 0);
}

static void home_create_clock_block(lv_obj_t *home)
{
    g_home.clock_block = home_plain_object(home);
    lv_obj_set_pos(g_home.clock_block, 40, 155);
    lv_obj_set_size(g_home.clock_block, 360, 190);

    g_home.greeting_label = home_label(
        g_home.clock_block, "", &lv_font_montserrat_14, 0x25775FU);
    lv_obj_set_pos(g_home.greeting_label, 0, 0);
    g_home.time_label = home_raw_label(
        g_home.clock_block, "", &lv_font_montserrat_48, 0x101B32U);
    lv_obj_set_pos(g_home.time_label, 0, 28);
    g_home.date_label = home_label(
        g_home.clock_block, "", &lv_font_montserrat_16, 0x13232EU);
    lv_obj_set_pos(g_home.date_label, 0, 100);
    g_home.lunar_label = home_label(
        g_home.clock_block, "", &lv_font_montserrat_14, 0x657B79U);
    lv_obj_set_pos(g_home.lunar_label, 0, 130);
}

static void home_create_weather_card(lv_obj_t *home)
{
    lv_obj_t *card = home_plain_object(home);
    lv_obj_t *symbol;

    lv_obj_set_pos(card, 600, 175);
    lv_obj_set_size(card, 380, 130);
    home_style_box(card, 0x2D6487U, LV_OPA_COVER, 24);

    g_home.weather_label = home_label(
        card, "", &lv_font_montserrat_16, 0xD7E7EFU);
    lv_obj_set_pos(g_home.weather_label, 175, 20);
    g_home.temperature_label = home_raw_label(
        card, "", &lv_font_montserrat_48, 0xFFFFFFU);
    lv_obj_set_pos(g_home.temperature_label, 175, 43);
    g_home.weather_details_label = home_label(
        card, "", &lv_font_montserrat_14, 0xC7DAE4U);
    lv_obj_set_pos(g_home.weather_details_label, 45, 105);

    symbol = home_raw_label(card, "~", &lv_font_montserrat_48, 0xFFFFFFU);
    lv_obj_set_pos(symbol, 75, 35);
}

static void home_create_forecast(lv_obj_t *home)
{
    uint32_t i;

    g_home.forecast = home_plain_object(home);
    lv_obj_set_pos(g_home.forecast, 40, 440);
    lv_obj_set_size(g_home.forecast, 940, 72);
    lv_obj_set_style_border_width(g_home.forecast, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(g_home.forecast, LV_BORDER_SIDE_TOP,
                                 LV_PART_MAIN);
    lv_obj_set_style_border_color(g_home.forecast, lv_color_hex(0x9DB7B0U),
                                  LV_PART_MAIN);
    lv_obj_set_flex_flow(g_home.forecast, LV_FLEX_FLOW_ROW);

    for (i = 0; i < 4U; ++i) {
        lv_obj_t *cell = home_plain_object(g_home.forecast);

        lv_obj_set_size(cell, LV_PCT(25), 70);
        g_home.forecast_labels[i] = home_label(
            cell, "", &lv_font_montserrat_14, 0x35534BU);
        lv_obj_set_style_text_align(g_home.forecast_labels[i],
                                    LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(g_home.forecast_labels[i]);
    }
}

static lv_obj_t *home_create_menu_button(lv_obj_t *parent, uint32_t index)
{
    static const char *const names[AI_ALBUM_HOME_MENU_COUNT] = {
        "ALBUM", "LIVE TRANSLATE", "AI CHAT", "SPEAKING PRACTICE", "SETTINGS"
    };
    static const char *const icons[AI_ALBUM_HOME_MENU_COUNT] = {
        LV_SYMBOL_IMAGE, "TR", "AI", "PR", LV_SYMBOL_SETTINGS
    };
    static const uint32_t colors[AI_ALBUM_HOME_MENU_COUNT] = {
        0x2BAF87U, 0x527EE5U, 0x8B63D7U, 0xE48843U, 0x65758AU
    };
    lv_obj_t *button = home_plain_object(parent);
    lv_obj_t *icon = home_plain_object(button);
    lv_obj_t *label;

    lv_obj_set_pos(button, 12, 10 + (int32_t)index * 91);
    lv_obj_set_size(button, 196, 80);
    home_style_box(button, 0xFFFFFFU, LV_OPA_10, 14);

    lv_obj_set_pos(icon, 12, 18);
    lv_obj_set_size(icon, 44, 44);
    home_style_box(icon, colors[index], LV_OPA_COVER, 11);
    label = home_raw_label(
        icon, icons[index], &lv_font_montserrat_16, 0xFFFFFFU);
    lv_obj_center(label);
    label = home_label(button, names[index], &lv_font_montserrat_14, 0xFFFFFFU);
    lv_obj_set_width(label, 120);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 68, 0);
    return button;
}

static void home_create_menu(lv_obj_t *home)
{
    uint32_t i;
    lv_obj_t *label;

    g_home.menu = home_plain_object(home);
    lv_obj_set_pos(g_home.menu, 16, 16);
    lv_obj_set_size(g_home.menu, 220, 476);
    home_style_box(g_home.menu, 0x173235U, LV_OPA_90, 20);
    for (i = 0; i < AI_ALBUM_HOME_MENU_COUNT; ++i) {
        g_home.menu_buttons[i] = home_create_menu_button(g_home.menu, i);
    }
    lv_obj_add_flag(g_home.menu, LV_OBJ_FLAG_HIDDEN);

    g_home.menu_hint = home_plain_object(home);
    lv_obj_set_pos(g_home.menu_hint, 24, 505);
    lv_obj_set_size(g_home.menu_hint, 190, 30);
    home_style_box(g_home.menu_hint, 0x193639U, LV_OPA_80, 15);
    label = home_label(g_home.menu_hint, "PRESS M TO OPEN MENU",
                       &lv_font_montserrat_14, 0xD9E8E3U);
    lv_obj_center(label);
}

static void home_update_menu_focus(void)
{
    uint32_t i;

    for (i = 0; i < AI_ALBUM_HOME_MENU_COUNT; ++i) {
        lv_obj_t *button = g_home.menu_buttons[i];
        uint8_t focused = (i == g_home.menu_focus);

        lv_obj_set_style_border_width(button, focused ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(button, lv_color_hex(0x36D1A4U),
                                      LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button, focused ? LV_OPA_30 : LV_OPA_10,
                                LV_PART_MAIN);
    }
}

static void home_set_menu_open(uint8_t open)
{
    g_home.menu_open = open;
    if (open) {
        lv_obj_clear_flag(g_home.menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_home.menu_hint, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(g_home.clock_block, 270);
        lv_obj_set_x(g_home.forecast, 270);
        lv_obj_set_width(g_home.forecast, 710);
        home_update_menu_focus();
        return;
    }
    lv_obj_add_flag(g_home.menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_home.menu_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(g_home.clock_block, 40);
    lv_obj_set_x(g_home.forecast, 40);
    lv_obj_set_width(g_home.forecast, 940);
}

static uint8_t home_set_label(lv_obj_t *label, const char *text)
{
    const char *current;
    const char *translated;

    if (label == NULL || text == NULL) {
        return 0U;
    }
    translated = ai_album_i18n_text(text);
    current = lv_label_get_text(label);
    if (current != NULL && strcmp(current, translated) == 0) {
        return 0U;
    }
    ai_album_i18n_set_label_text(
        label, text, lv_obj_get_style_text_font(label, LV_PART_MAIN));
    return 1U;
}

static uint8_t home_set_raw_label(lv_obj_t *label, const char *text)
{
    const char *current;

    if (label == NULL || text == NULL) return 0U;
    current = lv_label_get_text(label);
    if (current != NULL && strcmp(current, text) == 0) return 0U;
    lv_label_set_text(label, text);
    return 1U;
}

static void home_set_topbar_status(const char *status)
{
    char icon[8];
    const char *details;
    size_t icon_length;

    if (status == NULL) return;
    details = strchr(status, ' ');
    icon_length = details == NULL ? strlen(status) : (size_t)(details - status);
    if (icon_length >= sizeof(icon)) {
        icon_length = 0U;
        details = status;
    }
    memcpy(icon, status, icon_length);
    icon[icon_length] = '\0';
    while (details != NULL && *details == ' ') ++details;
    home_set_raw_label(g_home.volume_icon_label, icon);
    home_set_label(g_home.status_label, details == NULL ? "" : details);
}

static void home_render(const ai_album_home_model_t *model)
{
    uint32_t i;

    home_set_topbar_status(model->status);
    lv_bar_set_value(g_home.battery_bar, model->battery_percent,
                     LV_ANIM_OFF);
    lv_obj_set_style_bg_color(
        g_home.battery_bar,
        lv_color_hex(model->battery_external_power ? 0x27B58BU : 0xFFFFFFU),
        LV_PART_INDICATOR);
    if (model->battery_external_power) {
        lv_obj_clear_flag(g_home.battery_bolt_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_home.battery_bolt_label, LV_OBJ_FLAG_HIDDEN);
    }
    home_set_label(g_home.greeting_label, model->greeting);
    home_set_raw_label(g_home.time_label, model->time);
    home_set_label(g_home.date_label, model->date);
    home_set_label(g_home.lunar_label, model->lunar_date);
    home_set_label(g_home.weather_label, model->weather);
    home_set_raw_label(g_home.temperature_label, model->temperature);
    home_set_label(g_home.weather_details_label, model->weather_details);
    for (i = 0; i < 4U; ++i) {
        if (home_set_label(g_home.forecast_labels[i], model->forecast[i])) {
            lv_obj_center(g_home.forecast_labels[i]);
        }
    }
}

int ai_album_home_page_create(lv_display_t *display,
                              const ai_album_home_model_t *model)
{
    lv_obj_t *home;

    if (display == NULL || model == NULL || g_home.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_home, 0, sizeof(g_home));
    g_home.screen = lv_display_get_screen_active(display);
    if (g_home.screen == NULL) {
        return RET_ERR;
    }
    lv_obj_clean(g_home.screen);
    lv_obj_clear_flag(g_home.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_home.screen, lv_color_hex(0xD7E7E2U),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_home.screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_home.screen, 0, LV_PART_MAIN);

    home = home_plain_object(g_home.screen);
    lv_obj_set_pos(home, 0, 48);
    lv_obj_set_size(home, 1024, 552);
    lv_obj_set_style_bg_color(home, lv_color_hex(0xD9E9E5U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, LV_PART_MAIN);

    home_create_topbar();
    home_create_clock_block(home);
    home_create_weather_card(home);
    home_create_forecast(home);
    home_create_menu(home);
    g_home.model = model;
    home_render(model);
    return RET_OK;
}

void ai_album_home_page_update(const ai_album_home_model_t *model)
{
    lv_display_t *display;

    if (g_home.screen == NULL || model == NULL) {
        return;
    }
    g_home.model = model;
    display = lv_obj_get_display(g_home.screen);
    if (display == NULL || lv_display_get_screen_active(display) != g_home.screen) {
        g_home.update_pending = 1U;
        return;
    }
    home_render(model);
    g_home.update_pending = 0U;
}

int ai_album_home_page_show(void)
{
    if (g_home.screen == NULL) {
        return RET_ERR;
    }
    if (g_home.update_pending && g_home.model != NULL) {
        home_render(g_home.model);
        g_home.update_pending = 0U;
    }
    lv_screen_load(g_home.screen);
    return RET_OK;
}

void ai_album_home_page_destroy(void)
{
    if (g_home.screen != NULL) {
        lv_obj_clean(g_home.screen);
    }
    memset(&g_home, 0, sizeof(g_home));
}

ai_album_ui_route_t ai_album_home_page_handle_action(
    ai_album_ui_action_t action)
{
    static const ai_album_ui_route_t routes[AI_ALBUM_HOME_MENU_COUNT] = {
        AI_ALBUM_UI_ROUTE_ALBUM,
        AI_ALBUM_UI_ROUTE_TRANSLATE,
        AI_ALBUM_UI_ROUTE_AI_CHAT,
        AI_ALBUM_UI_ROUTE_PRACTICE,
        AI_ALBUM_UI_ROUTE_SETTINGS,
    };

    if (action == AI_ALBUM_UI_ACTION_BACK) {
        if (g_home.menu_open) {
            home_set_menu_open(0U);
        }
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_MENU) {
        home_set_menu_open(!g_home.menu_open);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (!g_home.menu_open) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_UP) {
        g_home.menu_focus =
            (g_home.menu_focus + AI_ALBUM_HOME_MENU_COUNT - 1U) %
            AI_ALBUM_HOME_MENU_COUNT;
        home_update_menu_focus();
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT ||
               action == AI_ALBUM_UI_ACTION_DOWN) {
        g_home.menu_focus =
            (g_home.menu_focus + 1U) % AI_ALBUM_HOME_MENU_COUNT;
        home_update_menu_focus();
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        return routes[g_home.menu_focus];
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
