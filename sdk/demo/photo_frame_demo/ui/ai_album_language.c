#include "ui/ai_album_language.h"

#include "basic_include.h"
#include "lib/syscfg/syscfg.h" /* syscfg_save() */
#include "syscfg.h"            /* 项目sys_cfgs(album_lang追加字段) */

/*
 * 移植适配:语言持久化不复用独立命名syscfg记录(vendor库不给新名字
 * 分配槽位,write Fail addr:0),改存"syscfg"主记录的追加字段
 * sys_cfgs.album_lang(0xFF=未设置),经syscfg_save()整条写入。
 */

static ai_album_language_t g_language = AI_ALBUM_LANGUAGE_ENGLISH;
static uint32_t g_language_revision;
static uint8_t g_language_initialized;
static uint8_t g_language_storage_ready;

int ai_album_language_init(void)
{
    if (g_language_initialized) return RET_OK;

    if (sys_cfgs.album_lang < AI_ALBUM_LANGUAGE_COUNT) {
        g_language = (ai_album_language_t)sys_cfgs.album_lang;
        g_language_storage_ready = 1U;
    } else {
        /* 首次使用:字段为0xFF,落默认语言 */
        g_language = AI_ALBUM_LANGUAGE_ENGLISH;
        g_language_storage_ready = 0U;
    }
    g_language_initialized = 1U;
    os_printf("ai_album: language=%s storage=%s\r\n",
              ai_album_language_locale(g_language),
              g_language_storage_ready ? "ready" : "unset");
    return RET_OK;
}

ai_album_language_t ai_album_language_get(void)
{
    (void)ai_album_language_init();
    return g_language;
}

int ai_album_language_set(ai_album_language_t language)
{
    uint8_t old_lang = sys_cfgs.album_lang;

    (void)ai_album_language_init();
    if (language >= AI_ALBUM_LANGUAGE_COUNT) {
        return RET_ERR;
    }
    if (language == g_language && g_language_storage_ready) return RET_OK;
    sys_cfgs.album_lang = (uint8_t)language;
    if (syscfg_save() != RET_OK) {
        sys_cfgs.album_lang = old_lang;
        os_printf("ai_album: language save failed\r\n");
        return RET_ERR;
    }
    g_language_storage_ready = 1U;
    g_language = language;
    g_language_revision++;
    os_printf("ai_album: language applied=%s revision=%u\r\n",
              ai_album_language_locale(language),
              (unsigned)g_language_revision);
    return RET_OK;
}

uint32_t ai_album_language_revision(void)
{
    (void)ai_album_language_init();
    return g_language_revision;
}

const char *ai_album_language_locale(ai_album_language_t language)
{
    static const char *const locales[AI_ALBUM_LANGUAGE_COUNT] = {
        "en-US", "zh-CN", "ja-JP",
    };

    return language < AI_ALBUM_LANGUAGE_COUNT ? locales[language] : locales[0];
}

const char *ai_album_language_native_name(ai_album_language_t language)
{
    static const char *const names[AI_ALBUM_LANGUAGE_COUNT] = {
        "English", "中文", "日本語",
    };

    return language < AI_ALBUM_LANGUAGE_COUNT ? names[language] : names[0];
}
