#include "album/ai_album_album_gallery_loader.h"

#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_store.h"
#include "basic_include.h"
#include "lv_image_cache_compat.h"
enum {
    ALBUM_GALLERY_THUMB_WIDTH = 232U,
    ALBUM_GALLERY_THUMB_HEIGHT = 128U,
    ALBUM_GALLERY_THUMB_BLOCK_BYTES =
        ALBUM_GALLERY_THUMB_WIDTH * ALBUM_GALLERY_THUMB_HEIGHT * 2U,
    ALBUM_GALLERY_THUMB_PAGE_BYTES =
        AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE *
        ALBUM_GALLERY_THUMB_BLOCK_BYTES,
    ALBUM_GALLERY_PAGES_PER_SLOT = 1U,
    ALBUM_GALLERY_CACHE_PAGE_COUNT =
        AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT *
        ALBUM_GALLERY_PAGES_PER_SLOT,
    ALBUM_GALLERY_PAGE_NONE = 0xFFU,
    ALBUM_GALLERY_CACHE_NONE = 0xFFU,
};

_Static_assert(ALBUM_GALLERY_THUMB_PAGE_BYTES *
                       ALBUM_GALLERY_PAGES_PER_SLOT <=
                   AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_BYTES,
               "Gallery thumbnail pages exceed shared image slot");
_Static_assert(ALBUM_GALLERY_THUMB_BLOCK_BYTES %
                       AI_ALBUM_ALBUM_IMAGE_LOADER_TARGET_ALIGNMENT == 0U,
               "Gallery thumbnail offsets must be loader aligned");
typedef enum {
    ALBUM_GALLERY_ITEM_EMPTY = 0,
    ALBUM_GALLERY_ITEM_LOADING,
    ALBUM_GALLERY_ITEM_READY,
    ALBUM_GALLERY_ITEM_FAILED,
    ALBUM_GALLERY_ITEM_SKIPPED,
} album_gallery_item_state_t;
typedef struct {
    lv_image_dsc_t descriptor;
    ai_album_album_image_view_result_t result;
    album_gallery_item_state_t state;
} album_gallery_item_t;
typedef struct {
    /* The two large loader slots hold pages; the UI still owns nine cards. */
    album_gallery_item_t items[AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE];
    uint32_t age;
    uint8_t page;
    uint8_t valid;
} album_gallery_cache_page_t;
typedef struct {
    ai_album_album_art_view_t *cards;
    lv_timer_t *start_timer;
    album_gallery_cache_page_t pages[ALBUM_GALLERY_CACHE_PAGE_COUNT];
    uint32_t generation;
    uint32_t cache_clock;
    uint8_t photo_count;
    uint8_t page_count;
    uint8_t card_capacity;
    uint8_t current_page;
    uint8_t loading_cache;
    uint8_t loading_page;
    uint8_t loading_cell;
    uint8_t loading;
    uint8_t active;
} album_gallery_loader_state_t;
static album_gallery_loader_state_t g_gallery_loader;
static void gallery_loader_cache_drop(album_gallery_cache_page_t *cache);
static void gallery_loader_start_timer_cb(lv_timer_t *timer);
static void gallery_loader_arm_retry(void)
{
    if (!g_gallery_loader.active || g_gallery_loader.start_timer != NULL) {
        return;
    }
    g_gallery_loader.start_timer = lv_timer_create(
        gallery_loader_start_timer_cb, 50U, NULL);
    if (g_gallery_loader.start_timer == NULL) {
        os_printf("[ALBUM_GALLERY] retry timer unavailable\r\n");
    } else {
        lv_timer_set_repeat_count(g_gallery_loader.start_timer, 1);
    }
}

static uint8_t gallery_loader_card_valid(uint8_t cell)
{
    return (uint8_t)(g_gallery_loader.cards != NULL &&
                     cell < g_gallery_loader.card_capacity &&
                     g_gallery_loader.cards[cell].root != NULL &&
                     g_gallery_loader.cards[cell].image != NULL &&
                     lv_obj_is_valid(g_gallery_loader.cards[cell].root) &&
                     lv_obj_is_valid(g_gallery_loader.cards[cell].image));
}

