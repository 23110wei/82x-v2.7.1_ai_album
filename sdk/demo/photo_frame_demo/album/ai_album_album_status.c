#include "album/ai_album_album_status.h"

#include "album/ai_album_album_store.h"
#include "basic_include.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_ui_common.h"

const char *ai_album_album_status_empty_text(void)
{
    ai_album_album_store_status_t status = ai_album_album_store_status();

    if (status == AI_ALBUM_ALBUM_STORE_STATUS_SD_UNAVAILABLE) {
        return "SD NOT READY  ·  CHECK CARD";
    }
    if (status == AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY) {
        return "SD READY  ·  NO JPEG PHOTOS";
    }
    return "NO PHOTOS";
}

void ai_album_album_status_set_photo(lv_obj_t *label, const char *text)
{
    if (label == NULL) {
        return;
    }
    if (text == NULL || text[0] == '\0') {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    ai_album_ui_common_set_label_text(label, text);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
}

static const char *album_status_storage_text(void)
{
    ai_album_album_store_status_t status = ai_album_album_store_status();

    if (status == AI_ALBUM_ALBUM_STORE_STATUS_SD_READY) {
        return "SD READY  ·  JPEG PHOTOS";
    }
    if (status == AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY) {
        return "SD READY  ·  NO JPEG PHOTOS";
    }
    if (status == AI_ALBUM_ALBUM_STORE_STATUS_SD_UNAVAILABLE) {
        return "SD NOT READY  ·  CHECK CARD";
    }
    return "SD NOT SCANNED";
}

void ai_album_album_status_set_gallery(lv_obj_t *count_label,
                                       lv_obj_t *storage_label,
                                       uint8_t count)
{
    char count_text[24];

    if (count_label != NULL) {
        os_snprintf(count_text, sizeof(count_text),
                    ai_album_i18n_text("%u PHOTOS"),
                    (unsigned)count);
        ai_album_ui_common_set_label_raw(count_label, count_text);
    }
    if (storage_label != NULL) {
        ai_album_ui_common_set_label_text(
            storage_label, album_status_storage_text());
    }
}
