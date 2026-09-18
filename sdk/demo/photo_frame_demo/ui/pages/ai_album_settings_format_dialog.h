#ifndef AI_ALBUM_SETTINGS_FORMAT_DIALOG_H
#define AI_ALBUM_SETTINGS_FORMAT_DIALOG_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"

int ai_album_settings_format_dialog_open(lv_obj_t *screen);
uint8_t ai_album_settings_format_dialog_is_open(void);
uint8_t ai_album_settings_format_dialog_handle_action(
    ai_album_ui_action_t action);
void ai_album_settings_format_dialog_destroy(void);

#endif
