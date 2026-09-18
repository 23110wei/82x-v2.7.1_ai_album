#ifndef AI_ALBUM_UI_COMMON_H
#define AI_ALBUM_UI_COMMON_H

#include "ui/ai_album_compat.h"

#define AI_ALBUM_UI_COLOR_BG       0xE7F0EEU
#define AI_ALBUM_UI_COLOR_NAVY     0x14202DU
#define AI_ALBUM_UI_COLOR_TOPBAR   0x152A2EU
#define AI_ALBUM_UI_COLOR_TEXT     0x13232EU
#define AI_ALBUM_UI_COLOR_MUTED    0x657B79U
#define AI_ALBUM_UI_COLOR_WHITE    0xFFFFFFU
#define AI_ALBUM_UI_COLOR_GREEN    0x27B58BU
#define AI_ALBUM_UI_COLOR_BLUE     0x527EE5U
#define AI_ALBUM_UI_COLOR_PURPLE   0x8B63D7U
#define AI_ALBUM_UI_COLOR_ORANGE   0xE48843U
#define AI_ALBUM_UI_COLOR_RED      0xD95F5FU

lv_obj_t *ai_album_ui_common_prepare(lv_display_t *display,
                                     const char *title,
                                     uint32_t background);
lv_obj_t *ai_album_ui_common_plain(lv_obj_t *parent);
lv_obj_t *ai_album_ui_common_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color);
void ai_album_ui_common_set_label_text(lv_obj_t *label, const char *text);
void ai_album_ui_common_set_label_raw(lv_obj_t *label, const char *text);
lv_obj_t *ai_album_ui_common_panel(lv_obj_t *parent, uint32_t color,
                                   int32_t radius);
lv_obj_t *ai_album_ui_common_button(lv_obj_t *parent, const char *title,
                                    const char *subtitle);
void ai_album_ui_common_focus(lv_obj_t *obj, uint8_t focused,
                              uint32_t accent);
/* Persistent selection marker, independent of the transient focus ring. */
void ai_album_ui_common_select(lv_obj_t *obj, uint8_t selected,
                               uint32_t accent);
void ai_album_ui_common_footer(lv_obj_t *screen, const char *hint);
void ai_album_ui_common_format_volume(char *buffer, size_t capacity);
void ai_album_ui_common_refresh_volume(void);
void ai_album_ui_common_set_screen_chrome_hidden(lv_obj_t *topbar,
                                                  lv_obj_t *footer,
                                                  uint8_t hidden);

#endif
