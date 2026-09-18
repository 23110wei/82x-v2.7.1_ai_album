#include "ui/ai_album_chat_runtime.h"

#include "basic_include.h"
#include "brtc_agent/brtc_agent.h"

#include <string.h>

typedef struct {
    ai_album_chat_snapshot_t snapshot;
    uint8_t initialized;
    uint8_t waiting_answer;
    uint8_t persona_active;
    uint8_t greeting_pending;
    char greeting[AI_ALBUM_CHAT_TEXT_CAPACITY];
} ai_album_chat_runtime_t;

static ai_album_chat_runtime_t g_chat_runtime;

static ai_album_chat_stage_t chat_stage_for_agent(brtc_agent_state_t state)
{
    if (state == BRTC_AGENT_STATE_READY) {
        return AI_ALBUM_CHAT_STAGE_READY;
    }
    if (state == BRTC_AGENT_STATE_STARTING ||
        state == BRTC_AGENT_STATE_CONNECTING ||
        state == BRTC_AGENT_STATE_STOPPING) {
        return AI_ALBUM_CHAT_STAGE_CONNECTING;
    }
    if (state == BRTC_AGENT_STATE_ERROR) {
        return AI_ALBUM_CHAT_STAGE_ERROR;
    }
    return AI_ALBUM_CHAT_STAGE_OFFLINE;
}

static size_t chat_utf8_lead_size(uint8_t byte)
{
    if ((byte & 0x80U) == 0U) {
        return 1U;
    }
    if ((byte & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((byte & 0xF0U) == 0xE0U) {
        return 3U;
    }
    if ((byte & 0xF8U) == 0xF0U) {
        return 4U;
    }
    return 1U;
}

static size_t chat_complete_utf8_length(const char *text, size_t length)
{
    size_t lead = length;
    size_t expected;

    while (lead > 0U && ((uint8_t)text[lead - 1U] & 0xC0U) == 0x80U) {
        --lead;
    }
    if (lead == length) {
        if (length > 0U &&
            chat_utf8_lead_size((uint8_t)text[length - 1U]) > 1U) {
            return length - 1U;
        }
        return length;
    }
    if (lead == 0U) {
        return 0U;
    }
    --lead;
    expected = chat_utf8_lead_size((uint8_t)text[lead]);
    return length - lead < expected ? lead : length;
}

static void chat_copy_text(char *destination, size_t capacity,
                           const char *text, size_t text_length)
{
    size_t length;

    if (capacity == 0U) {
        return;
    }
    if (text == NULL || text_length == 0U) {
        destination[0] = '\0';
        return;
    }
    length = text_length < capacity - 1U ? text_length : capacity - 1U;
    if (length < text_length) {
        length = chat_complete_utf8_length(text, length);
    }
    memcpy(destination, text, length);
    destination[length] = '\0';
}

static void chat_try_speak_greeting(void)
{
    char greeting[AI_ALBUM_CHAT_TEXT_CAPACITY];
    int result;

    os_sched_disable();
    if (!g_chat_runtime.persona_active ||
        !g_chat_runtime.greeting_pending ||
        g_chat_runtime.snapshot.agent_state != BRTC_AGENT_STATE_READY) {
        os_sched_enbale();
        return;
    }
    os_snprintf(greeting, sizeof(greeting), "%s", g_chat_runtime.greeting);
    os_sched_enbale();

    result = brtc_agent_speak_text(greeting);
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_CHAT] persona greeting deferred ret=%d\r\n", result);
        return;
    }
    os_sched_disable();
    g_chat_runtime.greeting_pending = 0U;
    os_sched_enbale();
    os_printf("[AI_CHAT] persona greeting sent\r\n");
}

