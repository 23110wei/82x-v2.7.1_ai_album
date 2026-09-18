#include "album/ai_album_album_perf.h"

#include "basic_include.h"
#include "display/ai_album_direct_display.h"

typedef struct {
    uint64 start_us;
    uint32_t start_frame_count;
    uint32_t start_flush_count;
    uint32_t route_create_ms;
    uint32_t route_show_ms;
    uint32_t scan_us;
    uint32_t image_prepare_us;
    uint32_t scan_count;
    uint32_t scan_failures;
    uint32_t image_prepare_count;
    uint32_t image_prepare_failures;
    uint32_t image_draw_count;
    uint32_t file_image_draw_count;
    ai_album_album_perf_operation_t operation;
    uint8_t active;
} ai_album_album_perf_state_t;

static ai_album_album_perf_state_t g_album_perf;

static const char *album_perf_operation_name(
    ai_album_album_perf_operation_t operation)
{
    static const char *const names[AI_ALBUM_ALBUM_PERF_OPERATION_COUNT] = {
        "OPEN_ALBUM", "OPEN_GALLERY", "OPEN_IMAGE_AI", "RETURN_HOME",
        "FOCUS",
    };

    return operation < AI_ALBUM_ALBUM_PERF_OPERATION_COUNT ?
               names[operation] : "UNKNOWN";
}

static const char *album_perf_clean_mode_name(
    ai_album_cache_clean_mode_t mode)
{
    if (mode == AI_ALBUM_CACHE_CLEAN_ROWS) {
        return "rows";
    }
    if (mode == AI_ALBUM_CACHE_CLEAN_SPANS) {
        return "spans";
    }
    return mode == AI_ALBUM_CACHE_CLEAN_FULL ? "full" : "unknown";
}

static void album_perf_report(
    const ai_album_album_perf_state_t *perf,
    const ai_album_direct_display_stats_t *display,
    uint32_t elapsed_us)
{
    os_printf("[ALBUM_PERF] op=%s total=%uus frames=%u flush=%u "
              "route=%u+%ums scan=%u/%uus/%ufail prep=%u/%uus/%ufail "
              "draw=%u file_draw=%u clean=%s/%uB/%uus/%ucalls "
              "regions=%u/%u/%u swap=%ums\r\n",
              album_perf_operation_name(perf->operation),
              (unsigned)elapsed_us,
              (unsigned)(display->frame_count - perf->start_frame_count),
              (unsigned)(display->flush_count - perf->start_flush_count),
              (unsigned)perf->route_create_ms,
              (unsigned)perf->route_show_ms,
              (unsigned)perf->scan_count, (unsigned)perf->scan_us,
              (unsigned)perf->scan_failures,
              (unsigned)perf->image_prepare_count,
              (unsigned)perf->image_prepare_us,
              (unsigned)perf->image_prepare_failures,
              (unsigned)perf->image_draw_count,
              (unsigned)perf->file_image_draw_count,
              album_perf_clean_mode_name(display->last_cache_clean_mode),
              (unsigned)display->last_cache_clean_bytes,
              (unsigned)display->last_cache_clean_us,
              (unsigned)display->last_cache_clean_call_count,
              (unsigned)display->last_dirty_region_count,
              (unsigned)display->last_sync_region_count,
              (unsigned)display->last_clean_region_count,
              (unsigned)display->last_swap_wait_ms);
}

void ai_album_album_perf_begin(ai_album_album_perf_operation_t operation)
{
    ai_album_direct_display_stats_t display;

    if (operation >= AI_ALBUM_ALBUM_PERF_OPERATION_COUNT) {
        return;
    }
    ai_album_direct_display_get_stats(&display);
    memset(&g_album_perf, 0, sizeof(g_album_perf));
    g_album_perf.operation = operation;
    g_album_perf.start_us = os_useconds();
    g_album_perf.start_frame_count = display.frame_count;
    g_album_perf.start_flush_count = display.flush_count;
    g_album_perf.active = 1U;
}

void ai_album_album_perf_record_route(uint32_t create_ms, uint32_t show_ms)
{
    if (!g_album_perf.active) {
        return;
    }
    g_album_perf.route_create_ms += create_ms;
    g_album_perf.route_show_ms += show_ms;
}

void ai_album_album_perf_record_scan(uint32_t elapsed_us, int result)
{
    if (!g_album_perf.active) {
        return;
    }
    g_album_perf.scan_count++;
    g_album_perf.scan_us += elapsed_us;
    if (result < 0) {
        g_album_perf.scan_failures++;
    }
}

void ai_album_album_perf_record_image_prepare(uint32_t elapsed_us, int result)
{
    if (!g_album_perf.active) {
        return;
    }
    g_album_perf.image_prepare_count++;
    g_album_perf.image_prepare_us += elapsed_us;
    if (result < 0) {
        g_album_perf.image_prepare_failures++;
    }
}

void ai_album_album_perf_record_image_draw(uint8_t file_source)
{
    if (!g_album_perf.active) {
        return;
    }
    g_album_perf.image_draw_count++;
    if (file_source) {
        g_album_perf.file_image_draw_count++;
    }
}

void ai_album_album_perf_poll(void)
{
    ai_album_direct_display_stats_t display;
    ai_album_album_perf_state_t completed;
    uint32_t elapsed_us;

    if (!g_album_perf.active) {
        return;
    }
    ai_album_direct_display_get_stats(&display);
    if (display.frame_count == g_album_perf.start_frame_count) {
        return;
    }
    elapsed_us = (uint32_t)(os_useconds() - g_album_perf.start_us);
    completed = g_album_perf;
    g_album_perf.active = 0U;
    album_perf_report(&completed, &display, elapsed_us);
}
