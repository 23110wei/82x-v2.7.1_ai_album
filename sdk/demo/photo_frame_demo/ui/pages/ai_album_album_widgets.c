#include "ui/pages/ai_album_album_widgets.h"

#include "ui/ai_album_ui_common.h"

void ai_album_album_widgets_create_heading(lv_obj_t *screen,
                                            const char *eyebrow,
                                            const char *title)
{
    lv_obj_t *label = ai_album_ui_common_label(
        screen, eyebrow, &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_GREEN);

    lv_obj_set_pos(label, 40, 70);
    label = ai_album_ui_common_label(
        screen, title, &lv_font_montserrat_24, AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(label, 40, 92);
}

ai_album_album_nav_button_t ai_album_album_widgets_create_photo_nav(
    lv_obj_t *screen, const char *title, const char *subtitle, int32_t x)
{
    ai_album_album_nav_button_t nav;

    nav.root = ai_album_ui_common_panel(screen, 0x15262BU, 14);
    nav.title = ai_album_ui_common_label(
        nav.root, title, &lv_font_montserrat_16, AI_ALBUM_UI_COLOR_WHITE);
    lv_obj_set_pos(nav.title, 18, subtitle == NULL ? 20 : 12);
    nav.subtitle = NULL;
    if (subtitle != NULL) {
        nav.subtitle = ai_album_ui_common_label(
            nav.root, subtitle, &lv_font_montserrat_14, 0xB9C9C6U);
        lv_obj_set_pos(nav.subtitle, 18, 39);
    }
    lv_obj_set_pos(nav.root, x, 500);
    lv_obj_set_size(nav.root, 480, 62);
    lv_obj_set_style_bg_opa(nav.root, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(nav.root, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(nav.root, lv_color_hex(0x536A6BU),
                                  LV_PART_MAIN);
    return nav;
}

ai_album_album_gallery_card_t ai_album_album_widgets_create_gallery_card(
    lv_obj_t *screen,
    const ai_album_album_image_bounds_t *bounds,
    const ai_album_album_photo_t *photo)
{
    ai_album_album_gallery_card_t card;

    card.view = ai_album_album_art_create(screen, bounds, 1U);
    ai_album_album_art_set_loading(&card.view, photo);
    return card;
}
