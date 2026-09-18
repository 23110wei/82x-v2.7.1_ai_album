#include "display/ai_album_brightness.h"

#include "basic_include.h"
#include "dev.h"
#include "devid.h"
#include "hal/pwm.h"
#include "lib/syscfg/syscfg.h" /* syscfg_save() */
#include "pin_param.h"
#include "syscfg.h"            /* 项目sys_cfgs(album_bl_*追加字段) */

/*
 * 移植自ai_album工程。背光调光:PWM0通道0输出到LCD_BACKLIGHT_IO(PA9),
 * 五档占空比(20/40/60/80/100%),PWM失败时回退为GPIO全亮。
 * 持久化:vendor syscfg库不给新命名记录分配槽位(write Fail addr:0),
 * 档位追加存于"syscfg"主记录的保留字段(sys_cfgs.album_bl_*),用项目
 * 的syscfg_save()整条写入——该通路与WiFi配置持久化相同,已验证可靠。
 */

#define AI_ALBUM_BRIGHTNESS_DEFAULT_LEVEL 3U
#define AI_ALBUM_BRIGHTNESS_CONFIG_MARKER 0xB7U
#define AI_ALBUM_BRIGHTNESS_PWM_PERIOD 9600U

static const uint8_t g_brightness_percentages[] = {
    20U, 40U, 60U, 80U, 100U,
};
static struct pwm_device *g_brightness_pwm;
static uint8_t g_brightness_level = AI_ALBUM_BRIGHTNESS_DEFAULT_LEVEL;
static uint8_t g_brightness_initialized;
static uint8_t g_brightness_output_enabled;

static uint32_t brightness_level_duty(uint8_t level)
{
    return AI_ALBUM_BRIGHTNESS_PWM_PERIOD *
           g_brightness_percentages[level] / 100U;
}

static uint8_t brightness_load_level(void)
{
    if (sys_cfgs.album_bl_marker != AI_ALBUM_BRIGHTNESS_CONFIG_MARKER) {
        return AI_ALBUM_BRIGHTNESS_DEFAULT_LEVEL;
    }
    for (uint8_t index = 0U; index < ARRAY_SIZE(g_brightness_percentages);
         ++index) {
        if (sys_cfgs.album_bl_pct == g_brightness_percentages[index]) {
            return index;
        }
    }
    return AI_ALBUM_BRIGHTNESS_DEFAULT_LEVEL;
}

static int brightness_save_level(uint8_t level)
{
    uint8_t old_pct  = sys_cfgs.album_bl_pct;
    uint8_t old_mark = sys_cfgs.album_bl_marker;

    sys_cfgs.album_bl_pct    = g_brightness_percentages[level];
    sys_cfgs.album_bl_marker = AI_ALBUM_BRIGHTNESS_CONFIG_MARKER;
    if (syscfg_save() == RET_OK) {
        return RET_OK;
    }
    sys_cfgs.album_bl_pct    = old_pct;
    sys_cfgs.album_bl_marker = old_mark;
    os_printf("ai_album: brightness save failed\r\n");
    return RET_ERR;
}

static int brightness_pwm_update(uint32_t duty)
{
    int result;

    if (g_brightness_pwm == NULL || duty > AI_ALBUM_BRIGHTNESS_PWM_PERIOD) {
        return RET_ERR;
    }
    result = pwm_ioctl(g_brightness_pwm, PWM_CHANNEL_0,
                       PWM_IOCTL_CMD_SET_PERIOD_DUTY_IMMEDIATELY,
                       AI_ALBUM_BRIGHTNESS_PWM_PERIOD, duty);
    os_printf("ai_album: brightness pwm period=%u duty=%u ret=%d\r\n",
              (unsigned)AI_ALBUM_BRIGHTNESS_PWM_PERIOD,
              (unsigned)duty, result);
    return result;
}

static int brightness_apply_level(uint8_t level)
{
    if (level >= ARRAY_SIZE(g_brightness_percentages) ||
        !g_brightness_output_enabled) {
        return RET_ERR;
    }
    return brightness_pwm_update(brightness_level_duty(level));
}

static void brightness_restore_gpio_on(void)
{
    uint32_t pin = MACRO_PIN(LCD_BACKLIGHT_IO);

    gpio_iomap_output(pin, GPIO_IOMAP_OUTPUT);
    gpio_set_mode(pin, GPIO_PULL_NONE, GPIO_PULL_LEVEL_NONE);
    gpio_set_dir(pin, GPIO_DIR_OUTPUT);
    gpio_set_val(pin, 1U);
}

