#ifndef AI_ALBUM_VOLUME_H
#define AI_ALBUM_VOLUME_H

#include <stdint.h>

#define AI_ALBUM_VOLUME_LEVEL_COUNT 5U

int ai_album_volume_init(void);
uint8_t ai_album_volume_get_percent(void);
int ai_album_volume_adjust(int8_t delta);
void ai_album_volume_restore(void);

#endif
