#include "ui/pages/ai_album_lcd_test_page.h"

#include "basic_include.h"

#define AI_ALBUM_LCD_TEST_BAR_COUNT 8U
#define AI_ALBUM_LCD_TEST_WIDTH 1024
#define AI_ALBUM_LCD_TEST_HEIGHT 600
#define AI_ALBUM_LCD_TEST_TOGGLE_MS 500U

static lv_obj_t *g_lcd_test_bars[AI_ALBUM_LCD_TEST_BAR_COUNT];
static uint8_t g_lcd_test_toggle;

static void lcd_test_toggle_cb(lv_timer_t *timer)
{
    lv_obj_t *bar;
    uint32_t color;

    LV_UNUSED(timer);
    bar = g_lcd_test_bars[AI_ALBUM_LCD_TEST_BAR_COUNT - 1U];
    if (bar == NULL || !lv_obj_is_valid(bar)) return;
    g_lcd_test_toggle ^= 1U;
    color = g_lcd_test_toggle ? 0xFFFFFFU : 0xFFFF00U;
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_MAIN);
}

static void lcd_test_style_solid(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

int ai_album_lcd_test_page_create(lv_display_t *display)
{
    static const uint32_t colors[AI_ALBUM_LCD_TEST_BAR_COUNT] = {
        0x000000U, 0x0000FFU, 0xFF0000U, 0xFF00FFU,
        0x00FF00U, 0x00FFFFU, 0xFFFF00U, 0xFFFFFFU,
    };
    lv_obj_t *screen;
    uint32_t i;

    if (display == NULL) return RET_ERR;
    screen = lv_display_get_screen_active(display);
    if (screen == NULL) return RET_ERR;
    memset(g_lcd_test_bars, 0, sizeof(g_lcd_test_bars));
    g_lcd_test_toggle = 0U;
    lv_obj_clean(screen);
    lcd_test_style_solid(screen, 0x000000U);
    for (i = 0U; i < AI_ALBUM_LCD_TEST_BAR_COUNT; ++i) {
        lv_obj_t *bar = lv_obj_create(screen);

        if (bar == NULL) return RET_ERR;
        g_lcd_test_bars[i] = bar;
        lcd_test_style_solid(bar, colors[i]);
        lv_obj_set_pos(bar, (int32_t)i * 128, 0);
        lv_obj_set_size(bar, 128, AI_ALBUM_LCD_TEST_HEIGHT);
    }
    os_printf("ai_album: LVGL colorbar test ready bars=%u size=%ux%u\r\n",
              (unsigned)AI_ALBUM_LCD_TEST_BAR_COUNT,
              (unsigned)AI_ALBUM_LCD_TEST_WIDTH,
              (unsigned)AI_ALBUM_LCD_TEST_HEIGHT);
    lv_timer_create(lcd_test_toggle_cb, AI_ALBUM_LCD_TEST_TOGGLE_MS, NULL);
    return RET_OK;
}
