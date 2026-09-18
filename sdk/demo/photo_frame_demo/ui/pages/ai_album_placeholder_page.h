#ifndef AI_ALBUM_PLACEHOLDER_PAGE_H
#define AI_ALBUM_PLACEHOLDER_PAGE_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"
#include "ui/ai_album_ui_route.h"

/*
 * 未移植页面的占位页:显示页面名+"功能开发中",
 * BACK/MENU返回主页。后续各页面逐个替换。
 */
int ai_album_placeholder_page_create(lv_display_t *display,
                                     ai_album_ui_route_t route);
int ai_album_placeholder_page_show(void);
void ai_album_placeholder_page_destroy(void);
ai_album_ui_route_t ai_album_placeholder_page_handle_action(
    ai_album_ui_action_t action);

#endif
