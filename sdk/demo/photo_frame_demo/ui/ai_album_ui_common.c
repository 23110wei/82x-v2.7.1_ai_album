#include "ui/ai_album_ui_common.h"

#include "audio/ai_album_volume.h"
#include "basic_include.h"
#include "hardware/battery_detect.h"
#include "hardware/power_ctrl.h"
#include "network/wifi_provision.h"
#include "system/ai_album_time_service.h"
#include "ui/ai_album_i18n.h"

#include <string.h>

static lv_obj_t *g_active_topbar_status;
static lv_timer_t *g_topbar_timer;

void ai_album_ui_common_format_volume(char *buffer, size_t capacity)
{
    uint8_t percent = ai_album_volume_get_percent();
    const char *icon = percent == 0U ? LV_SYMBOL_MUTE :
        (percent <= 50U ? LV_SYMBOL_VOLUME_MID : LV_SYMBOL_VOLUME_MAX);

    if (buffer != NULL && capacity > 0U) {
        os_snprintf(buffer, capacity, "%s %u%%", icon, (unsigned)percent);
    }
}

static void common_set_raw_if_changed(lv_obj_t *label, const char *text)
{
    const char *current = lv_label_get_text(label);

    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void common_set_status_if_changed(lv_obj_t *label, const char *source)
{
    const char *text = ai_album_i18n_text(source);
    const char *current = lv_label_get_text(label);

    if (current == NULL || strcmp(current, text) != 0) {
        ai_album_i18n_set_label_text(
            label, source, &lv_font_montserrat_14);
    }
}

/* 电量:固定长度轨道(满电宽度),真实电量只体现为内部填充;
 * ⚡在轨道左侧,仅外部供电(VBUS)时出现。只在值真的变了才写,
 * 否则秒级定时器会把标题栏整块标脏 */
static void common_update_topbar_battery(lv_obj_t *bolt, lv_obj_t *bar,
                                         uint8_t external_power)
{
    uint8_t percent = battery_detect_display_percent(external_power);
    lv_color_t indicator =
        lv_color_hex(external_power ? AI_ALBUM_UI_COLOR_GREEN : 0xFFFFFFU);
    uint8_t hidden = (uint8_t)lv_obj_has_flag(bolt, LV_OBJ_FLAG_HIDDEN);

    if (lv_bar_get_value(bar) != (int32_t)percent) {
        lv_bar_set_value(bar, percent, LV_ANIM_OFF);
    }
    if (lv_obj_get_style_bg_color(bar, LV_PART_INDICATOR).full !=
        indicator.full) {
        lv_obj_set_style_bg_color(bar, indicator, LV_PART_INDICATOR);
    }
    if (external_power && hidden) {
        lv_obj_clear_flag(bolt, LV_OBJ_FLAG_HIDDEN);
    } else if (!external_power && !hidden) {
        lv_obj_add_flag(bolt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void common_update_topbar_status(lv_obj_t *status)
{
    char volume[20];
    char date[48];
    char clock[8];
    wifi_provision_status_t wifi;
    lv_obj_t *volume_label;
    lv_obj_t *network_label;
    lv_obj_t *time_label;
    lv_obj_t *battery_bolt;
    lv_obj_t *battery_bar;

    if (status == NULL || !lv_obj_is_valid(status) ||
        lv_obj_get_child_count(status) < 5U) {
        return;
    }
    volume_label = lv_obj_get_child(status, 0);
    network_label = lv_obj_get_child(status, 1);
    time_label = lv_obj_get_child(status, 2);
    battery_bolt = lv_obj_get_child(status, 3);
    battery_bar = lv_obj_get_child(status, 4);
    ai_album_ui_common_format_volume(volume, sizeof(volume));
    wifi_provision_get_status(&wifi);
    ai_album_time_service_update();
    (void)ai_album_time_service_format(
        date, sizeof(date), clock, sizeof(clock));
    common_set_raw_if_changed(volume_label, volume);
    common_set_status_if_changed(
        network_label,
        wifi.state == WIFI_PROVISION_STATE_ONLINE ? "ONLINE" : "OFFLINE");
    common_set_raw_if_changed(time_label, clock);
    common_update_topbar_battery(battery_bolt, battery_bar,
                                 power_ctrl_charging());
}

static void common_topbar_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_active_topbar_status != NULL &&
        lv_obj_is_valid(g_active_topbar_status)) {
        common_update_topbar_status(g_active_topbar_status);
    } else {
        g_active_topbar_status = NULL;
    }
}

static void common_topbar_screen_event(lv_event_t *event)
{
    lv_obj_t *label = lv_event_get_user_data(event);

    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        g_active_topbar_status = label;
        common_update_topbar_status(label);
    } else if (g_active_topbar_status == label) {
        g_active_topbar_status = NULL;
    }
}

void ai_album_ui_common_refresh_volume(void)
{
    if (g_active_topbar_status != NULL &&
        lv_obj_is_valid(g_active_topbar_status)) {
        common_update_topbar_status(g_active_topbar_status);
    }
}

lv_obj_t *ai_album_ui_common_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t *ai_album_ui_common_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    ai_album_i18n_set_label_text(label, text, font);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

void ai_album_ui_common_set_label_text(lv_obj_t *label, const char *text)
{
    const lv_font_t *font;

    if (label == NULL || text == NULL) return;
    font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (font == NULL) font = &lv_font_montserrat_16;
    ai_album_i18n_set_label_text(label, text, font);
}

void ai_album_ui_common_set_label_raw(lv_obj_t *label, const char *text)
{
    const lv_font_t *font;

    if (label == NULL || text == NULL) return;
    font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (font == NULL) font = &lv_font_montserrat_16;
    ai_album_i18n_set_label_raw(label, text, font);
}

lv_obj_t *ai_album_ui_common_panel(lv_obj_t *parent, uint32_t color,
                                   int32_t radius)
{
    lv_obj_t *panel = ai_album_ui_common_plain(parent);

    lv_obj_set_style_bg_color(panel, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    return panel;
}

static lv_obj_t *common_raw_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_text(label, text == NULL ? "" : text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

/* 挂共享标题栏。首页直接复用display的默认screen(不走prepare),故单独暴露 */
void ai_album_ui_common_attach_topbar(lv_obj_t *screen, const char *title)
{
    lv_obj_t *bar = ai_album_ui_common_panel(
        screen, AI_ALBUM_UI_COLOR_TOPBAR, 0);
    lv_obj_t *status;
    lv_obj_t *label;
    lv_obj_t *battery;

    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 1024, 48);
    label = ai_album_ui_common_label(
        bar, "AI FRAME", &lv_font_montserrat_14, 0xEDF4F1U);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 22, 0);
    label = ai_album_ui_common_label(
        bar, title, &lv_font_montserrat_16, AI_ALBUM_UI_COLOR_WHITE);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    status = ai_album_ui_common_plain(bar);
    lv_obj_set_pos(status, 720, 0);
    lv_obj_set_size(status, 282, 48);
    label = common_raw_label(
        status, "", &lv_font_montserrat_14, 0x8DE4C8U);
    lv_obj_set_width(label, 62);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    label = ai_album_ui_common_label(
        status, "", &lv_font_montserrat_14, 0x8DE4C8U);
    lv_obj_set_width(label, 74);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 70, 0);
    label = common_raw_label(
        status, "", &lv_font_montserrat_14, 0x8DE4C8U);
    lv_obj_set_width(label, 46);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 152, 0);
    /* 电量占最右:轨道长度固定(满电),外部供电时左侧亮点⚡ */
    label = common_raw_label(
        status, LV_SYMBOL_CHARGE, &lv_font_montserrat_14, 0x27B58BU);
    lv_obj_set_width(label, 14);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 206, 0);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    battery = lv_bar_create(status);
    lv_obj_set_size(battery, 56, 16);
    lv_obj_align(battery, LV_ALIGN_LEFT_MID, 226, 0);
    lv_bar_set_range(battery, 0, 100);
    lv_obj_set_style_bg_color(battery, lv_color_hex(0x2F4A44U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(battery, 4, LV_PART_INDICATOR);
    common_update_topbar_status(status);
    lv_obj_add_event_cb(screen, common_topbar_screen_event,
                        LV_EVENT_SCREEN_LOADED, status);
    lv_obj_add_event_cb(screen, common_topbar_screen_event,
                        LV_EVENT_SCREEN_UNLOADED, status);
    /* 首页挂在display的默认screen上,建栏时它已经是活动屏,不会再收到
     * SCREEN_LOADED;不在这里认领,秒级刷新就一直找不到刷新目标 */
    if (lv_display_get_screen_active(lv_obj_get_display(screen)) == screen) {
        g_active_topbar_status = status;
    }
    if (g_topbar_timer == NULL) {
        g_topbar_timer = lv_timer_create(common_topbar_timer_cb, 1000U, NULL);
    }
}

lv_obj_t *ai_album_ui_common_prepare(lv_display_t *display,
                                     const char *title,
                                     uint32_t background)
{
    lv_obj_t *screen;

    if (display == NULL || title == NULL ||
        lv_display_get_default() != display) {
        return NULL;
    }
    screen = lv_obj_create(NULL);
    if (screen == NULL) {
        return NULL;
    }
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    ai_album_ui_common_attach_topbar(screen, title);
    return screen;
}

lv_obj_t *ai_album_ui_common_button(lv_obj_t *parent, const char *title,
                                    const char *subtitle)
{
    lv_obj_t *button = ai_album_ui_common_panel(
        parent, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_t *label = ai_album_ui_common_label(
        button, title, &lv_font_montserrat_16, AI_ALBUM_UI_COLOR_TEXT);

    lv_obj_set_pos(label, 18, subtitle != NULL ? 12 : 20);
    if (subtitle != NULL) {
        label = ai_album_ui_common_label(
            button, subtitle, &lv_font_montserrat_14,
            AI_ALBUM_UI_COLOR_MUTED);
        lv_obj_set_pos(label, 18, 39);
    }
    return button;
}

void ai_album_ui_common_focus(lv_obj_t *obj, uint8_t focused,
                              uint32_t accent)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_set_style_border_width(obj, focused ? 3 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        obj, lv_color_hex(focused ? accent : 0xCBDAD6U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, focused ? LV_OPA_COVER : LV_OPA_90,
                            LV_PART_MAIN);
}

void ai_album_ui_common_select(lv_obj_t *obj, uint8_t selected,
                               uint32_t accent)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_set_style_border_width(obj, selected ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        obj, lv_color_hex(selected ? accent : 0xCBDAD6U), LV_PART_MAIN);
    /* Unselected neighbours stay dimmer than an unfocused focusable. */
    lv_obj_set_style_bg_opa(obj, selected ? LV_OPA_COVER : LV_OPA_70,
                            LV_PART_MAIN);
}

void ai_album_ui_common_footer(lv_obj_t *screen, const char *hint)
{
    lv_obj_t *label = ai_album_ui_common_label(
        screen, hint, &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);

    lv_obj_align(label, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
}

void ai_album_ui_common_set_screen_chrome_hidden(lv_obj_t *topbar,
                                                  lv_obj_t *footer,
                                                  uint8_t hidden)
{
    if (topbar != NULL && lv_obj_is_valid(topbar)) {
        lv_obj_set_flag(topbar, LV_OBJ_FLAG_HIDDEN, hidden);
    }
    if (footer != NULL && lv_obj_is_valid(footer)) {
        lv_obj_set_flag(footer, LV_OBJ_FLAG_HIDDEN, hidden);
    }
}
