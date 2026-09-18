#include "brtc_agent_internal.h"

_Static_assert(sizeof(ASRExtInfo) == 140U, "ASRExtInfo ABI size mismatch");
_Static_assert(offsetof(ASRExtInfo, emotion) == 68U,
               "ASRExtInfo emotion ABI offset mismatch");
_Static_assert(offsetof(ASRExtInfo, sessionId) == 132U,
               "ASRExtInfo sessionId ABI offset mismatch");
_Static_assert(sizeof(BaiduChatAgentEvent) == 68U,
               "BaiduChatAgentEvent ABI size mismatch");
_Static_assert(offsetof(BaiduChatAgentEvent, onMediaGenerateAck) == 60U,
               "BaiduChatAgentEvent media ack ABI offset mismatch");
_Static_assert(offsetof(BaiduChatAgentEvent, onAgentEventUpdated) == 64U,
               "BaiduChatAgentEvent agent event ABI offset mismatch");

static const char *brtc_agent_skip_prefixes(const char *text)
{
    const char *cursor = text;

    if (!cursor) {
        return NULL;
    }
    for (;;) {
        if (!os_strncmp(cursor, "[Q]:", 4U) ||
            !os_strncmp(cursor, "[A]:", 4U) ||
            !os_strncmp(cursor, "[M]:", 4U) ||
            !os_strncmp(cursor, "[C]:", 4U)) {
            cursor += 4;
            continue;
        }
        break;
    }
    return cursor;
}

static size_t brtc_agent_subtitle_length(const char *text, size_t text_len)
{
    size_t i;

    if (!text) {
        return 0U;
    }
    for (i = 0U; i + 2U < text_len; ++i) {
        if (text[i] == '|' && text[i + 1U] == '|' && text[i + 2U] == '|') {
            return i;
        }
    }
    return text_len;
}

static size_t brtc_agent_bounded_length(const char *text, size_t capacity)
{
    size_t length = 0U;

    while (text && length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static void brtc_agent_on_error(int error_code, const char *message)
{
    os_printf("[BRTC_AGENT] SDK error %d: %s\r\n", error_code,
              message ? message : "");
    brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR, error_code, message);
}

static void brtc_agent_on_call_state(AGentCallState state)
{
    os_printf("[BRTC_AGENT] call state=%d\r\n", state);
    if (state == AGENT_CALL_FAIL || state == AGENT_LOGIN_FAIL) {
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR, (int)state,
                                      "baidu_call_failed");
    }
}

static void brtc_agent_on_connection_state(AGentConnectState state)
{
    os_printf("[BRTC_AGENT] connection state=%d\r\n", state);
    if (state == AGENT_CONNECTION_STATE_DISCONNECTED &&
        brtc_agent_get_state() != BRTC_AGENT_STATE_STOPPING) {
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR, (int)state,
                                      "baidu_connection_disconnected");
    }
}

