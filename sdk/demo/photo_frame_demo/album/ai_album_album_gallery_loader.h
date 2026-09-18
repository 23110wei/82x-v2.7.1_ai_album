#ifndef AI_ALBUM_ALBUM_GALLERY_LOADER_H
#define AI_ALBUM_ALBUM_GALLERY_LOADER_H

#include <stdint.h>

#include "album/ai_album_album_art.h"
#include "album/ai_album_album_image_loader.h"

enum {
    AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE = 12U,
};

int ai_album_album_gallery_loader_start(
    ai_album_album_art_view_t *cards,
    uint8_t card_capacity,
    uint8_t current_page);
int ai_album_album_gallery_loader_show_page(uint8_t page);
void ai_album_album_gallery_loader_stop(void);

#endif
