#include "ui/pages/ai_album_album_gallery_page.h"

#include "album/ai_album_album_gallery_loader.h"
#include "album/ai_album_album_perf.h"
#include "album/ai_album_album_status.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"
#include "ui/ai_album_ui_common.h"
#include "ui/ai_album_i18n.h"
#include "ui/pages/ai_album_album_widgets.h"

enum {
    ALBUM_GALLERY_COLUMNS = 4U,
    ALBUM_GALLERY_CARD_X = 24,
    ALBUM_GALLERY_CARD_Y = 126,
    ALBUM_GALLERY_CARD_X_STEP = 248,
    ALBUM_GALLERY_CARD_Y_STEP = 140,
    ALBUM_GALLERY_CARD_WIDTH = 232,
    ALBUM_GALLERY_CARD_HEIGHT = 128,
    ALBUM_GALLERY_ACTION_DELETE = 0U,
    ALBUM_GALLERY_ACTION_IMAGE_AI,
    ALBUM_GALLERY_ACTION_COUNT,
};

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *status_label;
    lv_obj_t *storage_status_label;
    lv_obj_t *page_label;
    lv_obj_t *empty_label;
    lv_obj_t *confirm_overlay;
    lv_obj_t *confirm_buttons[ALBUM_GALLERY_ACTION_COUNT];
    ai_album_album_art_view_t
        cards[AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE];
    uint8_t focus;
    uint8_t current_page;
    uint8_t confirm_focus;
    uint8_t confirm_active;
} album_gallery_page_state_t;

static album_gallery_page_state_t g_gallery_page;

