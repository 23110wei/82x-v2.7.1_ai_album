#include "album/ai_album_album_image_view.h"

#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_perf.h"
#include "basic_include.h"
#include "lv_image_cache_compat.h"

#define ALBUM_IMAGE_VIEW_SLOT_NONE 0xFFU

typedef struct {
    lv_image_dsc_t descriptor;
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    uint32_t file_size;
    uint32_t age;
    uint8_t valid;
} album_image_cache_slot_t;

typedef struct {
    lv_obj_t *image;
    album_image_cache_slot_t slots[AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT];
    ai_album_album_image_view_completion_cb_t completion_cb;
    void *completion_user_data;
    char pending_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    uint32_t generation;
    uint32_t pending_file_size;
    uint32_t cache_clock;
    uint16_t max_width;
    uint16_t max_height;
    uint8_t loader_slot;
    ai_album_album_image_view_result_t result;
    uint8_t displayed_slot;
    uint8_t prepared_slot;
    uint8_t pending_slot;
    uint8_t auto_commit;
} album_image_view_state_t;

static album_image_view_state_t *album_image_view_state(lv_obj_t *image)
{
    return image == NULL ? NULL : lv_obj_get_user_data(image);
}

static void album_image_drop_descriptor(album_image_view_state_t *state,
                                        uint8_t slot)
{
    if (state == NULL ||
        slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT) return;
    lv_image_cache_drop(&state->slots[slot].descriptor);
    memset(&state->slots[slot], 0, sizeof(state->slots[slot]));
    if (state->prepared_slot == slot) {
        state->prepared_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    }
}

static void album_image_cancel_pending(album_image_view_state_t *state)
{
    if (state == NULL) return;
    ai_album_album_image_loader_cancel(state);
    state->completion_cb = NULL;
    state->completion_user_data = NULL;
    state->generation = 0U;
    state->pending_path[0] = '\0';
    state->pending_file_size = 0U;
    state->pending_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    state->auto_commit = 0U;
}

static void album_image_discard_prepared(album_image_view_state_t *state)
{
    if (state == NULL || state->prepared_slot == ALBUM_IMAGE_VIEW_SLOT_NONE) {
        return;
    }
    state->prepared_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
}

static void album_image_reset_source(lv_obj_t *image,
                                     album_image_view_state_t *state)
{
    uint8_t slot;

    album_image_cancel_pending(state);
    lv_image_set_src(image, NULL);
    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        album_image_drop_descriptor(state, slot);
    }
    ai_album_album_image_loader_release_owner(state);
    if (state != NULL) {
        state->displayed_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
        state->prepared_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
        state->cache_clock = 0U;
    }
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
}

static void album_image_delete_event_cb(lv_event_t *event)
{
    lv_obj_t *image = lv_event_get_target_obj(event);
    album_image_view_state_t *state = album_image_view_state(image);
    uint8_t slot;

    if (state == NULL) return;
    album_image_cancel_pending(state);
    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        album_image_drop_descriptor(state, slot);
    }
    ai_album_album_image_loader_release_owner(state);
    lv_free(state);
}

static void album_image_draw_event_cb(lv_event_t *event)
{
    lv_obj_t *image = lv_event_get_target_obj(event);
    const void *source = lv_image_get_src(image);
    uint8_t file_source = (uint8_t)(
        source != NULL && ai_album_img_src_is_file(source));

    ai_album_album_perf_record_image_draw(file_source);
}

