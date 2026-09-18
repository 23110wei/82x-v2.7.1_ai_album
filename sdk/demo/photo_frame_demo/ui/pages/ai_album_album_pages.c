#include "ui/pages/ai_album_album_pages.h"
#include "ui/pages/ai_album_album_gallery_page.h"
#include "ui/pages/ai_album_album_widgets.h"
#include "basic_include.h"
#include "album/ai_album_album_art.h"
#include "album/ai_album_album_image_ai.h"
#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_photo_navigation.h"
#include "album/ai_album_album_photo_page.h"
#include "album/ai_album_album_photo_cache.h"
#include "album/ai_album_album_slideshow.h"
#include "album/ai_album_album_status.h"
#include "album/ai_album_album_store.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_ui_common.h"
#define ALBUM_MAX_FOCUSABLES 2U /* STYLE selector + SAVE */
#define ALBUM_PAGE_SLOT_COUNT 3U
#define ALBUM_IMAGE_AI_STYLE_WINDOW 3U
#define ALBUM_IMAGE_AI_STYLE_MIDDLE 1U /* slot that carries the selection */
#define ALBUM_IMAGE_AI_STYLE_SLOT_WIDTH 150 /* fits the longest style name */
#define ALBUM_IMAGE_AI_STYLE_SLOT_GAP 10
#define ALBUM_IMAGE_AI_SAVE_WIDTH 200
enum {
    ALBUM_IMAGE_AI_SOURCE_LOADER_SLOT = 0U,
    ALBUM_IMAGE_AI_RESULT_LOADER_SLOT = 1U,
    ALBUM_IMAGE_AI_FOCUS_STYLE = 0U,
    ALBUM_IMAGE_AI_FOCUS_SAVE = 1U,
};
typedef ai_album_album_image_bounds_t album_bounds_t;
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *chrome_topbar;
    lv_obj_t *chrome_footer;
    lv_obj_t *focusables[ALBUM_MAX_FOCUSABLES];
    lv_obj_t *status_label;
    lv_obj_t *slideshow_button_title_label;
    lv_obj_t *counter_label;
    lv_obj_t *result_label;
    lv_obj_t *image_ai_style_buttons[ALBUM_IMAGE_AI_STYLE_WINDOW];
    lv_obj_t *image_ai_save_button;
    ai_album_album_art_view_t photo_art;
    ai_album_album_art_view_t source_art;
    ai_album_album_art_view_t result_art;
    ai_album_ui_route_t route;
    uint8_t focus_count;
    uint8_t focus;
    uint8_t rendered_photo_index;
    uint8_t rendered_photo_valid;
    uint32_t rendered_photo_store_version;
    uint8_t image_ai_source_rendered;
    uint8_t image_ai_source_index;
    uint32_t image_ai_source_store_version;
    uint32_t image_ai_rendered_revision;
} album_page_state_t;
typedef struct {
    uint8_t slideshow;
    uint8_t style;
    uint8_t source_photo;
} album_model_t;
static album_page_state_t g_page;
static album_page_state_t g_inactive_pages[ALBUM_PAGE_SLOT_COUNT];
static album_model_t g_album;
static lv_display_t *g_album_display;
static void album_add_focusable(lv_obj_t *obj)
{
    if (obj != NULL && g_page.focus_count < ALBUM_MAX_FOCUSABLES) {
        g_page.focusables[g_page.focus_count++] = obj;
    }
}
static void album_update_focus_item(uint8_t index, uint8_t focused)
{
    ai_album_ui_common_focus(g_page.focusables[index], focused,
                             AI_ALBUM_UI_COLOR_GREEN);
}
static void album_update_focus(void)
{
    uint8_t index;

    if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        /* The three style slots form a ring: neighbours only preview it. */
        for (index = 0U; index < ALBUM_IMAGE_AI_STYLE_WINDOW; ++index) {
            if (g_page.image_ai_style_buttons[index] != NULL &&
                index != ALBUM_IMAGE_AI_STYLE_MIDDLE) {
                ai_album_ui_common_select(
                    g_page.image_ai_style_buttons[index], 0U,
                    AI_ALBUM_UI_COLOR_GREEN);
            }
        }
    }
    for (index = 0U; index < g_page.focus_count; ++index) {
        album_update_focus_item(index, (uint8_t)(index == g_page.focus));
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        if (g_page.focus != ALBUM_IMAGE_AI_FOCUS_STYLE) {
            /* The selection stays marked while focus rests on SAVE. */
            ai_album_ui_common_select(
                g_page.image_ai_style_buttons[ALBUM_IMAGE_AI_STYLE_MIDDLE], 1U,
                AI_ALBUM_UI_COLOR_GREEN);
        }
        if (g_page.focus != ALBUM_IMAGE_AI_FOCUS_SAVE) {
            /* SAVE keeps the idle look until it joins the ring. */
            ai_album_ui_common_focus(g_page.image_ai_save_button, 0U,
                                     AI_ALBUM_UI_COLOR_GREEN);
        }
    }
}
static void album_image_ai_sync_focus(void);
/* g_album.style is the selection; the window is a ring around it, so every
 * style stays reachable from both ends. */
