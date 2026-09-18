#ifndef AI_ALBUM_ALBUM_ART_H
#define AI_ALBUM_ALBUM_ART_H

#include <stdint.h>

#include "album/ai_album_album_image_view.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *image;
    lv_obj_t *metadata;
    lv_obj_t *title;
    lv_obj_t *origin;
    lv_obj_t *status;
    uint8_t thumbnail;
} ai_album_album_art_view_t;

ai_album_album_art_view_t ai_album_album_art_create(
    lv_obj_t *parent,
    const ai_album_album_image_bounds_t *bounds,
    uint8_t thumbnail);

/* Keeps only the decoded image visible; metadata remains owned by the view. */
void ai_album_album_art_configure_image_only(
    const ai_album_album_art_view_t *view);

ai_album_album_image_view_result_t ai_album_album_art_set(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo);
int ai_album_album_art_set_loader_slot(
    const ai_album_album_art_view_t *view, uint8_t slot);
void ai_album_album_art_set_loading(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo);
void ai_album_album_art_set_error(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_result_t result);
ai_album_album_image_view_result_t ai_album_album_art_set_decoded(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    const lv_image_dsc_t *descriptor);
ai_album_album_image_view_result_t ai_album_album_art_set_async(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo);
ai_album_album_image_view_result_t ai_album_album_art_prepare_async(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data);
ai_album_album_image_view_result_t ai_album_album_art_commit_prepared(
    const ai_album_album_art_view_t *view,
    const ai_album_album_photo_t *photo);
void ai_album_album_art_cancel_prepare(
    const ai_album_album_art_view_t *view);
void ai_album_album_art_cancel_async(
    const ai_album_album_art_view_t *view);
void ai_album_album_art_clear_cache(
    const ai_album_album_art_view_t *view);

#endif