static uint8_t gallery_loader_page_count(uint8_t photo_count)
{
    return photo_count == 0U ? 0U :
           (uint8_t)((photo_count + AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE - 1U) /
                     AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE);
}
static uint16_t gallery_loader_photo_index(uint8_t page, uint8_t cell)
{
    return (uint16_t)page * AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE + cell;
}
static uint8_t gallery_loader_window_start(uint8_t center)
{
    uint8_t start = center > 0U ? (uint8_t)(center - 1U) : 0U;
    uint8_t max_start = g_gallery_loader.page_count >
                                ALBUM_GALLERY_CACHE_PAGE_COUNT ?
                            (uint8_t)(g_gallery_loader.page_count -
                                      ALBUM_GALLERY_CACHE_PAGE_COUNT) : 0U;
    return start > max_start ? max_start : start;
}
static uint8_t gallery_loader_priority_page(uint8_t center, uint8_t priority)
{
    uint8_t count;
    uint8_t next_count;
    uint8_t start;
    if (g_gallery_loader.page_count == 0U ||
        center >= g_gallery_loader.page_count ||
        priority >= g_gallery_loader.page_count) return center;
    if (priority == 0U) return center;
    count = g_gallery_loader.page_count < ALBUM_GALLERY_CACHE_PAGE_COUNT ?
                g_gallery_loader.page_count : ALBUM_GALLERY_CACHE_PAGE_COUNT;
    start = gallery_loader_window_start(center);
    next_count = (uint8_t)(start + count - center - 1U);
    if (center != start && next_count > 2U) next_count = 2U;
    return priority <= next_count ? (uint8_t)(center + priority) :
           (uint8_t)(center - (priority - next_count));
}
static album_gallery_cache_page_t *gallery_loader_cache(uint8_t index)
{
    return index < ALBUM_GALLERY_CACHE_PAGE_COUNT ?
               &g_gallery_loader.pages[index] : NULL;
}
static album_gallery_cache_page_t *gallery_loader_find_cache(uint8_t page)
{
    uint8_t index;
    for (index = 0U; index < ALBUM_GALLERY_CACHE_PAGE_COUNT; ++index) {
        album_gallery_cache_page_t *cache = &g_gallery_loader.pages[index];
        if (cache->valid && cache->page == page) return cache;
    }
    return NULL;
}
static uint8_t gallery_loader_cache_index(
    const album_gallery_cache_page_t *cache)
{
    if (cache == NULL || cache < g_gallery_loader.pages ||
        cache >= g_gallery_loader.pages + ALBUM_GALLERY_CACHE_PAGE_COUNT) {
        return ALBUM_GALLERY_CACHE_NONE;
    }
    return (uint8_t)(cache - g_gallery_loader.pages);
}
static uint8_t gallery_loader_target_slot(uint8_t cache_index)
{
    return (uint8_t)(cache_index / ALBUM_GALLERY_PAGES_PER_SLOT);
}
static uint32_t gallery_loader_target_offset(uint8_t cache_index, uint8_t cell)
{
    if (cache_index >= ALBUM_GALLERY_CACHE_PAGE_COUNT ||
        cell >= AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE) return 0xFFFFFFFFU;
    return (uint32_t)(cache_index % ALBUM_GALLERY_PAGES_PER_SLOT) *
               ALBUM_GALLERY_THUMB_PAGE_BYTES +
           (uint32_t)cell * ALBUM_GALLERY_THUMB_BLOCK_BYTES;
}
static void gallery_loader_touch_cache(album_gallery_cache_page_t *cache)
{
    if (cache == NULL || !cache->valid) return;
    g_gallery_loader.cache_clock++;
    if (g_gallery_loader.cache_clock == 0U) {
        g_gallery_loader.cache_clock = 1U;
    }
    cache->age = g_gallery_loader.cache_clock;
}
static void gallery_loader_cache_drop(album_gallery_cache_page_t *cache)
{
    uint8_t cell;
    if (cache == NULL) return;
    for (cell = 0U; cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE; ++cell) {
        lv_image_dsc_t *descriptor = &cache->items[cell].descriptor;
        if (descriptor->data != NULL) lv_image_cache_drop(descriptor);
    }
    memset(cache, 0, sizeof(*cache));
    cache->page = ALBUM_GALLERY_PAGE_NONE;
}
static void gallery_loader_cache_prepare(
    album_gallery_cache_page_t *cache,
    uint8_t page)
{
    gallery_loader_cache_drop(cache);
    cache->page = page;
    cache->valid = 1U;
    gallery_loader_touch_cache(cache);
}

