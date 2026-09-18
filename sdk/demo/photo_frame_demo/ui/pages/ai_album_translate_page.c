#include "ui/pages/ai_album_translate_page.h"
#include "audio/ai_album_volume.h"
#include "basic_include.h"
#include "ui/ai_album_chat_runtime.h"
#include "ui/ai_album_font_manager.h"
#include "ui/ai_album_ui_common.h"
#define TRANSLATE_FOCUS_COUNT 2U
#define TRANSLATE_LANGUAGE_COUNT 26U
#define TRANSLATE_TEXT_CAPACITY 192U
enum {
    TRANSLATE_FOCUS_SOURCE = 0,
    TRANSLATE_FOCUS_TARGET,
};
typedef struct {
    const char *native_name;
    const char *english_name;
    const char *locale;
} translate_language_t;
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *focusables[TRANSLATE_FOCUS_COUNT];
    lv_obj_t *source_language_label;
    lv_obj_t *target_language_label;
    lv_obj_t *pair_label;
    lv_obj_t *status_label;
    lv_obj_t *source_text_label;
    lv_obj_t *target_text_label;
    lv_obj_t *mic_panel;
    lv_obj_t *mic_label;
    lv_timer_t *runtime_timer;
    uint32_t observed_sequence;
    uint64_t talk_pressed_at;
    uint8_t focus;
    uint8_t active;
} translate_page_state_t;
typedef struct {
    uint8_t source_language;
    uint8_t target_language;
    uint8_t recording;
    uint8_t has_source_text;
    uint8_t has_target_text;
    char source_text[TRANSLATE_TEXT_CAPACITY];
    char target_text[TRANSLATE_TEXT_CAPACITY];
} translate_model_t;

static const translate_language_t g_languages[TRANSLATE_LANGUAGE_COUNT] = {
    {"中文", "Chinese", "zh-CN"},
    {"英文", "English", "en-US"},
    {"日文", "Japanese", "ja-JP"},
    {"韩文", "Korean", "ko-KR"},
    {"法文", "French", "fr-FR"},
    {"西班牙文", "Spanish", "es-ES"},
    {"葡萄牙文", "Portuguese", "pt-PT"},
    {"印尼文", "Indonesian", "id-ID"},
    {"俄文", "Russian", "ru-RU"},
    {"马来文", "Malay", "ms-MY"},
    {"德文", "German", "de-DE"},
    {"菲律宾文", "Filipino", "fil-PH"},
    {"泰文", "Thai", "th-TH"},
    {"阿拉伯文", "Arabic", "ar-SA"},
    {"越南文", "Vietnamese", "vi-VN"},
    {"丹麦文", "Danish", "da-DK"},
    {"希腊文", "Greek", "el-GR"},
    {"芬兰文", "Finnish", "fi-FI"},
    {"希伯来文", "Hebrew", "he-IL"},
    {"印地文", "Hindi", "hi-IN"},
    {"意大利文", "Italian", "it-IT"},
    {"荷兰文", "Dutch", "nl-NL"},
    {"挪威文", "Norwegian", "nb-NO"},
    {"波兰文", "Polish", "pl-PL"},
    {"瑞典文", "Swedish", "sv-SE"},
    {"土耳其文", "Turkish", "tr-TR"},
};

static translate_page_state_t g_page;
static translate_model_t g_translate = {
    .source_language = 0U,
    .target_language = 1U,
};
static void translate_start_voice(void);

static const translate_language_t *translate_language(uint8_t index)
{
    if (index >= TRANSLATE_LANGUAGE_COUNT) {
        return &g_languages[0];
    }
    return &g_languages[index];
}

static void translate_set_status(const char *text, uint32_t color)
{
    if (g_page.status_label == NULL || text == NULL) {
        return;
    }
    ai_album_ui_common_set_label_text(g_page.status_label, text);
    lv_obj_set_style_text_color(g_page.status_label, lv_color_hex(color),
                                LV_PART_MAIN);
}

