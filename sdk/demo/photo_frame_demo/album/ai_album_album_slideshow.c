#include "album/ai_album_album_slideshow.h"

#include "album/ai_album_album_photo_cache.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"

#define ALBUM_SLIDESHOW_PERIOD_MS 3000U
#define ALBUM_SLIDESHOW_SERVICE_MS 50U

typedef enum {
    ALBUM_SLIDESHOW_PENDING_IDLE = 0,
    ALBUM_SLIDESHOW_PENDING_LOADING,
    ALBUM_SLIDESHOW_PENDING_IMAGE_READY,
    ALBUM_SLIDESHOW_PENDING_DIRECT_READY,
} album_slideshow_pending_t;

typedef struct {
    ai_album_album_art_view_t *art;
    ai_album_album_slideshow_commit_cb_t commit_cb;
    void *user_data;
    lv_timer_t *timer;
    uint32_t store_version;
    uint32_t cycle_start_tick;
    uint8_t candidate_index;
    uint8_t pending_index;
    uint8_t attempts;
    uint8_t active;
    uint8_t cycle_started;
    uint8_t deadline_elapsed;
    album_slideshow_pending_t pending;
} album_slideshow_state_t;

static album_slideshow_state_t g_album_slideshow;

static uint8_t album_slideshow_next_index(uint8_t index, uint8_t count)
{
    return (uint8_t)((index + 1U) % count);
}

static uint8_t album_slideshow_store_valid(void)
{
    uint8_t count = ai_album_album_store_count();

    return (uint8_t)(g_album_slideshow.active && count > 1U &&
                     ai_album_album_store_selected() < count &&
                     ai_album_album_store_version() ==
                         g_album_slideshow.store_version);
}

static uint8_t album_slideshow_front_settled(void)
{
    const ai_album_album_photo_t *photo = ai_album_album_store_get(
        ai_album_album_store_selected());

    if (photo == NULL || g_album_slideshow.art == NULL) return 0U;
    if (!photo->storage_backed) return 1U;
    return (uint8_t)(ai_album_album_image_view_get_result(
                         g_album_slideshow.art->image) !=
                     AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING);
}

static void album_slideshow_image_complete(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data);

static int album_slideshow_prepare_candidate(void)
{
    uint8_t count = ai_album_album_store_count();

    if (count < 2U || ai_album_album_store_selected() >= count) {
        return RET_ERR;
    }
    while (g_album_slideshow.attempts < (uint8_t)(count - 1U)) {
        const ai_album_album_photo_t *photo;
        ai_album_album_image_view_result_t result;
        uint8_t index = g_album_slideshow.candidate_index;

        g_album_slideshow.candidate_index =
            album_slideshow_next_index(index, count);
        g_album_slideshow.attempts++;
        photo = ai_album_album_store_get(index);
        if (photo == NULL) continue;
        g_album_slideshow.pending_index = index;
        if (!photo->storage_backed) {
            g_album_slideshow.pending =
                ALBUM_SLIDESHOW_PENDING_DIRECT_READY;
            os_printf("[ALBUM_SLIDE] ready direct index=%u\r\n",
                      (unsigned)index);
            return RET_OK;
        }
        g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_LOADING;
        result = ai_album_album_art_prepare_async(
            g_album_slideshow.art, photo, album_slideshow_image_complete,
            &g_album_slideshow);
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_PENDING) {
            os_printf("[ALBUM_SLIDE] prefetch index=%u attempt=%u\r\n",
                      (unsigned)index,
                      (unsigned)g_album_slideshow.attempts);
            return RET_OK;
        }
        if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
            g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IMAGE_READY;
            os_printf("[ALBUM_SLIDE] ready cached index=%u\r\n",
                      (unsigned)index);
            return RET_OK;
        }
        g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IDLE;
        os_printf("[ALBUM_SLIDE] reject index=%u result=%d\r\n",
                  (unsigned)index, (int)result);
    }
    return RET_ERR;
}

static int album_slideshow_begin_cycle(void)
{
    uint8_t count = ai_album_album_store_count();

    if (count < 2U || ai_album_album_store_selected() >= count) {
        return RET_ERR;
    }
    g_album_slideshow.cycle_start_tick = lv_tick_get();
    g_album_slideshow.cycle_started = 1U;
    g_album_slideshow.deadline_elapsed = 0U;
    g_album_slideshow.attempts = 0U;
    g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IDLE;
    g_album_slideshow.candidate_index = album_slideshow_next_index(
        ai_album_album_store_selected(), count);
    return album_slideshow_prepare_candidate();
}

static uint8_t album_slideshow_commit_art(
    const ai_album_album_photo_t *photo)
{
    ai_album_album_image_view_result_t result;

    if (g_album_slideshow.pending ==
        ALBUM_SLIDESHOW_PENDING_IMAGE_READY) {
        result = ai_album_album_art_commit_prepared(
            g_album_slideshow.art, photo);
        return (uint8_t)(result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK);
    }
    if (g_album_slideshow.pending ==
        ALBUM_SLIDESHOW_PENDING_DIRECT_READY) {
        result = ai_album_album_art_set(g_album_slideshow.art, photo);
        return (uint8_t)(result >= AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK);
    }
    return 0U;
}

