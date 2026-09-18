#include "album/ai_album_album_art.h"
#include "album/ai_album_album_image_view.h"

#include "basic_include.h"
#include "ui/ai_album_ui_common.h"

static uint32_t album_art_background(uint8_t thumbnail)
{
    return thumbnail ? AI_ALBUM_UI_COLOR_WHITE : 0x0B1116U;
}

static void album_art_set_status(const ai_album_album_photo_t *photo,
                                 ai_album_album_image_view_result_t result,
                                 const char **origin,
                                 const char **status)
{
    *origin = photo == NULL ? "WAITING FOR PHOTO" : photo->origin;
    *status = photo == NULL ? "NO PHOTO" : "";
    if (photo != NULL && photo->storage_backed &&
        result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) {
        *status = "LOADING PHOTO";
    } else if (photo != NULL && photo->storage_backed &&
               result < AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        *origin = result ==
                      AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE ?
                  "IMAGE FILE UNAVAILABLE" : "JPEG FORMAT NOT SUPPORTED";
        *status = *origin;
    } else if (photo != NULL && !photo->storage_backed) {
        *origin = photo->generated ? "GENERATED PREVIEW" : "PREVIEW ONLY";
        *status = *origin;
    }
}

static lv_obj_t *album_art_create_metadata(
    lv_obj_t *root,
    const ai_album_album_image_bounds_t *bounds,
    uint8_t thumbnail)
{
    lv_obj_t *metadata;

    if (thumbnail) {
        return NULL;
    }
    metadata = ai_album_ui_common_panel(root, 0x0B1116U, 0);
    lv_obj_set_pos(metadata, 0, 44);
    lv_obj_set_size(metadata, bounds->width, 54);
    /* Keep the caption readable without reserving an opaque strip over the photo. */
    lv_obj_set_style_bg_opa(metadata, LV_OPA_TRANSP, LV_PART_MAIN);
    return metadata;
}

