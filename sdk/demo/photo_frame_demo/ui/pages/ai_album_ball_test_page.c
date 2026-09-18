#include "ui/pages/ai_album_ball_test_page.h"

#include "basic_include.h"
#include "src/draw/lv_draw_triangle.h"

/* Regular-pentagon unit vectors scaled by 256 (angles -90, -18, 54,
 * 126, 198 degrees), integer math only. */
static const int32_t pentagon_unit[5][2] = {
    {0, -256}, {243, -79}, {150, 207}, {-150, 207}, {-243, -79},
};

/* Shared color table for the block and polygon shapes (AT-selectable). */
static const uint32_t ball_block_colors[] = {
    0x000000U, 0xFF0000U, 0x00FF00U, 0x0000FFU,
    0xFFFFFFU, 0xFFFF00U, 0x00FFFFU, 0xFF00FFU, 0x808080U,
};
static const char *const ball_block_color_names[] = {
    "off", "red", "green", "blue", "white", "yellow", "cyan", "magenta",
    "gray50",
};
/* Polygon fill colors (indices into ball_block_colors), AT-selectable. */
static volatile uint8_t g_ball_tri_color_req = 5U;  /* yellow */
static volatile uint8_t g_ball_pent_color_req = 6U; /* cyan */

static void ball_poly_draw_points(lv_layer_t *layer,
                                  const lv_point_precise_t *pts,
                                  uint32_t count, uint32_t color)
{
    lv_draw_triangle_dsc_t dsc;
    uint32_t i;

    for (i = 1U; i + 1U < count; ++i) {
        lv_draw_triangle_dsc_init(&dsc);
        dsc.color = lv_color_hex(color);
        dsc.opa = LV_OPA_COVER;
        dsc.p[0] = pts[0];
        dsc.p[1] = pts[i];
        dsc.p[2] = pts[i + 1U];
        lv_draw_triangle(layer, &dsc);
    }
}

static void ball_poly_triangle_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_point_precise_t pts[3];

    lv_obj_get_coords(obj, &a);
    pts[0] = (lv_point_precise_t){a.x1 + lv_area_get_width(&a) / 2,
                                  a.y1 + 2};
    pts[1] = (lv_point_precise_t){a.x1 + lv_area_get_width(&a) - 2,
                                  a.y1 + lv_area_get_height(&a) - 2};
    pts[2] = (lv_point_precise_t){a.x1 + 2,
                                  a.y1 + lv_area_get_height(&a) - 2};
    ball_poly_draw_points(layer, pts, 3U,
                          ball_block_colors[g_ball_tri_color_req &
                                             7U]);
}

static void ball_poly_pentagon_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_point_precise_t pts[5];
    int32_t cx, cy, r;
    uint32_t i;

    lv_obj_get_coords(obj, &a);
    cx = a.x1 + lv_area_get_width(&a) / 2;
    cy = a.y1 + lv_area_get_height(&a) / 2;
    r = (LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a)) / 2) - 3;
    for (i = 0U; i < 5U; ++i) {
        pts[i] = (lv_point_precise_t){
            cx + (pentagon_unit[i][0] * r) / 256,
            cy + (pentagon_unit[i][1] * r) / 256};
    }
    /* Fan triangulation from vertex 0 fills the pentagon. */
    ball_poly_draw_points(layer, pts, 5U,
                          ball_block_colors[g_ball_pent_color_req &
                                             7U]);
}

#define AI_ALBUM_BALL_TEST_SCREEN_W 1024
#define AI_ALBUM_BALL_TEST_SCREEN_H 600
#define AI_ALBUM_BALL_TEST_SIZE 60U
#define AI_ALBUM_BALL_TEST_PERIOD_MS 20U
#define AI_ALBUM_BALL_TEST_SPEED_X 5
#define AI_ALBUM_BALL_TEST_SPEED_Y 3
#define AI_ALBUM_BALL_TEST_LOG_US (1000U * 1000U)
#define AI_ALBUM_BALL_TEST_BLOCK_SIZE 100U
#define AI_ALBUM_BALL_TEST_BLOCK_SPEED_X 4
#define AI_ALBUM_BALL_TEST_BLOCK_SPEED_Y 6
#define AI_ALBUM_BALL_TEST_POLY_SIZE 100U
#define AI_ALBUM_BALL_TEST_TRI_SPEED_X 3
#define AI_ALBUM_BALL_TEST_TRI_SPEED_Y 7
#define AI_ALBUM_BALL_TEST_PENT_SPEED_X 6
#define AI_ALBUM_BALL_TEST_PENT_SPEED_Y 2

