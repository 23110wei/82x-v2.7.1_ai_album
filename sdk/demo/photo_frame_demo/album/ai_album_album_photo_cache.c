#include "album/ai_album_album_photo_cache.h"

#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"

#define ALBUM_PHOTO_CACHE_SERVICE_MS 50U
#define ALBUM_PHOTO_CACHE_NEIGHBORS 1U

typedef struct {
    ai_album_album_art_view_t *art;
    lv_timer_t *timer;
    uint32_t store_version;
    uint8_t current_index;
    uint8_t targets[ALBUM_PHOTO_CACHE_NEIGHBORS];
    uint8_t target_count;
    uint8_t target_position;
    uint8_t pending_index;
    uint8_t loading;
    uint8_t paused;
    uint8_t active;
} album_photo_cache_state_t;

static album_photo_cache_state_t g_album_photo_cache;

static uint8_t album_photo_cache_store_valid(void)
{
    uint8_t count = ai_album_album_store_count();

    return (uint8_t)(g_album_photo_cache.active &&
                     g_album_photo_cache.art != NULL &&
                     g_album_photo_cache.art->image != NULL &&
                     lv_obj_is_valid(g_album_photo_cache.art->image) &&
                     count > 1U && ai_album_album_store_selected() < count &&
                     ai_album_album_store_version() ==
                         g_album_photo_cache.store_version);
}

static void album_photo_cache_reset_targets(void)
{
    uint8_t count = ai_album_album_store_count();
    uint8_t current = ai_album_album_store_selected();

    g_album_photo_cache.current_index = current;
    g_album_photo_cache.targets[0] = (uint8_t)((current + 1U) % count);
    g_album_photo_cache.target_count = 1U;
    g_album_photo_cache.target_position = 0U;
    os_printf("[ALBUM_CACHE] neighbor current=%u next=%u\r\n",
              (unsigned)current,
              (unsigned)g_album_photo_cache.targets[0]);
}

static void album_photo_cache_complete(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data);

static void album_photo_cache_prepare_targets(void)
{
    while (g_album_photo_cache.target_position <
           g_album_photo_cache.target_count) {
        const ai_album_album_photo_t *photo;
        ai_album_album_image_view_result_t result;
        uint8_t index = g_album_photo_cache.targets[
            g_album_photo_cache.target_position];

        photo = ai_album_album_store_get(index);
        if (photo == NULL || !photo->storage_backed) {
            g_album_photo_cache.target_position++;
            continue;
        }
        g_album_photo_cache.pending_index = index;
        result = ai_album_album_art_prepare_async(
            g_album_photo_cache.art, photo, album_photo_cache_complete,
            &g_album_photo_cache);
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) {
            g_album_photo_cache.target_position++;
            g_album_photo_cache.loading = 1U;
            os_printf("[ALBUM_CACHE] prefetch index=%u\r\n",
                      (unsigned)index);
            return;
        }
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
            g_album_photo_cache.target_position++;
            os_printf("[ALBUM_CACHE] cached index=%u\r\n", (unsigned)index);
        } else {
            os_printf("[ALBUM_CACHE] reject index=%u result=%d\r\n",
                      (unsigned)index, (int)result);
            return;
        }
    }
}

static void album_photo_cache_complete(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data)
{
    (void)image;
    if (user_data != &g_album_photo_cache ||
        !g_album_photo_cache.active || !g_album_photo_cache.loading) return;
    g_album_photo_cache.loading = 0U;
    if (!album_photo_cache_store_valid()) {
        ai_album_album_photo_cache_stop();
        return;
    }
    if (ai_album_album_store_selected() != g_album_photo_cache.current_index) {
        album_photo_cache_reset_targets();
    }
    if (g_album_photo_cache.paused) return;
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        os_printf("[ALBUM_CACHE] ready index=%u\r\n",
                  (unsigned)g_album_photo_cache.pending_index);
    } else {
        os_printf("[ALBUM_CACHE] failed index=%u result=%d\r\n",
                  (unsigned)g_album_photo_cache.pending_index, (int)result);
    }
    album_photo_cache_prepare_targets();
}

static void album_photo_cache_service(lv_timer_t *timer)
{
    uint8_t selected;

    (void)timer;
    if (!album_photo_cache_store_valid()) {
        ai_album_album_photo_cache_stop();
        return;
    }
    if (g_album_photo_cache.paused) return;
    selected = ai_album_album_store_selected();
    if (selected != g_album_photo_cache.current_index) {
        ai_album_album_art_cancel_prepare(g_album_photo_cache.art);
        g_album_photo_cache.loading = 0U;
        album_photo_cache_reset_targets();
    }
    if (g_album_photo_cache.loading ||
        ai_album_album_image_view_get_result(
            g_album_photo_cache.art->image) ==
            AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) return;
    album_photo_cache_prepare_targets();
}

int ai_album_album_photo_cache_start(ai_album_album_art_view_t *art)
{
    if (art == NULL || art->image == NULL ||
        ai_album_album_store_count() < 2U) return RET_ERR;
    ai_album_album_photo_cache_stop();
    g_album_photo_cache.art = art;
    g_album_photo_cache.store_version = ai_album_album_store_version();
    g_album_photo_cache.active = 1U;
    album_photo_cache_reset_targets();
    g_album_photo_cache.timer = lv_timer_create(
        album_photo_cache_service, ALBUM_PHOTO_CACHE_SERVICE_MS, NULL);
    if (g_album_photo_cache.timer == NULL) {
        memset(&g_album_photo_cache, 0, sizeof(g_album_photo_cache));
        return RET_ERR;
    }
    os_printf("[ALBUM_CACHE] start slots=%u\r\n",
              (unsigned)AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT);
    album_photo_cache_service(g_album_photo_cache.timer);
    return RET_OK;
}

void ai_album_album_photo_cache_pause(void)
{
    if (g_album_photo_cache.active) g_album_photo_cache.paused = 1U;
}

void ai_album_album_photo_cache_resume(void)
{
    if (!g_album_photo_cache.active) return;
    g_album_photo_cache.paused = 0U;
    album_photo_cache_service(g_album_photo_cache.timer);
}

uint8_t ai_album_album_photo_cache_is_loading(void)
{
    return (uint8_t)(g_album_photo_cache.active &&
                     g_album_photo_cache.loading);
}

void ai_album_album_photo_cache_stop(void)
{
    ai_album_album_art_view_t *art = g_album_photo_cache.art;
    lv_timer_t *timer = g_album_photo_cache.timer;
    uint8_t was_active = g_album_photo_cache.active;

    memset(&g_album_photo_cache, 0, sizeof(g_album_photo_cache));
    if (art != NULL) ai_album_album_art_cancel_prepare(art);
    if (timer != NULL) lv_timer_delete(timer);
    if (was_active) os_printf("[ALBUM_CACHE] stop\r\n");
}

int ai_album_album_photo_cache_move(int8_t delta)
{
    uint8_t count = ai_album_album_store_count();
    uint8_t selected = ai_album_album_store_selected();
    uint8_t offset;

    if (delta == 0 || count < 2U || selected >= count) return RET_ERR;
    offset = delta < 0 ? (uint8_t)(count - 1U) : 1U;
    return ai_album_album_store_select(
        (uint8_t)((selected + offset) % count));
}
