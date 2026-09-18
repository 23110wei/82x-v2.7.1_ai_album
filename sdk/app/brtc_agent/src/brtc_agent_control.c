#include "brtc_agent_internal.h"

#include "lib/heap/sysheap.h"
#include "osal/sleep.h"

typedef enum brtc_agent_command {
    BRTC_AGENT_COMMAND_START = 1,
    BRTC_AGENT_COMMAND_STOP,
    BRTC_AGENT_COMMAND_SWITCH_LANGUAGE,
} brtc_agent_command_t;

static void brtc_agent_debug_heap(const char *stage)
{
    os_printf("[BRTC_AGENT][DEBUG_TEMP] heap %s: sram=%u psram=%u\r\n",
              stage, (unsigned)sysheap_freesize(&sram_heap),
              (unsigned)sysheap_freesize(&psram_heap));
}

static void brtc_agent_fill_engine_params(AgentEngineParams *params)
{
    const brtc_agent_runtime_config_t *config = &g_brtc_agent.config;

    os_memset(params, 0, sizeof(*params));
    os_snprintf(params->agent_platform_url,
                sizeof(params->agent_platform_url), "%s",
                config->platform_url);
    os_snprintf(params->appid, sizeof(params->appid), "%s", config->app_id);
    os_snprintf(params->userId, sizeof(params->userId), "%s",
                config->user_id[0] ? config->user_id :
                                     brtc_agent_device_id());
    os_snprintf(params->cer, sizeof(params->cer), "%s", "./a.cer");
    os_snprintf(params->workflow, sizeof(params->workflow), "%s",
                config->workflow);
    os_snprintf(params->license_key, sizeof(params->license_key), "%s",
                config->license_key);
    os_snprintf(params->llm, sizeof(params->llm), "%s", config->llm);
    os_snprintf(params->lang, sizeof(params->lang), "%s", config->language);
    os_snprintf(params->AudioIncodecType,
                sizeof(params->AudioIncodecType), "%s", "pcm");
    os_snprintf(params->AudioOutcodecType,
                sizeof(params->AudioOutcodecType), "%s", "pcm");
    os_snprintf(params->remote_params, sizeof(params->remote_params), "%s",
                g_brtc_agent.remote_params);

    params->verbose = config->verbose;
    params->enable_internal_device = false;
    params->enable_local_agent = false;
    params->enable_voice_interrupt = config->enable_voice_interrupt;
    params->level_voice_interrupt = config->voice_interrupt_level;
    params->AudioInChannel = 1;
    /* 如实声明麦克风采样率(audio_adc_init 16000):引擎按此值对
     * send_audio 的PCM做内部重采样(rtc_ac=pcmu 8k传输)。旁系写8000
     * 但mic实际16k,属其历史遗留;如实声明避免ASR变调 */
    params->AudioInFrequency = 16000;
    params->enable_video = config->enable_video;
    params->region = REGION_BD_DEV;
}

static bool brtc_agent_config_is_complete(void)
{
    return g_brtc_agent.config.platform_url[0] != '\0' &&
           g_brtc_agent.config.create_url[0] != '\0' &&
           g_brtc_agent.config.app_id[0] != '\0' &&
           g_brtc_agent.config.license_key[0] != '\0';
}

static void brtc_agent_start_internal(void)
{
    BaiduChatAgentEvent events;
    AgentEngineParams params;
    BaiduChatAgentEngine *engine = NULL;
    const char *device_id = brtc_agent_device_id();
    int ret = BRTC_AGENT_ERR_HTTP;
    uint8_t attempt;

    if (!brtc_agent_config_is_complete()) {
        os_printf("[BRTC_AGENT] missing platform/app/license configuration\r\n");
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR,
                                      BRTC_AGENT_ERR_CONFIG,
                                      "missing_baidu_configuration");
        return;
    }

    g_brtc_agent.stop_requested = false;
    g_brtc_agent.ptt_active = false;
    g_brtc_agent.remote_params[0] = '\0';
    g_brtc_agent.instance_id[0] = '\0';
    brtc_agent_internal_set_state(BRTC_AGENT_STATE_STARTING, 0, NULL);

    for (attempt = 0U; attempt < g_brtc_agent.config.start_retries; ++attempt) {
        if (g_brtc_agent.stop_requested) {
            return;
        }
        ret = brtc_agent_http_create(&g_brtc_agent.config, device_id,
                                     g_brtc_agent.remote_params,
                                     sizeof(g_brtc_agent.remote_params),
                                     g_brtc_agent.instance_id,
                                     sizeof(g_brtc_agent.instance_id));
        if (ret == BRTC_AGENT_OK) {
            break;
        }
        os_printf("[BRTC_AGENT] create HTTP attempt %u/%u failed: %d\r\n",
                  (unsigned)(attempt + 1U),
                  (unsigned)g_brtc_agent.config.start_retries, ret);
        if ((attempt + 1U) < g_brtc_agent.config.start_retries) {
            os_sleep_ms(3000U);
        }
    }
    if (ret != BRTC_AGENT_OK || g_brtc_agent.stop_requested) {
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR, ret,
                                      "create_agent_failed");
        return;
    }

    brtc_agent_debug_heap("before_engine_create");
    brtc_agent_event_table_init(&events);
    engine = baidu_create_chat_agent_engine(&events);
    if (!engine) {
        ret = BRTC_AGENT_ERR_ENGINE;
        goto start_failed;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    g_brtc_agent.engine = engine;
    os_mutex_unlock(&g_brtc_agent.lock);

    brtc_agent_fill_engine_params(&params);
    ret = baidu_chat_agent_engine_init(engine, &params);
    if (ret != 200) {
        os_printf("[BRTC_AGENT] engine init failed: %d\r\n", ret);
        ret = BRTC_AGENT_ERR_ENGINE;
        goto start_failed;
    }
    brtc_agent_debug_heap("after_engine_init");

    ret = brtc_agent_audio_start();
    if (ret != BRTC_AGENT_OK) {
        goto start_failed;
    }
    brtc_agent_debug_heap("after_audio_start");

    brtc_agent_internal_set_state(BRTC_AGENT_STATE_CONNECTING, 0, NULL);
    baidu_chat_agent_engine_call(engine);
    brtc_agent_debug_heap("after_engine_call");
    os_printf("[BRTC_AGENT] call started, waiting for media setup\r\n");
    return;

start_failed:
    brtc_agent_audio_stop();
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    g_brtc_agent.engine = NULL;
    os_mutex_unlock(&g_brtc_agent.lock);
    if (engine) {
        baidu_chat_agent_engine_destroy(engine);
    }
    (void)brtc_agent_http_stop(&g_brtc_agent.config, device_id,
                               g_brtc_agent.instance_id);
    brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR, ret,
                                  "engine_start_failed");
}