static ai_album_album_image_view_result_t album_image_check_file(
    const char *path)
{
    lv_fs_file_t file;
    lv_fs_res_t open_result;

    open_result = lv_fs_open(&file, path, LV_FS_MODE_RD);
    if (open_result != LV_FS_RES_OK) {
        os_printf("ai_album: album image open_failed path=%s result=%d\r\n",
                  path, (int)open_result);
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    lv_fs_close(&file);
    return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
}

static int album_image_build_lvgl_path(
    char *destination,
    uint32_t destination_size,
    const char *storage_path)
{
    const char *extension;
    char *cursor;
    char driver_letter;
    int length;

    if (destination == NULL || destination_size == 0U ||
        storage_path == NULL || storage_path[0] != '0' ||
        storage_path[1] != ':') {
        return RET_ERR;
    }
    #if defined(LV_FS_FATFS_LETTER)
    driver_letter = (char)LV_FS_FATFS_LETTER;
    #else
    driver_letter = 'S';
    #endif
    length = os_snprintf(destination, destination_size, "%c:%s",
                         driver_letter, storage_path + 2);
    if (length < 0 || (uint32_t)length >= destination_size) {
        return RET_ERR;
    }
    extension = os_strrchr(destination, '.');
    if (extension == NULL) {
        return RET_ERR;
    }
    for (cursor = (char *)extension + 1; *cursor != '\0'; ++cursor) {
        if (*cursor >= 'A' && *cursor <= 'Z') {
            *cursor = (char)(*cursor - 'A' + 'a');
        }
    }
    return RET_OK;
}

lv_obj_t *ai_album_album_image_view_create(
    lv_obj_t *parent,
    const ai_album_album_image_bounds_t *bounds)
{
    lv_obj_t *image;
    album_image_view_state_t *state;

    if (parent == NULL || bounds == NULL || bounds->width <= 0 ||
        bounds->height <= 0) {
        return NULL;
    }
    image = lv_image_create(parent);
    if (image == NULL) {
        return NULL;
    }
    state = lv_malloc_zeroed(sizeof(*state));
    if (state == NULL) {
        lv_obj_delete(image);
        return NULL;
    }
    state->image = image;
    state->max_width = (uint16_t)bounds->width;
    state->max_height = (uint16_t)bounds->height;
    state->loader_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY;
    state->displayed_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    state->prepared_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    state->pending_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    lv_obj_set_user_data(image, state);
    lv_obj_set_pos(image, bounds->x, bounds->y);
    lv_obj_set_size(image, bounds->width, bounds->height);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_SCROLLABLE);
    /* Keep letterboxed areas deterministic instead of exposing the parent UI. */
    lv_obj_set_style_bg_color(image, lv_color_hex(0x0B1116U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_antialias(image, true);
    lv_obj_add_event_cb(image, album_image_draw_event_cb,
                        LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(image, album_image_delete_event_cb,
                        LV_EVENT_DELETE, NULL);
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    return image;
}

int ai_album_album_image_view_set_alignment(lv_obj_t *image,
                                             lv_image_align_t alignment)
{
    /* 9.0无ALIGN_CONTAIN/COVER,两种均映射为SIZE_MODE_REAL(1:1源尺寸) */
    if (image == NULL || (alignment != LV_IMAGE_ALIGN_CONTAIN &&
                          alignment != LV_IMAGE_ALIGN_COVER)) {
        return RET_ERR;
    }
    lv_image_set_inner_align(image, alignment);
    return RET_OK;
}

static ai_album_album_image_view_result_t album_image_set(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo)
{
    char lvgl_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    ai_album_album_image_view_result_t result;
    const void *source;
    int32_t width;
    int32_t height;

    if (image == NULL) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    if (photo == NULL || !photo->storage_backed || photo->path[0] == '\0' ||
        album_image_build_lvgl_path(lvgl_path, sizeof(lvgl_path),
                                     photo->path) != RET_OK) {
        lv_image_set_src(image, NULL);
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY;
    }
    result = album_image_check_file(lvgl_path);
    if (result != AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        lv_image_set_src(image, NULL);
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return result;
    }
    lv_image_set_src(image, lvgl_path);
    source = lv_image_get_src(image);
    width = lv_image_get_src_width(image);
    height = lv_image_get_src_height(image);
    if (source == NULL || width <= 0 || height <= 0) {
        os_printf("ai_album: album image decode_failed path=%s\r\n",
                  photo->path);
        lv_image_set_src(image, NULL);
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
    }
    os_printf("ai_album: album image ready path=%s size=%d x %d\r\n",
              photo->path, (int)width, (int)height);
    ai_album_album_image_view_apply_contain(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
    return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
}

ai_album_album_image_view_result_t ai_album_album_image_view_set(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo)
{
    uint64 start_us = os_useconds();
    album_image_view_state_t *state = album_image_view_state(image);
    ai_album_album_image_view_result_t result;

    if (image != NULL) album_image_reset_source(image, state);
    result = album_image_set(image, photo);
    if (state != NULL) state->result = result;

    ai_album_album_perf_record_image_prepare(
        (uint32_t)(os_useconds() - start_us), result);
    return result;
}

static ai_album_album_image_view_result_t album_image_loader_result(
    ai_album_album_image_loader_result_t result)
{
    if (result == AI_ALBUM_ALBUM_IMAGE_LOADER_OK) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
    }
    if (result == AI_ALBUM_ALBUM_IMAGE_LOADER_FILE_UNAVAILABLE) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
}

static uint8_t album_image_find_cached(
    const album_image_view_state_t *state,
    const ai_album_album_photo_t *photo)
{
    uint8_t slot;

    if (state == NULL || photo == NULL || !photo->storage_backed) {
        return ALBUM_IMAGE_VIEW_SLOT_NONE;
    }
    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        const album_image_cache_slot_t *entry = &state->slots[slot];

        if (entry->valid && entry->file_size == photo->file_size &&
            os_strcmp(entry->path, photo->path) == 0) return slot;
    }
    return ALBUM_IMAGE_VIEW_SLOT_NONE;
}

static void album_image_touch_slot(album_image_view_state_t *state,
                                   uint8_t slot)
{
    if (state == NULL || slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        !state->slots[slot].valid) return;
    state->cache_clock++;
    if (state->cache_clock == 0U) state->cache_clock = 1U;
    state->slots[slot].age = state->cache_clock;
}

static uint8_t album_image_choose_target(album_image_view_state_t *state)
{
    uint8_t candidate = ALBUM_IMAGE_VIEW_SLOT_NONE;
    uint8_t slot;

    if (state == NULL) return ALBUM_IMAGE_VIEW_SLOT_NONE;
    if (state->loader_slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT) {
        return ai_album_album_image_loader_slot_available(
                   state, state->loader_slot) ?
                   state->loader_slot : ALBUM_IMAGE_VIEW_SLOT_NONE;
    }
    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        if (slot == state->displayed_slot ||
            !ai_album_album_image_loader_slot_available(state, slot)) {
            continue;
        }
        if (!state->slots[slot].valid) return slot;
        if (candidate == ALBUM_IMAGE_VIEW_SLOT_NONE ||
            state->slots[slot].age < state->slots[candidate].age) {
            candidate = slot;
        }
    }
    return candidate;
}

int ai_album_album_image_view_set_loader_slot(lv_obj_t *image,
                                               uint8_t slot)
{
    album_image_view_state_t *state = album_image_view_state(image);

    if (state == NULL || slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        state->generation != 0U ||
        state->displayed_slot != ALBUM_IMAGE_VIEW_SLOT_NONE ||
        (state->loader_slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT &&
         state->loader_slot != slot)) {
        return RET_ERR;
    }
    if (ai_album_album_image_loader_claim_slot(state, slot) != RET_OK) {
        return RET_ERR;
    }
    state->loader_slot = slot;
    return RET_OK;
}

static ai_album_album_image_view_result_t album_image_commit_slot(
    album_image_view_state_t *state,
    uint8_t next_slot)
{
    uint8_t previous_slot;

    if (state == NULL || next_slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        !state->slots[next_slot].valid) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
    }
    previous_slot = state->displayed_slot;
    lv_image_set_src(state->image, &state->slots[next_slot].descriptor);
    ai_album_album_image_view_apply_contain(state->image);
    lv_obj_clear_flag(state->image, LV_OBJ_FLAG_HIDDEN);
    state->displayed_slot = next_slot;
    if (state->prepared_slot == next_slot) {
        state->prepared_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    }
    state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
    if (previous_slot != ALBUM_IMAGE_VIEW_SLOT_NONE &&
        previous_slot != next_slot) {
        album_image_touch_slot(state, previous_slot);
    }
    album_image_touch_slot(state, next_slot);
    return state->result;
}

