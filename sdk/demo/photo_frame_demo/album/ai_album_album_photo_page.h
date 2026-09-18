#ifndef AI_ALBUM_ALBUM_PHOTO_PAGE_H
#define AI_ALBUM_ALBUM_PHOTO_PAGE_H

#include <stdint.h>

#include "album/ai_album_album_art.h"

typedef struct {
    ai_album_album_art_view_t *art;
    lv_obj_t *counter_label;
    lv_obj_t *status_label;
    uint8_t *rendered_index;
    uint8_t *rendered_valid;
    uint32_t *rendered_store_version;
} ai_album_album_photo_page_context_t;

void ai_album_album_photo_page_render(
    const ai_album_album_photo_page_context_t *context);

#endif