/* Written by the AT command task, read by the LVGL loop. */
static volatile uint8_t g_ball_page_request;
static volatile uint8_t g_ball_speed_req = 1U;
static volatile uint8_t g_ball_block_color_req;
/* Everything below is owned by the LVGL loop only. */
static uint8_t g_ball_page_active;
static uint8_t g_ball_speed_cur = 1U;
static uint8_t g_ball_block_color_cur;
static lv_obj_t *g_ball_block_obj;
static int32_t g_ball_block_x, g_ball_block_y;
static int32_t g_ball_block_vx, g_ball_block_vy;
static volatile uint8_t g_ball_poly_req;
static uint8_t g_ball_poly_cur;
static lv_obj_t *g_ball_tri_obj;
static lv_obj_t *g_ball_pent_obj;
static int32_t g_ball_tri_x, g_ball_tri_y, g_ball_tri_vx, g_ball_tri_vy;
static int32_t g_ball_pent_x, g_ball_pent_y, g_ball_pent_vx, g_ball_pent_vy;
static lv_obj_t *g_ball_screen;
static lv_obj_t *g_ball_prev_screen;
static lv_obj_t *g_ball_obj;
static lv_obj_t *g_ball_label;
static lv_timer_t *g_ball_timer;
static int32_t g_ball_x, g_ball_y, g_ball_vx, g_ball_vy;
static uint32_t g_ball_ticks;
static uint64_t g_ball_last_log_us;

