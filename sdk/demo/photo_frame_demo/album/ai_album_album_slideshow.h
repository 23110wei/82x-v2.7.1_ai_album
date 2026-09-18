#ifndef AI_ALBUM_ALBUM_SLIDESHOW_H
#define AI_ALBUM_ALBUM_SLIDESHOW_H

#include <stdint.h>

#include "album/ai_album_album_art.h"

typedef void (*ai_album_album_slideshow_commit_cb_t)(
    uint8_t photo_index,
    void *user_data);

int ai_album_album_slideshow_start(
    ai_album_album_art_view_t *art,
    ai_album_album_slideshow_commit_cb_t commit_cb,
    void *user_data);
void ai_album_album_slideshow_stop(void);

#endif
