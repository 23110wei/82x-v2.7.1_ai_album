#ifndef AI_ALBUM_FONT_MANAGER_H
#define AI_ALBUM_FONT_MANAGER_H

#include "lvgl.h"

#include <stdbool.h>

const lv_font_t *ai_album_font_ui(void);
void ai_album_font_set_ui_text(lv_obj_t *label, const char *text);
bool ai_album_font_set_dynamic_text(lv_obj_t *label, const char *locale,
                                    const char *text);

#endif