ai_album_album_art_view_t ai_album_album_art_create(
    lv_obj_t *parent,
    const ai_album_album_image_bounds_t *bounds,
    uint8_t thumbnail)
{
    ai_album_album_art_view_t view;
    ai_album_album_image_bounds_t image_bounds;
    int32_t margin;
    int32_t image_height;
    uint32_t origin_color;
    lv_obj_t *label_parent;

    memset(&view, 0, sizeof(view));
    if (parent == NULL || bounds == NULL || bounds->width <= 0 ||
        bounds->height <= 0) {
        return view;
    }
    margin = thumbnail ? 8 : 0;
    /* Thumbnail origin text is hidden, so reclaim its row for the preview. */
    image_height = thumbnail ? bounds->height - 24 : bounds->height;
    origin_color = thumbnail ? AI_ALBUM_UI_COLOR_MUTED : 0xC6D5D9U;
    view.thumbnail = thumbnail;
    view.root = ai_album_ui_common_panel(parent, album_art_background(thumbnail),
                                         thumbnail ? 12 : 16);
    lv_obj_set_pos(view.root, bounds->x, bounds->y);
    lv_obj_set_size(view.root, bounds->width, bounds->height);
    lv_obj_set_style_border_width(view.root, thumbnail ? 1 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(view.root, lv_color_hex(0x43535BU),
                                  LV_PART_MAIN);

    image_bounds.x = margin;
    image_bounds.y = margin;
    image_bounds.width = bounds->width - margin * 2;
    image_bounds.height = image_height - margin;
    view.image = ai_album_album_image_view_create(view.root, &image_bounds);
    (void)ai_album_album_image_view_set_alignment(
        view.image, LV_IMAGE_ALIGN_CONTAIN);
    view.metadata = album_art_create_metadata(view.root, bounds, thumbnail);
    label_parent = thumbnail ? view.root : view.metadata;
    view.title = ai_album_ui_common_label(
        label_parent, "NO PHOTO", thumbnail ? &lv_font_montserrat_14 :
                                              &lv_font_montserrat_16,
        thumbnail ? AI_ALBUM_UI_COLOR_TEXT : AI_ALBUM_UI_COLOR_WHITE);
    lv_obj_set_pos(view.title, thumbnail ? 10 : 20,
                   thumbnail ? bounds->height - 22 : 5);
    view.origin = ai_album_ui_common_label(
        label_parent, "WAITING FOR PHOTO", &lv_font_montserrat_14,
        origin_color);
    lv_obj_set_pos(view.origin, thumbnail ? 10 : 20,
                   thumbnail ? bounds->height - 18 : 30);
    if (thumbnail) {
        lv_obj_add_flag(view.origin, LV_OBJ_FLAG_HIDDEN);
    }
    view.status = ai_album_ui_common_label(
        view.root, "NO PHOTO", &lv_font_montserrat_14, origin_color);
    lv_obj_center(view.status);
    return view;
}

static void album_art_hide_object(lv_obj_t *object)
{
    if (object != NULL) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void album_art_hide_text(lv_obj_t *object)
{
    if (object != NULL) {
        /* Async status callbacks may clear HIDDEN; opacity keeps this mode text-free. */
        lv_obj_set_style_text_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

void ai_album_album_art_configure_image_only(
    const ai_album_album_art_view_t *view)
{
    if (view == NULL) {
        return;
    }
    if (view->root != NULL) {
        lv_obj_set_style_bg_opa(view->root, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(view->root, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(view->root, 0, LV_PART_MAIN);
    }
    if (view->image != NULL) {
        lv_obj_set_style_bg_opa(view->image, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(view->image, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(view->image, 0, LV_PART_MAIN);
    }
    album_art_hide_object(view->metadata);
    album_art_hide_text(view->title);
    album_art_hide_text(view->origin);
    album_art_hide_text(view->status);
}

static void album_art_update_labels(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_result_t result)
{
    const char *origin;
    const char *status;

    album_art_set_status(photo, result, &origin, &status);
    if (photo == NULL) {
        ai_album_ui_common_set_label_text(view->title, "NO PHOTO");
    } else {
        /* 文件名是数据不是界面文案, 原样显示 */
        ai_album_ui_common_set_label_raw(view->title, photo->name);
    }
    ai_album_ui_common_set_label_text(view->origin, origin);
    ai_album_ui_common_set_label_text(view->status, status);
    lv_obj_set_flag(view->status, LV_OBJ_FLAG_HIDDEN,
                    (uint8_t)(status[0] == '\0'));
}

static void album_art_update_background(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo)
{
    lv_obj_set_style_bg_color(view->root,
                              lv_color_hex(photo != NULL && photo->generated ?
                                           photo->sky_color :
                                           album_art_background(view->thumbnail)),
                              LV_PART_MAIN);
}

ai_album_album_image_view_result_t ai_album_album_art_set(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo)
{
    ai_album_album_image_view_result_t result;

    if (view == NULL || view->root == NULL || view->image == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    album_art_update_background(view, photo);
    result = ai_album_album_image_view_set(view->image, photo);
    album_art_update_labels(view, photo, result);
    return result;
}

int ai_album_album_art_set_loader_slot(
    const ai_album_album_art_view_t *view, uint8_t slot)
{
    if (view == NULL || view->image == NULL) {
        return RET_ERR;
    }
    return ai_album_album_image_view_set_loader_slot(view->image, slot);
}

void ai_album_album_art_set_loading(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo)
{
    ai_album_album_image_view_result_t result;

    if (view == NULL || view->root == NULL || view->image == NULL) {
        return;
    }
    result = photo != NULL && photo->storage_backed ?
                 AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING :
                 AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY;
    (void)ai_album_album_image_view_set(view->image, NULL);
    album_art_update_background(view, photo);
    album_art_update_labels(view, photo, result);
}

void ai_album_album_art_set_error(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_result_t result)
{
    if (view == NULL || view->root == NULL || view->image == NULL) {
        return;
    }
    album_art_update_background(view, photo);
    album_art_update_labels(view, photo, result);
}

ai_album_album_image_view_result_t ai_album_album_art_set_decoded(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    const lv_image_dsc_t *descriptor)
{
    if (view == NULL || view->root == NULL || view->image == NULL ||
        descriptor == NULL || descriptor->data == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
    }
    lv_image_set_src(view->image, descriptor);
    ai_album_album_image_view_apply_contain(view->image);
    lv_obj_clear_flag(view->image, LV_OBJ_FLAG_HIDDEN);
    album_art_update_background(view, photo);
    album_art_update_labels(view, photo,
                            AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK);
    return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
}

static void album_art_async_complete(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data)
{
    lv_obj_t *status = user_data;
    const char *text;

    (void)image;
    if (status == NULL || !lv_obj_is_valid(status)) return;
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        lv_obj_add_flag(status, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    text = result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE ?
               "IMAGE FILE UNAVAILABLE" : "JPEG FORMAT NOT SUPPORTED";
    ai_album_ui_common_set_label_text(status, text);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_HIDDEN);
}

ai_album_album_image_view_result_t ai_album_album_art_set_async(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo)
{
    ai_album_album_image_view_result_t result;

    if (view == NULL || view->root == NULL || view->image == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    album_art_update_background(view, photo);
    result = ai_album_album_image_view_set_async(
        view->image, photo, album_art_async_complete, view->status);
    album_art_update_labels(view, photo, result);
    return result;
}

ai_album_album_image_view_result_t ai_album_album_art_prepare_async(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data)
{
    if (view == NULL || view->image == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    return ai_album_album_image_view_prepare_async(
        view->image, photo, completion_cb, user_data);
}

ai_album_album_image_view_result_t ai_album_album_art_commit_prepared(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo)
{
    ai_album_album_image_view_result_t result;

    if (view == NULL || view->root == NULL || view->image == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    result = ai_album_album_image_view_commit_prepared(view->image);
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        album_art_update_background(view, photo);
        album_art_update_labels(view, photo, result);
    }
    return result;
}

void ai_album_album_art_cancel_prepare(
    const ai_album_album_art_view_t *view)
{
    if (view != NULL && view->image != NULL) {
        ai_album_album_image_view_cancel_prepare(view->image);
    }
}

void ai_album_album_art_cancel_async(
    const ai_album_album_art_view_t *view)
{
    if (view != NULL && view->image != NULL) {
        ai_album_album_image_view_cancel_async(view->image);
    }
}

void ai_album_album_art_clear_cache(
    const ai_album_album_art_view_t *view)
{
    if (view != NULL && view->image != NULL) {
        ai_album_album_image_view_clear_cache(view->image);
    }
}