static ai_album_chat_message_t *chat_message_slot(ai_album_chat_role_t role)
{
    ai_album_chat_snapshot_t *snapshot = &g_chat_runtime.snapshot;
    ai_album_chat_message_t *message;
    int index;

    for (index = (int)snapshot->message_count - 1; index >= 0; --index) {
        message = &snapshot->messages[index];
        if (message->role == role && !message->final) {
            return message;
        }
    }
    if (snapshot->message_count == AI_ALBUM_CHAT_MESSAGE_COUNT) {
        memmove(&snapshot->messages[0], &snapshot->messages[1],
                sizeof(snapshot->messages[0]) *
                    (AI_ALBUM_CHAT_MESSAGE_COUNT - 1U));
        snapshot->message_count--;
    }
    message = &snapshot->messages[snapshot->message_count++];
    memset(message, 0, sizeof(*message));
    message->role = role;
    return message;
}

static void chat_finalize_open_messages(void)
{
    ai_album_chat_snapshot_t *snapshot = &g_chat_runtime.snapshot;
    uint8_t index;

    for (index = 0U; index < snapshot->message_count; ++index) {
        snapshot->messages[index].final = 1U;
    }
}

static void chat_store_message(const brtc_agent_event_t *event,
                               ai_album_chat_role_t role)
{
    ai_album_chat_message_t *message;

    if (event->text == NULL || event->text_len == 0U) {
        return;
    }
    message = chat_message_slot(role);
    chat_copy_text(message->text, sizeof(message->text), event->text,
                   event->text_len);
    message->final = event->final ? 1U : 0U;
}

static void chat_apply_agent_state(brtc_agent_state_t state)
{
    ai_album_chat_snapshot_t *snapshot = &g_chat_runtime.snapshot;

    snapshot->agent_state = state;
    if (state != BRTC_AGENT_STATE_ERROR) {
        snapshot->error_code = 0;
        snapshot->error_text[0] = '\0';
    }
    if (state != BRTC_AGENT_STATE_READY ||
        snapshot->stage == AI_ALBUM_CHAT_STAGE_OFFLINE ||
        snapshot->stage == AI_ALBUM_CHAT_STAGE_CONNECTING ||
        snapshot->stage == AI_ALBUM_CHAT_STAGE_ERROR) {
        snapshot->stage = chat_stage_for_agent(state);
    }
    if (state != BRTC_AGENT_STATE_READY) {
        snapshot->ptt_active = 0U;
        snapshot->speaking = 0U;
        g_chat_runtime.waiting_answer = 0U;
    }
}

void ai_album_chat_runtime_init(void)
{
    if (!g_chat_runtime.initialized) {
        memset(&g_chat_runtime, 0, sizeof(g_chat_runtime));
        g_chat_runtime.initialized = 1U;
    }
    ai_album_chat_runtime_sync_agent_state();
}

void ai_album_chat_runtime_sync_agent_state(void)
{
    brtc_agent_state_t state = brtc_agent_get_state();

    os_sched_disable();
    if (state != g_chat_runtime.snapshot.agent_state) {
        chat_apply_agent_state(state);
        g_chat_runtime.snapshot.sequence++;
    }
    os_sched_enbale();
    chat_try_speak_greeting();
}

static void chat_handle_speaking(const brtc_agent_event_t *event)
{
    ai_album_chat_snapshot_t *snapshot = &g_chat_runtime.snapshot;

    snapshot->speaking = event->active ? 1U : 0U;
    if (snapshot->ptt_active) {
        snapshot->stage = AI_ALBUM_CHAT_STAGE_LISTENING;
    } else if (snapshot->speaking) {
        snapshot->stage = AI_ALBUM_CHAT_STAGE_SPEAKING;
    } else if (g_chat_runtime.waiting_answer) {
        snapshot->stage = AI_ALBUM_CHAT_STAGE_THINKING;
    } else {
        snapshot->stage = chat_stage_for_agent(snapshot->agent_state);
    }
}

