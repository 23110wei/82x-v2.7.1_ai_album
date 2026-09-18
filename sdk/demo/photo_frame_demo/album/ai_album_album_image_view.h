#ifndef AI_ALBUM_ALBUM_IMAGE_VIEW_H
#define AI_ALBUM_ALBUM_IMAGE_VIEW_H

#include <stdint.h>

#include "album/ai_album_album_store.h"
#include "ui/ai_album_compat.h"

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} ai_album_album_image_bounds_t;

typedef enum {
    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK = 0,
    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY = 1,
    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING = 2,
    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE = -2,
    AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED = -3,
} ai_album_album_image_view_result_t;

typedef void (*ai_album_album_image_view_completion_cb_t)(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data);

lv_obj_t *ai_album_album_image_view_create(
    lv_obj_t *parent,
    const ai_album_album_image_bounds_t *bounds);
int ai_album_album_image_view_set_alignment(lv_obj_t *image,
                                             lv_image_align_t alignment);
/* Reserve one of the shared JPEG-loader slots for this view. */
int ai_album_album_image_view_set_loader_slot(lv_obj_t *image,
                                               uint8_t slot);
ai_album_album_image_view_result_t ai_album_album_image_view_set(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo);
ai_album_album_image_view_result_t ai_album_album_image_view_set_async(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data);
ai_album_album_image_view_result_t ai_album_album_image_view_prepare_async(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data);
ai_album_album_image_view_result_t
ai_album_album_image_view_commit_prepared(lv_obj_t *image);
void ai_album_album_image_view_cancel_prepare(lv_obj_t *image);
void ai_album_album_image_view_cancel_async(lv_obj_t *image);
void ai_album_album_image_view_clear_cache(lv_obj_t *image);
ai_album_album_image_view_result_t ai_album_album_image_view_get_result(
    const lv_obj_t *image);

/* LVGL 9.0适配:将图像控件缩放/居中到解码尺寸(替代9.5的CONTAIN对齐) */
void ai_album_album_image_view_apply_contain(lv_obj_t *image);

#endif
