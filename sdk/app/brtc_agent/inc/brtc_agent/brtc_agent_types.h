#ifndef BRTC_AGENT_TYPES_H
#define BRTC_AGENT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum brtc_agent_state {
    BRTC_AGENT_STATE_UNINITIALIZED = 0,
    BRTC_AGENT_STATE_IDLE,
    BRTC_AGENT_STATE_STARTING,
    BRTC_AGENT_STATE_CONNECTING,
    BRTC_AGENT_STATE_READY,
    BRTC_AGENT_STATE_STOPPING,
    BRTC_AGENT_STATE_ERROR,
} brtc_agent_state_t;

typedef enum brtc_agent_event_type {
    BRTC_AGENT_EVENT_STATE_CHANGED = 0,
    BRTC_AGENT_EVENT_ERROR,
    BRTC_AGENT_EVENT_ASR,
    BRTC_AGENT_EVENT_ANSWER,
    BRTC_AGENT_EVENT_SPEAKING,
    BRTC_AGENT_EVENT_EMOTION,
    BRTC_AGENT_EVENT_LICENSE,
    BRTC_AGENT_EVENT_AGENT_RAW,
    BRTC_AGENT_EVENT_FUNCTION_CALL,
    BRTC_AGENT_EVENT_VIDEO_DATA,
    BRTC_AGENT_EVENT_MEDIA_GENERATE_RESULT,
    BRTC_AGENT_EVENT_MEDIA_GENERATE_ACK,
} brtc_agent_event_type_t;

typedef enum brtc_agent_media_type {
    BRTC_AGENT_MEDIA_UNKNOWN = 0,
    BRTC_AGENT_MEDIA_JPEG = 1,
    BRTC_AGENT_MEDIA_H263,
    BRTC_AGENT_MEDIA_H264,
    BRTC_AGENT_MEDIA_I420P,
    BRTC_AGENT_MEDIA_RGB,
    BRTC_AGENT_MEDIA_VP8,
} brtc_agent_media_type_t;

typedef struct brtc_agent_event {
    brtc_agent_event_type_t type;
    brtc_agent_state_t state;
    int code;
    bool final;
    bool active;
    const char *text;
    size_t text_len;
    const uint8_t *data;
    size_t data_len;
    brtc_agent_media_type_t media_type;
    int media_width;
    int media_height;
} brtc_agent_event_t;

typedef void (*brtc_agent_event_cb_t)(const brtc_agent_event_t *event,
                                      void *user_data);

typedef struct brtc_agent_config {
    const char *platform_url;
    const char *create_url;
    const char *stop_url;
    const char *app_id;
    const char *license_key;
    const char *user_id;
    const char *llm;
    const char *language;
    const char *workflow;
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
} brtc_agent_config_t;

enum {
    BRTC_AGENT_OK = 0,
    BRTC_AGENT_IN_PROGRESS = 1,
    BRTC_AGENT_ERR_INVALID_ARG = -1,
    BRTC_AGENT_ERR_INVALID_STATE = -2,
    BRTC_AGENT_ERR_NO_MEMORY = -3,
    BRTC_AGENT_ERR_QUEUE_FULL = -4,
    BRTC_AGENT_ERR_HTTP = -5,
    BRTC_AGENT_ERR_ENGINE = -6,
    BRTC_AGENT_ERR_AUDIO = -7,
    BRTC_AGENT_ERR_CONFIG = -8,
};

#ifdef __cplusplus
}
#endif

#endif
