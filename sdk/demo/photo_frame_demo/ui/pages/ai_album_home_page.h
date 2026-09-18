#ifndef AI_ALBUM_HOME_PAGE_H
#define AI_ALBUM_HOME_PAGE_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"
#include "ui/ai_album_ui_model.h"
#include "ui/ai_album_ui_route.h"

int ai_album_home_page_create(lv_display_t *display,
                              const ai_album_home_model_t *model);
void ai_album_home_page_update(const ai_album_home_model_t *model);
int ai_album_home_page_show(void);
void ai_album_home_page_destroy(void);
ai_album_ui_route_t ai_album_home_page_handle_action(
    ai_album_ui_action_t action);

#endif
