#ifndef BRTC_AGENT_INTERNAL_H
#define BRTC_AGENT_INTERNAL_H

#include "basic_include.h"
#include "brtc_agent/brtc_agent.h"
#include "baidu_chat_agents_engine.h"
#include "osal/msgqueue.h"
#include "osal/mutex.h"

/* Sizes the query-enhancement strings set by brtc_agent_set_translation_languages,
 * which stay short by design.  Media-generation prompts are long-form and need
 * their own bound. */
#define BRTC_AGENT_QUERY_PROMPT_CAPACITY 256U
#define BRTC_AGENT_MEDIA_PROMPT_MAX 512U

typedef struct brtc_agent_runtime_config {
    char platform_url[256];
    char create_url[256];
    char stop_url[256];
    char app_id[64];
    char license_key[256];
    char user_id[256];
    char llm[64];
    char language[16];
    char workflow[64];
    uint32_t http_timeout_ms;
    uint8_t start_retries;
    bool verbose;
    bool enable_voice_interrupt;
    int voice_interrupt_level;
    bool enable_video;
    uint32_t screen_width;
    uint32_t screen_height;
    brtc_agent_event_cb_t event_cb;
    void *event_user_data;
} brtc_agent_runtime_config_t;

typedef struct brtc_agent_context {
    brtc_agent_runtime_config_t config;
    os_mutex_t lock;
    os_msgqueue_t commands;
    void *control_task;
    void *control_stack;
    BaiduChatAgentEngine *engine;
    brtc_agent_state_t state;
    volatile bool initialized;
    volatile bool ptt_active;
    volatile bool stop_requested;
    bool start_pending;
    bool query_enhancement_enabled;
    bool language_switch_pending;
    bool language_switch_restart;
    char remote_params[1024];
    char instance_id[128];
    char query_pre[BRTC_AGENT_QUERY_PROMPT_CAPACITY];
    char query_post[BRTC_AGENT_QUERY_PROMPT_CAPACITY];
    char pending_language[16];
} brtc_agent_context_t;

extern brtc_agent_context_t g_brtc_agent;

int brtc_agent_http_create(const brtc_agent_runtime_config_t *config,
                           const char *device_id, char *remote_params,
                           size_t remote_params_size, char *instance_id,
                           size_t instance_id_size);
int brtc_agent_http_stop(const brtc_agent_runtime_config_t *config,
                         const char *device_id, const char *instance_id);

int brtc_agent_audio_start(void);
void brtc_agent_audio_stop(void);
void brtc_agent_audio_play_pcm(const uint8_t *data, size_t len);

void brtc_agent_event_table_init(BaiduChatAgentEvent *events);
const char *brtc_agent_device_id(void);

int brtc_agent_compat_init(void);
void brtc_agent_compat_reclaim_engine_stacks(void);

void brtc_agent_internal_emit(const brtc_agent_event_t *event);
void brtc_agent_internal_set_state(brtc_agent_state_t state, int code,
                                   const char *message);
void brtc_agent_internal_apply_query_enhancement(void);
void brtc_agent_internal_send_audio(const uint8_t *data, size_t len);
bool brtc_agent_internal_ptt_active(void);
void brtc_agent_control_task(void *arg);

#endif
