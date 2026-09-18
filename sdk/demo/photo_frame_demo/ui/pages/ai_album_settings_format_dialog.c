#include "ui/pages/ai_album_settings_format_dialog.h"

#include "album/ai_album_album_image_ai.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_ui_common.h"

typedef enum {
    FORMAT_DIALOG_STAGE_TARGET = 0,
    FORMAT_DIALOG_STAGE_CONFIRM,
    FORMAT_DIALOG_STAGE_RESULT,
} format_dialog_stage_t;

enum {
    FORMAT_CONFIRM_NO = 0U,
    FORMAT_CONFIRM_YES,
    FORMAT_CONFIRM_COUNT,
};

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *title;
    lv_obj_t *message;
    lv_obj_t *hint;
    lv_obj_t *target_buttons[3];
    lv_obj_t *confirm_buttons[FORMAT_CONFIRM_COUNT];
    format_dialog_stage_t stage;
    ai_album_album_store_format_target_t target;
    uint8_t confirm_focus;
    uint8_t target_available[3];
} format_dialog_state_t;

static const char *const g_target_names[] = {
    "SD DATA", "FLASH", "BOTH",
};
static const char *const g_target_subtitles[] = {
    "CLEAR DATA, KEEP FONTS", "FORMAT FLASH", "KEEP SD FONTS",
};
static const char *const g_confirm_messages[] = {
    "Clear SD data? Font files will be preserved.",
    "Format Flash? This operation cannot be undone.",
    "Clear SD data and format Flash? Fonts will be preserved.",
};
static const char *const g_success_messages[] = {
    "SD data cleared. Font files were preserved.",
    "Flash is ready to use.",
    "Storage reset complete. Font files were preserved.",
};
static format_dialog_state_t g_dialog;

static void format_dialog_set_hidden(lv_obj_t *object, uint8_t hidden)
{
    if (object != NULL) {
        lv_obj_set_flag(object, LV_OBJ_FLAG_HIDDEN, hidden);
    }
}

static void format_dialog_update_target_focus(void)
{
    uint8_t index;

    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_buttons); ++index) {
        ai_album_ui_common_focus(
            g_dialog.target_buttons[index], index == (uint8_t)g_dialog.target,
            AI_ALBUM_UI_COLOR_GREEN);
        lv_obj_set_style_opa(g_dialog.target_buttons[index],
                             g_dialog.target_available[index] ?
                                 LV_OPA_COVER : LV_OPA_50,
                             LV_PART_MAIN);
    }
}

static void format_dialog_update_confirm_focus(void)
{
    uint8_t index;

    for (index = 0U; index < FORMAT_CONFIRM_COUNT; ++index) {
        ai_album_ui_common_focus(
            g_dialog.confirm_buttons[index], index == g_dialog.confirm_focus,
            index == FORMAT_CONFIRM_YES ? AI_ALBUM_UI_COLOR_RED :
                                          AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void format_dialog_show_target(void)
{
    uint8_t index;

    g_dialog.stage = FORMAT_DIALOG_STAGE_TARGET;
    ai_album_ui_common_set_label_text(g_dialog.title, "STORAGE RESET");
    lv_label_set_text_fmt(g_dialog.message, ai_album_i18n_text("ACTION: %s"),
                          ai_album_i18n_text(g_target_names[g_dialog.target]));
    ai_album_ui_common_set_label_text(
        g_dialog.hint,
        "LEFT / RIGHT SELECT   OK CONTINUE   POWER CANCEL");
    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_buttons); ++index) {
        format_dialog_set_hidden(g_dialog.target_buttons[index], 0U);
    }
    for (index = 0U; index < FORMAT_CONFIRM_COUNT; ++index) {
        format_dialog_set_hidden(g_dialog.confirm_buttons[index], 1U);
    }
    format_dialog_update_target_focus();
}

static void format_dialog_show_confirm(void)
{
    uint8_t index;

    g_dialog.stage = FORMAT_DIALOG_STAGE_CONFIRM;
    g_dialog.confirm_focus = FORMAT_CONFIRM_NO;
    ai_album_ui_common_set_label_text(
        g_dialog.title, "CONFIRM STORAGE RESET");
    ai_album_ui_common_set_label_text(
        g_dialog.message, g_confirm_messages[g_dialog.target]);
    ai_album_ui_common_set_label_text(
        g_dialog.hint,
        "LEFT / RIGHT SELECT   OK CONFIRM   POWER CANCEL");
    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_buttons); ++index) {
        format_dialog_set_hidden(g_dialog.target_buttons[index], 1U);
    }
    for (index = 0U; index < FORMAT_CONFIRM_COUNT; ++index) {
        format_dialog_set_hidden(g_dialog.confirm_buttons[index], 0U);
    }
    format_dialog_update_confirm_focus();
}