static void translate_update_focus(void)
{
    uint8_t i;

    for (i = 0U; i < TRANSLATE_FOCUS_COUNT; ++i) {
        ai_album_ui_common_focus(g_page.focusables[i], i == g_page.focus,
                                 AI_ALBUM_UI_COLOR_BLUE);
    }
    if (g_page.mic_panel != NULL) {
        ai_album_ui_common_focus(g_page.mic_panel, g_translate.recording != 0U,
                                 AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void translate_format_language(char *buffer, size_t capacity,
                                      uint8_t index)
{
    const translate_language_t *language = translate_language(index);

    os_snprintf(buffer, capacity, "%s  /  %s", language->native_name,
                language->english_name);
}

static void translate_update_language_ui(void)
{
    char source_title[64];
    char target_title[64];
    char pair_title[96];
    const translate_language_t *source;
    const translate_language_t *target;

    source = translate_language(g_translate.source_language);
    target = translate_language(g_translate.target_language);
    translate_format_language(source_title, sizeof(source_title),
                              g_translate.source_language);
    translate_format_language(target_title, sizeof(target_title),
                              g_translate.target_language);
    os_snprintf(pair_title, sizeof(pair_title), "%s  ->  %s",
                source->english_name, target->english_name);
    if (g_page.source_language_label != NULL) {
        lv_label_set_text(g_page.source_language_label, source_title);
    }
    if (g_page.target_language_label != NULL) {
        lv_label_set_text(g_page.target_language_label, target_title);
    }
    if (g_page.pair_label != NULL) {
        lv_label_set_text(g_page.pair_label, pair_title);
    }
}

static void translate_delete_runtime_timer(void)
{
    if (g_page.runtime_timer != NULL) {
        lv_timer_delete(g_page.runtime_timer);
        g_page.runtime_timer = NULL;
    }
}

static void translate_configure_runtime(void)
{
    const translate_language_t *source;
    const translate_language_t *target;
    int result;

    source = translate_language(g_translate.source_language);
    target = translate_language(g_translate.target_language);
    result = ai_album_chat_runtime_set_translation(source->native_name,
                                                    target->native_name);
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_TRANSLATE] configure prompt failed: %d\r\n", result);
    }
}

static void translate_reset_result(void)
{
    g_translate.recording = 0U;
    g_translate.has_source_text = 0U;
    g_translate.has_target_text = 0U;
    g_translate.source_text[0] = '\0';
    g_translate.target_text[0] = '\0';
    if (g_page.source_text_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.source_text_label, "Hold M to speak");
    }
    if (g_page.target_text_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.target_text_label, "Baidu translation appears here");
    }
    if (g_page.mic_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.mic_label, "HOLD M TO SPEAK");
    }
    translate_set_status("READY  >  HOLD M TO SPEAK", AI_ALBUM_UI_COLOR_MUTED);
    translate_update_focus();
}

static const ai_album_chat_message_t *translate_latest_message(
    const ai_album_chat_snapshot_t *snapshot, ai_album_chat_role_t role)
{
    int index;

    if (snapshot == NULL) {
        return NULL;
    }
    for (index = (int)snapshot->message_count - 1; index >= 0; --index) {
        if (snapshot->messages[index].role == role &&
            snapshot->messages[index].text[0] != '\0') {
            return &snapshot->messages[index];
        }
    }
    return NULL;
}