void ai_album_chat_runtime_handle_event(const brtc_agent_event_t *event)
{
    ai_album_chat_snapshot_t *snapshot = &g_chat_runtime.snapshot;

    if (event == NULL) {
        return;
    }
    os_sched_disable();
    if (event->type == BRTC_AGENT_EVENT_STATE_CHANGED) {
        chat_apply_agent_state(event->state);
    } else if (event->type == BRTC_AGENT_EVENT_ERROR) {
        snapshot->agent_state = BRTC_AGENT_STATE_ERROR;
        snapshot->stage = AI_ALBUM_CHAT_STAGE_ERROR;
        snapshot->ptt_active = 0U;
        snapshot->speaking = 0U;
        g_chat_runtime.waiting_answer = 0U;
        snapshot->error_code = event->code;
        chat_copy_text(snapshot->error_text, sizeof(snapshot->error_text),
                       event->text, event->text_len);
    } else if (event->type == BRTC_AGENT_EVENT_ASR) {
        chat_store_message(event, AI_ALBUM_CHAT_ROLE_USER);
        if (event->final) {
            snapshot->stage = snapshot->ptt_active ?
                AI_ALBUM_CHAT_STAGE_LISTENING : AI_ALBUM_CHAT_STAGE_THINKING;
            g_chat_runtime.waiting_answer = 1U;
        }
    } else if (event->type == BRTC_AGENT_EVENT_ANSWER) {
        chat_store_message(event, AI_ALBUM_CHAT_ROLE_ASSISTANT);
        snapshot->stage = snapshot->speaking ?
            AI_ALBUM_CHAT_STAGE_SPEAKING : AI_ALBUM_CHAT_STAGE_THINKING;
        if (event->final) {
            g_chat_runtime.waiting_answer = 0U;
            if (!snapshot->speaking) {
                snapshot->stage = chat_stage_for_agent(snapshot->agent_state);
            }
        }
    } else if (event->type == BRTC_AGENT_EVENT_SPEAKING) {
        chat_handle_speaking(event);
    }
    snapshot->sequence++;
    os_sched_enbale();
}

void ai_album_chat_runtime_get_snapshot(ai_album_chat_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    os_sched_disable();
    *out = g_chat_runtime.snapshot;
    os_sched_enbale();
}

static int chat_change_ptt(uint8_t start)
{
    int result;

    result = start ? brtc_agent_ptt_start() : brtc_agent_ptt_stop();
    os_printf("[AI_CHAT] PTT %s ret=%d\r\n",
              start ? "start" : "stop", result);
    os_sched_disable();
    if (result == BRTC_AGENT_OK) {
        if (start) {
            chat_finalize_open_messages();
        }
        g_chat_runtime.snapshot.ptt_active = start;
        g_chat_runtime.waiting_answer = start ? 0U : 1U;
        g_chat_runtime.snapshot.stage = start ?
            AI_ALBUM_CHAT_STAGE_LISTENING :
            AI_ALBUM_CHAT_STAGE_RECOGNIZING;
        g_chat_runtime.snapshot.error_code = 0;
        g_chat_runtime.snapshot.error_text[0] = '\0';
    } else {
        g_chat_runtime.snapshot.error_code = result;
        g_chat_runtime.snapshot.stage = AI_ALBUM_CHAT_STAGE_ERROR;
        os_snprintf(g_chat_runtime.snapshot.error_text,
                    sizeof(g_chat_runtime.snapshot.error_text),
                    "PTT unavailable (%d)", result);
    }
    g_chat_runtime.snapshot.sequence++;
    os_sched_enbale();
    return result;
}

static uint8_t chat_ptt_is_active(void)
{
    uint8_t active;

    os_sched_disable();
    active = g_chat_runtime.snapshot.ptt_active;
    os_sched_enbale();
    return active;
}

int ai_album_chat_runtime_start_ptt(void)
{
    if (chat_ptt_is_active()) {
        return BRTC_AGENT_OK;
    }
    return chat_change_ptt(1U);
}

int ai_album_chat_runtime_finish_ptt(void)
{
    if (!chat_ptt_is_active()) {
        return BRTC_AGENT_OK;
    }
    return chat_change_ptt(0U);
}

