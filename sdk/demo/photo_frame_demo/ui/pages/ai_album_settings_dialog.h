#ifndef AI_ALBUM_SETTINGS_DIALOG_H
#define AI_ALBUM_SETTINGS_DIALOG_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"

typedef enum {
    AI_ALBUM_SETTINGS_DIALOG_LANGUAGE = 0,
    AI_ALBUM_SETTINGS_DIALOG_BRIGHTNESS,
    AI_ALBUM_SETTINGS_DIALOG_ABOUT,
} ai_album_settings_dialog_type_t;

int ai_album_settings_dialog_open(
    lv_obj_t *screen, ai_album_settings_dialog_type_t type);
uint8_t ai_album_settings_dialog_is_open(void);
uint8_t ai_album_settings_dialog_handle_action(
    ai_album_ui_action_t action);
void ai_album_settings_dialog_destroy(void);
const char *ai_album_settings_dialog_language_name(void);
const char *ai_album_settings_dialog_brightness_name(void);

#endif
