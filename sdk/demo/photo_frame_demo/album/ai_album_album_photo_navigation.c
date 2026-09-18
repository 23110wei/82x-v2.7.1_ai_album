#include "album/ai_album_album_photo_navigation.h"

#include "album/ai_album_album_image_view.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"

#define ALBUM_PHOTO_NAVIGATION_DEBOUNCE_MS 150U

typedef struct {
    ai_album_album_art_view_t *art;
    lv_obj_t *counter_label;
    ai_album_album_photo_navigation_cb_t render_cb;
    ai_album_album_photo_navigation_cb_t pause_cache_cb;
    ai_album_album_photo_navigation_cb_t resume_cache_cb;
    ai_album_album_photo_navigation_busy_cb_t cache_busy_cb;
    void *user_data;
    uint64 last_input_ms;
    uint64 retry_at_ms;
    uint32_t store_version;
    uint8_t requested_index;
    uint8_t requested_valid;
    uint8_t request_active;
    uint8_t cache_paused;
    uint8_t active;
} album_photo_navigation_state_t;

static album_photo_navigation_state_t g_album_photo_navigation;

static uint8_t album_photo_navigation_view_valid(void)
{
    return (uint8_t)(g_album_photo_navigation.active &&
                     g_album_photo_navigation.art != NULL &&
                     g_album_photo_navigation.art->image != NULL &&
                     lv_obj_is_valid(g_album_photo_navigation.art->image));
}

static uint8_t album_photo_navigation_elapsed(uint64 now_ms, uint64 since_ms,
                                              uint32_t duration_ms)
{
    return (uint8_t)(now_ms - since_ms >= duration_ms);
}

static void album_photo_navigation_update_counter(void)
{
    char text[32];
    uint8_t count;
    uint8_t selected;

    if (g_album_photo_navigation.counter_label == NULL ||
        !lv_obj_is_valid(g_album_photo_navigation.counter_label)) {
        return;
    }
    count = ai_album_album_store_count();
    selected = ai_album_album_store_selected();
    if (selected >= count) selected = 0U;
    os_snprintf(text, sizeof(text), "%u / %u",
                (unsigned)(count == 0U ? 0U : selected + 1U),
                (unsigned)count);
    lv_label_set_text(g_album_photo_navigation.counter_label, text);
}

static void album_photo_navigation_pause_cache(void)
{
    if (g_album_photo_navigation.cache_paused) return;
    g_album_photo_navigation.cache_paused = 1U;
    if (g_album_photo_navigation.pause_cache_cb != NULL) {
        g_album_photo_navigation.pause_cache_cb(
            g_album_photo_navigation.user_data);
    }
}

static void album_photo_navigation_resume_cache(void)
{
    if (!g_album_photo_navigation.cache_paused) return;
    g_album_photo_navigation.cache_paused = 0U;
    if (g_album_photo_navigation.resume_cache_cb != NULL) {
        g_album_photo_navigation.resume_cache_cb(
            g_album_photo_navigation.user_data);
    }
}

static void album_photo_navigation_submit(uint8_t selected, uint64 now_ms)
{
    ai_album_album_image_view_result_t result;

    if (g_album_photo_navigation.render_cb == NULL) return;
    g_album_photo_navigation.render_cb(g_album_photo_navigation.user_data);
    g_album_photo_navigation.requested_index = selected;
    g_album_photo_navigation.requested_valid = 1U;
    result = ai_album_album_image_view_get_result(
        g_album_photo_navigation.art->image);
    g_album_photo_navigation.request_active =
        (uint8_t)(result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING);
    if (result < AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        g_album_photo_navigation.retry_at_ms =
            now_ms + ALBUM_PHOTO_NAVIGATION_DEBOUNCE_MS;
    } else {
        g_album_photo_navigation.retry_at_ms = 0U;
    }
}

