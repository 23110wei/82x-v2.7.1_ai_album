#ifndef AI_ALBUM_ALBUM_PAGES_H
#define AI_ALBUM_ALBUM_PAGES_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"
#include "ui/ai_album_ui_route.h"

int ai_album_album_pages_create(lv_display_t *display,
                                ai_album_ui_route_t route);
int ai_album_album_pages_switch(ai_album_ui_route_t route);
int ai_album_album_pages_show(void);
void ai_album_album_pages_poll(void);
void ai_album_album_pages_destroy(void);
ai_album_ui_route_t ai_album_album_pages_handle_action(
    ai_album_ui_action_t action);

#endif
