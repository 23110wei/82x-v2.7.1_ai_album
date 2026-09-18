#include "ui/ai_album_font_manager.h"
#include "fonts/ai_album_sd_font.h"

#include <string.h>

#define AI_ALBUM_FONT_MISSING_TEXT \
    "字体文件缺失\n请检查 SD 卡 /ai_album/fonts"

LV_FONT_DECLARE(ai_album_font_ui_16);

static uint8_t ai_album_font_locale_is(const char *locale, const char *code)
{
    return (uint8_t)(locale != NULL && strncmp(locale, code, 2U) == 0);
}

static ai_album_sd_font_family_t ai_album_font_family(const char *locale)
{
    if (ai_album_font_locale_is(locale, "zh")) return AI_ALBUM_SD_FONT_SC;
    if (ai_album_font_locale_is(locale, "ja")) return AI_ALBUM_SD_FONT_JP;
    if (ai_album_font_locale_is(locale, "ko")) return AI_ALBUM_SD_FONT_KR;
    if (ai_album_font_locale_is(locale, "th")) return AI_ALBUM_SD_FONT_THAI;
    if (ai_album_font_locale_is(locale, "ar")) return AI_ALBUM_SD_FONT_ARABIC;
    if (ai_album_font_locale_is(locale, "he")) return AI_ALBUM_SD_FONT_HEBREW;
    if (ai_album_font_locale_is(locale, "hi")) {
        return AI_ALBUM_SD_FONT_DEVANAGARI;
    }
    return AI_ALBUM_SD_FONT_LATIN;
}

static const lv_font_t *ai_album_font_load(ai_album_sd_font_family_t family)
{
    return ai_album_sd_font_get(family, &ai_album_font_ui_16);
}

const lv_font_t *ai_album_font_ui(void)
{
    return &ai_album_font_ui_16;
}

void ai_album_font_set_ui_text(lv_obj_t *label, const char *text)
{
    if (label == NULL) return;
    lv_obj_set_style_text_font(label, ai_album_font_ui(), LV_PART_MAIN);
    lv_label_set_text(label, text == NULL ? "" : text);
}

static bool ai_album_font_applied(lv_obj_t *label, const lv_font_t *font,
                                  const char *text)
{
    const char *current = lv_label_get_text(label);

    return font != NULL &&
           lv_obj_get_style_text_font(label, LV_PART_MAIN) == font &&
           current != NULL && strcmp(current, text) == 0;
}

bool ai_album_font_set_dynamic_text(lv_obj_t *label, const char *locale,
                                    const char *text)
{
    const lv_font_t *font;

    if (label == NULL) return false;
    if (text == NULL) text = "";
    font = ai_album_font_load(ai_album_font_family(locale));
    if (font == NULL) {
        if (!ai_album_font_applied(label, ai_album_font_ui(),
                                   AI_ALBUM_FONT_MISSING_TEXT)) {
            ai_album_font_set_ui_text(label, AI_ALBUM_FONT_MISSING_TEXT);
        }
        return false;
    }
    if (ai_album_font_applied(label, font, text)) return true;
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_text(label, text);
    return true;
}
