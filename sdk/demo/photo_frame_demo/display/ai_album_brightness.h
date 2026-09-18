#ifndef AI_ALBUM_BRIGHTNESS_H
#define AI_ALBUM_BRIGHTNESS_H

#include <stdint.h>

#define AI_ALBUM_BRIGHTNESS_LEVEL_COUNT 5U

int ai_album_brightness_init(void);
uint8_t ai_album_brightness_get_level(void);
uint8_t ai_album_brightness_get_percent(void);
int ai_album_brightness_preview_level(uint8_t level);
int ai_album_brightness_set_level(uint8_t level);
int ai_album_brightness_set_output_enabled(uint8_t enabled);

#endif