static void brtc_agent_stop_internal(void)
{
    BaiduChatAgentEngine *engine;

    if (brtc_agent_get_state() == BRTC_AGENT_STATE_IDLE) {
        return;
    }
    brtc_agent_internal_set_state(BRTC_AGENT_STATE_STOPPING, 0, NULL);
    g_brtc_agent.ptt_active = false;
    brtc_agent_audio_stop();

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    engine = g_brtc_agent.engine;
    g_brtc_agent.engine = NULL;
    os_mutex_unlock(&g_brtc_agent.lock);

    if (engine) {
        baidu_chat_agent_engine_destroy(engine);
    }

    if (g_brtc_agent.instance_id[0]) {
        (void)brtc_agent_http_stop(&g_brtc_agent.config,
                                   brtc_agent_device_id(),
                                   g_brtc_agent.instance_id);
    }

    os_sleep_ms(500U);
    brtc_agent_compat_reclaim_engine_stacks();
    g_brtc_agent.remote_params[0] = '\0';
    g_brtc_agent.instance_id[0] = '\0';
    g_brtc_agent.stop_requested = false;
    brtc_agent_internal_set_state(BRTC_AGENT_STATE_IDLE, 0, NULL);
}

static int brtc_agent_copy_language(char *destination, size_t capacity,
                                    const char *language)
{
    size_t length;

    if (!destination || capacity == 0U || !language ||
        language[0] == '\0') {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    destination[0] = '\0';
    length = os_strlen(language);
    if (length >= capacity) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    os_memcpy(destination, language, length + 1U);
    return BRTC_AGENT_OK;
}

static void brtc_agent_switch_language_internal(void)
{
    char desired[sizeof(g_brtc_agent.config.language)];
    bool changed;
    bool restart;

    for (;;) {
        os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
        (void)brtc_agent_copy_language(desired, sizeof(desired),
                                       g_brtc_agent.pending_language);
        os_mutex_unlock(&g_brtc_agent.lock);

        brtc_agent_stop_internal();
        os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
        (void)brtc_agent_copy_language(g_brtc_agent.config.language,
                                       sizeof(g_brtc_agent.config.language),
                                       desired);
        changed = os_strcmp(g_brtc_agent.pending_language, desired) != 0;
        restart = g_brtc_agent.language_switch_restart;
        if (!changed && restart) {
            g_brtc_agent.stop_requested = false;
        } else if (!changed) {
            g_brtc_agent.language_switch_pending = false;
        }
        os_mutex_unlock(&g_brtc_agent.lock);
        if (changed) {
            continue;
        }

        os_printf("[BRTC_AGENT] language configured: %s restart=%u\r\n",
                  desired, (unsigned)restart);
        if (!restart) {
            return;
        }
        brtc_agent_start_internal();

        os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
        changed = os_strcmp(g_brtc_agent.pending_language,
                            g_brtc_agent.config.language) != 0;
        if (!changed) {
            g_brtc_agent.language_switch_pending = false;
        }
        os_mutex_unlock(&g_brtc_agent.lock);
        if (!changed) {
            return;
        }
    }
}

void brtc_agent_control_task(void *arg)
{
    int32 error;
    uint32 command;

    (void)arg;
    for (;;) {
        error = RET_OK;
        command = os_msgq_get2(&g_brtc_agent.commands, osWaitForever, &error);
        if (error != RET_OK) {
            continue;
        }
        if (command == BRTC_AGENT_COMMAND_START) {
            brtc_agent_state_t state = brtc_agent_get_state();

            if (state == BRTC_AGENT_STATE_IDLE ||
                state == BRTC_AGENT_STATE_ERROR) {
                brtc_agent_start_internal();
            } else {
                os_printf("[BRTC_AGENT] skip duplicate start, state=%d\r\n",
                          state);
            }
            os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
            g_brtc_agent.start_pending = false;
            os_mutex_unlock(&g_brtc_agent.lock);
        } else if (command == BRTC_AGENT_COMMAND_STOP) {
            brtc_agent_stop_internal();
        } else if (command == BRTC_AGENT_COMMAND_SWITCH_LANGUAGE) {
            brtc_agent_switch_language_internal();
        }
    }
}

int brtc_agent_start(void)
{
    brtc_agent_state_t state;
    int32 queue_ret;

    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    state = g_brtc_agent.state;
    if (state == BRTC_AGENT_STATE_STARTING ||
        state == BRTC_AGENT_STATE_CONNECTING ||
        state == BRTC_AGENT_STATE_READY || g_brtc_agent.start_pending) {
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_OK;
    }
    g_brtc_agent.stop_requested = false;
    g_brtc_agent.start_pending = true;
    os_mutex_unlock(&g_brtc_agent.lock);

    queue_ret = os_msgq_put(&g_brtc_agent.commands,
                            BRTC_AGENT_COMMAND_START, 0);
    if (queue_ret != RET_OK) {
        os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
        g_brtc_agent.start_pending = false;
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_QUEUE_FULL;
    }
    return BRTC_AGENT_OK;
}

int brtc_agent_stop(void)
{
    brtc_agent_state_t state;
    int32 queue_ret;

    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    g_brtc_agent.stop_requested = true;
    g_brtc_agent.ptt_active = false;
    if (g_brtc_agent.language_switch_pending) {
        g_brtc_agent.language_switch_restart = false;
    }
    state = g_brtc_agent.state;
    os_mutex_unlock(&g_brtc_agent.lock);
    if (state == BRTC_AGENT_STATE_IDLE) {
        return BRTC_AGENT_OK;
    }
    queue_ret = os_msgq_put(&g_brtc_agent.commands,
                            BRTC_AGENT_COMMAND_STOP, 0);
    return queue_ret == RET_OK ? BRTC_AGENT_OK :
                                BRTC_AGENT_ERR_QUEUE_FULL;
}

int brtc_agent_get_language(char *language, size_t capacity)
{
    int result;

    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    result = brtc_agent_copy_language(language, capacity,
                                      g_brtc_agent.config.language);
    os_mutex_unlock(&g_brtc_agent.lock);
    return result;
}

int brtc_agent_switch_language(const char *language)
{
    brtc_agent_state_t state;
    int32 queue_ret;
    int result;

    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    if (!language || language[0] == '\0' ||
        os_strlen(language) >= sizeof(g_brtc_agent.config.language)) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (g_brtc_agent.language_switch_pending) {
        result = brtc_agent_copy_language(
            g_brtc_agent.pending_language,
            sizeof(g_brtc_agent.pending_language), language);
        os_mutex_unlock(&g_brtc_agent.lock);
        return result == BRTC_AGENT_OK ? BRTC_AGENT_IN_PROGRESS : result;
    }
    if (os_strcmp(g_brtc_agent.config.language, language) == 0) {
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_OK;
    }
    state = g_brtc_agent.state;
    if (state == BRTC_AGENT_STATE_IDLE) {
        result = brtc_agent_copy_language(
            g_brtc_agent.config.language,
            sizeof(g_brtc_agent.config.language), language);
        os_mutex_unlock(&g_brtc_agent.lock);
        if (result == BRTC_AGENT_OK) {
            os_printf("[BRTC_AGENT] idle language configured: %s\r\n",
                      language);
        }
        return result;
    }

    (void)brtc_agent_copy_language(g_brtc_agent.pending_language,
                                   sizeof(g_brtc_agent.pending_language),
                                   language);
    g_brtc_agent.language_switch_pending = true;
    g_brtc_agent.language_switch_restart =
        state != BRTC_AGENT_STATE_STOPPING;
    g_brtc_agent.stop_requested = true;
    g_brtc_agent.ptt_active = false;
    queue_ret = os_msgq_put(&g_brtc_agent.commands,
                            BRTC_AGENT_COMMAND_SWITCH_LANGUAGE, 0);
    if (queue_ret != RET_OK) {
        g_brtc_agent.language_switch_pending = false;
        g_brtc_agent.language_switch_restart = false;
        if (state != BRTC_AGENT_STATE_STOPPING) {
            g_brtc_agent.stop_requested = false;
        }
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_QUEUE_FULL;
    }
    os_mutex_unlock(&g_brtc_agent.lock);
    os_printf("[BRTC_AGENT] language switch requested: %s\r\n", language);
    return BRTC_AGENT_IN_PROGRESS;
}
