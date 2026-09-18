#ifndef AI_ALBUM_I18N_H
#define AI_ALBUM_I18N_H

#include "ui/ai_album_compat.h"
#include "ui/ai_album_language.h"

const char *ai_album_i18n_text(const char *source);
const char *ai_album_i18n_text_for(ai_album_language_t language,
                                   const char *source);
void ai_album_i18n_set_label_text(lv_obj_t *label, const char *source,
                                  const lv_font_t *english_font);
void ai_album_i18n_set_label_raw(lv_obj_t *label, const char *text,
                                 const lv_font_t *english_font);
void ai_album_i18n_set_label_native(lv_obj_t *label,
                                    ai_album_language_t language,
                                    const char *text,
                                    const lv_font_t *english_font);

#endif