static uint8_t album_style_index(uint8_t window_offset)
{
    return (uint8_t)((g_album.style + AI_ALBUM_ALBUM_STYLE_COUNT +
                      window_offset - ALBUM_IMAGE_AI_STYLE_MIDDLE) %
                     AI_ALBUM_ALBUM_STYLE_COUNT);
}
static void album_render_style_window(void)
{
    uint8_t index;

    for (index = 0U; index < ALBUM_IMAGE_AI_STYLE_WINDOW; ++index) {
        if (g_page.image_ai_style_buttons[index] != NULL) {
            ai_album_ui_common_set_label_text(
                lv_obj_get_child(g_page.image_ai_style_buttons[index], 0),
                ai_album_album_image_ai_style_name(album_style_index(index)));
        }
    }
}
static void album_style_step(int8_t delta)
{
    int32_t next = (int32_t)g_album.style + delta;

    if (next < 0) {
        next = (int32_t)AI_ALBUM_ALBUM_STYLE_COUNT - 1;
    } else if (next >= (int32_t)AI_ALBUM_ALBUM_STYLE_COUNT) {
        next = 0;
    }
    g_album.style = (uint8_t)next;
    album_render_style_window();
    album_update_focus();
}
/* The shared button pins its label to its top-left corner, which clips the
 * longer style names; centre it and let it wrap inside the slot instead. */
