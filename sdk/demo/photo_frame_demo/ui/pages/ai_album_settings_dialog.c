#include "ui/pages/ai_album_settings_dialog.h"

#include "basic_include.h"
#include "display/ai_album_brightness.h"
#include "network/wifi_sta.h"
#include "syscfg.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_language.h"
#include "ui/ai_album_ui_common.h"
#include "version.h"

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *message;
    lv_obj_t *value_panel;
    lv_obj_t *value;
    lv_obj_t *hint;
    ai_album_settings_dialog_type_t type;
    uint8_t selection;
    uint8_t original_brightness;
} settings_dialog_state_t;

static settings_dialog_state_t g_dialog;

static const char *const g_brightness_names[] = {
    "20%", "40%", "60%", "80%", "100%",
};

static void settings_dialog_set_hidden(lv_obj_t *object, uint8_t hidden)
{
    if (object != NULL) {
        lv_obj_set_flag(object, LV_OBJ_FLAG_HIDDEN, hidden);
    }
}

static int settings_dialog_create_objects(lv_obj_t *screen)
{
    g_dialog.overlay = ai_album_ui_common_plain(screen);
    if (g_dialog.overlay == NULL) {
        return RET_ERR;
    }
    lv_obj_set_size(g_dialog.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_dialog.overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_dialog.overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(g_dialog.overlay, LV_OBJ_FLAG_FLOATING);
    g_dialog.panel = ai_album_ui_common_panel(
        g_dialog.overlay, AI_ALBUM_UI_COLOR_WHITE, 20);
    if (g_dialog.panel == NULL) {
        return RET_ERR;
    }
    lv_obj_set_size(g_dialog.panel, 820, 400);
    lv_obj_center(g_dialog.panel);
    g_dialog.title = ai_album_ui_common_label(
        g_dialog.panel, "", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    g_dialog.message = ai_album_ui_common_label(
        g_dialog.panel, "", &lv_font_montserrat_16,
        AI_ALBUM_UI_COLOR_MUTED);
    g_dialog.value_panel = ai_album_ui_common_panel(
        g_dialog.panel, 0xF3F7F5U, 16);
    g_dialog.value = ai_album_ui_common_label(
        g_dialog.value_panel, "", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    g_dialog.hint = ai_album_ui_common_label(
        g_dialog.panel, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    if (g_dialog.title == NULL || g_dialog.message == NULL ||
        g_dialog.value_panel == NULL || g_dialog.value == NULL ||
        g_dialog.hint == NULL) {
        return RET_ERR;
    }
    lv_obj_align(g_dialog.title, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_align(g_dialog.message, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_size(g_dialog.value_panel, 540, 110);
    lv_obj_align(g_dialog.value_panel, LV_ALIGN_TOP_MID, 0, 125);
    lv_obj_center(g_dialog.value);
    lv_obj_align(g_dialog.hint, LV_ALIGN_BOTTOM_MID, 0, -24);
    ai_album_ui_common_focus(g_dialog.value_panel, 1U,
                             AI_ALBUM_UI_COLOR_GREEN);
    return RET_OK;
}

static void settings_dialog_show_selection(const char *title,
                                           const char *message,
                                           const char *value,
                                           const char *hint)
{
    ai_album_ui_common_set_label_text(g_dialog.title, title);
    ai_album_ui_common_set_label_text(g_dialog.message, message);
    ai_album_ui_common_set_label_text(g_dialog.value, value);
    ai_album_ui_common_set_label_text(g_dialog.hint, hint);
    settings_dialog_set_hidden(g_dialog.value_panel, 0U);
}

static void settings_dialog_show_language(void)
{
    g_dialog.selection = (uint8_t)ai_album_language_get();
    settings_dialog_show_selection(
        "DISPLAY LANGUAGE", "SELECT A LANGUAGE",
        "",
        "LEFT / RIGHT SELECT   OK APPLY   POWER CANCEL");
    ai_album_i18n_set_label_native(
        g_dialog.value, (ai_album_language_t)g_dialog.selection,
        ai_album_language_native_name(
            (ai_album_language_t)g_dialog.selection),
        &lv_font_montserrat_24);
}

static void settings_dialog_show_brightness(void)
{
    g_dialog.original_brightness = ai_album_brightness_get_level();
    g_dialog.selection = g_dialog.original_brightness;
    settings_dialog_show_selection(
        "SCREEN BRIGHTNESS", "CHANGES ARE PREVIEWED BEFORE SAVING",
        g_brightness_names[g_dialog.selection],
        "LEFT / RIGHT PREVIEW   OK SAVE   POWER CANCEL");
}

static void settings_dialog_show_about(void)
{
    /* 动态信息:SDK版本/构建时间/设备MAC/网络状态 */
    static char about[320];
    const char *ip = wifi_sta_get_ip();

    os_snprintf(about, sizeof(about),
                "MODEL        AI PHOTO FRAME TXW827\n\n"
                "FIRMWARE     TXSDK-%d.%d.%d r%u app %u\n\n"
                "BUILD        %s %s\n\n"
                "SCREEN       1024 x 600 RGB888\n\n"
                "DEVICE ID    %02X:%02X:%02X:%02X:%02X:%02X\n\n"
                "NETWORK      %s",
                SDK_MVER, SDK_BVER, SDK_PVER, (unsigned)SVN_VERSION,
                (unsigned)APP_VERSION, __DATE__, __TIME__,
                sys_cfgs.mac[0], sys_cfgs.mac[1], sys_cfgs.mac[2],
                sys_cfgs.mac[3], sys_cfgs.mac[4], sys_cfgs.mac[5],
                (ip != NULL && ip[0] != '\0') ? ip : "OFFLINE");
    ai_album_ui_common_set_label_text(g_dialog.title, "ABOUT AI FRAME");
    ai_album_ui_common_set_label_text(g_dialog.message, about);
    lv_obj_set_width(g_dialog.message, 700);
    lv_obj_align(g_dialog.message, LV_ALIGN_TOP_LEFT, 55, 85);
    ai_album_ui_common_set_label_text(g_dialog.hint, "OK OR POWER TO CLOSE");
    settings_dialog_set_hidden(g_dialog.value_panel, 1U);
}

static void settings_dialog_move_language(int8_t delta)
{
    int8_t next = (int8_t)g_dialog.selection + delta;

    if (next < 0) {
        next = (int8_t)AI_ALBUM_LANGUAGE_COUNT - 1;
    } else if (next >= (int8_t)AI_ALBUM_LANGUAGE_COUNT) {
        next = 0;
    }
    g_dialog.selection = (uint8_t)next;
    ai_album_i18n_set_label_native(
        g_dialog.value, (ai_album_language_t)g_dialog.selection,
        ai_album_language_native_name(
            (ai_album_language_t)g_dialog.selection),
        &lv_font_montserrat_24);
}

static void settings_dialog_move_brightness(int8_t delta)
{
    int8_t next = (int8_t)g_dialog.selection + delta;

    if (next < 0) {
        next = (int8_t)ARRAY_SIZE(g_brightness_names) - 1;
    } else if (next >= (int8_t)ARRAY_SIZE(g_brightness_names)) {
        next = 0;
    }
    if (ai_album_brightness_preview_level((uint8_t)next) != RET_OK) {
        ai_album_ui_common_set_label_text(
            g_dialog.message, "BRIGHTNESS PREVIEW FAILED");
        lv_obj_set_style_text_color(
            g_dialog.message, lv_color_hex(AI_ALBUM_UI_COLOR_RED),
            LV_PART_MAIN);
        return;
    }
    g_dialog.selection = (uint8_t)next;
    ai_album_ui_common_set_label_raw(
        g_dialog.value, g_brightness_names[g_dialog.selection]);
    ai_album_ui_common_set_label_text(
        g_dialog.message, "CHANGES ARE PREVIEWED BEFORE SAVING");
    lv_obj_set_style_text_color(
        g_dialog.message, lv_color_hex(AI_ALBUM_UI_COLOR_MUTED),
        LV_PART_MAIN);
}

static void settings_dialog_move(int8_t delta)
{
    if (g_dialog.type == AI_ALBUM_SETTINGS_DIALOG_LANGUAGE) {
        settings_dialog_move_language(delta);
    } else if (g_dialog.type == AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS) {
        settings_dialog_move_brightness(delta);
    }
}

static void settings_dialog_apply(void)
{
    if (g_dialog.type == AI_ALBUM_SETTINGS_DIALOG_LANGUAGE) {
        if (ai_album_language_set(
                (ai_album_language_t)g_dialog.selection) != RET_OK) {
            ai_album_ui_common_set_label_text(
                g_dialog.message, "LANGUAGE SAVE FAILED");
            lv_obj_set_style_text_color(
                g_dialog.message, lv_color_hex(AI_ALBUM_UI_COLOR_RED),
                LV_PART_MAIN);
            return;
        }
    } else if (g_dialog.type == AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS) {
        if (ai_album_brightness_set_level(g_dialog.selection) != RET_OK) {
            ai_album_ui_common_set_label_text(
                g_dialog.message, "BRIGHTNESS SAVE FAILED");
            lv_obj_set_style_text_color(
                g_dialog.message, lv_color_hex(AI_ALBUM_UI_COLOR_RED),
                LV_PART_MAIN);
            return;
        }
        g_dialog.original_brightness = g_dialog.selection;
    }
    ai_album_settings_dialog_destroy();
}

int ai_album_settings_dialog_open(
    lv_obj_t *screen, ai_album_settings_dialog_type_t type)
{
    if (screen == NULL || g_dialog.overlay != NULL ||
        type > AI_ALBUM_SETTINGS_DIALOG_ABOUT) {
        return RET_ERR;
    }
    memset(&g_dialog, 0, sizeof(g_dialog));
    g_dialog.type = type;
    if (type == AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS) {
        g_dialog.original_brightness = ai_album_brightness_get_level();
    }
    if (settings_dialog_create_objects(screen) != RET_OK) {
        ai_album_settings_dialog_destroy();
        return RET_ERR;
    }
    if (type == AI_ALBUM_SETTINGS_DIALOG_LANGUAGE) {
        settings_dialog_show_language();
    } else if (type == AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS) {
        settings_dialog_show_brightness();
    } else {
        settings_dialog_show_about();
    }
    return RET_OK;
}

uint8_t ai_album_settings_dialog_is_open(void)
{
    return (uint8_t)(g_dialog.overlay != NULL);
}

uint8_t ai_album_settings_dialog_handle_action(ai_album_ui_action_t action)
{
    if (g_dialog.overlay == NULL) {
        return 0U;
    }
    if (action == AI_ALBUM_UI_ACTION_BACK ||
        action == AI_ALBUM_UI_ACTION_MENU) {
        ai_album_settings_dialog_destroy();
    } else if (action == AI_ALBUM_UI_ACTION_LEFT) {
        settings_dialog_move(-1);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        settings_dialog_move(1);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        settings_dialog_apply();
    }
    return 1U;
}

void ai_album_settings_dialog_destroy(void)
{
    if (g_dialog.overlay != NULL &&
        g_dialog.type == AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS) {
        (void)ai_album_brightness_preview_level(
            g_dialog.original_brightness);
    }
    if (g_dialog.overlay != NULL && lv_obj_is_valid(g_dialog.overlay)) {
        lv_obj_delete(g_dialog.overlay);
    }
    memset(&g_dialog, 0, sizeof(g_dialog));
}

const char *ai_album_settings_dialog_language_name(void)
{
    ai_album_language_t language = ai_album_language_get();

    return ai_album_language_native_name(language);
}

const char *ai_album_settings_dialog_brightness_name(void)
{
    uint8_t level = ai_album_brightness_get_level();

    return level < ARRAY_SIZE(g_brightness_names) ?
               g_brightness_names[level] : g_brightness_names[0];
}