static ai_album_album_image_view_result_t album_image_commit_prepared(
    album_image_view_state_t *state)
{
    return state == NULL ? AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED :
           album_image_commit_slot(state, state->prepared_slot);
}

static void album_image_prepare_descriptor(
    album_image_view_state_t *state,
    const ai_album_album_image_loader_completion_t *completion)
{
    album_image_cache_slot_t *entry = &state->slots[completion->target_slot];
    lv_image_dsc_t *descriptor = &entry->descriptor;

    album_image_drop_descriptor(state, completion->target_slot);
    /* 本工程LVGL 9.0的lv_img_header_t布局: cf:5/always_zero:3/reserved:2/w:11/h:11
     * (无9.5的magic/flags/stride字段);cf用TRUE_COLOR(工程RGB565, stride=w*2) */
    *descriptor = (lv_image_dsc_t){
        .header.cf = LV_IMG_CF_TRUE_COLOR,
        .header.always_zero = 0,
        .header.reserved = 0,
        .header.w = completion->width,
        .header.h = completion->height,
        .data_size = completion->data_size,
        .data = completion->data,
    };
    os_strncpy(entry->path, state->pending_path, sizeof(entry->path) - 1U);
    entry->path[sizeof(entry->path) - 1U] = '\0';
    entry->file_size = state->pending_file_size;
    entry->valid = 1U;
    album_image_touch_slot(state, completion->target_slot);
    state->prepared_slot = completion->target_slot;
}