static void translate_render_chat_snapshot(
    const ai_album_chat_snapshot_t *snapshot)
{
    const ai_album_chat_message_t *source;
    const ai_album_chat_message_t *target;
    const char *status;
    uint32_t color;

    if (snapshot == NULL || g_page.screen == NULL) {
        return;
    }
    source = translate_latest_message(snapshot, AI_ALBUM_CHAT_ROLE_USER);
    target = translate_latest_message(snapshot, AI_ALBUM_CHAT_ROLE_ASSISTANT);
    if (source != NULL) {
        os_snprintf(g_translate.source_text, sizeof(g_translate.source_text),
                    "%s", source->text);
        g_translate.has_source_text = 1U;
        (void)ai_album_font_set_dynamic_text(
            g_page.source_text_label,
            translate_language(g_translate.source_language)->locale,
            g_translate.source_text);
    }
    if (target != NULL) {
        os_snprintf(g_translate.target_text, sizeof(g_translate.target_text),
                    "%s", target->text);
        g_translate.has_target_text = 1U;
        (void)ai_album_font_set_dynamic_text(
            g_page.target_text_label,
            translate_language(g_translate.target_language)->locale,
            g_translate.target_text);
    }
    g_translate.recording = snapshot->ptt_active ? 1U : 0U;
    if (snapshot->error_text[0] != '\0') {
        status = snapshot->error_text;
        color = AI_ALBUM_UI_COLOR_RED;
    } else {
        status = "READY  >  HOLD M TO SPEAK";
        color = AI_ALBUM_UI_COLOR_MUTED;
        switch (snapshot->stage) {
        case AI_ALBUM_CHAT_STAGE_CONNECTING:
            status = "BAIDU  >  CONNECTING";
            color = AI_ALBUM_UI_COLOR_BLUE;
            break;
        case AI_ALBUM_CHAT_STAGE_LISTENING:
            status = "LISTENING  >  RELEASE M TO SEND";
            color = AI_ALBUM_UI_COLOR_GREEN;
            break;
        case AI_ALBUM_CHAT_STAGE_RECOGNIZING:
            status = "RELEASED  >  BAIDU ASR";
            color = AI_ALBUM_UI_COLOR_ORANGE;
            break;
        case AI_ALBUM_CHAT_STAGE_THINKING:
            status = "RECOGNIZED  >  BAIDU TRANSLATING";
            color = AI_ALBUM_UI_COLOR_ORANGE;
            break;
        case AI_ALBUM_CHAT_STAGE_SPEAKING:
            status = "TRANSLATION  >  PLAYING TARGET";
            color = AI_ALBUM_UI_COLOR_GREEN;
            break;
        case AI_ALBUM_CHAT_STAGE_ERROR:
            status = "BAIDU  >  ERROR";
            color = AI_ALBUM_UI_COLOR_RED;
            break;
        case AI_ALBUM_CHAT_STAGE_READY:
            if (g_translate.has_target_text) {
                status = "READY  >  TRANSLATION COMPLETE";
                color = AI_ALBUM_UI_COLOR_GREEN;
            }
            break;
        case AI_ALBUM_CHAT_STAGE_OFFLINE:
        default:
            status = "BAIDU  >  OFFLINE";
            color = AI_ALBUM_UI_COLOR_RED;
            break;
        }
    }
    if (g_page.mic_label != NULL) {
        if (snapshot->ptt_active) {
            ai_album_ui_common_set_label_text(
                g_page.mic_label, "RELEASE M TO SEND");
        } else if (snapshot->stage == AI_ALBUM_CHAT_STAGE_RECOGNIZING ||
                   snapshot->stage == AI_ALBUM_CHAT_STAGE_THINKING) {
            ai_album_ui_common_set_label_text(
                g_page.mic_label, "WAITING FOR BAIDU");
        } else {
            ai_album_ui_common_set_label_text(
                g_page.mic_label, "HOLD M TO SPEAK");
        }
    }
    translate_set_status(status, color);
    translate_update_focus();
}

static void translate_poll_runtime(lv_timer_t *timer)
{
    ai_album_chat_snapshot_t snapshot;

    (void)timer;
    if (g_page.screen == NULL || !g_page.active) {
        return;
    }
    ai_album_chat_runtime_sync_agent_state();
    ai_album_chat_runtime_get_snapshot(&snapshot);
    if (snapshot.sequence == g_page.observed_sequence) {
        return;
    }
    g_page.observed_sequence = snapshot.sequence;
    translate_render_chat_snapshot(&snapshot);
}

