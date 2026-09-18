#include "brtc_agent_internal.h"

#include "osal/msgqueue.h"
#include "osal/mutex.h"
#include "osal/task.h"

#define BRTC_AGENT_CONTROL_STACK_SIZE (16U * 1024U)
#define BRTC_AGENT_COMMAND_DEPTH      4

brtc_agent_context_t g_brtc_agent;

static void brtc_agent_apply_query_enhancement_locked(void)
{
    /* ENHANCE_QUERY is a signaling control message.  The vendor example
     * sends it from onMediaSetup; sending while CONNECTING can be silently
     * discarded before the data channel is ready. */
    if (!g_brtc_agent.query_enhancement_enabled || !g_brtc_agent.engine ||
        g_brtc_agent.state != BRTC_AGENT_STATE_READY) {
        return;
    }
    baidu_chat_agent_engine_set_enhance_query(
        g_brtc_agent.engine, ENHANCE_QUERY_TYPE_BOTHWAY,
        g_brtc_agent.query_pre, g_brtc_agent.query_post);
    os_printf("[BRTC_AGENT] query enhancement applied pre=%u post=%u\r\n",
              (unsigned)os_strlen(g_brtc_agent.query_pre),
              (unsigned)os_strlen(g_brtc_agent.query_post));
}

void brtc_agent_internal_apply_query_enhancement(void)
{
    if (!g_brtc_agent.initialized) {
        return;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    brtc_agent_apply_query_enhancement_locked();
    os_mutex_unlock(&g_brtc_agent.lock);
}

static int brtc_agent_copy_string(char *dst, size_t dst_size,
                                  const char *src)
{
    size_t len;

    if (!dst || dst_size == 0U) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    dst[0] = '\0';
    if (!src) {
        return BRTC_AGENT_OK;
    }
    len = os_strlen(src);
    if (len >= dst_size) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    os_memcpy(dst, src, len + 1U);
    return BRTC_AGENT_OK;
}

static int brtc_agent_normalize_config(const brtc_agent_config_t *source,
                                       brtc_agent_runtime_config_t *target)
{
    int ret;

    if (!source || !target) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if ((source->screen_width == 0U) != (source->screen_height == 0U)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    os_memset(target, 0, sizeof(*target));

#define COPY_CONFIG_FIELD(field)                                                \
    do {                                                                        \
        ret = brtc_agent_copy_string(target->field, sizeof(target->field),       \
                                     source->field);                             \
        if (ret != BRTC_AGENT_OK) {                                              \
            return ret;                                                         \
        }                                                                       \
    } while (0)

    COPY_CONFIG_FIELD(platform_url);
    COPY_CONFIG_FIELD(create_url);
    COPY_CONFIG_FIELD(stop_url);
    COPY_CONFIG_FIELD(app_id);
    COPY_CONFIG_FIELD(license_key);
    COPY_CONFIG_FIELD(user_id);
    COPY_CONFIG_FIELD(llm);
    COPY_CONFIG_FIELD(language);
    COPY_CONFIG_FIELD(workflow);
#undef COPY_CONFIG_FIELD

    if (target->create_url[0] == '\0') {
        brtc_agent_copy_string(target->create_url, sizeof(target->create_url),
                               target->platform_url);
    }
    if (target->stop_url[0] == '\0') {
        brtc_agent_copy_string(target->stop_url, sizeof(target->stop_url),
                               target->platform_url);
    }
    if (target->llm[0] == '\0') {
        brtc_agent_copy_string(target->llm, sizeof(target->llm), "LLMRacing");
    }
    if (target->language[0] == '\0') {
        brtc_agent_copy_string(target->language, sizeof(target->language), "zh");
    }
    if (target->workflow[0] == '\0') {
        brtc_agent_copy_string(target->workflow, sizeof(target->workflow),
                               "VoiceChat");
    }

    target->http_timeout_ms = source->http_timeout_ms
                                  ? source->http_timeout_ms
                                  : 10000U;
    target->start_retries = source->start_retries
                                ? source->start_retries
                                : 3U;
    target->verbose = source->verbose;
    target->enable_voice_interrupt = source->enable_voice_interrupt;
    target->voice_interrupt_level = source->voice_interrupt_level;
    target->enable_video = source->enable_video;
    target->screen_width = source->screen_width;
    target->screen_height = source->screen_height;
    target->event_cb = source->event_cb;
    target->event_user_data = source->event_user_data;
    return BRTC_AGENT_OK;
}

void brtc_agent_internal_emit(const brtc_agent_event_t *event)
{
    if (event && g_brtc_agent.initialized && g_brtc_agent.config.event_cb) {
        g_brtc_agent.config.event_cb(event,
                                     g_brtc_agent.config.event_user_data);
    }
}

void brtc_agent_internal_set_state(brtc_agent_state_t state, int code,
                                   const char *message)
{
    brtc_agent_event_t event;

    if (!g_brtc_agent.initialized) {
        return;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    g_brtc_agent.state = state;
    os_mutex_unlock(&g_brtc_agent.lock);

    os_memset(&event, 0, sizeof(event));
    event.type = (state == BRTC_AGENT_STATE_ERROR)
                     ? BRTC_AGENT_EVENT_ERROR
                     : BRTC_AGENT_EVENT_STATE_CHANGED;
    event.state = state;
    event.code = code;
    event.text = message;
    event.text_len = message ? os_strlen(message) : 0U;
    brtc_agent_internal_emit(&event);
}

bool brtc_agent_internal_ptt_active(void)
{
    return g_brtc_agent.ptt_active;
}

void brtc_agent_internal_send_audio(const uint8_t *data, size_t len)
{
    if (!data || len == 0U || !g_brtc_agent.ptt_active) {
        return;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (g_brtc_agent.engine &&
        g_brtc_agent.state == BRTC_AGENT_STATE_READY &&
        g_brtc_agent.ptt_active) {
        baidu_chat_agent_engine_send_audio(g_brtc_agent.engine, data, len);
    }
    os_mutex_unlock(&g_brtc_agent.lock);
}

int brtc_agent_init(const brtc_agent_config_t *config)
{
    int ret;

    if (!config) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if (g_brtc_agent.initialized) {
        return BRTC_AGENT_OK;
    }

    os_memset(&g_brtc_agent, 0, sizeof(g_brtc_agent));
    ret = brtc_agent_normalize_config(config, &g_brtc_agent.config);
    if (ret != BRTC_AGENT_OK) {
        return ret;
    }
    if (os_mutex_init(&g_brtc_agent.lock) != RET_OK) {
        return BRTC_AGENT_ERR_NO_MEMORY;
    }
    if (os_msgq_init(&g_brtc_agent.commands, BRTC_AGENT_COMMAND_DEPTH) != RET_OK) {
        os_mutex_del(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_NO_MEMORY;
    }
    if (brtc_agent_compat_init() != BRTC_AGENT_OK) {
        os_msgq_del(&g_brtc_agent.commands);
        os_mutex_del(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_NO_MEMORY;
    }

    g_brtc_agent.control_stack = os_malloc_psram(BRTC_AGENT_CONTROL_STACK_SIZE);
    if (!g_brtc_agent.control_stack) {
        os_msgq_del(&g_brtc_agent.commands);
        os_mutex_del(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_NO_MEMORY;
    }
    g_brtc_agent.state = BRTC_AGENT_STATE_IDLE;
    g_brtc_agent.initialized = true;
    g_brtc_agent.control_task = os_task_create(
        "brtc_agent", brtc_agent_control_task, NULL,
        OS_TASK_PRIORITY_BELOW_NORMAL, 0, g_brtc_agent.control_stack,
        BRTC_AGENT_CONTROL_STACK_SIZE);
    if (!g_brtc_agent.control_task) {
        g_brtc_agent.initialized = false;
        os_free_psram(g_brtc_agent.control_stack);
        g_brtc_agent.control_stack = NULL;
        os_msgq_del(&g_brtc_agent.commands);
        os_mutex_del(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_NO_MEMORY;
    }
    os_printf("[BRTC_AGENT] component initialized\r\n");
    return BRTC_AGENT_OK;
}

int brtc_agent_ptt_start(void)
{
    if (brtc_agent_get_state() != BRTC_AGENT_STATE_READY) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    g_brtc_agent.ptt_active = true;
    return BRTC_AGENT_OK;
}

int brtc_agent_ptt_stop(void)
{
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    g_brtc_agent.ptt_active = false;
    return BRTC_AGENT_OK;
}

static int brtc_agent_send_text_internal(const char *text, bool tts_only)
{
    if (!text || text[0] == '\0') {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if (brtc_agent_get_state() != BRTC_AGENT_STATE_READY) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (!g_brtc_agent.engine) {
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    if (tts_only) {
        baidu_chat_agent_engine_send_text_to_TTS(g_brtc_agent.engine, text);
    } else {
        baidu_chat_agent_engine_send_text(g_brtc_agent.engine, text);
    }
    os_mutex_unlock(&g_brtc_agent.lock);
    return BRTC_AGENT_OK;
}

int brtc_agent_send_image_generation(const uint8_t *jpeg_data,
                                     size_t jpeg_len,
                                     const char *prompt)
{
    size_t prompt_len;

    if (!jpeg_data || jpeg_len == 0U || !prompt || prompt[0] == '\0') {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    prompt_len = os_strlen(prompt);
    if (prompt_len >= BRTC_AGENT_MEDIA_PROMPT_MAX) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (!g_brtc_agent.engine || !g_brtc_agent.config.enable_video ||
        g_brtc_agent.state != BRTC_AGENT_STATE_READY) {
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    baidu_chat_agent_engine_update_visual_mode(
        g_brtc_agent.engine, VISION_MODE_IMAGE, 180);
    baidu_chat_agent_engine_send_event_to_agent(
        g_brtc_agent.engine, AGENT_EVENT_ENABLE_MEDIA_GENERATE);
    baidu_chat_agent_engine_send_video(g_brtc_agent.engine, jpeg_data, jpeg_len);
    baidu_chat_agent_engine_send_text(g_brtc_agent.engine, prompt);
    os_mutex_unlock(&g_brtc_agent.lock);
    os_printf("[BRTC_AGENT] image generation submitted jpeg=%u prompt=%u\r\n",
              (unsigned)jpeg_len, (unsigned)prompt_len);
    return BRTC_AGENT_OK;
}

int brtc_agent_exit_image_generation(void)
{
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (g_brtc_agent.engine &&
        g_brtc_agent.state == BRTC_AGENT_STATE_READY) {
        baidu_chat_agent_engine_send_event_to_agent(
            g_brtc_agent.engine, AGENT_EVENT_DISABLE_MEDIA_GENERATE);
    }
    os_mutex_unlock(&g_brtc_agent.lock);
    return BRTC_AGENT_OK;
}

int brtc_agent_send_text(const char *text)
{
    return brtc_agent_send_text_internal(text, false);
}

int brtc_agent_speak_text(const char *text)
{
    return brtc_agent_send_text_internal(text, true);
}

int brtc_agent_set_query_enhancement(const char *pre_query,
                                     const char *post_query)
{
    size_t pre_length;
    size_t post_length;

    if (!pre_query || !post_query) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    pre_length = os_strlen(pre_query);
    post_length = os_strlen(post_query);
    if (pre_length == 0U || post_length == 0U) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if (pre_length >= sizeof(g_brtc_agent.query_pre) ||
        post_length >= sizeof(g_brtc_agent.query_post)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }

    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    os_memcpy(g_brtc_agent.query_pre, pre_query, pre_length + 1U);
    os_memcpy(g_brtc_agent.query_post, post_query, post_length + 1U);
    g_brtc_agent.query_enhancement_enabled = true;
    brtc_agent_apply_query_enhancement_locked();
    os_mutex_unlock(&g_brtc_agent.lock);
    return BRTC_AGENT_OK;
}

int brtc_agent_clear_query_enhancement(void)
{
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    g_brtc_agent.query_enhancement_enabled = false;
    g_brtc_agent.query_pre[0] = '\0';
    g_brtc_agent.query_post[0] = '\0';
    if (g_brtc_agent.engine) {
        baidu_chat_agent_engine_set_enhance_query(
            g_brtc_agent.engine, ENHANCE_QUERY_TYPE_NULL, "", "");
    }
    os_mutex_unlock(&g_brtc_agent.lock);
    return BRTC_AGENT_OK;
}

int brtc_agent_set_translation_languages(const char *source_language,
                                         const char *target_language)
{
    char pre_query[BRTC_AGENT_QUERY_PROMPT_CAPACITY];
    char post_query[BRTC_AGENT_QUERY_PROMPT_CAPACITY];
    int written;
    int result;

    if (!source_language || !target_language || source_language[0] == '\0' ||
        target_language[0] == '\0') {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    written = os_snprintf(pre_query, sizeof(pre_query),
                          "请将%s翻译成%s", source_language, target_language);
    if (written < 0 || (size_t)written >= sizeof(pre_query)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    written = os_snprintf(post_query, sizeof(post_query),
                          "只输出译文，不要回答");
    if (written < 0 || (size_t)written >= sizeof(post_query)) {
        return BRTC_AGENT_ERR_CONFIG;
    }

    result = brtc_agent_set_query_enhancement(pre_query, post_query);
    if (result != BRTC_AGENT_OK) {
        return result;
    }
    os_printf("[BRTC_AGENT] translation prompt set: %s -> %s\r\n",
              source_language, target_language);
    return BRTC_AGENT_OK;
}

int brtc_agent_clear_translation(void)
{
    int result = brtc_agent_clear_query_enhancement();

    if (result != BRTC_AGENT_OK) {
        return result;
    }
    os_printf("[BRTC_AGENT] translation prompt cleared\r\n");
    return BRTC_AGENT_OK;
}

int brtc_agent_interrupt(void)
{
    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    if (!g_brtc_agent.engine) {
        os_mutex_unlock(&g_brtc_agent.lock);
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    baidu_chat_agent_engine_interrupt(g_brtc_agent.engine);
    os_mutex_unlock(&g_brtc_agent.lock);
    return BRTC_AGENT_OK;
}

brtc_agent_state_t brtc_agent_get_state(void)
{
    brtc_agent_state_t state;

    if (!g_brtc_agent.initialized) {
        return BRTC_AGENT_STATE_UNINITIALIZED;
    }
    os_mutex_lock(&g_brtc_agent.lock, osWaitForever);
    state = g_brtc_agent.state;
    os_mutex_unlock(&g_brtc_agent.lock);
    return state;
}