void ai_album_chat_runtime_stop_ptt(void)
{
    uint8_t active;

    os_sched_disable();
    active = g_chat_runtime.snapshot.ptt_active;
    os_sched_enbale();
    if (active) {
        (void)brtc_agent_ptt_stop();
    }
    os_sched_disable();
    g_chat_runtime.snapshot.ptt_active = 0U;
    g_chat_runtime.snapshot.speaking = 0U;
    g_chat_runtime.waiting_answer = 0U;
    g_chat_runtime.snapshot.stage =
        chat_stage_for_agent(g_chat_runtime.snapshot.agent_state);
    g_chat_runtime.snapshot.sequence++;
    os_sched_enbale();
}

void ai_album_chat_runtime_leave(void)
{
    int result;

    ai_album_chat_runtime_stop_ptt();
    result = brtc_agent_interrupt();
    os_printf("[AI_CHAT] interrupt on leave ret=%d\r\n", result);
}

void ai_album_chat_runtime_reset_session(uint8_t persona)
{
    ai_album_chat_runtime_stop_ptt();
    os_sched_disable();
    memset(g_chat_runtime.snapshot.messages, 0,
           sizeof(g_chat_runtime.snapshot.messages));
    g_chat_runtime.snapshot.message_count = 0U;
    g_chat_runtime.snapshot.persona = persona;
    g_chat_runtime.waiting_answer = 0U;
    g_chat_runtime.snapshot.stage =
        chat_stage_for_agent(g_chat_runtime.snapshot.agent_state);
    g_chat_runtime.snapshot.sequence++;
    os_sched_enbale();
    os_printf("[AI_CHAT] display persona=%u history reset\r\n",
              (unsigned)persona);
}

int ai_album_chat_runtime_activate_persona(uint8_t persona,
                                           const char *pre_query,
                                           const char *post_query,
                                           const char *greeting)
{
    int result;

    if (!pre_query || !post_query || !greeting || greeting[0] == '\0') {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    (void)brtc_agent_interrupt();
    ai_album_chat_runtime_reset_session(persona);
    result = brtc_agent_set_query_enhancement(pre_query, post_query);

    os_sched_disable();
    g_chat_runtime.persona_active = result == BRTC_AGENT_OK ? 1U : 0U;
    g_chat_runtime.greeting_pending = g_chat_runtime.persona_active;
    chat_copy_text(g_chat_runtime.greeting, sizeof(g_chat_runtime.greeting),
                   greeting, os_strlen(greeting));
    os_sched_enbale();
    if (result != BRTC_AGENT_OK) {
        (void)brtc_agent_clear_query_enhancement();
        os_printf("[AI_CHAT] persona=%u prompt failed ret=%d\r\n",
                  (unsigned)persona, result);
        return result;
    }
    os_printf("[AI_CHAT] persona=%u prompt applied\r\n", (unsigned)persona);
    chat_try_speak_greeting();
    return BRTC_AGENT_OK;
}

void ai_album_chat_runtime_clear_persona(void)
{
    uint8_t was_active;
    int result;

    os_sched_disable();
    was_active = g_chat_runtime.persona_active;
    g_chat_runtime.persona_active = 0U;
    g_chat_runtime.greeting_pending = 0U;
    g_chat_runtime.greeting[0] = '\0';
    os_sched_enbale();
    if (!was_active) {
        return;
    }
    result = brtc_agent_clear_query_enhancement();
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_CHAT] clear persona prompt failed ret=%d\r\n", result);
    }
}

int ai_album_chat_runtime_set_translation(const char *source_language,
                                          const char *target_language)
{
    return brtc_agent_set_translation_languages(source_language,
                                                target_language);
}

void ai_album_chat_runtime_clear_translation(void)
{
    int result = brtc_agent_clear_translation();

    if (result != BRTC_AGENT_OK && result != BRTC_AGENT_ERR_INVALID_STATE) {
        os_printf("[AI_CHAT] clear translation prompt failed: %d\r\n", result);
    }
}
