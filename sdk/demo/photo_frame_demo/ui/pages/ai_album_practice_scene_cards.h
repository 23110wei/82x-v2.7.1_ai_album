#ifndef AI_ALBUM_PRACTICE_SCENE_CARDS_H
#define AI_ALBUM_PRACTICE_SCENE_CARDS_H

#include "ui/ai_album_compat.h"

#include <stdint.h>

#define AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT 3U

typedef struct {
    lv_obj_t *cards[AI_ALBUM_PRACTICE_VISIBLE_SCENE_COUNT];
} ai_album_practice_scene_cards_t;

void ai_album_practice_scene_cards_create(
    ai_album_practice_scene_cards_t *view, lv_obj_t *parent);
void ai_album_practice_scene_cards_update(
    ai_album_practice_scene_cards_t *view, uint8_t selected_scene,
    uint8_t focused);

#endif
