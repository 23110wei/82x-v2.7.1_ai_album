#ifndef AI_ALBUM_BALL_TEST_PAGE_H
#define AI_ALBUM_BALL_TEST_PAGE_H

#include <stdint.h>
#include "ui/ai_album_compat.h"

/* DEBUG_TEMP: LCD stripe diagnostic. A bouncing ball + on-screen tick
 * counter that only moves while the LVGL render/flush path is alive, used
 * to judge whether the UI is actually refreshing during SD read
 * transactions. Requested via AT+BALLPAGE=0|1 from any task context;
 * page creation/destruction happens in the LVGL loop via _poll(). */

void ai_album_ball_test_page_request(uint8_t enable);
void ai_album_ball_test_page_poll(void);
/* Runtime speed control for the ball page: 0 pauses the ball (ticks keep
 * counting so the LVGL-alive indicator still works), 1 = default (5,3 px),
 * N = N times base speed. Applies whether or not the page is shown yet. */
void ai_album_ball_test_page_set_speed(uint8_t multiplier);
/* Show a moving solid color block on the ball page: 0 = hidden,
 * 1..7 = red/green/blue/white/yellow/cyan/magenta. Independent
 * trajectory from the ball; pair with speed=0 for block-only motion. */
void ai_album_ball_test_page_set_block_color(uint8_t color_id);
/* Show moving polygon shapes on the ball page (fan-drawn from
 * triangles): 0 = both hidden, 1 = triangle, 2 = pentagon, 3 = both.
 * Independent trajectories. */
void ai_album_ball_test_page_set_poly(uint8_t mask);
/* Polygon fill colors, indices 0..7 into the shared color table
 * (0=black 1=red 2=green 3=blue 4=white 5=yellow 6=cyan 7=magenta).
 * Defaults: triangle yellow, pentagon cyan. */
void ai_album_ball_test_page_set_poly_colors(uint8_t tri_color,
                                             uint8_t pent_color);

#endif
