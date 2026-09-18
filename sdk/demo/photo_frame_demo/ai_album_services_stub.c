#include "basic_include.h"

#include "display/ai_album_direct_display.h"
#include "ui/pages/ai_album_ball_test_page.h"

/*
 * 系统服务桩实现:存储信息/直显统计等未接入,给UI提供安全默认值。
 * 逐项替换为真实实现时删除对应段落。已接真实现:音量(audio/)、
 * 亮度(display/)、电源(hardware/)、天气(network/)、蓝牙(network/)、
 * WiFi配网(network/)、时间(system/)。
 */

/* ---------------- volume ---------------- */
/* 真实现见 audio/ai_album_volume.c(DAC硬件音量+syscfg持久化) */

/* ---------------- brightness ---------------- */
/* 真实现见 display/ai_album_brightness.c(PWM调光+syscfg持久化) */

/* ---------------- power ctrl ---------------- */
/* 真实现见 hardware/power_ctrl.c(开机锁存/释放/关机序列/USB充电检测) */

/* ---------------- weather ---------------- */
/* 真实现见 network/ai_album_weather_service.c */

/* ---------------- ble settings ---------------- */
/* 真实现见 network/ble_settings.c */

/* ---------------- wifi credentials/provision ---------------- */
/* 真实现见 network/wifi_credentials.c 与 network/wifi_provision.c */

/* ---------------- storage info ---------------- */
/* 真实现见 storage/ai_album_storage_info.c(FatFS f_getfree+SD挂载检测) */

/* ---------------- time service ---------------- */
/* 真实现见 system/ai_album_time_service.c */

/* ---------------- direct display stats (perf) ---------------- */

void ai_album_direct_display_get_stats(ai_album_direct_display_stats_t *stats)
{
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
}

/* ---------------- ball test page (弹球诊断页,依赖9.5绘制API,暂缓移植) ---------------- */

void ai_album_ball_test_page_request(uint8_t enable)
{
    (void)enable;
}

void ai_album_ball_test_page_poll(void)
{
}

void ai_album_ball_test_page_set_speed(uint8_t multiplier)
{
    (void)multiplier;
}

void ai_album_ball_test_page_set_block_color(uint8_t color_id)
{
    (void)color_id;
}

void ai_album_ball_test_page_set_poly(uint8_t mask)
{
    (void)mask;
}

void ai_album_ball_test_page_set_poly_colors(uint8_t tri_color,
                                             uint8_t pent_color)
{
    (void)tri_color;
    (void)pent_color;
}
