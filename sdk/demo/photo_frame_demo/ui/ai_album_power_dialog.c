#include "ui/ai_album_power_dialog.h"

#include "basic_include.h"
#include "hardware/power_ctrl.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_ui_common.h"

typedef enum {
    POWER_DIALOG_STAGE_CONFIRM = 0,
    POWER_DIALOG_STAGE_SHUTDOWN,
} power_dialog_stage_t;

enum {
    POWER_DIALOG_CANCEL = 0U,
    POWER_DIALOG_CONFIRM,
    POWER_DIALOG_BUTTON_COUNT,
};

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *title;
    lv_obj_t *message;
    lv_obj_t *hint;
    lv_obj_t *buttons[POWER_DIALOG_BUTTON_COUNT];
    power_dialog_stage_t stage;
    uint8_t focus;
} power_dialog_state_t;

static power_dialog_state_t g_dialog;

static void power_dialog_set_hidden(lv_obj_t *object, uint8_t hidden)
{
    if (object != NULL) {
        lv_obj_set_flag(object, LV_OBJ_FLAG_HIDDEN, hidden);
    }
}

static void power_dialog_update_focus(void)
{
    ai_album_ui_common_focus(
        g_dialog.buttons[POWER_DIALOG_CANCEL],
        g_dialog.focus == POWER_DIALOG_CANCEL, AI_ALBUM_UI_COLOR_GREEN);
    ai_album_ui_common_focus(
        g_dialog.buttons[POWER_DIALOG_CONFIRM],
        g_dialog.focus == POWER_DIALOG_CONFIRM, AI_ALBUM_UI_COLOR_RED);
}

static void power_dialog_refresh_text(void)
{
    ai_album_i18n_set_label_text(g_dialog.title, "POWER OFF",
                                 &lv_font_montserrat_24);
    ai_album_i18n_set_label_text(g_dialog.message, "Power off the device?",
                                 &lv_font_montserrat_16);
    ai_album_i18n_set_label_text(
        g_dialog.hint, "LEFT / RIGHT SELECT   OK CONFIRM   POWER CANCEL",
        &lv_font_montserrat_14);
}

static lv_obj_t *power_dialog_create_button(lv_obj_t *parent,
                                            const char *title,
                                            const char *subtitle,
                                            const lv_font_t *title_font,
                                            const lv_font_t *subtitle_font,
                                            int32_t x, int32_t y)
{
    lv_obj_t *button = ai_album_ui_common_panel(
        parent, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_t *label;

    if (button == NULL) return NULL;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 280, 82);
    label = ai_album_ui_common_label(button, "", title_font,
                                     AI_ALBUM_UI_COLOR_TEXT);
    if (label == NULL) return NULL;
    lv_obj_set_pos(label, 18, 12);
    ai_album_i18n_set_label_text(label, title, title_font);
    label = ai_album_ui_common_label(button, "", subtitle_font,
                                     AI_ALBUM_UI_COLOR_MUTED);
    if (label == NULL) return NULL;
    lv_obj_set_pos(label, 18, 39);
    ai_album_i18n_set_label_text(label, subtitle, subtitle_font);
    return button;
}

static int power_dialog_create_objects(void)
{
    lv_obj_t *panel;

    g_dialog.overlay = ai_album_ui_common_plain(lv_layer_top());
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
    g_dialog.buttons[POWER_DIALOG_CANCEL] = power_dialog_create_button(
        panel, "CANCEL", "KEEP RUNNING", &lv_font_montserrat_16,
        &lv_font_montserrat_14, 105, 155);
    g_dialog.buttons[POWER_DIALOG_CONFIRM] = power_dialog_create_button(
        panel, "POWER OFF", "CONFIRM SHUTDOWN", &lv_font_montserrat_16,
        &lv_font_montserrat_14, 435, 155);
    if (g_dialog.buttons[POWER_DIALOG_CANCEL] == NULL ||
        g_dialog.buttons[POWER_DIALOG_CONFIRM] == NULL) return RET_ERR;
    power_dialog_refresh_text();
    return RET_OK;
}

int ai_album_power_dialog_open(void)
{
    if (g_dialog.overlay != NULL) return RET_ERR;
    memset(&g_dialog, 0, sizeof(g_dialog));
    if (power_dialog_create_objects() != RET_OK) {
        ai_album_power_dialog_destroy();
        return RET_ERR;
    }
    g_dialog.stage = POWER_DIALOG_STAGE_CONFIRM;
    g_dialog.focus = POWER_DIALOG_CANCEL;
    power_dialog_update_focus();
    os_printf("power_dialog: opened\r\n");
    return RET_OK;
}

uint8_t ai_album_power_dialog_is_open(void)
{
    return (uint8_t)(g_dialog.overlay != NULL);
}

static void power_dialog_start_shutdown(void)
{
    uint8_t index;

    g_dialog.stage = POWER_DIALOG_STAGE_SHUTDOWN;
    ai_album_i18n_set_label_text(g_dialog.message, "Powering off...",
                                 &lv_font_montserrat_16);
    ai_album_ui_common_set_label_text(g_dialog.hint, "");
    for (index = 0U; index < POWER_DIALOG_BUTTON_COUNT; ++index) {
        power_dialog_set_hidden(g_dialog.buttons[index], 1U);
    }
    /* Push the farewell frame through the display pipeline before the
     * shutdown sequence turns the panel rails off. */
    lv_refr_now(NULL);
    os_sleep_ms(150);
    os_printf("power_dialog: shutdown confirmed\r\n");
    power_ctrl_shutdown_sequence(); /* never returns */
}

uint8_t ai_album_power_dialog_handle_action(ai_album_ui_action_t action)
{
    if (g_dialog.overlay == NULL) return 0U;
    if (g_dialog.stage == POWER_DIALOG_STAGE_SHUTDOWN) {
        return 1U; /* power-down in progress: swallow everything */
    }
    if (action == AI_ALBUM_UI_ACTION_BACK ||
        action == AI_ALBUM_UI_ACTION_MENU ||
        action == AI_ALBUM_UI_ACTION_POWER_OFF) {
        ai_album_power_dialog_destroy();
        return 1U;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_UP) {
        g_dialog.focus = POWER_DIALOG_CANCEL;
        power_dialog_update_focus();
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT ||
               action == AI_ALBUM_UI_ACTION_DOWN) {
        g_dialog.focus = POWER_DIALOG_CONFIRM;
        power_dialog_update_focus();
    } else if (action == AI_ALBUM_UI_ACTION_OK &&
               g_dialog.focus == POWER_DIALOG_CONFIRM) {
        power_dialog_start_shutdown();
    } else if (action == AI_ALBUM_UI_ACTION_OK &&
               g_dialog.focus == POWER_DIALOG_CANCEL) {
        /* OK确认当前选中项:选中CANCEL即关闭弹窗返回原页面 */
        ai_album_power_dialog_destroy();
    }
    return 1U;
}

void ai_album_power_dialog_destroy(void)
{
    if (g_dialog.overlay != NULL && lv_obj_is_valid(g_dialog.overlay)) {
        lv_obj_delete(g_dialog.overlay);
    }
    memset(&g_dialog, 0, sizeof(g_dialog));
}
