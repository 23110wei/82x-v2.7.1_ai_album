#ifndef AI_ALBUM_SD_FONT_H
#define AI_ALBUM_SD_FONT_H

#include "lvgl.h"

typedef enum {
    AI_ALBUM_SD_FONT_LATIN = 0,
    AI_ALBUM_SD_FONT_SC,
    AI_ALBUM_SD_FONT_JP,
    AI_ALBUM_SD_FONT_KR,
    AI_ALBUM_SD_FONT_THAI,
    AI_ALBUM_SD_FONT_ARABIC,
    AI_ALBUM_SD_FONT_HEBREW,
    AI_ALBUM_SD_FONT_DEVANAGARI,
    AI_ALBUM_SD_FONT_FAMILY_COUNT,
} ai_album_sd_font_family_t;

const lv_font_t *ai_album_sd_font_get(ai_album_sd_font_family_t family,
                                      const lv_font_t *fallback);

/* 关闭引擎常开的字体文件句柄(关机卸载SD卷前调用,避免带开文件强卸) */
void ai_album_sd_font_close_files(void);

#endif
