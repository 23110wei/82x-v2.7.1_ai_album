#ifndef AI_ALBUM_LANGUAGE_H
#define AI_ALBUM_LANGUAGE_H

#include <stdint.h>

typedef enum {
    AI_ALBUM_LANGUAGE_ENGLISH = 0,
    AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED,
    AI_ALBUM_LANGUAGE_JAPANESE,
    AI_ALBUM_LANGUAGE_COUNT,
} ai_album_language_t;

int ai_album_language_init(void);
ai_album_language_t ai_album_language_get(void);
int ai_album_language_set(ai_album_language_t language);
uint32_t ai_album_language_revision(void);
const char *ai_album_language_locale(ai_album_language_t language);
const char *ai_album_language_native_name(ai_album_language_t language);

#endif
