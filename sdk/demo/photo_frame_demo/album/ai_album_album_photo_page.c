#include "album/ai_album_album_photo_page.h"

#include "album/ai_album_album_status.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"

typedef struct {
    const ai_album_album_photo_t *photo;
    uint32_t store_version;
    uint8_t count;
    uint8_t selected;
} album_photo_page_model_t;

static ai_album_album_image_view_result_t album_photo_page_render_image(
    const ai_album_album_photo_page_context_t *context,
    const album_photo_page_model_t *model)
{
    if (model->count == 0U) {
        if (*context->rendered_valid) {
            (void)ai_album_album_art_set(context->art, NULL);
        }
        *context->rendered_valid = 0U;
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY;
    }
    if (!*context->rendered_valid ||
        *context->rendered_index != model->selected ||
        *context->rendered_store_version != model->store_version) {
        ai_album_album_image_view_result_t result =
            ai_album_album_art_set_async(context->art, model->photo);
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED) {
            *context->rendered_valid = 0U;
            return result;
        }
        *context->rendered_index = model->selected;
        *context->rendered_store_version = model->store_version;
        *context->rendered_valid = 1U;
    }
    return ai_album_album_image_view_get_result(context->art->image);
}

void ai_album_album_photo_page_render(
    const ai_album_album_photo_page_context_t *context)
{
    char text[32];
    album_photo_page_model_t model;
    ai_album_album_image_view_result_t image_result;

    if (context == NULL || context->art == NULL || context->art->image == NULL ||
        context->rendered_index == NULL || context->rendered_valid == NULL ||
        context->rendered_store_version == NULL) {
        return;
    }
    model.count = ai_album_album_store_count();
    model.selected = ai_album_album_store_selected();
    model.store_version = ai_album_album_store_version();
    if (model.selected >= model.count && model.count > 0U) {
        model.selected = 0U;
        (void)ai_album_album_store_select(model.selected);
    }
    model.photo = ai_album_album_store_get(model.selected);
    image_result = album_photo_page_render_image(context, &model);
    os_snprintf(text, sizeof(text), "%u / %u",
                (unsigned)(model.count == 0U ? 0U : model.selected + 1U),
                (unsigned)model.count);
    if (context->counter_label != NULL &&
        lv_obj_is_valid(context->counter_label)) {
        lv_label_set_text(context->counter_label, text);
    }
    if (context->status_label == NULL ||
        !lv_obj_is_valid(context->status_label)) {
        return;
    }
    if (model.count == 0U) {
        ai_album_album_status_set_photo(
            context->status_label, ai_album_album_status_empty_text());
    } else if (model.photo != NULL && model.photo->storage_backed &&
               image_result < AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        ai_album_album_status_set_photo(
            context->status_label,
            image_result ==
                    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE ?
                "IMAGE FILE UNAVAILABLE" : "JPEG FORMAT NOT SUPPORTED");
    } else if (ai_album_album_store_status() ==
               AI_ALBUM_ALBUM_STORE_STATUS_SD_UNAVAILABLE) {
        ai_album_album_status_set_photo(
            context->status_label, "SD NOT READY  ·  CHECK CARD");
    } else if (ai_album_album_store_status() ==
               AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY) {
        ai_album_album_status_set_photo(
            context->status_label, "SD READY  ·  NO JPEG PHOTOS");
    } else {
        ai_album_album_status_set_photo(context->status_label, NULL);
    }
}