static void album_slideshow_commit_ready(void)
{
    const ai_album_album_photo_t *photo = ai_album_album_store_get(
        g_album_slideshow.pending_index);
    uint8_t committed_index = g_album_slideshow.pending_index;

    if (photo == NULL || !album_slideshow_commit_art(photo)) {
        os_printf("[ALBUM_SLIDE] commit failed index=%u\r\n",
                  (unsigned)committed_index);
        g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IDLE;
        (void)album_slideshow_begin_cycle();
        return;
    }
    ai_album_album_store_select(committed_index);
    g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IDLE;
    os_printf("[ALBUM_SLIDE] swap index=%u\r\n",
              (unsigned)committed_index);
    if (g_album_slideshow.commit_cb != NULL) {
        g_album_slideshow.commit_cb(committed_index,
                                    g_album_slideshow.user_data);
    }
    if (g_album_slideshow.active) {
        (void)album_slideshow_begin_cycle();
    }
}

static void album_slideshow_image_complete(
    lv_obj_t *image,
    ai_album_album_image_view_result_t result,
    void *user_data)
{
    (void)image;
    if (user_data != &g_album_slideshow || !g_album_slideshow.active ||
        g_album_slideshow.pending != ALBUM_SLIDESHOW_PENDING_LOADING) {
        return;
    }
    if (!album_slideshow_store_valid()) {
        ai_album_album_slideshow_stop();
        return;
    }
    if (result == AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_OK) {
        g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IMAGE_READY;
        os_printf("[ALBUM_SLIDE] ready index=%u\r\n",
                  (unsigned)g_album_slideshow.pending_index);
    } else {
        os_printf("[ALBUM_SLIDE] failed index=%u result=%d\r\n",
                  (unsigned)g_album_slideshow.pending_index, (int)result);
        g_album_slideshow.pending = ALBUM_SLIDESHOW_PENDING_IDLE;
        (void)album_slideshow_prepare_candidate();
    }
    if (!g_album_slideshow.deadline_elapsed) return;
    if (g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_IMAGE_READY ||
        g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_DIRECT_READY) {
        album_slideshow_commit_ready();
    } else if (g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_IDLE) {
        (void)album_slideshow_begin_cycle();
    }
}

static void album_slideshow_service(lv_timer_t *timer)
{
    (void)timer;
    if (!album_slideshow_store_valid()) {
        os_printf("[ALBUM_SLIDE] stopped store changed\r\n");
        ai_album_album_slideshow_stop();
        return;
    }
    if (!g_album_slideshow.cycle_started) {
        if (album_slideshow_front_settled()) {
            (void)album_slideshow_begin_cycle();
        }
        return;
    }
    if (lv_tick_elaps(g_album_slideshow.cycle_start_tick) <
        ALBUM_SLIDESHOW_PERIOD_MS) {
        return;
    }
    if (!g_album_slideshow.deadline_elapsed &&
        g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_LOADING) {
        os_printf("[ALBUM_SLIDE] hold index=%u\r\n",
                  (unsigned)g_album_slideshow.pending_index);
    }
    g_album_slideshow.deadline_elapsed = 1U;
    if (g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_IMAGE_READY ||
        g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_DIRECT_READY) {
        album_slideshow_commit_ready();
    } else if (g_album_slideshow.pending == ALBUM_SLIDESHOW_PENDING_IDLE) {
        (void)album_slideshow_begin_cycle();
    }
}

int ai_album_album_slideshow_start(
    ai_album_album_art_view_t *art,
    ai_album_album_slideshow_commit_cb_t commit_cb,
    void *user_data)
{
    if (art == NULL || art->image == NULL || commit_cb == NULL ||
        ai_album_album_store_count() < 2U) {
        return RET_ERR;
    }
    ai_album_album_slideshow_stop();
    g_album_slideshow.art = art;
    g_album_slideshow.commit_cb = commit_cb;
    g_album_slideshow.user_data = user_data;
    g_album_slideshow.store_version = ai_album_album_store_version();
    g_album_slideshow.active = 1U;
    g_album_slideshow.timer = lv_timer_create(
        album_slideshow_service, ALBUM_SLIDESHOW_SERVICE_MS, NULL);
    if (g_album_slideshow.timer == NULL) {
        memset(&g_album_slideshow, 0, sizeof(g_album_slideshow));
        return RET_ERR;
    }
    os_printf("[ALBUM_SLIDE] start current=%u period=%ums\r\n",
              (unsigned)ai_album_album_store_selected(),
              (unsigned)ALBUM_SLIDESHOW_PERIOD_MS);
    if (album_slideshow_front_settled()) {
        (void)album_slideshow_begin_cycle();
    }
    return RET_OK;
}

void ai_album_album_slideshow_stop(void)
{
    ai_album_album_art_view_t *art = g_album_slideshow.art;
    lv_timer_t *timer = g_album_slideshow.timer;
    uint8_t was_active = g_album_slideshow.active;

    /* Both controllers share one loader client, so ownership must not overlap. */
    ai_album_album_photo_cache_stop();
    memset(&g_album_slideshow, 0, sizeof(g_album_slideshow));
    if (art != NULL) ai_album_album_art_cancel_prepare(art);
    if (timer != NULL) lv_timer_delete(timer);
    if (was_active) os_printf("[ALBUM_SLIDE] stop\r\n");
}
