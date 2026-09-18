#include "ui/pages/ai_album_placeholder_page.h"

#include "basic_include.h"

typedef struct {
    lv_obj_t *screen;
} ai_album_placeholder_page_state_t;

static ai_album_placeholder_page_state_t g_placeholder;

int ai_album_placeholder_page_create(lv_display_t *display,
                                     ai_album_ui_route_t route)
{
    static const char *const names[AI_ALBUM_UI_ROUTE_COUNT] = {
        "HOME", "ALBUM", "GALLERY", "IMAGE AI", "LIVE TRANSLATE",
        "AI CHAT", "SPEAKING PRACTICE", "SETTINGS", "SETTINGS WI-FI",
        "SETTINGS LOCATION", "SETTINGS LOCATION SEARCH",
        "SETTINGS WIFI PASSWORD",
    };
    lv_obj_t *label;

    if (display == NULL || g_placeholder.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_placeholder, 0, sizeof(g_placeholder));
    g_placeholder.screen = lv_display_get_screen_active(display);
    if (g_placeholder.screen == NULL) {
        return RET_ERR;
    }
    lv_obj_clean(g_placeholder.screen);
    lv_obj_clear_flag(g_placeholder.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_placeholder.screen, lv_color_hex(0xD9E9E5U),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_placeholder.screen, LV_OPA_COVER, LV_PART_MAIN);

    label = lv_label_create(g_placeholder.screen);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_label_set_text(label,
                      route < AI_ALBUM_UI_ROUTE_COUNT ? names[route] : "...");
    lv_obj_set_style_text_color(label, lv_color_hex(0x13232EU), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 120);

    label = lv_label_create(g_placeholder.screen);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_label_set_text(label, "FEATURE IN PROGRESS\nCOMING IN A LATER RELEASE");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x35534BU), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 40);

    label = lv_label_create(g_placeholder.screen);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(label, "PRESS POWER TO GO BACK");
    lv_obj_set_style_text_color(label, lv_color_hex(0x657B79U), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -60);
    return RET_OK;
}

int ai_album_placeholder_page_show(void)
{
    if (g_placeholder.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_placeholder.screen);
    return RET_OK;
}

void ai_album_placeholder_page_destroy(void)
{
    if (g_placeholder.screen != NULL) {
        lv_obj_clean(g_placeholder.screen);
    }
    memset(&g_placeholder, 0, sizeof(g_placeholder));
}

ai_album_ui_route_t ai_album_placeholder_page_handle_action(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK ||
        action == AI_ALBUM_UI_ACTION_MENU ||
        action == AI_ALBUM_UI_ACTION_OK) {
        return AI_ALBUM_UI_ROUTE_HOME;
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
