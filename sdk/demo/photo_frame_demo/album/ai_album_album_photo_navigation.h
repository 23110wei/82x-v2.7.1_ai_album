#ifndef AI_ALBUM_ALBUM_PHOTO_NAVIGATION_H
#define AI_ALBUM_ALBUM_PHOTO_NAVIGATION_H

#include <stdint.h>

#include "album/ai_album_album_art.h"

typedef void (*ai_album_album_photo_navigation_cb_t)(void *user_data);
typedef uint8_t (*ai_album_album_photo_navigation_busy_cb_t)(void *user_data);

typedef struct {
    ai_album_album_art_view_t *art;
    lv_obj_t *counter_label;
    ai_album_album_photo_navigation_cb_t render_cb;
    ai_album_album_photo_navigation_cb_t pause_cache_cb;
    ai_album_album_photo_navigation_cb_t resume_cache_cb;
    ai_album_album_photo_navigation_busy_cb_t cache_busy_cb;
    void *user_data;
} ai_album_album_photo_navigation_config_t;

int ai_album_album_photo_navigation_start(
    const ai_album_album_photo_navigation_config_t *config);
int ai_album_album_photo_navigation_move(int8_t delta);
void ai_album_album_photo_navigation_poll(void);
void ai_album_album_photo_navigation_stop(void);
uint8_t ai_album_album_photo_navigation_is_active(void);
uint8_t ai_album_album_photo_navigation_is_cache_paused(void);

#endif