static uint8_t gallery_page_count(uint8_t photo_count)
{
    return photo_count == 0U ? 0U :
           (uint8_t)((photo_count + AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE - 1U) /
                     AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
}

static ai_album_album_image_bounds_t gallery_page_card_bounds(uint8_t cell)
{
    ai_album_album_image_bounds_t bounds = {
        .x = ALBUM_GALLERY_CARD_X +
             (cell % ALBUM_GALLERY_COLUMNS) * ALBUM_GALLERY_CARD_X_STEP,
        .y = ALBUM_GALLERY_CARD_Y +
             (cell / ALBUM_GALLERY_COLUMNS) * ALBUM_GALLERY_CARD_Y_STEP,
        .width = ALBUM_GALLERY_CARD_WIDTH,
        .height = ALBUM_GALLERY_CARD_HEIGHT,
    };

    return bounds;
}

static int gallery_page_create_cards(lv_obj_t *screen)
{
    uint8_t cell;

    for (cell = 0U; cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE; ++cell) {
        ai_album_album_image_bounds_t bounds =
            gallery_page_card_bounds(cell);
        ai_album_album_gallery_card_t card =
            ai_album_album_widgets_create_gallery_card(screen, &bounds, NULL);

        if (card.view.root == NULL) return RET_ERR;
        g_gallery_page.cards[cell] = card.view;
        lv_obj_add_flag(card.view.root, LV_OBJ_FLAG_HIDDEN);
    }
    return RET_OK;
}

static void gallery_page_update_page_label(uint8_t photo_count)
{
    char text[24];
    uint8_t page_count = gallery_page_count(photo_count);

    os_snprintf(text, sizeof(text), ai_album_i18n_text("PAGE %u / %u"),
                (unsigned)(page_count == 0U ? 0U :
                           g_gallery_page.current_page + 1U),
                (unsigned)page_count);
    ai_album_ui_common_set_label_raw(g_gallery_page.page_label, text);
}

static void gallery_page_update_summary(void)
{
    uint8_t photo_count = ai_album_album_store_count();

    ai_album_album_status_set_gallery(g_gallery_page.status_label,
                                      g_gallery_page.storage_status_label,
                                      photo_count);
    gallery_page_update_page_label(photo_count);
    lv_obj_set_flag(g_gallery_page.empty_label, LV_OBJ_FLAG_HIDDEN,
                    (uint8_t)(photo_count != 0U));
}

static void gallery_page_hide_cards(void)
{
    uint8_t cell;

    for (cell = 0U; cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE; ++cell) {
        if (g_gallery_page.cards[cell].root != NULL) {
            lv_obj_add_flag(g_gallery_page.cards[cell].root,
                            LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void gallery_page_set_focus_style(uint8_t cell, uint8_t focused)
{
    lv_obj_t *root;

    if (cell >= AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE) return;
    root = g_gallery_page.cards[cell].root;
    if (root != NULL && lv_obj_is_valid(root)) {
        ai_album_ui_common_focus(root, focused, AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void gallery_page_update_focus(void)
{
    uint8_t cell;
    uint8_t focused_cell =
        (uint8_t)(g_gallery_page.focus % AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
    uint8_t photo_count = ai_album_album_store_count();

    for (cell = 0U; cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE; ++cell) {
        uint8_t focused = (uint8_t)(photo_count != 0U &&
                                    cell == focused_cell);

        gallery_page_set_focus_style(cell, focused);
    }
}

static void gallery_page_update_focus_delta(uint8_t previous_cell,
                                            uint8_t focused_cell)
{
    if (previous_cell != focused_cell) {
        gallery_page_set_focus_style(previous_cell, 0U);
    }
    gallery_page_set_focus_style(focused_cell, 1U);
}

static void gallery_page_create_labels(lv_obj_t *screen)
{
    ai_album_album_widgets_create_heading(screen, "ALBUM", "CHOOSE A PHOTO");
    g_gallery_page.status_label = ai_album_ui_common_label(
        screen, "0 PHOTOS", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(g_gallery_page.status_label, 850, 70);
    g_gallery_page.storage_status_label = ai_album_ui_common_label(
        screen, "SD NOT SCANNED", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(g_gallery_page.storage_status_label, 500, 96);
    /* 文本由 gallery_page_update_page_label 按语言填充, 这里保持空串 */
    g_gallery_page.page_label = ai_album_ui_common_label(
        screen, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(g_gallery_page.page_label, 850, 96);
    g_gallery_page.empty_label = ai_album_ui_common_label(
        screen, ai_album_album_status_empty_text(), &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_align(g_gallery_page.empty_label, LV_ALIGN_CENTER, 0, 20);
}

static void gallery_page_update_confirm_focus(void)
{
    uint8_t index;

    for (index = 0U; index < ALBUM_GALLERY_ACTION_COUNT; ++index) {
        ai_album_ui_common_focus(
            g_gallery_page.confirm_buttons[index],
            (uint8_t)(index == g_gallery_page.confirm_focus),
            AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void gallery_page_discard_confirm_overlay(void)
{
    if (g_gallery_page.confirm_overlay != NULL &&
        lv_obj_is_valid(g_gallery_page.confirm_overlay)) {
        lv_obj_delete(g_gallery_page.confirm_overlay);
    }
    g_gallery_page.confirm_overlay = NULL;
    g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_DELETE] = NULL;
    g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_IMAGE_AI] = NULL;
    g_gallery_page.confirm_active = 0U;
}

static lv_obj_t *gallery_page_create_confirm_button(
    lv_obj_t *parent, const char *title, const char *subtitle, int32_t x)
{
    lv_obj_t *button = ai_album_ui_common_button(parent, title, subtitle);

    lv_obj_set_pos(button, x, 130);
    lv_obj_set_size(button, 260, 78);
    return button;
}

static int gallery_page_create_image_ai_confirm(lv_obj_t *screen)
{
    lv_obj_t *dialog;
    lv_obj_t *label;

    if (screen == NULL) return RET_ERR;
    g_gallery_page.confirm_overlay = ai_album_ui_common_plain(screen);
    if (g_gallery_page.confirm_overlay == NULL) return RET_ERR;
    lv_obj_set_size(g_gallery_page.confirm_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_gallery_page.confirm_overlay,
                              lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_gallery_page.confirm_overlay,
                            LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(g_gallery_page.confirm_overlay,
                    LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN);
    dialog = ai_album_ui_common_panel(
        g_gallery_page.confirm_overlay, AI_ALBUM_UI_COLOR_WHITE, 20);
    if (dialog == NULL) goto fail;
    lv_obj_set_size(dialog, 620, 280);
    lv_obj_center(dialog);
    label = ai_album_ui_common_label(
        dialog, "PHOTO ACTION", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    if (label == NULL) goto fail;
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 25);
    label = ai_album_ui_common_label(
        dialog, "Choose an action for the selected photo.",
        &lv_font_montserrat_16, AI_ALBUM_UI_COLOR_MUTED);
    if (label == NULL) goto fail;
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 72);
    g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_DELETE] =
        gallery_page_create_confirm_button(
            dialog, "DELETE PHOTO", "REMOVE FROM SD CARD", 30);
    if (g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_DELETE] == NULL) {
        goto fail;
    }
    g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_IMAGE_AI] =
        gallery_page_create_confirm_button(
            dialog, "IMAGE AI", "OPEN GENERATION", 330);
    if (g_gallery_page.confirm_buttons[ALBUM_GALLERY_ACTION_IMAGE_AI] == NULL) {
        goto fail;
    }
    label = ai_album_ui_common_label(
        dialog, "ARROWS SELECT   OK CONFIRM   POWER CANCEL",
        &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);
    if (label == NULL) goto fail;
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -18);
    gallery_page_update_confirm_focus();
    return RET_OK;

fail:
    gallery_page_discard_confirm_overlay();
    return RET_ERR;
}

int ai_album_album_gallery_page_create(lv_obj_t *screen)
{
    if (screen == NULL || g_gallery_page.screen != NULL) return RET_ERR;
    memset(&g_gallery_page, 0, sizeof(g_gallery_page));
    g_gallery_page.screen = screen;
    gallery_page_create_labels(screen);
    if (g_gallery_page.status_label == NULL ||
        g_gallery_page.storage_status_label == NULL ||
        g_gallery_page.page_label == NULL ||
        g_gallery_page.empty_label == NULL ||
        gallery_page_create_cards(screen) != RET_OK) {
        memset(&g_gallery_page, 0, sizeof(g_gallery_page));
        return RET_ERR;
    }
    ai_album_ui_common_footer(
        screen, "POWER BACK   ARROWS SELECT   OK VIEW   HOLD OK ACTIONS");
    if (gallery_page_create_image_ai_confirm(screen) != RET_OK) {
        memset(&g_gallery_page, 0, sizeof(g_gallery_page));
        return RET_ERR;
    }
    gallery_page_update_summary();
    return RET_OK;
}

int ai_album_album_gallery_page_show(lv_obj_t *screen)
{
    uint8_t photo_count;
    uint8_t selected;

    if (screen == NULL || screen != g_gallery_page.screen) return RET_ERR;
    ai_album_album_gallery_loader_stop();
    photo_count = ai_album_album_store_count();
    selected = ai_album_album_store_selected();
    if (photo_count == 0U) {
        g_gallery_page.focus = 0U;
        g_gallery_page.current_page = 0U;
        gallery_page_hide_cards();
        gallery_page_update_summary();
        gallery_page_update_focus();
        return RET_OK;
    }
    if (selected >= photo_count) selected = 0U;
    g_gallery_page.focus = selected;
    g_gallery_page.current_page =
        (uint8_t)(selected / AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
    gallery_page_update_summary();
    if (ai_album_album_gallery_loader_start(
            g_gallery_page.cards, AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE,
            g_gallery_page.current_page) != RET_OK) {
        return RET_ERR;
    }
    gallery_page_update_focus();
    return RET_OK;
}

void ai_album_album_gallery_page_stop(lv_obj_t *screen)
{
    if (screen == NULL || screen != g_gallery_page.screen) return;
    ai_album_album_gallery_loader_stop();
}

void ai_album_album_gallery_page_destroy(lv_obj_t *screen)
{
    if (screen == NULL || screen != g_gallery_page.screen) return;
    ai_album_album_gallery_loader_stop();
    memset(&g_gallery_page, 0, sizeof(g_gallery_page));
}

static int8_t gallery_page_move_delta(ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT) return -1;
    if (action == AI_ALBUM_UI_ACTION_RIGHT) return 1;
    if (action == AI_ALBUM_UI_ACTION_UP) return -ALBUM_GALLERY_COLUMNS;
    if (action == AI_ALBUM_UI_ACTION_DOWN) return ALBUM_GALLERY_COLUMNS;
    return 0;
}

int ai_album_album_gallery_page_move(ai_album_ui_action_t action)
{
    uint8_t photo_count = ai_album_album_store_count();
    uint8_t previous_cell =
        (uint8_t)(g_gallery_page.focus % AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
    int8_t delta = gallery_page_move_delta(action);
    int16_t next;
    uint8_t next_page;

    if (g_gallery_page.screen == NULL || photo_count == 0U || delta == 0) {
        return RET_ERR;
    }
    next = (int16_t)g_gallery_page.focus + delta;
    while (next < 0) next += photo_count;
    next %= photo_count;
    if ((uint8_t)next == g_gallery_page.focus) return RET_OK;
    next_page = (uint8_t)((uint8_t)next /
                          AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
    if (next_page != g_gallery_page.current_page &&
        ai_album_album_gallery_loader_show_page(next_page) != RET_OK) {
        os_printf("[ALBUM_GALLERY] page move failed target=%u\r\n",
                  (unsigned)(next_page + 1U));
        return RET_ERR;
    }
    ai_album_album_perf_begin(AI_ALBUM_ALBUM_PERF_FOCUS);
    g_gallery_page.focus = (uint8_t)next;
    g_gallery_page.current_page = next_page;
    gallery_page_update_page_label(photo_count);
    gallery_page_update_focus_delta(
        previous_cell,
        (uint8_t)(g_gallery_page.focus % AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE));
    return RET_OK;
}

uint8_t ai_album_album_gallery_page_focused(void)
{
    return g_gallery_page.focus;
}

int ai_album_album_gallery_page_open_image_ai_confirm(void)
{
    uint8_t photo_count = ai_album_album_store_count();

    if (g_gallery_page.screen == NULL ||
        g_gallery_page.confirm_overlay == NULL ||
        g_gallery_page.focus >= photo_count) {
        return RET_ERR;
    }
    /* Keep the non-destructive action selected when the menu opens. */
    g_gallery_page.confirm_focus = ALBUM_GALLERY_ACTION_IMAGE_AI;
    g_gallery_page.confirm_active = 1U;
    gallery_page_update_confirm_focus();
    lv_obj_clear_flag(g_gallery_page.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    return RET_OK;
}

int ai_album_album_gallery_page_delete_selected(void)
{
    uint8_t selected = g_gallery_page.focus;
    uint8_t photo_count = ai_album_album_store_count();

    if (g_gallery_page.screen == NULL || selected >= photo_count ||
        ai_album_album_store_select(selected) != RET_OK) {
        return RET_ERR;
    }
    ai_album_album_gallery_loader_stop();
    if (ai_album_album_store_delete(selected) != RET_OK) {
        (void)ai_album_album_gallery_page_show(g_gallery_page.screen);
        if (g_gallery_page.status_label != NULL) {
            ai_album_ui_common_set_label_text(
                g_gallery_page.status_label, "DELETE FAILED");
        }
        return RET_ERR;
    }
    return ai_album_album_gallery_page_show(g_gallery_page.screen);
}

static void gallery_page_close_image_ai_confirm(void)
{
    lv_obj_add_flag(g_gallery_page.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    g_gallery_page.confirm_active = 0U;
}

ai_album_gallery_confirm_result_t
ai_album_album_gallery_page_handle_image_ai_confirm(
    ai_album_ui_action_t action)
{
    uint8_t selected_action;

    if (!g_gallery_page.confirm_active) {
        return AI_ALBUM_GALLERY_CONFIRM_NOT_ACTIVE;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_UP) {
        g_gallery_page.confirm_focus = ALBUM_GALLERY_ACTION_DELETE;
        gallery_page_update_confirm_focus();
        return AI_ALBUM_GALLERY_CONFIRM_CONSUMED;
    }
    if (action == AI_ALBUM_UI_ACTION_RIGHT ||
        action == AI_ALBUM_UI_ACTION_DOWN) {
        g_gallery_page.confirm_focus = ALBUM_GALLERY_ACTION_IMAGE_AI;
        gallery_page_update_confirm_focus();
        return AI_ALBUM_GALLERY_CONFIRM_CONSUMED;
    }
    if (action == AI_ALBUM_UI_ACTION_BACK ||
        action == AI_ALBUM_UI_ACTION_MENU) {
        gallery_page_close_image_ai_confirm();
        return AI_ALBUM_GALLERY_CONFIRM_CONSUMED;
    }
    if (action != AI_ALBUM_UI_ACTION_OK) {
        return AI_ALBUM_GALLERY_CONFIRM_CONSUMED;
    }
    selected_action = g_gallery_page.confirm_focus;
    gallery_page_close_image_ai_confirm();
    return selected_action == ALBUM_GALLERY_ACTION_DELETE ?
               AI_ALBUM_GALLERY_CONFIRM_DELETE_PHOTO :
               AI_ALBUM_GALLERY_CONFIRM_OPEN_IMAGE_AI;
}
