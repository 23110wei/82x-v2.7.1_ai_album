#include "hardware/battery_detect.h"

#include "basic_include.h"
#include "devid.h"
#include "hal/adc.h"

/* 移植自旁系ai_album工程src/hardware/battery_detect.c(TXW827-RGB888
 * GQ_XC001同板)。引脚按本工程惯例直接硬编码(与power_ctrl.c的
 * PA5/PA10/PA15/PD_13一致),不走config.cfg生成链 */
#define BATTERY_DETECT_PIN     PB_6
#define BATTERY_DETECT_SAMPLES 5u

/* ADC满量程3300mV对应raw 2047(与ADKEY阶梯同一基准);R54/R58 1M/1M
 * 分压把电池电压减半后进脚 */
#define BATTERY_DETECT_ADC_MAX      2047u
#define BATTERY_DETECT_VREF_MV      3300u
#define BATTERY_DETECT_DIVIDER_GAIN 2u

/* 锂电池带载电压曲线,升序: {VBAT mV, 电量%} */
static const uint16_t g_soc_table[][2] = {
    {3300, 0},  {3500, 5},  {3600, 10}, {3700, 20}, {3750, 35},
    {3800, 50}, {3900, 70}, {4000, 85}, {4100, 95}, {4200, 100},
};

#define BATTERY_DETECT_POINT_COUNT \
    (sizeof(g_soc_table) / sizeof(g_soc_table[0]))

static struct adc_device *g_adc;
static uint8_t g_configured;
static uint16_t g_percent_ema;
static uint8_t g_has_value;

static uint16_t battery_detect_raw_to_bat_mv(uint32_t raw)
{
    uint32_t pin_mv =
        raw * BATTERY_DETECT_VREF_MV / BATTERY_DETECT_ADC_MAX;

    return (uint16_t)(pin_mv * BATTERY_DETECT_DIVIDER_GAIN);
}

static uint16_t battery_detect_soc(uint16_t battery_mv)
{
    uint32_t i;

    if (battery_mv <= g_soc_table[0][0]) {
        return g_soc_table[0][1];
    }
    for (i = 1U; i < BATTERY_DETECT_POINT_COUNT; ++i) {
        uint16_t low_mv = g_soc_table[i - 1U][0];
        uint16_t high_mv = g_soc_table[i][0];

        if (battery_mv <= high_mv) {
            uint32_t span = (uint32_t)(high_mv - low_mv);
            uint32_t offset = (uint32_t)(battery_mv - low_mv);
            uint32_t low_pct = g_soc_table[i - 1U][1];
            uint32_t pct_span =
                (uint32_t)(g_soc_table[i][1] - low_pct);

            return (uint16_t)(low_pct + (offset * pct_span + span / 2u) /
                                             span);
        }
    }
    return g_soc_table[BATTERY_DETECT_POINT_COUNT - 1U][1];
}

static int battery_detect_sample_median(uint32_t *out)
{
    uint32_t samples[BATTERY_DETECT_SAMPLES];
    uint32_t i;
    uint32_t j;

    for (i = 0U; i < BATTERY_DETECT_SAMPLES; ++i) {
        if (adc_get_value(g_adc, BATTERY_DETECT_PIN, &samples[i]) !=
            RET_OK) {
            return RET_ERR;
        }
    }
    for (i = 1U; i < BATTERY_DETECT_SAMPLES; ++i) {
        uint32_t key = samples[i];

        for (j = i; j > 0U && samples[j - 1U] > key; --j) {
            samples[j] = samples[j - 1U];
        }
        samples[j] = key;
    }
    *out = samples[BATTERY_DETECT_SAMPLES / 2U];
    return RET_OK;
}

uint8_t battery_detect_percent(void)
{
    uint32_t raw;
    uint16_t percent;

    /* 仅单一轮询上下文调用;驱动互斥保证与按键扫描的硬件访问串行 */
    if (!g_configured) {
        int32_t ret;

        g_adc = (struct adc_device *)dev_get(HG_ADC0_DEVID);
        if (g_adc == NULL) {
            return 0U;
        }
        ret = adc_open(g_adc);
        if (ret != RET_OK && ret != -EBUSY) {
            os_printf("battery_detect: adc open failed ret=%d\r\n",
                      (int)ret);
            return 0U;
        }
        if (adc_add_channel(g_adc, BATTERY_DETECT_PIN) != RET_OK) {
            os_printf("battery_detect: add channel failed\r\n");
            return 0U;
        }
        g_configured = 1u;
    }

    if (battery_detect_sample_median(&raw) != RET_OK) {
        return (uint8_t)(g_has_value ? g_percent_ema : 0U);
    }

    percent = battery_detect_soc(battery_detect_raw_to_bat_mv(raw));
    if (!g_has_value) {
        g_has_value = 1u;
        g_percent_ema = percent;
    } else {
        g_percent_ema = (uint16_t)((g_percent_ema * 3u + percent) / 4u);
    }
    return (uint8_t)g_percent_ema;
}

/* 显示值全方向限速的慢变跟踪:任何情况都不允许跳变。
 * 上行:外部供电1%/20s(充电渐进),电池供电1%/10s(轻载电压恢复)
 * 下行:1%/5s(拔掉充电器后端电压从抬高值缓慢回落)
 * 偏差>=25%直接校正(首读/换电池/长期挂起后的大误差) */
#define BATTERY_DISPLAY_STEP_UP_EXTERNAL 20u
#define BATTERY_DISPLAY_STEP_UP_BATTERY  10u
#define BATTERY_DISPLAY_STEP_DOWN         5u
#define BATTERY_DISPLAY_SNAP_DELTA       25u

static uint8_t g_display_valid;
static uint8_t g_display_percent;
static uint8_t g_display_creep;

uint8_t battery_detect_display_percent(uint8_t external_power)
{
    uint8_t live = battery_detect_percent();
    uint8_t rate;
    uint8_t delta;

    if (!g_display_valid) {
        g_display_valid = 1u;
        g_display_percent = live;
    }
    if (live > g_display_percent) {
        delta = (uint8_t)(live - g_display_percent);
        if (delta >= BATTERY_DISPLAY_SNAP_DELTA) {
            g_display_percent = live;
            g_display_creep = 0u;
        } else {
            rate = external_power ? BATTERY_DISPLAY_STEP_UP_EXTERNAL
                                  : BATTERY_DISPLAY_STEP_UP_BATTERY;
            if (++g_display_creep >= rate) {
                g_display_creep = 0u;
                ++g_display_percent;
            }
        }
    } else if (live < g_display_percent) {
        delta = (uint8_t)(g_display_percent - live);
        if (delta >= BATTERY_DISPLAY_SNAP_DELTA) {
            g_display_percent = live;
            g_display_creep = 0u;
        } else if (++g_display_creep >= BATTERY_DISPLAY_STEP_DOWN) {
            g_display_creep = 0u;
            --g_display_percent;
        }
    } else {
        g_display_creep = 0u;
    }
    return g_display_percent;
}