static void brightness_pwm_release(void)
{
    if (g_brightness_pwm != NULL) {
        (void)pwm_stop(g_brightness_pwm, PWM_CHANNEL_0);
        (void)pwm_deinit(g_brightness_pwm, PWM_CHANNEL_0);
        dev_put((struct dev_obj *)g_brightness_pwm);
        g_brightness_pwm = NULL;
    }
    g_brightness_output_enabled = 0U;
    g_brightness_initialized = 0U;
}

static int brightness_pwm_init(uint8_t level)
{
    uint32_t duty = brightness_level_duty(level);
    int init_result;
    int start_result;
    int update_result;

    g_brightness_pwm = (struct pwm_device *)dev_get(HG_PWM0_DEVID);
    if (g_brightness_pwm == NULL) {
        os_printf("ai_album: brightness PWM0 unavailable\r\n");
        return RET_ERR;
    }
    init_result = pwm_init(g_brightness_pwm, PWM_CHANNEL_0,
                           AI_ALBUM_BRIGHTNESS_PWM_PERIOD, duty);
    start_result = init_result == RET_OK ?
                       pwm_start(g_brightness_pwm, PWM_CHANNEL_0) : RET_ERR;
    g_brightness_output_enabled = (uint8_t)(start_result == RET_OK);
    update_result = start_result == RET_OK ?
                        brightness_pwm_update(duty) : RET_ERR;
    os_printf("ai_album: brightness pwm init=%d start=%d update=%d\r\n",
              init_result, start_result, update_result);
    if (init_result != RET_OK || start_result != RET_OK ||
        update_result != RET_OK) {
        brightness_pwm_release();
        return RET_ERR;
    }
    return RET_OK;
}

int ai_album_brightness_init(void)
{
    if (g_brightness_initialized) {
        return RET_OK;
    }
    if (MACRO_PIN(PIN_PWM_CHANNEL_0) != MACRO_PIN(LCD_BACKLIGHT_IO)) {
        os_printf("ai_album: brightness PWM/backlight pin mismatch\r\n");
        return RET_ERR;
    }
    g_brightness_level = brightness_load_level();
    if (brightness_pwm_init(g_brightness_level) != RET_OK) {
        brightness_restore_gpio_on();
        os_printf("ai_album: brightness PWM init failed, using full GPIO\r\n");
        return RET_ERR;
    }
    g_brightness_initialized = 1U;
    os_printf("ai_album: brightness restored=%u%%\r\n",
              (unsigned)ai_album_brightness_get_percent());
    return RET_OK;
}

uint8_t ai_album_brightness_get_level(void)
{
    return g_brightness_level;
}

uint8_t ai_album_brightness_get_percent(void)
{
    return g_brightness_percentages[g_brightness_level];
}

int ai_album_brightness_preview_level(uint8_t level)
{
    if (level >= ARRAY_SIZE(g_brightness_percentages) ||
        (!g_brightness_initialized && ai_album_brightness_init() != RET_OK)) {
        return RET_ERR;
    }
    return brightness_apply_level(level);
}

int ai_album_brightness_set_level(uint8_t level)
{
    uint8_t old_level;

    if (level >= ARRAY_SIZE(g_brightness_percentages) ||
        (!g_brightness_initialized && ai_album_brightness_init() != RET_OK)) {
        return RET_ERR;
    }
    if (level == g_brightness_level) {
        return RET_OK;
    }
    old_level = g_brightness_level;
    if (brightness_apply_level(level) != RET_OK) {
        return RET_ERR;
    }
    if (brightness_save_level(level) != RET_OK) {
        (void)brightness_apply_level(old_level);
        return RET_ERR;
    }
    g_brightness_level = level;
    os_printf("ai_album: brightness applied=%u%%\r\n",
              (unsigned)ai_album_brightness_get_percent());
    return RET_OK;
}

int ai_album_brightness_set_output_enabled(uint8_t enabled)
{
    int result;

    if (!g_brightness_initialized) {
        return RET_ERR;
    }
    if (enabled) {
        result = pwm_start(g_brightness_pwm, PWM_CHANNEL_0);
        if (result != RET_OK) {
            return result;
        }
        g_brightness_output_enabled = 1U;
        result = brightness_apply_level(g_brightness_level);
        if (result != RET_OK) {
            g_brightness_output_enabled = 0U;
        }
        return result;
    }
    result = brightness_pwm_update(0U);
    if (result == RET_OK) {
        result = pwm_stop(g_brightness_pwm, PWM_CHANNEL_0);
    }
    if (result == RET_OK) {
        g_brightness_output_enabled = 0U;
    }
    return result;
}