int ai_album_album_photo_navigation_start(
    const ai_album_album_photo_navigation_config_t *config)
{
    ai_album_album_image_view_result_t result;

    if (config == NULL || config->art == NULL || config->art->image == NULL ||
        config->render_cb == NULL || ai_album_album_store_count() == 0U) {
        return RET_ERR;
    }
    ai_album_album_photo_navigation_stop();
    memset(&g_album_photo_navigation, 0, sizeof(g_album_photo_navigation));
    g_album_photo_navigation.art = config->art;
    g_album_photo_navigation.counter_label = config->counter_label;
    g_album_photo_navigation.render_cb = config->render_cb;
    g_album_photo_navigation.pause_cache_cb = config->pause_cache_cb;
    g_album_photo_navigation.resume_cache_cb = config->resume_cache_cb;
    g_album_photo_navigation.cache_busy_cb = config->cache_busy_cb;
    g_album_photo_navigation.user_data = config->user_data;
    g_album_photo_navigation.store_version =
        ai_album_album_store_version();
    g_album_photo_navigation.requested_index =
        ai_album_album_store_selected();
    g_album_photo_navigation.requested_valid = 1U;
    g_album_photo_navigation.active = 1U;
    result = ai_album_album_image_view_get_result(config->art->image);
    g_album_photo_navigation.request_active =
        (uint8_t)(result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING);
    if (result < AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        g_album_photo_navigation.last_input_ms = os_mseconds();
        g_album_photo_navigation.retry_at_ms =
            g_album_photo_navigation.last_input_ms +
            ALBUM_PHOTO_NAVIGATION_DEBOUNCE_MS;
        album_photo_navigation_pause_cache();
    }
    album_photo_navigation_update_counter();
    return RET_OK;
}

int ai_album_album_photo_navigation_move(int8_t delta)
{
    uint8_t count = ai_album_album_store_count();
    uint8_t selected = ai_album_album_store_selected();
    uint8_t offset;

    if (!album_photo_navigation_view_valid() || delta == 0 || count < 2U ||
        selected >= count) {
        return RET_ERR;
    }
    offset = delta < 0 ? (uint8_t)(count - 1U) : 1U;
    if (ai_album_album_store_select(
            (uint8_t)((selected + offset) % count)) != RET_OK) {
        return RET_ERR;
    }
    g_album_photo_navigation.last_input_ms = os_mseconds();
    g_album_photo_navigation.retry_at_ms = 0U;
    album_photo_navigation_update_counter();
    album_photo_navigation_pause_cache();
    return RET_OK;
}

void ai_album_album_photo_navigation_poll(void)
{
    ai_album_album_image_view_result_t result;
    uint8_t selected;
    uint64 now_ms;

    if (!album_photo_navigation_view_valid()) return;
    selected = ai_album_album_store_selected();
    if (selected >= ai_album_album_store_count()) return;
    now_ms = os_mseconds();
    if (ai_album_album_store_version() !=
        g_album_photo_navigation.store_version) {
        g_album_photo_navigation.store_version =
            ai_album_album_store_version();
        g_album_photo_navigation.requested_valid = 0U;
        g_album_photo_navigation.retry_at_ms = 0U;
        g_album_photo_navigation.last_input_ms = now_ms;
        album_photo_navigation_pause_cache();
    }
    result = ai_album_album_image_view_get_result(
        g_album_photo_navigation.art->image);
    if (g_album_photo_navigation.request_active) {
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) return;
        g_album_photo_navigation.request_active = 0U;
        if (result < AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
            g_album_photo_navigation.retry_at_ms =
                now_ms + ALBUM_PHOTO_NAVIGATION_DEBOUNCE_MS;
        }
    }
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) return;
    if (!g_album_photo_navigation.cache_paused) return;
    if (g_album_photo_navigation.cache_busy_cb != NULL &&
        g_album_photo_navigation.cache_busy_cb(
            g_album_photo_navigation.user_data)) {
        return;
    }
    if (!album_photo_navigation_elapsed(
            now_ms, g_album_photo_navigation.last_input_ms,
            ALBUM_PHOTO_NAVIGATION_DEBOUNCE_MS)) {
        return;
    }
    if (g_album_photo_navigation.requested_valid &&
        g_album_photo_navigation.requested_index == selected &&
        result >= AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK &&
        g_album_photo_navigation.retry_at_ms == 0U &&
        !g_album_photo_navigation.request_active) {
        album_photo_navigation_resume_cache();
        return;
    }
    if (g_album_photo_navigation.retry_at_ms != 0U &&
        now_ms < g_album_photo_navigation.retry_at_ms) {
        return;
    }
    album_photo_navigation_submit(selected, now_ms);
}

void ai_album_album_photo_navigation_stop(void)
{
    if (!g_album_photo_navigation.active) {
        memset(&g_album_photo_navigation, 0, sizeof(g_album_photo_navigation));
        return;
    }
    memset(&g_album_photo_navigation, 0, sizeof(g_album_photo_navigation));
}

uint8_t ai_album_album_photo_navigation_is_active(void)
{
    return g_album_photo_navigation.active;
}

uint8_t ai_album_album_photo_navigation_is_cache_paused(void)
{
    return (uint8_t)(g_album_photo_navigation.active &&
                     g_album_photo_navigation.cache_paused);
}