static void ball_test_style_solid(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

static void ball_test_move_bounce(int32_t *x, int32_t *y,
                                  int32_t *vx, int32_t *vy,
                                  lv_obj_t *obj, uint32_t size)
{
    *x += *vx;
    *y += *vy;
    if (*x <= 0) {
        *x = 0;
        *vx = -*vx;
    } else if (*x >= AI_ALBUM_BALL_TEST_SCREEN_W - (int32_t)size) {
        *x = AI_ALBUM_BALL_TEST_SCREEN_W - (int32_t)size;
        *vx = -*vx;
    }
    if (*y <= 0) {
        *y = 0;
        *vy = -*vy;
    } else if (*y >= AI_ALBUM_BALL_TEST_SCREEN_H - (int32_t)size) {
        *y = AI_ALBUM_BALL_TEST_SCREEN_H - (int32_t)size;
        *vy = -*vy;
    }
    lv_obj_set_pos(obj, *x, *y);
}

static void ball_test_timer_cb(lv_timer_t *timer)
{
    uint8_t speed = g_ball_speed_req;

    LV_UNUSED(timer);
    if (g_ball_obj == NULL || !lv_obj_is_valid(g_ball_obj)) return;

    if (speed != g_ball_speed_cur) {
        /* Rescale from the base velocity, preserving bounce direction. */
        g_ball_vx = (g_ball_vx < 0 ? -1 : 1) *
                    AI_ALBUM_BALL_TEST_SPEED_X * (int32_t)speed;
        g_ball_vy = (g_ball_vy < 0 ? -1 : 1) *
                    AI_ALBUM_BALL_TEST_SPEED_Y * (int32_t)speed;
        g_ball_speed_cur = speed;
    }
    if (speed != 0U) {
        g_ball_x += g_ball_vx;
        g_ball_y += g_ball_vy;
    if (g_ball_x <= 0) {
        g_ball_x = 0;
        g_ball_vx = -g_ball_vx;
    } else if (g_ball_x >= AI_ALBUM_BALL_TEST_SCREEN_W -
                               (int32_t)AI_ALBUM_BALL_TEST_SIZE) {
        g_ball_x = AI_ALBUM_BALL_TEST_SCREEN_W -
                   (int32_t)AI_ALBUM_BALL_TEST_SIZE;
        g_ball_vx = -g_ball_vx;
    }
    if (g_ball_y <= 0) {
        g_ball_y = 0;
        g_ball_vy = -g_ball_vy;
    } else if (g_ball_y >= AI_ALBUM_BALL_TEST_SCREEN_H -
                               (int32_t)AI_ALBUM_BALL_TEST_SIZE) {
        g_ball_y = AI_ALBUM_BALL_TEST_SCREEN_H -
                   (int32_t)AI_ALBUM_BALL_TEST_SIZE;
        g_ball_vy = -g_ball_vy;
    }
    } /* speed != 0U */
    lv_obj_set_pos(g_ball_obj, g_ball_x, g_ball_y);

    /* Solid color block: independent trajectory; color changes and
     * show/hide are applied here in the LVGL context. */
    if (g_ball_block_obj != NULL &&
        lv_obj_is_valid(g_ball_block_obj)) {
        if (g_ball_block_color_req != g_ball_block_color_cur) {
            g_ball_block_color_cur = g_ball_block_color_req;
            if (g_ball_block_color_cur == 0U) {
                lv_obj_add_flag(g_ball_block_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_style_bg_color(
                    g_ball_block_obj,
                    lv_color_hex(ball_block_colors[g_ball_block_color_cur]),
                    LV_PART_MAIN);
                lv_obj_clear_flag(g_ball_block_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (g_ball_block_color_cur != 0U) {
            ball_test_move_bounce(&g_ball_block_x, &g_ball_block_y,
                                  &g_ball_block_vx, &g_ball_block_vy,
                                  g_ball_block_obj,
                                  AI_ALBUM_BALL_TEST_BLOCK_SIZE);
        }
    }

    /* Triangle / pentagon shapes: fan-drawn via custom draw events;
     * visibility mask is applied here in the LVGL context. */
    if (g_ball_tri_obj != NULL && lv_obj_is_valid(g_ball_tri_obj)) {
        if (g_ball_poly_req != g_ball_poly_cur) {
            g_ball_poly_cur = g_ball_poly_req;
            if (g_ball_poly_cur & 1U) {
                lv_obj_clear_flag(g_ball_tri_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(g_ball_tri_obj, LV_OBJ_FLAG_HIDDEN);
            }
            if (g_ball_poly_cur & 2U) {
                lv_obj_clear_flag(g_ball_pent_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(g_ball_pent_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if ((g_ball_poly_cur & 1U) != 0U) {
            ball_test_move_bounce(&g_ball_tri_x, &g_ball_tri_y,
                                  &g_ball_tri_vx, &g_ball_tri_vy,
                                  g_ball_tri_obj,
                                  AI_ALBUM_BALL_TEST_POLY_SIZE);
        }
        if ((g_ball_poly_cur & 2U) != 0U &&
            g_ball_pent_obj != NULL && lv_obj_is_valid(g_ball_pent_obj)) {
            ball_test_move_bounce(&g_ball_pent_x, &g_ball_pent_y,
                                  &g_ball_pent_vx, &g_ball_pent_vy,
                                  g_ball_pent_obj,
                                  AI_ALBUM_BALL_TEST_POLY_SIZE);
        }
    }

    g_ball_ticks++;
    /* On-screen counter: only increments while the full render+flush path
     * works end to end; v= shows the current speed (0 = ball parked but
     * LVGL still alive). */
    lv_label_set_text_fmt(g_ball_label, "ticks %u v=%u",
                          (unsigned)g_ball_ticks, (unsigned)speed);

    {
        uint64_t now_us = os_useconds();
        if ((now_us - g_ball_last_log_us) >= AI_ALBUM_BALL_TEST_LOG_US) {
            g_ball_last_log_us = now_us;
            os_printf("[BALL] ticks=%u v=%u pos=%d,%d\r\n",
                      (unsigned)g_ball_ticks, (unsigned)speed,
                      (int)g_ball_x, (int)g_ball_y);
        }
    }
}

static void ball_test_build(lv_display_t *display)
{
    /* Build on a dedicated screen and load it, keeping the app UI objects
     * alive underneath. The app's page polls keep touching their objects
     * every second -- destroying them (lv_obj_clean on the active screen)
     * hangs the LVGL task on freed memory, observed on board. */
    g_ball_prev_screen = lv_display_get_screen_active(display);
    if (g_ball_prev_screen == NULL) return;

    g_ball_screen = lv_obj_create(NULL);
    if (g_ball_screen == NULL) return;
    ball_test_style_solid(g_ball_screen, 0x000000U);

    g_ball_obj = lv_obj_create(g_ball_screen);
    if (g_ball_obj == NULL) {
        lv_obj_delete(g_ball_screen);
        g_ball_screen = NULL;
        return;
    }
    ball_test_style_solid(g_ball_obj, 0xFFFFFFU);
    lv_obj_set_style_radius(g_ball_obj,
                            (int32_t)AI_ALBUM_BALL_TEST_SIZE / 2,
                            LV_PART_MAIN);
    lv_obj_set_size(g_ball_obj, AI_ALBUM_BALL_TEST_SIZE,
                    AI_ALBUM_BALL_TEST_SIZE);

    g_ball_label = lv_label_create(g_ball_screen);
    if (g_ball_label != NULL) {
        lv_obj_set_style_text_color(g_ball_label,
                                    lv_color_hex(0x00FF00U),
                                    LV_PART_MAIN);
        lv_label_set_text(g_ball_label, "ticks 0");
        lv_obj_set_pos(g_ball_label, 8, 8);
    }

    g_ball_block_obj = lv_obj_create(g_ball_screen);
    if (g_ball_block_obj != NULL) {
        ball_test_style_solid(g_ball_block_obj, 0xFF0000U);
        lv_obj_set_size(g_ball_block_obj, AI_ALBUM_BALL_TEST_BLOCK_SIZE,
                        AI_ALBUM_BALL_TEST_BLOCK_SIZE);
        g_ball_block_x = 300;
        g_ball_block_y = 350;
        g_ball_block_vx = AI_ALBUM_BALL_TEST_BLOCK_SPEED_X;
        g_ball_block_vy = AI_ALBUM_BALL_TEST_BLOCK_SPEED_Y;
        lv_obj_set_pos(g_ball_block_obj, g_ball_block_x, g_ball_block_y);
        lv_obj_add_flag(g_ball_block_obj, LV_OBJ_FLAG_HIDDEN);
        g_ball_block_color_cur = 0U;
    }

    g_ball_tri_obj = lv_obj_create(g_ball_screen);
    if (g_ball_tri_obj != NULL) {
        /* Transparent bounding box: only the drawn polygon is visible, so
         * overlapping objects are not occluded by the box. */
        lv_obj_remove_style_all(g_ball_tri_obj);
        lv_obj_clear_flag(g_ball_tri_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(g_ball_tri_obj, AI_ALBUM_BALL_TEST_POLY_SIZE,
                        AI_ALBUM_BALL_TEST_POLY_SIZE);
        lv_obj_add_event_cb(g_ball_tri_obj, ball_poly_triangle_draw_cb,
                            LV_EVENT_DRAW_MAIN, NULL);
        g_ball_tri_x = 600;
        g_ball_tri_y = 120;
        g_ball_tri_vx = AI_ALBUM_BALL_TEST_TRI_SPEED_X;
        g_ball_tri_vy = AI_ALBUM_BALL_TEST_TRI_SPEED_Y;
        lv_obj_set_pos(g_ball_tri_obj, g_ball_tri_x, g_ball_tri_y);
        lv_obj_add_flag(g_ball_tri_obj, LV_OBJ_FLAG_HIDDEN);
    }
    g_ball_pent_obj = lv_obj_create(g_ball_screen);
    if (g_ball_pent_obj != NULL) {
        lv_obj_remove_style_all(g_ball_pent_obj);
        lv_obj_clear_flag(g_ball_pent_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(g_ball_pent_obj, AI_ALBUM_BALL_TEST_POLY_SIZE,
                        AI_ALBUM_BALL_TEST_POLY_SIZE);
        lv_obj_add_event_cb(g_ball_pent_obj, ball_poly_pentagon_draw_cb,
                            LV_EVENT_DRAW_MAIN, NULL);
        g_ball_pent_x = 200;
        g_ball_pent_y = 450;
        g_ball_pent_vx = AI_ALBUM_BALL_TEST_PENT_SPEED_X;
        g_ball_pent_vy = AI_ALBUM_BALL_TEST_PENT_SPEED_Y;
        lv_obj_set_pos(g_ball_pent_obj, g_ball_pent_x, g_ball_pent_y);
        lv_obj_add_flag(g_ball_pent_obj, LV_OBJ_FLAG_HIDDEN);
    }
    g_ball_poly_cur = 0U;

    g_ball_x = (AI_ALBUM_BALL_TEST_SCREEN_W -
                (int32_t)AI_ALBUM_BALL_TEST_SIZE) / 2;
    g_ball_y = (AI_ALBUM_BALL_TEST_SCREEN_H -
                (int32_t)AI_ALBUM_BALL_TEST_SIZE) / 2;
    g_ball_vx = AI_ALBUM_BALL_TEST_SPEED_X;
    g_ball_vy = AI_ALBUM_BALL_TEST_SPEED_Y;
    g_ball_ticks = 0U;
    g_ball_last_log_us = os_useconds();
    /* 0 forces the first timer tick to apply the requested multiplier. */
    g_ball_speed_cur = 0U;
    lv_obj_set_pos(g_ball_obj, g_ball_x, g_ball_y);

    lv_screen_load(g_ball_screen);
    g_ball_timer = lv_timer_create(ball_test_timer_cb,
                                   AI_ALBUM_BALL_TEST_PERIOD_MS, NULL);
    g_ball_page_active = 1U;
    os_printf("[BALL_PAGE] on period=%ums size=%u\r\n",
              (unsigned)AI_ALBUM_BALL_TEST_PERIOD_MS,
              (unsigned)AI_ALBUM_BALL_TEST_SIZE);
}

static void ball_test_destroy(lv_display_t *display)
{
    (void)display;
    if (g_ball_timer != NULL) {
        lv_timer_del(g_ball_timer);
        g_ball_timer = NULL;
    }
    if (g_ball_prev_screen != NULL &&
        lv_obj_is_valid(g_ball_prev_screen)) {
        lv_screen_load(g_ball_prev_screen);
    }
    if (g_ball_screen != NULL && lv_obj_is_valid(g_ball_screen)) {
        lv_obj_delete(g_ball_screen);
    }
    g_ball_screen = NULL;
    g_ball_prev_screen = NULL;
    g_ball_obj = NULL;
    g_ball_label = NULL;
    g_ball_block_obj = NULL;
    g_ball_tri_obj = NULL;
    g_ball_pent_obj = NULL;
    g_ball_page_active = 0U;
    os_printf("[BALL_PAGE] off ticks=%u (app UI restored)\r\n",
              (unsigned)g_ball_ticks);
}

void ai_album_ball_test_page_request(uint8_t enable)
{
    g_ball_page_request = enable ? 1U : 0U;
    os_printf("[BALL_PAGE] request=%u\r\n", (unsigned)g_ball_page_request);
}

void ai_album_ball_test_page_set_speed(uint8_t multiplier)
{
    if (multiplier > 8U) {
        os_printf("[BALL_PAGE] speed must be 0..8 (0=stop, 1=normal)\r\n");
        return;
    }
    g_ball_speed_req = multiplier;
    os_printf("[BALL_PAGE] speed=%u%s\r\n", (unsigned)multiplier,
              multiplier == 0U ? " (paused, ticks keep counting)" : "");
}

void ai_album_ball_test_page_set_block_color(uint8_t color_id)
{
    if (color_id >= sizeof(ball_block_colors) /
                        sizeof(ball_block_colors[0])) {
        os_printf("[BALL_PAGE] block color must be 0..8 "
                  "(0=off 1=red 2=green 3=blue 4=white 5=yellow 6=cyan "
                  "7=magenta 8=gray50)\r\n");
        return;
    }
    g_ball_block_color_req = color_id;
    os_printf("[BALL_PAGE] block=%s\r\n",
              ball_block_color_names[color_id]);
}

void ai_album_ball_test_page_set_poly(uint8_t mask)
{
    if (mask > 3U) {
        os_printf("[BALL_PAGE] poly must be 0..3 "
                  "(0=off 1=triangle 2=pentagon 3=both)\r\n");
        return;
    }
    g_ball_poly_req = mask;
    os_printf("[BALL_PAGE] poly=%u (%s%s)\r\n", (unsigned)mask,
              (mask & 1U) ? "triangle " : "",
              (mask & 2U) ? "pentagon" : (mask & 1U) ? "" : "off");
}

void ai_album_ball_test_page_set_poly_colors(uint8_t tri_color,
                                             uint8_t pent_color)
{
    if (tri_color > 8U || pent_color > 8U) {
        os_printf("[BALL_PAGE] poly colors must be 0..8\r\n");
        return;
    }
    g_ball_tri_color_req = tri_color;
    g_ball_pent_color_req = pent_color;
    os_printf("[BALL_PAGE] tri=%s pent=%s\r\n",
              ball_block_color_names[tri_color],
              ball_block_color_names[pent_color]);
}

void ai_album_ball_test_page_poll(void)
{
    lv_display_t *display = lv_display_get_default();

    if (display == NULL) return;
    if (g_ball_page_request && !g_ball_page_active) {
        ball_test_build(display);
    } else if (!g_ball_page_request && g_ball_page_active) {
        ball_test_destroy(display);
    }
}
