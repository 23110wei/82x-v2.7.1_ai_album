#ifndef AI_ALBUM_ALBUM_STATUS_H
#define AI_ALBUM_ALBUM_STATUS_H

#include <stdint.h>

#include "ui/ai_album_compat.h"

const char *ai_album_album_status_empty_text(void);
void ai_album_album_status_set_photo(lv_obj_t *label, const char *text);
void ai_album_album_status_set_gallery(lv_obj_t *count_label,
                                       lv_obj_t *storage_label,
                                       uint8_t count);

#endif