static void album_button_center_label(lv_obj_t *button, lv_coord_t label_width)
{
    lv_obj_t *label;

    if (button == NULL) {
        return;
    }
    label = lv_obj_get_child(button, 0);
    if (label == NULL) {
        return;
    }
    if (label_width > 0) {
        lv_obj_set_width(label, label_width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}
static void album_update_slideshow_button(void)
{
    char text[40];

    if (g_page.slideshow_button_title_label == NULL) {
        return;
    }
    os_snprintf(text, sizeof(text), "%s: %s",
                ai_album_i18n_text("SLIDESHOW"),
                ai_album_i18n_text(g_album.slideshow ? "ON" : "OFF"));
    ai_album_ui_common_set_label_text(g_page.slideshow_button_title_label,
                                      text);
}
static void album_render_photo(void)
{
    ai_album_album_photo_page_context_t context = {
        .art = &g_page.photo_art,
        .counter_label = g_page.counter_label,
        .status_label = g_page.status_label,
        .rendered_index = &g_page.rendered_photo_index,
        .rendered_valid = &g_page.rendered_photo_valid,
        .rendered_store_version = &g_page.rendered_photo_store_version,
    };

    ai_album_album_photo_page_render(&context);
}

static void album_photo_navigation_render(void *user_data)
{
    (void)user_data;
    album_render_photo();
}

static void album_photo_navigation_pause_cache(void *user_data)
{
    (void)user_data;
    ai_album_album_photo_cache_pause();
}

static void album_photo_navigation_resume_cache(void *user_data)
{
    (void)user_data;
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM && g_page.screen != NULL &&
        !g_album.slideshow && ai_album_album_store_count() > 1U) {
        ai_album_album_photo_cache_resume();
    }
}

static uint8_t album_photo_navigation_cache_busy(void *user_data)
{
    (void)user_data;
    return ai_album_album_photo_cache_is_loading();
}

static void album_start_photo_navigation(void)
{
    ai_album_album_photo_navigation_config_t config = {
        .art = &g_page.photo_art,
        .counter_label = g_page.counter_label,
        .render_cb = album_photo_navigation_render,
        .pause_cache_cb = album_photo_navigation_pause_cache,
        .resume_cache_cb = album_photo_navigation_resume_cache,
        .cache_busy_cb = album_photo_navigation_cache_busy,
        .user_data = NULL,
    };

    (void)ai_album_album_photo_navigation_start(&config);
}

static void album_slideshow_committed(uint8_t index, void *user_data)
{
    if (g_page.route != AI_ALBUM_UI_ROUTE_ALBUM ||
        g_page.photo_art.image != user_data) return;
    g_page.rendered_photo_index = index;
    g_page.rendered_photo_store_version = ai_album_album_store_version();
    g_page.rendered_photo_valid = 1U;
    album_render_photo();
}
static void album_update_slideshow_runtime(void)
{
    uint8_t count = ai_album_album_store_count();
    if (count < 2U) g_album.slideshow = 0U;
    if (g_page.route != AI_ALBUM_UI_ROUTE_ALBUM || !g_album.slideshow) {
        ai_album_album_slideshow_stop();
        if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM && count > 1U &&
            !ai_album_album_photo_navigation_is_cache_paused()) {
            (void)ai_album_album_photo_cache_start(&g_page.photo_art);
        }
        return;
    }
    if (ai_album_album_slideshow_start(
            &g_page.photo_art, album_slideshow_committed,
            g_page.photo_art.image) != RET_OK) {
        g_album.slideshow = 0U;
        ai_album_album_status_set_photo(
            g_page.status_label, "SLIDESHOW UNAVAILABLE");
    }
}
static void album_stop_photo_runtime(void)
{
    ai_album_album_photo_navigation_stop();
    ai_album_album_slideshow_stop();
    ai_album_album_art_clear_cache(&g_page.photo_art);
    g_page.rendered_photo_valid = 0U;
}
static void album_toggle_photo_controls(void)
{
    uint8_t hidden;
    if (g_page.focus_count < 2U || g_page.focusables[0] == NULL ||
        g_page.focusables[1] == NULL) return;
    hidden = (uint8_t)!lv_obj_has_flag(
        g_page.focusables[0], LV_OBJ_FLAG_HIDDEN);
    ai_album_ui_common_set_screen_chrome_hidden(
        g_page.chrome_topbar, g_page.chrome_footer, hidden);
    lv_obj_set_flag(g_page.focusables[0], LV_OBJ_FLAG_HIDDEN, hidden);
    lv_obj_set_flag(g_page.focusables[1], LV_OBJ_FLAG_HIDDEN, hidden);
    if (!hidden) album_update_focus();
}
static int album_build_photo(lv_display_t *display)
{
    album_bounds_t bounds = {0, 0, 1024, 600};
    ai_album_album_nav_button_t nav;
    lv_obj_t *stage;
    lv_obj_t *topbar;
    g_page.screen = ai_album_ui_common_prepare(
        display, "ALBUM", 0x101820U);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    topbar = lv_obj_get_child(g_page.screen, 0);
    stage = ai_album_ui_common_panel(g_page.screen, 0x0B1116U, 0);
    lv_obj_set_pos(stage, 0, 0);
    lv_obj_set_size(stage, 1024, 600);
    /* Keep the photo layer behind the topbar and the bottom controls. */
    lv_obj_move_to_index(stage, 0);
    g_page.photo_art = ai_album_album_art_create(stage, &bounds, 0U);
    if (g_page.photo_art.metadata != NULL) {
        lv_obj_add_flag(g_page.photo_art.metadata, LV_OBJ_FLAG_HIDDEN);
    }
    g_page.counter_label = ai_album_ui_common_label(
        stage, "0 / 0", &lv_font_montserrat_14, 0xB6C8CCU);
    lv_obj_align(g_page.counter_label, LV_ALIGN_TOP_RIGHT, -20, 14);
    g_page.status_label = ai_album_ui_common_label(
        stage, "", &lv_font_montserrat_14, 0xB6C8CCU);
    lv_obj_align(g_page.status_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(g_page.status_label, LV_OBJ_FLAG_HIDDEN);
    nav = ai_album_album_widgets_create_photo_nav(
        g_page.screen, "SLIDESHOW", NULL, 16);
    g_page.slideshow_button_title_label = nav.title;
    album_add_focusable(nav.root);
    album_update_slideshow_button();
    nav = ai_album_album_widgets_create_photo_nav(
        g_page.screen, "GALLERY", NULL, 528);
    album_add_focusable(nav.root);
    ai_album_ui_common_footer(
        g_page.screen,
        "POWER BACK   M CONTROLS   UP/DOWN PHOTO   LEFT/RIGHT SELECT   OK ENTER");
    g_page.chrome_topbar = topbar;
    g_page.chrome_footer = lv_obj_get_child(
        g_page.screen, (int32_t)lv_obj_get_child_count(g_page.screen) - 1);
    return RET_OK;
}
static int album_build_gallery(lv_display_t *display)
{
    if (display == NULL) {
        return RET_ERR;
    }
    g_page.screen = ai_album_ui_common_prepare(
        display, "GALLERY", AI_ALBUM_UI_COLOR_BG);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    return ai_album_album_gallery_page_create(g_page.screen);
}
static const char *album_image_ai_reason_text(uint8_t reason)
{
    switch (reason) {
        case AI_ALBUM_ALBUM_IMAGE_AI_REASON_FORMAT:
            return "JPEG FORMAT NOT SUPPORTED";
        case AI_ALBUM_ALBUM_IMAGE_AI_REASON_WRITE:
            return "STORAGE WRITE FAILED";
        case AI_ALBUM_ALBUM_IMAGE_AI_REASON_SAVE:
            return "SAVE FAILED";
        case AI_ALBUM_ALBUM_IMAGE_AI_REASON_FULL:
            return "ALBUM FULL";
        case AI_ALBUM_ALBUM_IMAGE_AI_REASON_TIMEOUT:
            return "GENERATION TIMEOUT";
        default:
            return "GENERATION FAILED";
    }
}
static const char *album_image_ai_state_text(
    const ai_album_album_image_ai_snapshot_t *snapshot)
{
    if (snapshot->state == AI_ALBUM_ALBUM_IMAGE_AI_ERROR) {
        return album_image_ai_reason_text(snapshot->reason);
    }
    switch (snapshot->state) {
        case AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING:
        case AI_ALBUM_ALBUM_IMAGE_AI_WAITING:
        case AI_ALBUM_ALBUM_IMAGE_AI_WRITING:
            return "GENERATING...";
        case AI_ALBUM_ALBUM_IMAGE_AI_SAVING:
            return "SAVING...";
        case AI_ALBUM_ALBUM_IMAGE_AI_READY:
        case AI_ALBUM_ALBUM_IMAGE_AI_SAVED:
            return "RESULT READY";
        default:
            return "SELECT A STYLE";
    }
}
static void album_render_image_ai_source(
    const ai_album_album_photo_t *source)
{
    uint32_t store_version = ai_album_album_store_version();
    if (g_page.image_ai_source_rendered &&
        g_page.image_ai_source_index == g_album.source_photo &&
        g_page.image_ai_source_store_version == store_version) {
        return;
    }
    if (source != NULL) {
        if (ai_album_album_art_set_async(&g_page.source_art, source) ==
            AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED) {
            return;
        }
    } else {
        (void)ai_album_album_art_set(&g_page.source_art, NULL);
    }
    g_page.image_ai_source_rendered = 1U;
    g_page.image_ai_source_index = g_album.source_photo;
    g_page.image_ai_source_store_version = store_version;
}
static void album_render_image_ai(void)
{
    ai_album_album_image_ai_snapshot_t snapshot;
    const ai_album_album_photo_t *source =
        ai_album_album_store_get(g_album.source_photo);
    album_render_image_ai_source(source);
    if (ai_album_album_image_ai_get_snapshot(&snapshot) != RET_OK) {
        ai_album_ui_common_set_label_text(g_page.result_label,
                                          "AI SERVICE UNAVAILABLE");
        album_image_ai_sync_focus();
        return;
    }
    if ((snapshot.state == AI_ALBUM_ALBUM_IMAGE_AI_READY ||
         snapshot.state == AI_ALBUM_ALBUM_IMAGE_AI_SAVED) &&
        snapshot.result.storage_backed && snapshot.result.path[0] != '\0') {
        if (g_page.image_ai_rendered_revision != snapshot.revision) {
            if (ai_album_album_art_set_async(&g_page.result_art,
                                             &snapshot.result) !=
                AI_ALBUM_ALBUM_IMAGE_VIEW_RESULT_DECODE_FAILED) {
                g_page.image_ai_rendered_revision = snapshot.revision;
            }
        }
    } else if (g_page.image_ai_rendered_revision != snapshot.revision) {
        (void)ai_album_album_art_set(&g_page.result_art, NULL);
        g_page.image_ai_rendered_revision = snapshot.revision;
    }
    ai_album_ui_common_set_label_text(g_page.result_label,
                                      album_image_ai_state_text(&snapshot));
    album_image_ai_sync_focus();
}

static int album_build_image_ai(lv_display_t *display)
{
    /* Images sit in the upper portion; buttons share one row at the bottom.
     * The panels drop below the heading band so the title never clips them. */
    album_bounds_t source_bounds = {40, 128, 420, 262};
    album_bounds_t result_bounds = {564, 128, 420, 262};
    lv_obj_t *label;
    lv_obj_t *slot;
    uint8_t index;
    g_page.screen = ai_album_ui_common_prepare(
        display, "IMAGE AI", 0xEEF0F5U);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    ai_album_album_widgets_create_heading(
        g_page.screen, "ALBUM / AI CREATE", "IMAGE TO IMAGE");
    /* 操作反馈行: 默认隐藏, 只有需要提示时才出现; 风格/生成状态由右侧
     * result_label 唯一承载, 否则空闲态会出现两条“选择风格”。 */
    g_page.status_label = ai_album_ui_common_label(
        g_page.screen, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_GREEN);
    lv_obj_align(g_page.status_label, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_add_flag(g_page.status_label, LV_OBJ_FLAG_HIDDEN);
    g_page.source_art = ai_album_album_art_create(g_page.screen, &source_bounds,
                                                   0U);
    ai_album_album_art_configure_image_only(&g_page.source_art);
    if (ai_album_album_art_set_loader_slot(
            &g_page.source_art, ALBUM_IMAGE_AI_SOURCE_LOADER_SLOT) !=
        RET_OK) {
        return RET_ERR;
    }
    g_page.result_art = ai_album_album_art_create(g_page.screen, &result_bounds,
                                                   0U);
    ai_album_album_art_configure_image_only(&g_page.result_art);
    if (ai_album_album_art_set_loader_slot(
            &g_page.result_art, ALBUM_IMAGE_AI_RESULT_LOADER_SLOT) !=
        RET_OK) {
        return RET_ERR;
    }
    /* Arrow between the two image panels */
    label = ai_album_ui_common_label(
        g_page.screen, "->", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_PURPLE);
    lv_obj_set_pos(label, 490, 246);
    /* State label: right-aligned with the result panel. */
    g_page.result_label = ai_album_ui_common_label(
        g_page.screen, "SELECT A STYLE", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_align(g_page.result_label, LV_ALIGN_TOP_RIGHT, -40, 72);
    /* Style slots: 3 buttons under the source image, the middle one carrying
     * the selection. 150px is the narrowest slot that fits the longest name. */
    for (index = 0U; index < ALBUM_IMAGE_AI_STYLE_WINDOW; ++index) {
        slot = ai_album_ui_common_button(
            g_page.screen,
            ai_album_album_image_ai_style_name(album_style_index(index)), NULL);
        g_page.image_ai_style_buttons[index] = slot;
        lv_obj_set_pos(slot,
                       40 + index * (ALBUM_IMAGE_AI_STYLE_SLOT_WIDTH +
                                     ALBUM_IMAGE_AI_STYLE_SLOT_GAP),
                       410);
        lv_obj_set_size(slot, ALBUM_IMAGE_AI_STYLE_SLOT_WIDTH, 55);
        album_button_center_label(slot, ALBUM_IMAGE_AI_STYLE_SLOT_WIDTH - 16);
    }
    album_add_focusable(
        g_page.image_ai_style_buttons[ALBUM_IMAGE_AI_STYLE_MIDDLE]);
    /* SAVE stays visible from the start; only its focus waits for a result. */
    g_page.image_ai_save_button = ai_album_ui_common_button(
        g_page.screen, "SAVE", NULL);
    lv_obj_set_size(g_page.image_ai_save_button, ALBUM_IMAGE_AI_SAVE_WIDTH, 55);
    lv_obj_set_pos(g_page.image_ai_save_button,
                   result_bounds.x + (result_bounds.width -
                                      ALBUM_IMAGE_AI_SAVE_WIDTH) / 2,
                   410);
    album_button_center_label(g_page.image_ai_save_button, 0);
    album_add_focusable(g_page.image_ai_save_button);
    album_image_ai_sync_focus();
    ai_album_ui_common_footer(
        g_page.screen, "POWER BACK   ARROWS NAV   OK GENERATE / SAVE");
    return RET_OK;
}
static int8_t album_page_slot(ai_album_ui_route_t route)
{
    if (route == AI_ALBUM_UI_ROUTE_ALBUM) {
        return 0;
    }
    if (route == AI_ALBUM_UI_ROUTE_GALLERY) {
        return 1;
    }
    if (route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        return 2;
    }
    return -1;
}
static void album_page_destroy_state(album_page_state_t *state)
{
    if (state->route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        ai_album_album_image_ai_cancel();
    }
    if (state->route == AI_ALBUM_UI_ROUTE_GALLERY) {
        ai_album_album_gallery_page_destroy(state->screen);
    }
    if (state->screen != NULL) {
        lv_obj_delete(state->screen);
    }
    memset(state, 0, sizeof(*state));
}
static int album_page_build(ai_album_ui_route_t route)
{
    int result;
    memset(&g_page, 0, sizeof(g_page));
    g_page.route = route;
    if (route == AI_ALBUM_UI_ROUTE_ALBUM) {
        result = album_build_photo(g_album_display);
    } else if (route == AI_ALBUM_UI_ROUTE_GALLERY) {
        result = album_build_gallery(g_album_display);
    } else if (route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        g_album.source_photo = ai_album_album_store_selected();
        ai_album_album_image_ai_cancel();
        result = album_build_image_ai(g_album_display);
    } else {
        return RET_ERR;
    }
    if (result == RET_OK) {
        album_update_focus();
    }
    return result;
}
static int album_page_activate(ai_album_ui_route_t route)
{
    int8_t slot = album_page_slot(route);
    if (slot < 0 || g_album_display == NULL) {
        return RET_ERR;
    }
    if (g_inactive_pages[slot].screen == NULL) {
        return album_page_build(route);
    }
    g_page = g_inactive_pages[slot];
    memset(&g_inactive_pages[slot], 0, sizeof(g_inactive_pages[slot]));
    if (route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        g_album.source_photo = ai_album_album_store_selected();
        ai_album_album_image_ai_cancel();
        g_page.image_ai_source_rendered = 0U;
        g_page.image_ai_rendered_revision = 0U;
        g_page.focus = ALBUM_IMAGE_AI_FOCUS_STYLE;
        album_render_style_window();
        album_image_ai_sync_focus();
    }
    return RET_OK;
}
static int album_page_cache_current(void)
{
    int8_t slot = album_page_slot(g_page.route);
    if (slot < 0 || g_page.screen == NULL ||
        g_inactive_pages[slot].screen != NULL) {
        return RET_ERR;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM) {
        album_stop_photo_runtime();
    } else if (g_page.route == AI_ALBUM_UI_ROUTE_GALLERY) {
        ai_album_album_gallery_page_stop(g_page.screen);
    } else if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        ai_album_album_image_ai_cancel();
        ai_album_album_art_clear_cache(&g_page.source_art);
        ai_album_album_art_clear_cache(&g_page.result_art);
    }
    g_inactive_pages[slot] = g_page;
    memset(&g_page, 0, sizeof(g_page));
    return RET_OK;
}
int ai_album_album_pages_create(lv_display_t *display,
                                ai_album_ui_route_t route)
{
    int result;
    if (display == NULL || g_album_display != NULL ||
        album_page_slot(route) < 0) {
        return RET_ERR;
    }
    (void)ai_album_album_store_refresh();
    ai_album_album_image_ai_init();
    g_album_display = display;
    result = album_page_activate(route);
    if (result != RET_OK) {
        album_page_destroy_state(&g_page);
        g_album_display = NULL;
    }
    return result;
}
int ai_album_album_pages_switch(ai_album_ui_route_t route)
{
    ai_album_ui_route_t previous = g_page.route;
    if (route == previous || album_page_slot(route) < 0 ||
        album_page_cache_current() != RET_OK) {
        return RET_ERR;
    }
    if (album_page_activate(route) == RET_OK) {
        return ai_album_album_pages_show();
    }
    album_page_destroy_state(&g_page);
    (void)album_page_activate(previous);
    return RET_ERR;
}
int ai_album_album_pages_show(void)
{
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_page.screen);
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM) {
        album_render_photo();
        album_update_slideshow_runtime();
        album_update_slideshow_button();
        album_start_photo_navigation();
    } else if (g_page.route == AI_ALBUM_UI_ROUTE_GALLERY) {
        return ai_album_album_gallery_page_show(g_page.screen);
    } else if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
        album_render_image_ai();
    }
    return RET_OK;
}
void ai_album_album_pages_destroy(void)
{
    uint8_t slot;
    ai_album_album_photo_navigation_stop();
    ai_album_album_slideshow_stop(); ai_album_album_photo_cache_stop();
    ai_album_album_image_ai_cancel();
    album_page_destroy_state(&g_page);
    for (slot = 0U; slot < ALBUM_PAGE_SLOT_COUNT; ++slot) {
        album_page_destroy_state(&g_inactive_pages[slot]);
    }
    ai_album_album_image_loader_deinit();
    g_album_display = NULL;
}
static ai_album_ui_route_t album_activate_photo(void)
{
    if (g_page.focus == 0U) {
        if (ai_album_album_store_count() < 2U) {
            ai_album_album_status_set_photo(
                g_page.status_label, "NEED TWO PHOTOS FOR SLIDESHOW");
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        ai_album_album_photo_navigation_stop();
        g_album.slideshow = (uint8_t)!g_album.slideshow;
        album_render_photo();
        album_update_slideshow_runtime();
        album_update_slideshow_button();
        album_start_photo_navigation();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    return AI_ALBUM_UI_ROUTE_GALLERY;
}
static ai_album_ui_route_t album_activate_gallery(ai_album_ui_action_t action)
{
    uint8_t count = ai_album_album_store_count();
    uint8_t selected = ai_album_album_gallery_page_focused();
    if (count == 0U || selected >= count ||
        ai_album_album_store_select(selected) != RET_OK) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    g_album.source_photo = selected;
    if (action == AI_ALBUM_UI_ACTION_OK_LONG) {
        return AI_ALBUM_UI_ROUTE_IMAGE_AI;
    }
    return AI_ALBUM_UI_ROUTE_ALBUM;
}
/* SAVE joins the focus ring only once the engine holds a result.  This is the
 * single writer of focus_count, so the ring can never address a slot that is
 * not reachable, and focus never escapes the ring. */
static uint8_t album_image_ai_save_focusable(void)
{
    ai_album_album_image_ai_snapshot_t snapshot;

    if (ai_album_album_image_ai_get_snapshot(&snapshot) != RET_OK) {
        return 0U;
    }
    return (uint8_t)(snapshot.state == AI_ALBUM_ALBUM_IMAGE_AI_READY ||
                     snapshot.state == AI_ALBUM_ALBUM_IMAGE_AI_SAVED);
}
static void album_image_ai_sync_focus(void)
{
    uint8_t focusable = album_image_ai_save_focusable();
    uint8_t was_focusable = (uint8_t)(g_page.focus_count > 1U);

    g_page.focus_count = (uint8_t)(focusable ? 2U : 1U);
    if (g_page.focus >= g_page.focus_count) {
        g_page.focus = ALBUM_IMAGE_AI_FOCUS_STYLE;
    }
    if (focusable && !was_focusable) {
        g_page.focus = ALBUM_IMAGE_AI_FOCUS_SAVE; /* result ready: offer SAVE */
    }
    album_update_focus();
}
static ai_album_ui_route_t album_activate_image_ai(void)
{
    const ai_album_album_photo_t *source;
    int ret;

    if (g_page.focus == ALBUM_IMAGE_AI_FOCUS_SAVE) {
        uint8_t saved_index = 0U;
        uint8_t reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE;
        ai_album_album_image_ai_snapshot_t snapshot;

        if (ai_album_album_image_ai_get_snapshot(&snapshot) != RET_OK ||
            snapshot.state != AI_ALBUM_ALBUM_IMAGE_AI_READY) {
            ai_album_album_status_set_photo(g_page.status_label, "GENERATE FIRST");
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        if (ai_album_album_image_ai_save(&saved_index, &reason) != RET_OK) {
            ai_album_album_status_set_photo(g_page.status_label,
                                           album_image_ai_reason_text(reason));
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        (void)ai_album_album_store_select(saved_index);
        return AI_ALBUM_UI_ROUTE_GALLERY;
    }
    /* STYLE slot focused: generate with the selected style. */
    source = ai_album_album_store_get(g_album.source_photo);
    if (source == NULL) {
        ai_album_album_status_set_photo(g_page.status_label,
                                       "GENERATION START FAILED");
        album_render_image_ai();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    ret = ai_album_album_image_ai_start(source, g_album.style);
    if (ret == RET_OK) {
        ai_album_album_status_set_photo(
            g_page.status_label,
            ai_album_album_image_ai_style_name(g_album.style));
    }
    /* BUSY keeps the running progress text; any other failure is already
     * carried by the snapshot reason shown in the result label. */
    album_render_image_ai();
    return AI_ALBUM_UI_ROUTE_NONE;
}
void ai_album_album_pages_poll(void)
{
    ai_album_album_image_ai_snapshot_t snapshot;

    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM && g_page.screen != NULL) {
        ai_album_album_photo_navigation_poll();
        if (!ai_album_album_photo_navigation_is_active() &&
            !g_page.rendered_photo_valid &&
            ai_album_album_store_count() > 0U) {
            album_render_photo();
        }
        return;
    }
    if (g_page.route != AI_ALBUM_UI_ROUTE_IMAGE_AI || g_page.screen == NULL ||
        ai_album_album_image_ai_get_snapshot(&snapshot) != RET_OK) {
        return;
    }
    if (snapshot.revision != g_page.image_ai_rendered_revision ||
        !g_page.image_ai_source_rendered) {
        album_render_image_ai();
    }
}
static void album_move_focus(int8_t delta)
{
    uint8_t next;

    if (g_page.route != AI_ALBUM_UI_ROUTE_IMAGE_AI ||
        g_page.focus_count <= 1U) {
        return;
    }
    next = (uint8_t)((g_page.focus + g_page.focus_count + delta) %
                     g_page.focus_count);
    if (next == g_page.focus) {
        return;
    }
    g_page.focus = next;
    album_update_focus();
}
static uint8_t album_action_is_direction(ai_album_ui_action_t action)
{
    return (uint8_t)(action == AI_ALBUM_UI_ACTION_LEFT ||
                     action == AI_ALBUM_UI_ACTION_RIGHT ||
                     action == AI_ALBUM_UI_ACTION_UP ||
                     action == AI_ALBUM_UI_ACTION_DOWN);
}
static ai_album_ui_route_t album_handle_gallery_action(
    ai_album_ui_action_t action)
{
    ai_album_gallery_confirm_result_t confirm =
        ai_album_album_gallery_page_handle_image_ai_confirm(action);
    if (confirm != AI_ALBUM_GALLERY_CONFIRM_NOT_ACTIVE) {
        if (confirm == AI_ALBUM_GALLERY_CONFIRM_OPEN_IMAGE_AI) {
            return album_activate_gallery(AI_ALBUM_UI_ACTION_OK_LONG);
        }
        if (confirm == AI_ALBUM_GALLERY_CONFIRM_DELETE_PHOTO) {
            (void)ai_album_album_gallery_page_delete_selected();
        }
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        return AI_ALBUM_UI_ROUTE_ALBUM;
    }
    if (album_action_is_direction(action)) {
        (void)ai_album_album_gallery_page_move(action);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_OK_LONG) {
        if (ai_album_album_gallery_page_open_image_ai_confirm() != RET_OK) {
            os_printf("[ALBUM_GALLERY] action menu open failed\r\n");
        }
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_OK) {
        return album_activate_gallery(action);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
ai_album_ui_route_t ai_album_album_pages_handle_action(
    ai_album_ui_action_t action)
{
    if (g_page.route == AI_ALBUM_UI_ROUTE_GALLERY) {
        return album_handle_gallery_action(action);
    }
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
            /* Focus on SAVE: hand it back to the selected style.  The result
             * and the SAVE button stay available. */
            if (g_page.focus == ALBUM_IMAGE_AI_FOCUS_SAVE) {
                g_page.focus = ALBUM_IMAGE_AI_FOCUS_STYLE;
                album_update_focus();
                return AI_ALBUM_UI_ROUTE_NONE;
            }
            /* Focus on the style slot: leave the page. */
            return AI_ALBUM_UI_ROUTE_GALLERY;
        }
        if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM) album_stop_photo_runtime();
        return AI_ALBUM_UI_ROUTE_HOME;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM && action == AI_ALBUM_UI_ACTION_MENU) {
        album_toggle_photo_controls();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM && !g_album.slideshow &&
        (action == AI_ALBUM_UI_ACTION_UP || action == AI_ALBUM_UI_ACTION_DOWN)) {
        int8_t delta = action == AI_ALBUM_UI_ACTION_UP ? -1 : 1;
        (void)ai_album_album_photo_navigation_move(delta);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM &&
        lv_obj_has_flag(g_page.focusables[0], LV_OBJ_FLAG_HIDDEN)) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (album_action_is_direction(action)) {
        if (g_page.route == AI_ALBUM_UI_ROUTE_IMAGE_AI) {
            /* LEFT/RIGHT scrolls style carousel, UP/DOWN cycles focus */
            if (action == AI_ALBUM_UI_ACTION_LEFT) {
                album_style_step(-1);
            } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
                album_style_step(1);
            } else {
                int8_t delta = (action == AI_ALBUM_UI_ACTION_UP) ? -1 : 1;
                album_move_focus(delta);
            }
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM) {
            /* LEFT/RIGHT moves focus between SLIDESHOW and GALLERY buttons */
            int8_t delta = (action == AI_ALBUM_UI_ACTION_LEFT) ? -1 : 1;
            uint8_t focus_count = g_page.focus_count;
            uint8_t next;
            if (focus_count <= 1U || g_page.focus >= focus_count) {
                return AI_ALBUM_UI_ROUTE_NONE;
            }
            next = (uint8_t)((g_page.focus + focus_count + delta) % focus_count);
            if (next == g_page.focus) {
                return AI_ALBUM_UI_ROUTE_NONE;
            }
            g_page.focus = next;
            album_update_focus();
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        int8_t delta = (action == AI_ALBUM_UI_ACTION_LEFT ||
                        action == AI_ALBUM_UI_ACTION_UP) ? -1 : 1;
        album_move_focus(delta);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action != AI_ALBUM_UI_ACTION_OK) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.route == AI_ALBUM_UI_ROUTE_ALBUM) {
        return album_activate_photo();
    }
    return album_activate_image_ai();
}