static uint8_t gallery_loader_page_is_desired(uint8_t center, uint8_t page)
{
    uint8_t start = gallery_loader_window_start(center);
    uint8_t count = g_gallery_loader.page_count <
                            ALBUM_GALLERY_CACHE_PAGE_COUNT ?
                        g_gallery_loader.page_count :
                        ALBUM_GALLERY_CACHE_PAGE_COUNT;
    return (uint8_t)(page >= start && page < (uint8_t)(start + count));
}

static int gallery_loader_choose_victim(uint8_t center, uint8_t *index_out)
{
    uint8_t index;
    uint8_t oldest = ALBUM_GALLERY_CACHE_NONE;
    for (index = 0U; index < ALBUM_GALLERY_CACHE_PAGE_COUNT; ++index) {
        if (!g_gallery_loader.pages[index].valid) {
            *index_out = index;
            return RET_OK;
        }
    }
    for (index = 0U; index < ALBUM_GALLERY_CACHE_PAGE_COUNT; ++index) {
        album_gallery_cache_page_t *cache = &g_gallery_loader.pages[index];
        if (gallery_loader_page_is_desired(center, cache->page)) continue;
        if (oldest == ALBUM_GALLERY_CACHE_NONE ||
            cache->age < g_gallery_loader.pages[oldest].age) {
            oldest = index;
        }
    }
    if (oldest == ALBUM_GALLERY_CACHE_NONE) return RET_ERR;
    *index_out = oldest;
    return RET_OK;
}

static int gallery_loader_ensure_page(uint8_t center, uint8_t page)
{
    album_gallery_cache_page_t *cache = gallery_loader_find_cache(page);
    uint8_t index;
    if (cache != NULL) {
        gallery_loader_touch_cache(cache);
        return RET_OK;
    }
    if (gallery_loader_choose_victim(center, &index) != RET_OK) {
        return RET_ERR;
    }
    cache = &g_gallery_loader.pages[index];
    gallery_loader_cache_prepare(cache, page);
    os_printf("[ALBUM_GALLERY] cache assign page=%u slot=%u\r\n",
              (unsigned)(page + 1U),
              (unsigned)gallery_loader_target_slot(index));
    return RET_OK;
}

static int gallery_loader_reconcile_cache(uint8_t center)
{
    uint8_t priority;
    for (priority = 0U;
         priority < ALBUM_GALLERY_CACHE_PAGE_COUNT;
         ++priority) {
        uint8_t page = gallery_loader_priority_page(center, priority);
        if (gallery_loader_ensure_page(center, page) != RET_OK) {
            return RET_ERR;
        }
    }
    return RET_OK;
}

static album_gallery_item_t *gallery_loader_item(uint8_t page, uint8_t cell)
{
    album_gallery_cache_page_t *cache;
    if (cell >= AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE) return NULL;
    cache = gallery_loader_find_cache(page);
    return cache == NULL ? NULL : &cache->items[cell];
}

static void gallery_loader_detach_cards(void)
{
    uint8_t cell;
    for (cell = 0U; cell < g_gallery_loader.card_capacity; ++cell) {
        if (!gallery_loader_card_valid(cell)) continue;
        lv_image_set_src(g_gallery_loader.cards[cell].image, NULL);
        lv_obj_add_flag(g_gallery_loader.cards[cell].image,
                        LV_OBJ_FLAG_HIDDEN);
    }
}

static void gallery_loader_drop_caches(void)
{
    uint8_t index;
    for (index = 0U; index < ALBUM_GALLERY_CACHE_PAGE_COUNT; ++index) {
        gallery_loader_cache_drop(&g_gallery_loader.pages[index]);
    }
}

static void gallery_loader_cancel_request(void)
{
    album_gallery_item_t *item;
    if (!g_gallery_loader.loading) return;
    item = gallery_loader_item(g_gallery_loader.loading_page,
                               g_gallery_loader.loading_cell);
    if (item != NULL && item->state == ALBUM_GALLERY_ITEM_LOADING) {
        item->state = ALBUM_GALLERY_ITEM_EMPTY;
    }
    ai_album_album_image_loader_cancel(&g_gallery_loader);
    g_gallery_loader.loading = 0U;
    g_gallery_loader.loading_cache = ALBUM_GALLERY_CACHE_NONE;
    g_gallery_loader.generation = 0U;
}

