#ifndef AI_ALBUM_CHAT_RUNTIME_H
#define AI_ALBUM_CHAT_RUNTIME_H

#include "brtc_agent/brtc_agent_types.h"

#define AI_ALBUM_CHAT_MESSAGE_COUNT 6U
#define AI_ALBUM_CHAT_TEXT_CAPACITY 256U
#define AI_ALBUM_CHAT_ERROR_CAPACITY 96U

typedef enum {
    AI_ALBUM_CHAT_STAGE_OFFLINE = 0,
    AI_ALBUM_CHAT_STAGE_CONNECTING,
    AI_ALBUM_CHAT_STAGE_READY,
    AI_ALBUM_CHAT_STAGE_LISTENING,
    AI_ALBUM_CHAT_STAGE_RECOGNIZING,
    AI_ALBUM_CHAT_STAGE_THINKING,
    AI_ALBUM_CHAT_STAGE_SPEAKING,
    AI_ALBUM_CHAT_STAGE_ERROR,
} ai_album_chat_stage_t;

typedef enum {
    AI_ALBUM_CHAT_ROLE_USER = 0,
    AI_ALBUM_CHAT_ROLE_ASSISTANT,
} ai_album_chat_role_t;

typedef struct {
    ai_album_chat_role_t role;
    uint8_t final;
    char text[AI_ALBUM_CHAT_TEXT_CAPACITY];
} ai_album_chat_message_t;

typedef struct {
    uint32_t sequence;
    brtc_agent_state_t agent_state;
    ai_album_chat_stage_t stage;
    uint8_t persona;
    uint8_t ptt_active;
    uint8_t speaking;
    uint8_t message_count;
    int error_code;
    char error_text[AI_ALBUM_CHAT_ERROR_CAPACITY];
    ai_album_chat_message_t messages[AI_ALBUM_CHAT_MESSAGE_COUNT];
} ai_album_chat_snapshot_t;

void ai_album_chat_runtime_init(void);
void ai_album_chat_runtime_sync_agent_state(void);
void ai_album_chat_runtime_handle_event(const brtc_agent_event_t *event);
void ai_album_chat_runtime_get_snapshot(ai_album_chat_snapshot_t *out);
int ai_album_chat_runtime_start_ptt(void);
int ai_album_chat_runtime_finish_ptt(void);
void ai_album_chat_runtime_stop_ptt(void);
void ai_album_chat_runtime_leave(void);
void ai_album_chat_runtime_reset_session(uint8_t persona);
int ai_album_chat_runtime_activate_persona(uint8_t persona,
                                           const char *pre_query,
                                           const char *post_query,
                                           const char *greeting);
void ai_album_chat_runtime_clear_persona(void);
int ai_album_chat_runtime_set_translation(const char *source_language,
                                          const char *target_language);
void ai_album_chat_runtime_clear_translation(void);

#endif