static void translate_render_current_snapshot(void)
{
    ai_album_chat_snapshot_t snapshot;

    if (g_page.screen == NULL) {
        return;
    }
    ai_album_chat_runtime_sync_agent_state();
    ai_album_chat_runtime_get_snapshot(&snapshot);
    g_page.observed_sequence = snapshot.sequence;
    translate_render_chat_snapshot(&snapshot);
}

static void translate_prepare_runtime_session(void)
{
    ai_album_chat_snapshot_t snapshot;

    ai_album_chat_runtime_init();
    ai_album_chat_runtime_leave();
    ai_album_chat_runtime_reset_session(0U);
    translate_configure_runtime();
    ai_album_chat_runtime_get_snapshot(&snapshot);
    g_page.observed_sequence = snapshot.sequence;
}

static void translate_stop_runtime(void)
{
    ai_album_chat_runtime_leave();
    ai_album_chat_runtime_clear_translation();
    g_translate.recording = 0U;
    g_page.talk_pressed_at = 0U;
    g_page.active = 0U;
    if (g_page.runtime_timer != NULL) {
        lv_timer_pause(g_page.runtime_timer);
    }
}

static lv_obj_t *translate_create_language_selector(
    int32_t x, const char *caption, lv_obj_t **language_label)
{
    lv_obj_t *selector = ai_album_ui_common_panel(
        g_page.screen, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_t *caption_label;
    lv_obj_t *hint_label;

    lv_obj_set_pos(selector, x, 112);
    lv_obj_set_size(selector, 380, 62);
    caption_label = ai_album_ui_common_label(
        selector, caption, &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(caption_label, 18, 7);
    *language_label = ai_album_ui_common_label(
        selector, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(*language_label, 18, 29);
    hint_label = ai_album_ui_common_label(
        selector, "L/R", &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_align(hint_label, LV_ALIGN_RIGHT_MID, -16, 9);
    return selector;
}

static lv_obj_t *translate_create_text_card(int32_t x, const char *title,
                                             const char *role,
                                             lv_obj_t **content)
{
    lv_obj_t *card = ai_album_ui_common_panel(
        g_page.screen, AI_ALBUM_UI_COLOR_WHITE, 18);
    lv_obj_t *title_label;
    lv_obj_t *role_label;

    lv_obj_set_pos(card, x, 202);
    lv_obj_set_size(card, 445, 224);
    title_label = ai_album_ui_common_label(
        card, title, &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(title_label, 22, 16);
    role_label = ai_album_ui_common_label(
        card, role, &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_width(role_label, 210);
    lv_obj_set_style_text_align(role_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(role_label, LV_ALIGN_TOP_RIGHT, -22, 16);
    *content = ai_album_ui_common_label(
        card, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(*content, 22, 62);
    lv_obj_set_width(*content, 400);
    lv_obj_set_height(*content, 135);
    lv_label_set_long_mode(*content, LV_LABEL_LONG_WRAP);
    return card;
}

static void translate_create_header(void)
{
    lv_obj_t *title = ai_album_ui_common_label(
        g_page.screen, "LIVE TRANSLATE", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);

    lv_obj_set_pos(title, 40, 70);
    g_page.pair_label = ai_album_ui_common_label(
        g_page.screen, "", &lv_font_montserrat_16,
        AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(g_page.pair_label, 40, 94);
    g_page.status_label = ai_album_ui_common_label(
        g_page.screen, "READY", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_width(g_page.status_label, 440);
    lv_obj_align(g_page.status_label, LV_ALIGN_TOP_RIGHT, -40, 92);
    lv_obj_set_style_text_align(g_page.status_label, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);
}

static void translate_create_controls(void)
{
    g_page.focusables[TRANSLATE_FOCUS_SOURCE] =
        translate_create_language_selector(40, "SOURCE LANGUAGE",
                                            &g_page.source_language_label);
    g_page.focusables[TRANSLATE_FOCUS_TARGET] =
        translate_create_language_selector(604, "TARGET LANGUAGE",
                                           &g_page.target_language_label);
    g_page.mic_panel = ai_album_ui_common_button(
        g_page.screen, "VOICE INPUT", "HOLD M TO SPEAK");
    lv_obj_set_pos(g_page.mic_panel, 40, 455);
    lv_obj_set_size(g_page.mic_panel, 390, 58);
    lv_obj_set_pos(lv_obj_get_child(g_page.mic_panel, 0), 18, 20);
    g_page.mic_label = lv_obj_get_child(g_page.mic_panel, 1);
    lv_obj_set_pos(g_page.mic_label, 140, 21);
    lv_obj_set_width(g_page.mic_label, 230);
}

static void translate_start_voice(void)
{
    ai_album_chat_snapshot_t snapshot;
    int result;

    if (g_translate.recording) {
        return;
    }
    translate_prepare_runtime_session();
    g_translate.has_source_text = 0U;
    g_translate.has_target_text = 0U;
    g_translate.source_text[0] = '\0';
    g_translate.target_text[0] = '\0';
    result = ai_album_chat_runtime_start_ptt();
    if (result != BRTC_AGENT_OK) {
        ai_album_chat_runtime_get_snapshot(&snapshot);
        g_page.observed_sequence = snapshot.sequence;
        translate_render_chat_snapshot(&snapshot);
        return;
    }
    g_translate.recording = 1U;
    if (g_page.source_text_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.source_text_label, "Listening...\nSpeak now");
    }
    if (g_page.target_text_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.target_text_label, "Waiting for Baidu result...");
    }
    if (g_page.mic_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.mic_label, "RELEASE M TO SEND");
    }
    translate_set_status("LISTENING  >  RELEASE M TO FINISH",
                        AI_ALBUM_UI_COLOR_BLUE);
    translate_update_focus();
}

static void translate_finish_voice(void)
{
    ai_album_chat_snapshot_t snapshot;
    int result;

    if (!g_translate.recording) {
        return;
    }
    result = ai_album_chat_runtime_finish_ptt();
    g_translate.recording = 0U;
    if (result != BRTC_AGENT_OK) {
        ai_album_chat_runtime_get_snapshot(&snapshot);
        g_page.observed_sequence = snapshot.sequence;
        translate_render_chat_snapshot(&snapshot);
        return;
    }
    if (!g_translate.has_source_text) {
        ai_album_ui_common_set_label_text(
            g_page.source_text_label, "Recognizing source text...");
    }
    ai_album_ui_common_set_label_text(
        g_page.target_text_label, "Waiting for Baidu translation...");
    if (g_page.mic_label != NULL) {
        ai_album_ui_common_set_label_text(
            g_page.mic_label, "WAITING FOR BAIDU");
    }
    translate_set_status("RELEASED  >  BAIDU ASR",
                        AI_ALBUM_UI_COLOR_ORANGE);
    translate_poll_runtime(NULL);
}

static void translate_change_language(uint8_t source, int8_t delta)
{
    uint8_t *selected = source ? &g_translate.source_language
                               : &g_translate.target_language;
    uint8_t other = source ? g_translate.target_language
                           : g_translate.source_language;
    int16_t next = *selected;

    do {
        next = (next + TRANSLATE_LANGUAGE_COUNT + delta) %
               TRANSLATE_LANGUAGE_COUNT;
    } while ((uint8_t)next == other);
    *selected = (uint8_t)next;
    translate_prepare_runtime_session();
    translate_update_language_ui();
    translate_reset_result();
    translate_set_status(source ? "SOURCE LANGUAGE SELECTED"
                                : "TARGET LANGUAGE SELECTED",
                        AI_ALBUM_UI_COLOR_BLUE);
}

int ai_album_translate_page_create(lv_display_t *display)
{
    lv_obj_t *arrow;

    if (display == NULL || g_page.screen != NULL) {
        return RET_ERR;
    }
    if (g_translate.source_language >= TRANSLATE_LANGUAGE_COUNT ||
        g_translate.target_language >= TRANSLATE_LANGUAGE_COUNT ||
        g_translate.source_language == g_translate.target_language) {
        g_translate.source_language = 0U;
        g_translate.target_language = 1U;
    }
    memset(&g_page, 0, sizeof(g_page));
    g_page.screen = ai_album_ui_common_prepare(
        display, "TRANSLATE", 0xEDF3F8U);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    translate_create_header();
    translate_create_controls();
    translate_create_text_card(40, "SOURCE TEXT", "VOICE INPUT",
                               &g_page.source_text_label);
    translate_create_text_card(539, "TRANSLATION", "TARGET LANGUAGE",
                               &g_page.target_text_label);
    arrow = ai_album_ui_common_label(
        g_page.screen, "->", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(arrow, 486, 292);
    ai_album_ui_common_footer(
        g_page.screen, "POWER BACK   U/D VOLUME   L/R LANGUAGE   OK SOURCE/TARGET");
    translate_prepare_runtime_session();
    g_page.runtime_timer = lv_timer_create(translate_poll_runtime, 150U, NULL);
    if (g_page.runtime_timer == NULL) {
        ai_album_translate_page_destroy();
        return RET_ERR;
    }
    lv_timer_pause(g_page.runtime_timer);
    translate_update_language_ui();
    translate_reset_result();
    translate_render_current_snapshot();
    translate_update_focus();
    return RET_OK;
}

int ai_album_translate_page_show(void)
{
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    translate_prepare_runtime_session();
    g_page.focus = TRANSLATE_FOCUS_SOURCE;
    translate_reset_result();
    g_page.active = 1U;
    translate_render_current_snapshot();
    if (g_page.runtime_timer != NULL) {
        lv_timer_resume(g_page.runtime_timer);
    }
    lv_screen_load(g_page.screen);
    return RET_OK;
}

void ai_album_translate_page_destroy(void)
{
    translate_stop_runtime();
    translate_delete_runtime_timer();
    if (g_page.screen != NULL) {
        lv_obj_delete(g_page.screen);
    }
    memset(&g_page, 0, sizeof(g_page));
    g_translate.recording = 0U;
}

static void translate_activate(void)
{
    if (g_page.focus == TRANSLATE_FOCUS_SOURCE) {
        g_page.focus = TRANSLATE_FOCUS_TARGET;
        translate_set_status("SOURCE CONFIRMED  >  SELECT TARGET",
                             AI_ALBUM_UI_COLOR_BLUE);
    } else {
        g_page.focus = TRANSLATE_FOCUS_SOURCE;
        translate_set_status("TARGET CONFIRMED  >  SELECT SOURCE",
                             AI_ALBUM_UI_COLOR_BLUE);
    }
    translate_update_focus();
}

ai_album_ui_route_t ai_album_translate_page_handle_action(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        translate_stop_runtime();
        ai_album_chat_runtime_reset_session(0U);
        translate_reset_result();
        return AI_ALBUM_UI_ROUTE_HOME;
    }
    if (action == AI_ALBUM_UI_ACTION_MENU) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_TALK_PRESS) {
        g_page.talk_pressed_at = os_mseconds();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_TALK_RELEASE) {
        g_page.talk_pressed_at = 0U;
        translate_finish_voice();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_TALK_START) {
        g_page.talk_pressed_at = 0U;
        translate_start_voice();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_TALK_STOP) {
        g_page.talk_pressed_at = 0U;
        translate_finish_voice();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_UP) {
        (void)ai_album_volume_adjust(1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_DOWN) {
        (void)ai_album_volume_adjust(-1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_LEFT ||
               action == AI_ALBUM_UI_ACTION_RIGHT) {
        translate_change_language(
            g_page.focus == TRANSLATE_FOCUS_SOURCE,
            action == AI_ALBUM_UI_ACTION_LEFT ? -1 : 1);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        translate_activate();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