static void album_image_loader_complete(
    const ai_album_album_image_loader_completion_t *completion,
    void *client)
{
    album_image_view_state_t *state = client;
    ai_album_album_image_view_completion_cb_t callback;
    void *user_data;
    uint8_t auto_commit;
    ai_album_album_image_view_result_t result;

    if (state == NULL || completion == NULL ||
        state->generation != completion->generation ||
        state->pending_slot != completion->target_slot) {
        return;
    }
    callback = state->completion_cb;
    user_data = state->completion_user_data;
    auto_commit = state->auto_commit;
    result = album_image_loader_result(completion->result);
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        album_image_prepare_descriptor(state, completion);
    }
    state->completion_cb = NULL;
    state->completion_user_data = NULL;
    state->generation = 0U;
    state->pending_path[0] = '\0';
    state->pending_file_size = 0U;
    state->pending_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    state->auto_commit = 0U;
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        if (auto_commit) result = album_image_commit_prepared(state);
    } else if (auto_commit) {
        state->result = result;
    }
    if (callback != NULL) {
        callback(state->image, result, user_data);
    }
}

typedef struct {
    const ai_album_album_photo_t *photo;
    ai_album_album_image_view_completion_cb_t completion_cb;
    void *user_data;
    uint8_t auto_commit;
} album_image_async_params_t;

static ai_album_album_image_view_result_t album_image_start_async(
    album_image_view_state_t *state,
    const album_image_async_params_t *params)
{
    ai_album_album_image_loader_request_t request;
    ai_album_album_image_view_result_t result;
    uint8_t cached_slot;
    uint8_t target_slot;

    /* A cache hit still supersedes an in-flight decode for another photo. */
    album_image_cancel_pending(state);
    album_image_discard_prepared(state);
    cached_slot = album_image_find_cached(state, params->photo);
    if (cached_slot != ALBUM_IMAGE_VIEW_SLOT_NONE) {
        if (!params->auto_commit) {
            state->prepared_slot = cached_slot;
            return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK;
        }
        result = album_image_commit_slot(state, cached_slot);
        os_printf("[ALBUM_CACHE] hit slot=%u path=%s\r\n",
                  (unsigned)cached_slot, params->photo->path);
        return result;
    }
    state->max_width = (uint16_t)lv_obj_get_width(state->image);
    state->max_height = (uint16_t)lv_obj_get_height(state->image);
    target_slot = album_image_choose_target(state);
    if (target_slot == ALBUM_IMAGE_VIEW_SLOT_NONE) {
        if (params->auto_commit) {
            state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
        }
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
    }
    if (state->displayed_slot == target_slot) {
        lv_image_set_src(state->image, NULL);
        lv_obj_add_flag(state->image, LV_OBJ_FLAG_HIDDEN);
        state->displayed_slot = ALBUM_IMAGE_VIEW_SLOT_NONE;
    }
    album_image_drop_descriptor(state, target_slot);
    state->completion_cb = params->completion_cb;
    state->completion_user_data = params->user_data;
    os_strncpy(state->pending_path, params->photo->path,
               sizeof(state->pending_path) - 1U);
    state->pending_path[sizeof(state->pending_path) - 1U] = '\0';
    state->pending_file_size = params->photo->file_size;
    state->pending_slot = target_slot;
    state->auto_commit = params->auto_commit;
    request = (ai_album_album_image_loader_request_t){
        .path = params->photo->path,
        .max_width = state->max_width,
        .max_height = state->max_height,
        .target_slot = target_slot,
        .completion_cb = album_image_loader_complete,
        .client = state,
    };
    if (ai_album_album_image_loader_submit(&request, &state->generation) !=
        RET_OK) {
        album_image_cancel_pending(state);
        if (params->auto_commit) {
            state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
        }
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
    }
    if (params->auto_commit) {
        state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING;
    }
    return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING;
}