static lv_obj_t *format_dialog_create_button(lv_obj_t *parent,
                                              const char *title,
                                              const char *subtitle,
                                              int32_t x, int32_t y,
                                              int32_t width)
{
    lv_obj_t *button = ai_album_ui_common_button(parent, title, subtitle);

    if (button != NULL) {
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, 82);
    }
    return button;
}

static int format_dialog_create_objects(lv_obj_t *screen)
{
    lv_obj_t *panel;
    uint8_t index;

    g_dialog.overlay = ai_album_ui_common_plain(screen);
    if (g_dialog.overlay == NULL) return RET_ERR;
    lv_obj_set_size(g_dialog.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_dialog.overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_dialog.overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(g_dialog.overlay, LV_OBJ_FLAG_FLOATING);
    panel = ai_album_ui_common_panel(
        g_dialog.overlay, AI_ALBUM_UI_COLOR_WHITE, 20);
    if (panel == NULL) return RET_ERR;
    lv_obj_set_size(panel, 820, 390);
    lv_obj_center(panel);
    g_dialog.title = ai_album_ui_common_label(
        panel, "", &lv_font_montserrat_24, AI_ALBUM_UI_COLOR_TEXT);
    g_dialog.message = ai_album_ui_common_label(
        panel, "", &lv_font_montserrat_16, AI_ALBUM_UI_COLOR_MUTED);
    g_dialog.hint = ai_album_ui_common_label(
        panel, "", &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);
    if (g_dialog.title == NULL || g_dialog.message == NULL ||
        g_dialog.hint == NULL) return RET_ERR;
    lv_obj_align(g_dialog.title, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_align(g_dialog.message, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_align(g_dialog.hint, LV_ALIGN_BOTTOM_MID, 0, -22);
    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_buttons); ++index) {
        g_dialog.target_buttons[index] = format_dialog_create_button(
            panel, g_target_names[index],
            g_dialog.target_available[index] ? g_target_subtitles[index] :
                                               "NOT AVAILABLE",
            30 + index * 260, 135, 240);
        if (g_dialog.target_buttons[index] == NULL) return RET_ERR;
    }
    g_dialog.confirm_buttons[FORMAT_CONFIRM_NO] = format_dialog_create_button(
        panel, "NO", "RETURN TO SETTINGS", 105, 155, 280);
    g_dialog.confirm_buttons[FORMAT_CONFIRM_YES] = format_dialog_create_button(
        panel, "YES", "RUN SELECTED ACTION", 435, 155, 280);
    return g_dialog.confirm_buttons[FORMAT_CONFIRM_NO] != NULL &&
                   g_dialog.confirm_buttons[FORMAT_CONFIRM_YES] != NULL ?
               RET_OK : RET_ERR;
}

static void format_dialog_move_target(int8_t delta)
{
    uint8_t attempts;
    int8_t next = (int8_t)g_dialog.target;

    for (attempts = 0U; attempts < ARRAY_SIZE(g_dialog.target_buttons);
         ++attempts) {
        next = (int8_t)(next + delta);
        if (next < 0) next = (int8_t)ARRAY_SIZE(g_dialog.target_buttons) - 1;
        if (next >= (int8_t)ARRAY_SIZE(g_dialog.target_buttons)) next = 0;
        if (g_dialog.target_available[(uint8_t)next]) {
            g_dialog.target = (ai_album_album_store_format_target_t)next;
            lv_label_set_text_fmt(
                g_dialog.message, ai_album_i18n_text("ACTION: %s"),
                ai_album_i18n_text(g_target_names[g_dialog.target]));
            format_dialog_update_target_focus();
            return;
        }
    }
}

static void format_dialog_show_result(int result)
{
    uint8_t index;

    g_dialog.stage = FORMAT_DIALOG_STAGE_RESULT;
    ai_album_ui_common_set_label_text(
        g_dialog.title, result == RET_OK ? "STORAGE RESET COMPLETE" :
                                           "STORAGE RESET FAILED");
    ai_album_ui_common_set_label_text(
        g_dialog.message,
        result == RET_OK ? g_success_messages[g_dialog.target] :
                           "Storage was not fully reset. Check the media.");
    ai_album_ui_common_set_label_text(g_dialog.hint, "OK OR POWER TO CLOSE");
    for (index = 0U; index < FORMAT_CONFIRM_COUNT; ++index) {
        format_dialog_set_hidden(g_dialog.confirm_buttons[index], 1U);
    }
}

static void format_dialog_execute(void)
{
    int result;

    ai_album_ui_common_set_label_text(g_dialog.title, "RESETTING STORAGE");
    ai_album_ui_common_set_label_text(
        g_dialog.message, "Please wait. Do not remove storage.");
    ai_album_ui_common_set_label_text(
        g_dialog.hint, "STORAGE RESET IN PROGRESS");
    format_dialog_set_hidden(g_dialog.confirm_buttons[FORMAT_CONFIRM_NO], 1U);
    format_dialog_set_hidden(g_dialog.confirm_buttons[FORMAT_CONFIRM_YES], 1U);
    lv_refr_now(NULL);
    if (g_dialog.target != AI_ALBUM_ALBUM_STORE_FORMAT_FLASH) {
        ai_album_album_image_ai_cancel();
    }
    result = ai_album_album_store_format(g_dialog.target);
    format_dialog_show_result(result);
}

int ai_album_settings_format_dialog_open(lv_obj_t *screen)
{
    uint8_t index;
    uint8_t found = 0U;

    if (screen == NULL || g_dialog.overlay != NULL) return RET_ERR;
    memset(&g_dialog, 0, sizeof(g_dialog));
    (void)ai_album_album_store_refresh();
    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_available); ++index) {
        g_dialog.target_available[index] =
            ai_album_album_store_format_available(
                (ai_album_album_store_format_target_t)index);
    }
    g_dialog.target = AI_ALBUM_ALBUM_STORE_FORMAT_SD;
    for (index = 0U; index < ARRAY_SIZE(g_dialog.target_available); ++index) {
        if (g_dialog.target_available[index]) {
            g_dialog.target = (ai_album_album_store_format_target_t)index;
            found = 1U;
            break;
        }
    }
    if (!found) {
        memset(&g_dialog, 0, sizeof(g_dialog));
        return RET_ERR;
    }
    if (format_dialog_create_objects(screen) != RET_OK) {
        ai_album_settings_format_dialog_destroy();
        return RET_ERR;
    }
    format_dialog_show_target();
    return RET_OK;
}