static void gallery_loader_reset(void)
{
    if (g_gallery_loader.start_timer != NULL) {
        lv_timer_delete(g_gallery_loader.start_timer);
        g_gallery_loader.start_timer = NULL;
    }
    gallery_loader_cancel_request();
    gallery_loader_detach_cards();
    gallery_loader_drop_caches();
    ai_album_album_image_loader_release_owner(&g_gallery_loader);
    memset(&g_gallery_loader, 0, sizeof(g_gallery_loader));
}

static ai_album_album_image_view_result_t gallery_loader_view_result(
    ai_album_album_image_loader_result_t result)
{
    return result == AI_ALBUM_ALBUM_IMAGE_LOADER_FILE_UNAVAILABLE ?
               AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_FILE_UNAVAILABLE :
               AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED;
}

static void gallery_loader_bind_card(uint8_t cell)
{
    uint16_t photo_index;
    const ai_album_album_photo_t *photo;
    album_gallery_item_t *item;
    ai_album_album_art_view_t *card;
    if (!gallery_loader_card_valid(cell)) return;
    card = &g_gallery_loader.cards[cell];
    photo_index = gallery_loader_photo_index(g_gallery_loader.current_page,
                                              cell);
    if (photo_index >= g_gallery_loader.photo_count) {
        lv_image_set_src(card->image, NULL);
        lv_obj_add_flag(card->image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card->root, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_HIDDEN);
    photo = ai_album_album_store_get((uint8_t)photo_index);
    item = gallery_loader_item(g_gallery_loader.current_page, cell);
    if (item != NULL && item->state == ALBUM_GALLERY_ITEM_READY) {
        /* 直接换图,不清空中转:避免翻页时整页缩略图闪烁 */
        (void)ai_album_album_art_set_decoded(card, photo, &item->descriptor);
    } else if (item != NULL && item->state == ALBUM_GALLERY_ITEM_FAILED) {
        ai_album_album_art_set_error(card, photo, item->result);
    } else {
        lv_image_set_src(card->image, NULL);
        lv_obj_add_flag(card->image, LV_OBJ_FLAG_HIDDEN);
        ai_album_album_art_set_loading(card, photo);
    }
}

static void gallery_loader_bind_page(void)
{
    uint8_t cell;
    for (cell = 0U; cell < g_gallery_loader.card_capacity; ++cell) {
        gallery_loader_bind_card(cell);
    }
}

static void gallery_loader_dimensions(uint8_t cell,
                                      uint16_t *width,
                                      uint16_t *height)
{
    int32_t image_width = ALBUM_GALLERY_THUMB_WIDTH;
    int32_t image_height = ALBUM_GALLERY_THUMB_HEIGHT;
    if (gallery_loader_card_valid(cell)) {
        image_width = lv_obj_get_width(g_gallery_loader.cards[cell].image);
        image_height = lv_obj_get_height(g_gallery_loader.cards[cell].image);
    }
    if (image_width <= 0 || image_width > ALBUM_GALLERY_THUMB_WIDTH) {
        image_width = ALBUM_GALLERY_THUMB_WIDTH;
    }
    if (image_height <= 0 || image_height > ALBUM_GALLERY_THUMB_HEIGHT) {
        image_height = ALBUM_GALLERY_THUMB_HEIGHT;
    }
    *width = (uint16_t)image_width;
    *height = (uint16_t)image_height;
}

static uint8_t gallery_loader_photo_can_load(uint8_t page, uint8_t cell)
{
    uint16_t index = gallery_loader_photo_index(page, cell);
    const ai_album_album_photo_t *photo;
    if (index >= g_gallery_loader.photo_count) return 0U;
    photo = ai_album_album_store_get((uint8_t)index);
    return (uint8_t)(photo != NULL && photo->storage_backed &&
                     photo->path[0] != '\0');
}

static int gallery_loader_find_in_cache(uint8_t cache_index,
                                        uint8_t *cell_out)
{
    album_gallery_cache_page_t *cache =
        gallery_loader_cache(cache_index);
    uint8_t cell;
    if (cache == NULL || !cache->valid) return RET_ERR;
    for (cell = 0U; cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE; ++cell) {
        album_gallery_item_t *item = &cache->items[cell];
        if (item->state != ALBUM_GALLERY_ITEM_EMPTY) continue;
        if (!gallery_loader_photo_can_load(cache->page, cell)) {
            item->state = ALBUM_GALLERY_ITEM_SKIPPED;
            continue;
        }
        *cell_out = cell;
        return RET_OK;
    }
    return RET_ERR;
}

static int gallery_loader_find_next(uint8_t *cache_out, uint8_t *cell_out)
{
    uint8_t priority;
    for (priority = 0U;
         priority < ALBUM_GALLERY_CACHE_PAGE_COUNT;
         ++priority) {
        uint8_t page = gallery_loader_priority_page(
            g_gallery_loader.current_page, priority);
        album_gallery_cache_page_t *cache = gallery_loader_find_cache(page);
        uint8_t cache_index;
        if (cache == NULL) continue;
        cache_index = gallery_loader_cache_index(cache);
        if (gallery_loader_find_in_cache(cache_index, cell_out) == RET_OK) {
            *cache_out = cache_index;
            return RET_OK;
        }
    }
    return RET_ERR;
}

static void gallery_loader_complete(
    const ai_album_album_image_loader_completion_t *completion,
    void *client);
static int gallery_loader_submit(uint8_t cache_index, uint8_t cell)
{
    album_gallery_cache_page_t *cache = gallery_loader_cache(cache_index);
    uint16_t photo_index;
    const ai_album_album_photo_t *photo;
    album_gallery_item_t *item;
    ai_album_album_image_loader_request_t request;
    uint16_t width;
    uint16_t height;
    if (cache == NULL || !cache->valid ||
        cell >= AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE) {
        return RET_ERR;
    }
    photo_index = gallery_loader_photo_index(cache->page, cell);
    if (photo_index >= g_gallery_loader.photo_count) return RET_ERR;
    photo = ai_album_album_store_get((uint8_t)photo_index);
    item = &cache->items[cell];
    if (photo == NULL || item->state != ALBUM_GALLERY_ITEM_EMPTY) {
        return RET_ERR;
    }
    gallery_loader_dimensions(cell, &width, &height);
    request = (ai_album_album_image_loader_request_t){
        .path = photo->path,
        .max_width = width,
        .max_height = height,
        .target_offset = gallery_loader_target_offset(cache_index, cell),
        .target_slot = gallery_loader_target_slot(cache_index),
        .completion_cb = gallery_loader_complete,
        .client = &g_gallery_loader,
    };
    /* Publish the callback context before waking the worker. */
    item->state = ALBUM_GALLERY_ITEM_LOADING;
    g_gallery_loader.loading_cache = cache_index;
    g_gallery_loader.loading_page = cache->page;
    g_gallery_loader.loading_cell = cell;
    g_gallery_loader.loading = 1U;
    if (ai_album_album_image_loader_submit(
            &request, &g_gallery_loader.generation) != RET_OK) {
        item->state = ALBUM_GALLERY_ITEM_EMPTY;
        g_gallery_loader.loading = 0U;
        g_gallery_loader.loading_cache = ALBUM_GALLERY_CACHE_NONE;
        g_gallery_loader.generation = 0U;
        return RET_ERR;
    }
    os_printf("[ALBUM_GALLERY] load page=%u slot=%u cell=%u index=%u path=%s\r\n",
              (unsigned)(cache->page + 1U),
              (unsigned)gallery_loader_target_slot(cache_index),
              (unsigned)cell, (unsigned)photo_index, photo->path);
    return RET_OK;
}

static void gallery_loader_schedule_next(void)
{
    uint8_t cache_index;
    uint8_t cell;
    while (g_gallery_loader.active && !g_gallery_loader.loading &&
           gallery_loader_find_next(&cache_index, &cell) == RET_OK) {
        if (gallery_loader_submit(cache_index, cell) == RET_OK) return;
        /* A full loader queue is transient; keep the item retryable instead
         * of converting a resource race into a permanent decode failure. */
        gallery_loader_arm_retry();
        return;
    }
}

static int gallery_loader_prepare_descriptor(
    album_gallery_item_t *item,
    const ai_album_album_image_loader_completion_t *completion)
{
    uint32_t expected_size;
    if (item == NULL || completion == NULL || completion->data == NULL ||
        completion->width == 0U || completion->height == 0U ||
        completion->stride == 0U ||
        completion->width > ALBUM_GALLERY_THUMB_WIDTH ||
        completion->height > ALBUM_GALLERY_THUMB_HEIGHT) {
        return RET_ERR;
    }
    expected_size = completion->stride * completion->height;
    if (completion->stride != (uint32_t)completion->width * 2U ||
        expected_size != completion->data_size ||
        completion->data_size > ALBUM_GALLERY_THUMB_BLOCK_BYTES) {
        return RET_ERR;
    }
    /* 本工程LVGL 9.0的lv_img_header_t布局(无9.5的magic/flags/stride);
     * cf用TRUE_COLOR(工程RGB565, stride=w*2由LVGL自算) */
    item->descriptor = (lv_image_dsc_t){
        .header.cf = LV_IMG_CF_TRUE_COLOR,
        .header.always_zero = 0,
        .header.reserved = 0,
        .header.w = completion->width,
        .header.h = completion->height,
        .data_size = completion->data_size,
        .data = completion->data,
    };
    return RET_OK;
}

static uint8_t gallery_loader_completion_matches(
    const ai_album_album_image_loader_completion_t *completion,
    void *client)
{
    album_gallery_cache_page_t *cache =
        gallery_loader_cache(g_gallery_loader.loading_cache);
    album_gallery_item_t *item = NULL;
    uint32_t expected_offset =
        gallery_loader_target_offset(g_gallery_loader.loading_cache,
                                     g_gallery_loader.loading_cell);
    if (cache != NULL &&
        g_gallery_loader.loading_cell < AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE) {
        item = &cache->items[g_gallery_loader.loading_cell];
    }
    return (uint8_t)(client == &g_gallery_loader && completion != NULL &&
                     g_gallery_loader.active && g_gallery_loader.loading &&
                     cache != NULL && cache->valid &&
                     cache->page == g_gallery_loader.loading_page &&
                     item != NULL && item->state == ALBUM_GALLERY_ITEM_LOADING &&
                     completion->generation == g_gallery_loader.generation &&
                     completion->target_slot ==
                         gallery_loader_target_slot(
                             g_gallery_loader.loading_cache) &&
                     completion->target_offset == expected_offset);
}

static void gallery_loader_complete(
    const ai_album_album_image_loader_completion_t *completion,
    void *client)
{
    album_gallery_cache_page_t *cache;
    album_gallery_item_t *item;
    uint8_t page;
    uint8_t cell;
    if (!gallery_loader_completion_matches(completion, client)) return;
    cache = gallery_loader_cache(g_gallery_loader.loading_cache);
    page = g_gallery_loader.loading_page;
    cell = g_gallery_loader.loading_cell;
    item = cache == NULL ? NULL : &cache->items[cell];
    g_gallery_loader.loading = 0U;
    if (item == NULL) {
        g_gallery_loader.loading_cache = ALBUM_GALLERY_CACHE_NONE;
        return;
    }
    if (completion->result == AI_ALBUM_ALBUM_IMAGE_LOADER_OK &&
        gallery_loader_prepare_descriptor(item, completion) == RET_OK) {
        item->state = ALBUM_GALLERY_ITEM_READY;
        os_printf("[ALBUM_GALLERY] ready page=%u slot=%u cell=%u size=%ux%u\r\n",
                  (unsigned)(page + 1U),
                  (unsigned)gallery_loader_target_slot(
                      g_gallery_loader.loading_cache),
                  (unsigned)cell, (unsigned)completion->width,
                  (unsigned)completion->height);
    } else {
        item->state = ALBUM_GALLERY_ITEM_FAILED;
        item->result = gallery_loader_view_result(completion->result);
        os_printf("[ALBUM_GALLERY] failed page=%u slot=%u cell=%u result=%d\r\n",
                  (unsigned)(page + 1U),
                  (unsigned)gallery_loader_target_slot(
                      g_gallery_loader.loading_cache),
                  (unsigned)cell, (int)completion->result);
    }
    if (page == g_gallery_loader.current_page) gallery_loader_bind_card(cell);
    g_gallery_loader.loading_cache = ALBUM_GALLERY_CACHE_NONE;
    gallery_loader_schedule_next();
}

static void gallery_loader_start_timer_cb(lv_timer_t *timer)
{
    if (timer != g_gallery_loader.start_timer) return;
    g_gallery_loader.start_timer = NULL;
    gallery_loader_schedule_next();
}

int ai_album_album_gallery_loader_start(
    ai_album_album_art_view_t *cards,
    uint8_t card_capacity,
    uint8_t current_page)
{
    uint8_t index;
    uint8_t photo_count = ai_album_album_store_count();
    uint8_t page_count = gallery_loader_page_count(photo_count);
    if (cards == NULL || card_capacity != AI_ALBUM_ALBUM_GALLERY_PAGE_SIZE ||
        page_count == 0U || current_page >= page_count) {
        return RET_ERR;
    }
    gallery_loader_reset();
    g_gallery_loader.cards = cards;
    g_gallery_loader.photo_count = photo_count;
    g_gallery_loader.page_count = page_count;
    g_gallery_loader.card_capacity = card_capacity;
    g_gallery_loader.current_page = current_page;
    g_gallery_loader.loading_cache = ALBUM_GALLERY_CACHE_NONE;
    for (index = 0U; index < ALBUM_GALLERY_CACHE_PAGE_COUNT; ++index) {
        g_gallery_loader.pages[index].page = ALBUM_GALLERY_PAGE_NONE;
    }
    g_gallery_loader.active = 1U;
    for (index = 0U;
         index < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++index) {
        if (ai_album_album_image_loader_claim_slot(&g_gallery_loader, index) !=
            RET_OK) {
            os_printf("[ALBUM_GALLERY] shared slot claim failed slot=%u\r\n",
                      (unsigned)index);
            gallery_loader_reset();
            return RET_ERR;
        }
    }
    if (gallery_loader_reconcile_cache(current_page) != RET_OK) {
        gallery_loader_reset();
        return RET_ERR;
    }
    gallery_loader_bind_page();
    os_printf("[ALBUM_GALLERY] start photos=%u page=%u/%u\r\n",
              (unsigned)photo_count, (unsigned)(current_page + 1U),
              (unsigned)page_count);
    g_gallery_loader.start_timer = lv_timer_create(
        gallery_loader_start_timer_cb, 1U, NULL);
    if (g_gallery_loader.start_timer == NULL) {
        gallery_loader_schedule_next();
        return RET_OK;
    }
    lv_timer_set_repeat_count(g_gallery_loader.start_timer, 1);
    return RET_OK;
}

int ai_album_album_gallery_loader_show_page(uint8_t page)
{
    uint8_t previous_page;
    uint8_t cache_hit;
    if (!g_gallery_loader.active || page >= g_gallery_loader.page_count) {
        return RET_ERR;
    }
    if (page == g_gallery_loader.current_page) return RET_OK;
    previous_page = g_gallery_loader.current_page;
    cache_hit = (uint8_t)(gallery_loader_find_cache(page) != NULL);
    gallery_loader_cancel_request();
    /* 不再整体detach: bind_card按卡处理清空,READY卡直接换图防闪烁 */
    g_gallery_loader.current_page = page;
    if (gallery_loader_reconcile_cache(page) != RET_OK) {
        g_gallery_loader.current_page = previous_page;
        gallery_loader_bind_page();
        if (g_gallery_loader.start_timer == NULL) gallery_loader_schedule_next();
        return RET_ERR;
    }
    gallery_loader_bind_page();
    if (g_gallery_loader.start_timer == NULL) gallery_loader_schedule_next();
    os_printf("[ALBUM_GALLERY] show page=%u/%u cache=%s\r\n",
              (unsigned)(page + 1U), (unsigned)g_gallery_loader.page_count,
              cache_hit ? "hit" : "miss");
    return RET_OK;
}

void ai_album_album_gallery_loader_stop(void)
{
    if (!g_gallery_loader.active && g_gallery_loader.cards == NULL) return;
    os_printf("[ALBUM_GALLERY] stop\r\n");
    gallery_loader_reset();
}