static void brtc_agent_emit_asr_emotion(const ASRExtInfo *info, bool final)
{
    brtc_agent_event_t event;
    size_t emotion_len;

    if (!info) {
        return;
    }
    emotion_len = brtc_agent_bounded_length(info->emotion,
                                             sizeof(info->emotion));
    if (emotion_len == 0U) {
        return;
    }

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_EMOTION;
    event.final = final;
    event.text = info->emotion;
    event.text_len = emotion_len;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_asr(const char *text, size_t text_len, bool final,
                              const ASRExtInfo *info)
{
    brtc_agent_event_t event;
    const char *payload = brtc_agent_skip_prefixes(text);
    size_t prefix_len = (payload && text) ? (size_t)(payload - text) : 0U;
    size_t payload_len = (text_len >= prefix_len) ? text_len - prefix_len : 0U;
    size_t subtitle_len = brtc_agent_subtitle_length(payload, payload_len);

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_ASR;
    event.final = final;
    event.text = payload;
    event.text_len = subtitle_len;
    brtc_agent_internal_emit(&event);
    brtc_agent_emit_asr_emotion(info, final);
}

static void brtc_agent_on_answer(const char *text, bool final)
{
    brtc_agent_event_t event;
    const char *payload = brtc_agent_skip_prefixes(text);
    size_t payload_len = payload ? os_strlen(payload) : 0U;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_ANSWER;
    event.final = final;
    event.text = payload;
    event.text_len = brtc_agent_subtitle_length(payload, payload_len);
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_speaking(bool speaking)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_SPEAKING;
    event.active = speaking;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_function_call(const char *id, const char *params)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_FUNCTION_CALL;
    event.text = params ? params : id;
    event.text_len = event.text ? os_strlen(event.text) : 0U;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_audio_player(const char *path, bool start)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_AGENT_RAW;
    event.active = start;
    event.text = path;
    event.text_len = path ? os_strlen(path) : 0U;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_media_setup(void)
{
    if (brtc_agent_get_state() == BRTC_AGENT_STATE_CONNECTING) {
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_READY, 0, NULL);
        os_printf("[BRTC_AGENT] media setup ready\r\n");
    }
    /* The vendor example applies ENHANCE_QUERY only after media setup.  At
     * this point the signaling channel is ready to carry the control event;
     * sending it immediately after engine_call can silently drop it. */
    brtc_agent_internal_apply_query_enhancement();
}

static void brtc_agent_on_audio(const uint8_t *data, size_t len)
{
    brtc_agent_audio_play_pcm(data, len);
}

static void brtc_agent_on_license(bool valid)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_LICENSE;
    event.active = valid;
    event.code = valid ? 0 : BRTC_AGENT_ERR_ENGINE;
    brtc_agent_internal_emit(&event);
    if (!valid) {
        brtc_agent_internal_set_state(BRTC_AGENT_STATE_ERROR,
                                      BRTC_AGENT_ERR_ENGINE,
                                      "baidu_license_rejected");
    }
}

static void brtc_agent_on_agent_event(const char *message, size_t len)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_AGENT_RAW;
    event.text = message;
    event.text_len = len;
    brtc_agent_internal_emit(&event);
}

static brtc_agent_media_type_t brtc_agent_media_type(RtcImageType type)
{
    if (type >= RTC_IMAGE_TYPE_JPEG && type <= RTC_IMAGE_TYPE_VP8) {
        return (brtc_agent_media_type_t)type;
    }
    return BRTC_AGENT_MEDIA_UNKNOWN;
}

static void brtc_agent_on_video_data(const uint8_t *data, size_t len,
                                     RtcImageType type, int width, int height)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_VIDEO_DATA;
    event.data = data;
    event.data_len = len;
    event.media_type = brtc_agent_media_type(type);
    event.media_width = width;
    event.media_height = height;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_media_generate_result(const char *result)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_MEDIA_GENERATE_RESULT;
    event.text = result;
    event.text_len = result ? os_strlen(result) : 0U;
    brtc_agent_internal_emit(&event);
}

static void brtc_agent_on_media_generate_ack(const char *result)
{
    brtc_agent_event_t event;

    os_memset(&event, 0, sizeof(event));
    event.type = BRTC_AGENT_EVENT_MEDIA_GENERATE_ACK;
    event.text = result;
    event.text_len = result ? os_strlen(result) : 0U;
    brtc_agent_internal_emit(&event);
}

void brtc_agent_event_table_init(BaiduChatAgentEvent *events)
{
    os_memset(events, 0, sizeof(*events));
    events->onError = brtc_agent_on_error;
    events->onCallStateChange = brtc_agent_on_call_state;
    events->onConnectionStateChange = brtc_agent_on_connection_state;
    events->onUserAsrSubtitle = brtc_agent_on_asr;
    events->onAIAgentSubtitle = brtc_agent_on_answer;
    events->onAIAgentSpeaking = brtc_agent_on_speaking;
    events->onFunctionCall = brtc_agent_on_function_call;
    events->onAudioPlayerOp = brtc_agent_on_audio_player;
    events->onMediaSetup = brtc_agent_on_media_setup;
    events->onAudioData = brtc_agent_on_audio;
    events->onVideoData = brtc_agent_on_video_data;
    events->onLicenseResult = brtc_agent_on_license;
    events->onMediaGenerateResult = brtc_agent_on_media_generate_result;
    events->onMediaGenerateAck = brtc_agent_on_media_generate_ack;
    events->onAgentEventUpdated = brtc_agent_on_agent_event;
}
