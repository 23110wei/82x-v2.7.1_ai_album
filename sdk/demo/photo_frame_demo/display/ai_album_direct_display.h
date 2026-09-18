#ifndef AI_ALBUM_DIRECT_DISPLAY_H
#define AI_ALBUM_DIRECT_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui/ai_album_compat.h"

struct lcdc_device;

typedef struct {
    struct lcdc_device *lcdc;
    void *scan_buffer;
    void *draw_buffer;
    uint32_t frame_bytes;
} ai_album_direct_display_config_t;

typedef enum {
    AI_ALBUM_CACHE_CLEAN_ROWS = 0,
    AI_ALBUM_CACHE_CLEAN_SPANS,
    AI_ALBUM_CACHE_CLEAN_FULL,
} ai_album_cache_clean_mode_t;

typedef struct {
    uint32_t flush_count;
    uint32_t frame_count;
    uint32_t rejected_count;
    uint32_t wait_timeout_count;
    /* DEBUG_TEMP: per-frame hardware OSD DMA start-address checks that
     * disagreed with the software scan pointer (LCD_DONE hook). */
    uint32_t osd_addr_mismatch_count;
    uint32_t cache_clean_count;
    uint32_t last_cache_clean_bytes;
    uint32_t last_cache_clean_us;
    uint32_t max_cache_clean_us;
    uint32_t last_dirty_region_count;
    uint32_t last_sync_region_count;
    uint32_t last_clean_region_count;
    uint32_t last_cache_clean_call_count;
    ai_album_cache_clean_mode_t last_cache_clean_mode;
    uint32_t last_swap_wait_ms;
    uint32_t max_swap_wait_ms;
    void *front_buffer;
    void *back_buffer;
    void *scan_buffer;
    void *pending_buffer;
    uint32_t buffer_bytes;
    void *capture_buffer;
    uint32_t capture_bytes;
    uint32_t capture_sequence;
    uint32_t capture_failed_count;
    void *commit_capture_buffer;
    uint32_t commit_capture_bytes;
    uint32_t commit_capture_sequence;
    uint32_t commit_capture_failed_count;
    uint32_t diagnostic_sequence;
    uint32_t diagnostic_scan_sample_hash;
    uint32_t diagnostic_draw_sample_hash;
    int32_t diagnostic_lcdc_clk_status;
    uint8_t capture_valid;
} ai_album_direct_display_stats_t;

int ai_album_direct_display_init(
    lv_display_t *display, const ai_album_direct_display_config_t *config);
void ai_album_direct_display_get_stats(
    ai_album_direct_display_stats_t *stats);
void ai_album_direct_display_poll_diagnostics(void);


/* Request a one-shot copy of the next completed pre-LCDC frame. The copy is
 * kept in PSRAM and can be exported later with a debugger; no filesystem I/O
 * is performed from the LVGL flush path. */
void ai_album_direct_display_capture_request(void);

/* Persist the last completed snapshot from the non-flush command path. */
int ai_album_direct_display_capture_save(const char *path);

/* DEBUG_TEMP capture point 2: same as capture_request but samples the buffer
 * right after the LCD_DONE hook handed it to LCDC scanout (the frame entering
 * the LCD). Uses a dedicated PSRAM copy independent of capture_request. */
void ai_album_direct_display_commit_capture_request(void);

/* Persist the post-commit snapshot from the non-flush command path. */
int ai_album_direct_display_commit_capture_save(const char *path);

/* DEBUG_TEMP: point LCDC at the post-commit snapshot (same caveat as
 * capture_show). */
int ai_album_direct_display_commit_capture_show(void);

/* DEBUG_TEMP: point LCDC at the last captured, cache-clean frame. The caller
 * must avoid UI updates until reset because this intentionally bypasses the
 * normal LVGL buffer ownership state. */
int ai_album_direct_display_capture_show(void);

/* DEBUG_TEMP: compare the cached and post-invalidate view of the snapshot and
 * report the LCDC OSD register state. */
int ai_album_direct_display_cache_probe(void);

/* DEBUG_TEMP: display a static generated test pattern (0=black 1=white
 * 2=gray50 3=red 4=checker1 5=checker8 6=left-text/right-blank
 * 7=top-text/bottom-blank 8=clone of the current scan-buffer content);
 * 255 turns the mode off and restores the live frame. While active,
 * frame-done address commits are suppressed so LCDC keeps scanning the
 * pattern buffer. */
int ai_album_direct_display_pattern_show(uint32_t pattern_id);

/* DEBUG_TEMP: dump LCDC register space (+0x00..+0xF8) for snapshot diffing. */
int ai_album_direct_display_reg_dump(void);

/* DEBUG_TEMP: freeze the OSD DMA address on the current scan buffer -- no
 * buffer switching at all while enabled. Stripe experiment: if stripes
 * vanish in this mode, the flip/switch itself participates. */
void ai_album_direct_display_set_fixed_buffer(uint8_t enable);

/* DEBUG_TEMP: copy the buffer LCDC is currently scanning into capture_buffer
 * and write it to path without triggering any flush/redraw. Prints the scan
 * buffer sample hash for offline comparison. */
int ai_album_direct_display_scan_snapshot(const char *path);

#ifdef __cplusplus
}
#endif

#endif
