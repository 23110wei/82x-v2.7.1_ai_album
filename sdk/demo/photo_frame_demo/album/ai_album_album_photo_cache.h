#ifndef AI_ALBUM_ALBUM_PHOTO_CACHE_H
#define AI_ALBUM_ALBUM_PHOTO_CACHE_H

#include <stdint.h>

#include "album/ai_album_album_art.h"

int ai_album_album_photo_cache_start(ai_album_album_art_view_t *art);
void ai_album_album_photo_cache_pause(void);
void ai_album_album_photo_cache_resume(void);
uint8_t ai_album_album_photo_cache_is_loading(void);
void ai_album_album_photo_cache_stop(void);
int ai_album_album_photo_cache_move(int8_t delta);

#endif
