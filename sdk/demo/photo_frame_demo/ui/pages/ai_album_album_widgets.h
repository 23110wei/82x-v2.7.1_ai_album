#ifndef AI_ALBUM_ALBUM_WIDGETS_H
#define AI_ALBUM_ALBUM_WIDGETS_H

#include "album/ai_album_album_art.h"
#include "ui/ai_album_compat.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *subtitle;
} ai_album_album_nav_button_t;

typedef struct {
    ai_album_album_art_view_t view;
} ai_album_album_gallery_card_t;

void ai_album_album_widgets_create_heading(lv_obj_t *screen,
                                            const char *eyebrow,
                                            const char *title);
ai_album_album_nav_button_t ai_album_album_widgets_create_photo_nav(
    lv_obj_t *screen, const char *title, const char *subtitle, int32_t x);
ai_album_album_gallery_card_t ai_album_album_widgets_create_gallery_card(
    lv_obj_t *screen,
    const ai_album_album_image_bounds_t *bounds,
    const ai_album_album_photo_t *photo);

#endif
