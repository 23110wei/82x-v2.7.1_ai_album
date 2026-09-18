#include "ui/pages/ai_album_practice_scene_cards.h"

#include "basic_include.h"
#include "ui/ai_album_language.h"
#include "ui/ai_album_font_manager.h"
#include "ui/ai_album_practice_runtime.h"
#include "ui/ai_album_ui_common.h"

#include <string.h>

static const uint32_t g_scene_colors[AI_ALBUM_PRACTICE_SCENE_COUNT] = {
    0xD98548U, 0x4D83B5U, 0x7E6AB0U,
    0x4C8A6AU, 0xB86472U, 0x397D86U,
};

static const int32_t
    g_scene_positions[AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT] = {
        40, 367, 694,
    };

static void practice_scene_card_create(
    ai_album_practice_scene_cards_t *view, lv_obj_t *parent, uint8_t slot)
{
    const ai_album_practice_scene_t *scene =
        ai_album_practice_runtime_scene(slot);
    lv_obj_t *card = ai_album_ui_common_panel(
        parent, g_scene_colors[slot], 18);
    lv_obj_t *label;

    view->cards[slot] = card;
    lv_obj_set_pos(card, g_scene_positions[slot], 102);
    lv_obj_set_size(card, 290, 180);
    label = ai_album_ui_common_label(
        card, "A1 / A2  SPEAKING", &lv_font_montserrat_14, 0xFFF1E6U);
    lv_obj_set_pos(label, 20, 22);
    label = ai_album_ui_common_label(
        card, scene->title, &lv_font_montserrat_20,
        AI_ALBUM_UI_COLOR_WHITE);
    lv_obj_set_pos(label, 20, 66);
    if (ai_album_language_get() == AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED) {
        label = ai_album_ui_common_label(
            card, scene->chinese_title, ai_album_font_ui(),
            AI_ALBUM_UI_COLOR_WHITE);
        lv_obj_set_pos(label, 20, 108);
    }
    label = ai_album_ui_common_label(
        card, "OK  START", &lv_font_montserrat_14, 0xFFF1E6U);
    lv_obj_set_pos(label, 20, 144);
}

void ai_album_practice_scene_cards_create(
    ai_album_practice_scene_cards_t *view, lv_obj_t *parent)
{
    uint8_t slot;

    if (view == NULL || parent == NULL) return;
    memset(view, 0, sizeof(*view));
    for (slot = 0U; slot < AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT; ++slot) {
        practice_scene_card_create(view, parent, slot);
    }
}

void ai_album_practice_scene_cards_update(
    ai_album_practice_scene_cards_t *view, uint8_t selected_scene,
    uint8_t focused)
{
    uint8_t first_scene;
    uint8_t slot;

    if (view == NULL || selected_scene >= AI_ALBUM_PRACTICE_SCENE_COUNT) return;
    first_scene = (uint8_t)(selected_scene -
        selected_scene % AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT);
    for (slot = 0U; slot < AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT; ++slot) {
        uint8_t scene_index = (uint8_t)(
            (first_scene + slot) % AI_ALBUM_PRACTICE_SCENE_COUNT);
        const ai_album_practice_scene_t *scene =
            ai_album_practice_runtime_scene(scene_index);

        lv_obj_set_style_bg_color(view->cards[slot],
                                  lv_color_hex(g_scene_colors[scene_index]),
                                  LV_PART_MAIN);
        ai_album_ui_common_set_label_text(
            lv_obj_get_child(view->cards[slot], 1), scene->title);
        if (ai_album_language_get() ==
            AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED &&
            lv_obj_get_child_count(view->cards[slot]) > 2) {
            ai_album_ui_common_set_label_raw(
                lv_obj_get_child(view->cards[slot], 2),
                scene->chinese_title);
        }
        ai_album_ui_common_focus(view->cards[slot],
                                 focused && scene_index == selected_scene,
                                 AI_ALBUM_UI_COLOR_ORANGE);
    }
}
