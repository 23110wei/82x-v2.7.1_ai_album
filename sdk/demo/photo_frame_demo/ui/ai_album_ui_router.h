#ifndef AI_ALBUM_UI_ROUTER_H
#define AI_ALBUM_UI_ROUTER_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"
#include "ui/ai_album_ui_model.h"

int ai_album_ui_router_init(lv_display_t *display,
                            const ai_album_home_model_t *home_model);
void ai_album_ui_router_handle(ai_album_ui_action_t action);
void ai_album_ui_router_poll(void);

#endif
