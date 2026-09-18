#include "audio/ai_album_volume.h"

#include "basic_include.h"
#include "dev/audio/ausys.h"   /* ausys_da_change_volume(mixer内部同款) */
#include "lib/syscfg/syscfg.h" /* syscfg_save() */
#include "syscfg.h"            /* 项目sys_cfgs(album_vol追加字段) */

/*
 * 移植自ai_album工程,按本SDK重写。音量作用于DAC硬件(ausys层,
 * 与mixer.c的MSI_CMD_SET_DAC_VOLUME最终调用相同),五档百分比。
 * 持久化:vendor syscfg库不给新命名记录分配槽位(write Fail addr:0),
 * 档位追加存于"syscfg"主记录的保留字段(sys_cfgs.album_vol),用项目
 * 的syscfg_save()整条写入——与亮度(album_bl_*)同一条已验证通路。
 * 注意:不能像旁系那样用msi找"R_AUDAC"(本工程无该闭源组件),也不
 * 能按type找mixer(msi_find2的type扫描会自匹配同type源组件)。
 */

#define AI_ALBUM_VOLUME_DEFAULT_LEVEL (AI_ALBUM_VOLUME_LEVEL_COUNT - 1U)

static const uint8_t g_volume_percentages[AI_ALBUM_VOLUME_LEVEL_COUNT] = {
    0U, 25U, 50U, 75U, 100U,
};
static uint8_t g_volume_level = AI_ALBUM_VOLUME_DEFAULT_LEVEL;
static uint8_t g_volume_initialized;

static uint8_t volume_load_level(void)
{
    for (uint8_t index = 0U; index < AI_ALBUM_VOLUME_LEVEL_COUNT; ++index) {
        if (sys_cfgs.album_vol == g_volume_percentages[index]) {
            return index;
        }
    }
    /* 0xFF=未设置(或非法值),落默认档 */
    return AI_ALBUM_VOLUME_DEFAULT_LEVEL;
}

static int volume_save_level(uint8_t level)
{
    uint8_t old_pct = sys_cfgs.album_vol;

    sys_cfgs.album_vol = g_volume_percentages[level];
    if (syscfg_save() == RET_OK) {
        return RET_OK;
    }
    sys_cfgs.album_vol = old_pct;
    os_printf("ai_album: volume save failed\r\n");
    return RET_ERR;
}

static int volume_apply_level(uint8_t level)
{
    int32_t ret = ausys_da_change_volume(g_volume_percentages[level]);

    if (ret != RET_OK) {
        os_printf("ai_album: volume apply %u%% ret=%d\r\n",
                  (unsigned)g_volume_percentages[level], (int)ret);
        return RET_ERR;
    }
    return RET_OK;
}

int ai_album_volume_init(void)
{
    int32_t ret;

    if (g_volume_initialized) {
        return RET_OK;
    }
    g_volume_level = volume_load_level();
    g_volume_initialized = 1U;
    ret = volume_apply_level(g_volume_level);
    os_printf("ai_album: volume restored=%u%% ret=%d\r\n",
              (unsigned)g_volume_percentages[g_volume_level], (int)ret);
    return ret == RET_OK ? RET_OK : RET_ERR;
}

uint8_t ai_album_volume_get_percent(void)
{
    if (!g_volume_initialized) {
        (void)ai_album_volume_init();
    }
    return g_volume_percentages[g_volume_level];
}

int ai_album_volume_adjust(int8_t delta)
{
    int32_t next_level;

    if (!g_volume_initialized) {
        (void)ai_album_volume_init();
    }
    next_level = (int32_t)g_volume_level + delta;
    if (next_level < 0) {
        next_level = 0;
    } else if (next_level >= (int32_t)AI_ALBUM_VOLUME_LEVEL_COUNT) {
        next_level = (int32_t)AI_ALBUM_VOLUME_LEVEL_COUNT - 1;
    }
    if ((uint8_t)next_level == g_volume_level) {
        /* 已在边界,重发一次当前值(覆盖外部改动)但不重复写flash */
        return volume_apply_level(g_volume_level);
    }
    if (volume_apply_level((uint8_t)next_level) != RET_OK) {
        return RET_ERR;
    }
    g_volume_level = (uint8_t)next_level;
    return volume_save_level(g_volume_level);
}

void ai_album_volume_restore(void)
{
    if (!g_volume_initialized) {
        (void)ai_album_volume_init();
        return;
    }
    (void)volume_apply_level(g_volume_level);
}
