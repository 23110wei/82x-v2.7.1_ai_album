#ifndef AI_ALBUM_SETTINGS_WIFI_PASSWORD_H
#define AI_ALBUM_SETTINGS_WIFI_PASSWORD_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_ui_input.h"
#include "ui/ai_album_ui_route.h"

int ai_album_settings_wifi_password_create(lv_display_t *display,
                                            const char *ssid);
int ai_album_settings_wifi_password_show(void);
ai_album_ui_route_t ai_album_settings_wifi_password_poll(void);
void ai_album_settings_wifi_password_destroy(void);
ai_album_ui_route_t ai_album_settings_wifi_password_handle_action(
    ai_album_ui_action_t action);

#endif
