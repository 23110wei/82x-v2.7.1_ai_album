#include "ui/ai_album_ui.h"

#include "basic_include.h"
#include "ui/ai_album_home_runtime.h"
#include "ui/ai_album_ui_input_internal.h"
#include "ui/ai_album_ui_route.h"
#include "ui/ai_album_ui_router.h"
#include "ui/pages/ai_album_album_pages.h"
#include "ui/pages/ai_album_ball_test_page.h"
#include "ui/pages/ai_album_lcd_test_page.h"

#include "album/ai_album_album_image_ai.h"
#include "album/ai_album_album_image_loader.h"

#ifndef AI_ALBUM_LCD_COLORBAR_TEST
#define AI_ALBUM_LCD_COLORBAR_TEST 0
#endif

/*
 * 移植适配:原工程自有LVGL任务循环每帧调用ai_album_ui_process();
 * 本工程显示走SDK的app_lvgl_init/lvgl_run任务,这里改为lv_timer
 * 消费按键队列+页面轮询(30ms周期,一次弹出一个动作,与原节奏一致)。
 */

#define AI_ALBUM_UI_PROCESS_PERIOD_MS 30

static lv_timer_t *g_ui_process_timer;

static void ai_album_ui_process(void)
{
    ai_album_ui_action_t action;

    ai_album_album_image_ai_poll();
    ai_album_album_image_loader_poll(); /* JPEG解码完成回调(原工程在LVGL循环调用) */
    ai_album_album_pages_poll();
    ai_album_ball_test_page_poll();
    /* Yield between queued actions so image loading can observe cancellation. */
    if (ai_album_ui_input_try_pop(&action) == RET_OK) {
        ai_album_ui_router_handle(action);
    }
    ai_album_ui_router_poll();
}

static void ai_album_ui_process_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ai_album_ui_process();
}

void ai_album_ui_bootstrap(void)
{
    const ai_album_home_model_t *home_model;

#if AI_ALBUM_LCD_COLORBAR_TEST
    if (ai_album_lcd_test_page_create(lv_disp_get_default()) != RET_OK) {
        os_printf("ai_album: lcd test page failed\r\n");
        return;
    }
    os_printf("ai_album: UI colorbar diagnostic mode\r\n");
    return;
#endif

    if (ai_album_ui_input_init() != RET_OK) {
        os_printf("ai_album: ui input init failed\r\n");
        return;
    }
    home_model = ai_album_home_runtime_prepare();
    if (ai_album_ui_router_init(lv_disp_get_default(), home_model) != RET_OK) {
        os_printf("ai_album: ui router init failed\r\n");
        return;
    }
    if (ai_album_home_runtime_start() != RET_OK) {
        os_printf("ai_album: home runtime start failed\r\n");
        return;
    }
    g_ui_process_timer =
        lv_timer_create(ai_album_ui_process_timer_cb,
                        AI_ALBUM_UI_PROCESS_PERIOD_MS, NULL);
    if (g_ui_process_timer == NULL) {
        os_printf("ai_album: ui process timer failed\r\n");
        return;
    }
    os_printf("ai_album: UI ready 1024x600 routes=%u\r\n",
              (unsigned)AI_ALBUM_UI_ROUTE_COUNT);
}
