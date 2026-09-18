#ifndef AI_ALBUM_ALBUM_GALLERY_PAGE_H
#define AI_ALBUM_ALBUM_GALLERY_PAGE_H

#include <stdint.h>

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"

typedef enum {
    AI_ALBUM_GALLERY_CONFIRM_NOT_ACTIVE = 0,
    AI_ALBUM_GALLERY_CONFIRM_CONSUMED,
    AI_ALBUM_GALLERY_CONFIRM_OPEN_IMAGE_AI,
    AI_ALBUM_GALLERY_CONFIRM_DELETE_PHOTO,
} ai_album_gallery_confirm_result_t;

int ai_album_album_gallery_page_create(lv_obj_t *screen);
int ai_album_album_gallery_page_show(lv_obj_t *screen);
void ai_album_album_gallery_page_stop(lv_obj_t *screen);
void ai_album_album_gallery_page_destroy(lv_obj_t *screen);
int ai_album_album_gallery_page_move(ai_album_ui_action_t action);
uint8_t ai_album_album_gallery_page_focused(void);
int ai_album_album_gallery_page_open_image_ai_confirm(void);
int ai_album_album_gallery_page_delete_selected(void);
ai_album_gallery_confirm_result_t
ai_album_album_gallery_page_handle_image_ai_confirm(
    ai_album_ui_action_t action);

#endif