uint8_t ai_album_settings_format_dialog_is_open(void)
{
    return (uint8_t)(g_dialog.overlay != NULL);
}

static void format_dialog_handle_target(ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        format_dialog_move_target(-1);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        format_dialog_move_target(1);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        format_dialog_show_confirm();
    }
}

static void format_dialog_handle_confirm(ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_UP) {
        g_dialog.confirm_focus = FORMAT_CONFIRM_NO;
        format_dialog_update_confirm_focus();
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT ||
               action == AI_ALBUM_UI_ACTION_DOWN) {
        g_dialog.confirm_focus = FORMAT_CONFIRM_YES;
        format_dialog_update_confirm_focus();
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        if (g_dialog.confirm_focus == FORMAT_CONFIRM_YES) {
            format_dialog_execute();
        } else {
            ai_album_settings_format_dialog_destroy();
        }
    }
}

uint8_t ai_album_settings_format_dialog_handle_action(
    ai_album_ui_action_t action)
{
    if (g_dialog.overlay == NULL) return 0U;
    if (action == AI_ALBUM_UI_ACTION_BACK ||
        action == AI_ALBUM_UI_ACTION_MENU) {
        ai_album_settings_format_dialog_destroy();
        return 1U;
    }
    if (g_dialog.stage == FORMAT_DIALOG_STAGE_TARGET) {
        format_dialog_handle_target(action);
    } else if (g_dialog.stage == FORMAT_DIALOG_STAGE_CONFIRM) {
        format_dialog_handle_confirm(action);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        ai_album_settings_format_dialog_destroy();
    }
    return 1U;
}

void ai_album_settings_format_dialog_destroy(void)
{
    if (g_dialog.overlay != NULL && lv_obj_is_valid(g_dialog.overlay)) {
        lv_obj_delete(g_dialog.overlay);
    }
    memset(&g_dialog, 0, sizeof(g_dialog));
}
