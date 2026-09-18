#ifndef AI_ALBUM_POWER_DIALOG_H
#define AI_ALBUM_POWER_DIALOG_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"

/* Global power-off confirmation dialog, drawn on the LVGL top layer so it
 * works from any page. Opened via AI_ALBUM_UI_ACTION_POWER_OFF (POWER key
 * long press); the router feeds actions here while it is open. */
int ai_album_power_dialog_open(void);
uint8_t ai_album_power_dialog_is_open(void);
uint8_t ai_album_power_dialog_handle_action(ai_album_ui_action_t action);
void ai_album_power_dialog_destroy(void);

#endif
