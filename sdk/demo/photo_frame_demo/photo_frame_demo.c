#include "basic_include.h"
#include "demo/app_common.h"
#include "demo/app_mem.h"
#include "app_lcd/app_lcd.h"
#include "user_work/user_work.h"
#include "keyWork.h"
#ifdef PIN_FROM_PARAM
#include "pin_param.h"
#endif

// 板级电源引脚(TXW827-RGB888_GQ_XC001):
// AVDD_EN=PD13 屏模拟电源使能;VGH/VGL由屏模组内部从AVDD自举,无独立使能脚
// PD12是DAC/音频功放使能,不要作为屏电源操作
// 背光=LCD_BACKLIGHT_IO(config.cfg配置为PA_9)
#define PHOTO_FRAME_AVDD_EN_PIN      PD_13
#define PHOTO_FRAME_POWER_STEP_MS    50

extern void user_workqueue_init(uint16 pri, void *stack, uint16 stack_size);
extern void ai_album_ui_bootstrap(void);
extern void photo_frame_key_input_init(void);
extern void photo_frame_net_init(void);
extern void ble_settings_init(void);
extern int ai_album_volume_init(void);
extern int32_t app_sd_init(uint8_t start_ota, const char *ota_path);
extern int32 jpg_mutex_init(void);
extern void jpg_mem_init(int num);

static void photo_frame_gpio_write(uint32 pin, uint8 val)
{
    gpio_iomap_output(pin, GPIO_IOMAP_OUTPUT);
    gpio_set_mode(pin, GPIO_PULL_NONE, GPIO_PULL_LEVEL_NONE);
    gpio_set_dir(pin, GPIO_DIR_OUTPUT);
    gpio_set_val(pin, val);
}

// AVDD上电,屏模组内部产生VGH/VGL,各路间隔>=50ms
static void photo_frame_panel_power_on(void)
{
    photo_frame_gpio_write(PHOTO_FRAME_AVDD_EN_PIN, 1);
    os_sleep_ms(PHOTO_FRAME_POWER_STEP_MS);
}

// LVGL输出第一帧后再开背光(lcd_ready_callback由LCD中断在首帧完成后回调),
// 避免LCDC还在配置阶段背光电流冲击/花屏。开背光即启动PWM调光(恢复
// 上次保存的亮度档,失败则内部回退GPIO全亮)
static void photo_frame_backlight_on(void)
{
    extern int ai_album_brightness_init(void);
    if (ai_album_brightness_init() == RET_OK)
    {
        os_printf(KERN_INFO "photo_frame: backlight on (pwm)\n");
    }
    else
    {
        os_printf(KERN_INFO "photo_frame: backlight on (gpio)\n");
    }
}

static void app_print_init(void)
{
    print_level(7);
    disable_print_color(1);
}

static void app_heap_init(void)
{
    // 视频内存分配,分别配置av_psram和av_heap
    video_psram_init(0, CONFIG_PSRAM_AVHEAP_SIZE);
    video_sram_init(0, CONFIG_AVHEAP_SIZE);
}

static void app_workqueue_init(void)
{
    user_workqueue_init(OS_TASK_PRIORITY_HIGH, NULL, 2048);
}

static int32_t app_hardware_init(void)
{
    int32_t ret;

    // 6个AD按键共用PA15,经keyWork扫描后由photo_frame_key_input映射为UI动作
    keyWork_init(10);
    photo_frame_key_input_init();

    // 蓝牙:若上次为开启状态则恢复广播
    ble_settings_init();

    // SD卡:注册FatFS到VFS并挂载"0:"(相册/字体/存储信息依赖;无卡不阻塞)
    ret = app_sd_init(0, NULL);
    if (ret != RET_OK) {
        os_printf(KERN_WARNING "photo_frame: sd init ret=%d (no card?)\n",
                  (int)ret);
    }

    // JPEG子系统互斥锁/内存初始化(相册硬解码依赖,缺失会导致
    // jpg_mutex_lock断言: "mutex && mutex->init == 1")
    jpg_mutex_init();
    jpg_mem_init(32);

    // 音频链(brtc语音依赖,照coze AI对话demo配方):aurpc堆+采集建
    // S_AUADC+MIC/SPK设备热插拔+混音。PCM解码设备挂载/pcm_dec_msi_init
    // /audio_coder_msi_init由main.c Codec_init在PCM_DEC_CTRL下官方完成
    // (photo_frame_config.h经typesdef链对其可见),此处不可重复初始化
    app_audio_init(16000, 16000);
    // 音量档恢复到DAC硬件(依赖ausys_da_init完成,须在app_audio_init后)
    ai_album_volume_init();

    // 屏电源先上电,再初始化LCDC
    photo_frame_panel_power_on();
    app_lcd_init(NULL);
    lcd_ready_callback(photo_frame_backlight_on);
    return RET_OK;
}

static void app_function_init(void)
{
    /* indev传0:不用SDK的lvgl按键indev,按键动作由photo_frame_key_input
     * 直接投递到ai_album UI输入队列 */
    app_lvgl_init(ai_album_ui_bootstrap, 0);
}

void photo_frame_demo_init(void)
{
    /* 电源锁存最先执行:电池供电时松开SW8电源轨即衰减,必须在任何
     * 阻塞初始化前锁存(USB供电时内部直接锁存,无延时) */
    power_ctrl_hold();

    app_print_init();
    app_heap_init();
    app_power_init();
    app_workqueue_init();
    app_hardware_init();
    app_function_init();
    /* 百度语音Agent:init+事件注册,WiFi DHCP完成后自动start */
    photo_frame_net_init();
}