static uint8_t album_image_async_photo_valid(
    const ai_album_album_photo_t *photo)
{
    return (uint8_t)(photo != NULL && photo->storage_backed &&
                     photo->path[0] != '\0');
}

ai_album_album_image_view_result_t ai_album_album_image_view_set_async(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data)
{
    album_image_view_state_t *state = album_image_view_state(image);
    uint64 start_us = os_useconds();
    ai_album_album_image_view_result_t result;
    album_image_async_params_t params = {
        .photo = photo,
        .completion_cb = completion_cb,
        .user_data = user_data,
        .auto_commit = 1U,
    };

    if (state == NULL || !album_image_async_photo_valid(photo)) {
        return ai_album_album_image_view_set(image, photo);
    }
    result = album_image_start_async(state, &params);
    ai_album_album_perf_record_image_prepare(
        (uint32_t)(os_useconds() - start_us), result);
    return result;
}

ai_album_album_image_view_result_t ai_album_album_image_view_prepare_async(
    lv_obj_t *image,
    const ai_album_album_photo_t *photo,
    ai_album_album_image_view_completion_cb_t completion_cb,
    void *user_data)
{
    album_image_view_state_t *state = album_image_view_state(image);
    album_image_async_params_t params = {
        .photo = photo,
        .completion_cb = completion_cb,
        .user_data = user_data,
        .auto_commit = 0U,
    };

    if (state == NULL || !album_image_async_photo_valid(photo)) {
        return AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE;
    }
    return album_image_start_async(state, &params);
}

ai_album_album_image_view_result_t
ai_album_album_image_view_commit_prepared(lv_obj_t *image)
{
    return album_image_commit_prepared(album_image_view_state(image));
}

void ai_album_album_image_view_cancel_prepare(lv_obj_t *image)
{
    album_image_view_state_t *state = album_image_view_state(image);

    if (state != NULL && !state->auto_commit) {
        album_image_cancel_pending(state);
    }
    album_image_discard_prepared(state);
}

void ai_album_album_image_view_cancel_async(lv_obj_t *image)
{
    album_image_view_state_t *state = album_image_view_state(image);

    if (state == NULL) return;
    album_image_cancel_pending(state);
    album_image_discard_prepared(state);
}

void ai_album_album_image_view_clear_cache(lv_obj_t *image)
{
    album_image_view_state_t *state = album_image_view_state(image);

    if (state == NULL) return;
    album_image_reset_source(image, state);
    state->result = AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_EMPTY;
}

ai_album_album_image_view_result_t ai_album_album_image_view_get_result(
    const lv_obj_t *image)
{
    album_image_view_state_t *state = image == NULL ? NULL :
        lv_obj_get_user_data((lv_obj_t *)image);

    return state == NULL ? AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE :
                           state->result;
}

/* ---- 本工程LVGL 9.0适配辅助(compat.h声明) ---- */
#include "lv_img_cache.h"
#include "lv_img_decoder.h"

int32_t ai_album_img_src_dim(lv_obj_t *obj, uint8 want_width)
{
    lv_img_header_t header;
    const void *src = lv_img_get_src(obj);

    if (src == NULL ||
        lv_img_decoder_get_info(src, &header) != LV_RES_OK) {
        return 0;
    }
    return want_width ? (int32_t)header.w : (int32_t)header.h;
}

uint8_t ai_album_img_src_is_file(const void *src)
{
    /* 变量源是文件路径字符串(如"0:/IMG/x.jpg"),变量源是内存dsc指针;
     * 简化判据:路径首字符为可打印ASCII且非PNG头 */
    const char *path = (const char *)src;

    if (src == NULL || ((uintptr_t)src & 0x3U) != 0U) {
        return 0U;
    }
    return (uint8_t)(path[0] >= 0x20U && path[0] < 0x7FU);
}

void ai_album_album_image_view_apply_contain(lv_obj_t *image)
{
    int32_t width;
    int32_t height;

    if (image == NULL || !lv_obj_is_valid(image)) {
        return;
    }
    width = ai_album_img_src_dim(image, 1);
    height = ai_album_img_src_dim(image, 0);
    if (width <= 0 || height <= 0) {
        return;
    }
    /* 9.0 has no CONTAIN align: size widget to decoded dims and center */
    lv_obj_set_size(image, width, height);
    lv_obj_align(image, LV_ALIGN_CENTER, 0, 0);
}
