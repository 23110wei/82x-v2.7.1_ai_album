#include "ui/pages/ai_album_practice_page.h"
#include "audio/ai_album_volume.h"
#include "basic_include.h"
#include "brtc_agent/brtc_agent.h"
#include "ui/ai_album_font_manager.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_practice_runtime.h"
#include "ui/pages/ai_album_practice_scene_cards.h"

#include "ui/ai_album_ui_common.h"
#include <string.h>
#define PRACTICE_HISTORY_CAPACITY 1800U
typedef enum {
    PRACTICE_MODE_SCENE_SELECT = 0,
    PRACTICE_MODE_SESSION,
} practice_mode_t;

typedef enum {
    PRACTICE_FOCUS_SCENE = 0,
    PRACTICE_FOCUS_LANGUAGE,
} practice_selection_focus_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *selection_view;
    lv_obj_t *session_view;
    ai_album_practice_scene_cards_t scene_cards;
    lv_obj_t *language_panel;
    lv_obj_t *language_label;
    lv_obj_t *selection_title;
    lv_obj_t *selection_description;
    lv_obj_t *selection_sample;
    lv_obj_t *selection_status;
    lv_obj_t *session_title;
    lv_obj_t *session_chinese_title;
    lv_obj_t *partner;
    lv_obj_t *goal;
    lv_obj_t *sample;
    lv_obj_t *turn;
    lv_obj_t *status;
    lv_obj_t *dialogue_panel;
    lv_obj_t *conversation;
    lv_obj_t *talk_panel;
    lv_obj_t *talk_label;
    lv_timer_t *timer;
    uint32_t observed_sequence;
    uint32_t turn_count;
    uint64_t talk_pressed_at;
    uint8_t selected_scene;
    uint8_t selected_language;
    practice_selection_focus_t selection_focus;
    practice_mode_t mode;
    char history[PRACTICE_HISTORY_CAPACITY];
    char turn_text[40];
} practice_page_state_t;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} practice_writer_t;
static practice_page_state_t g_page;
static void practice_set_status(const char *text, uint32_t color)
{
    if (g_page.status == NULL || text == NULL) {
        return;
    }
    ai_album_ui_common_set_label_text(g_page.status, text);
    lv_obj_set_style_text_color(g_page.status, lv_color_hex(color),
                                LV_PART_MAIN);
}
static void practice_writer_append(practice_writer_t *writer,
                                   const char *speaker, const char *text)
{
    int written;
    if (writer->length >= writer->capacity - 1U) {
        return;
    }
    written = os_snprintf(writer->buffer + writer->length,
                          writer->capacity - writer->length,
                          "%s%s\n\n", speaker, text);
    if (written <= 0) {
        return;
    }
    writer->length += (size_t)written;
    if (writer->length >= writer->capacity) {
        writer->length = writer->capacity - 1U;
    }
}

static void practice_format_history(
    const ai_album_chat_snapshot_t *snapshot)
{
    const char *greeting = ai_album_practice_runtime_greeting(
        g_page.selected_scene, g_page.selected_language);
    practice_writer_t writer = {
        .buffer = g_page.history,
        .capacity = sizeof(g_page.history),
    };
    uint8_t index;
    g_page.history[0] = '\0';
    if (snapshot->message_count == 0U) {
        practice_writer_append(&writer, ai_album_i18n_text("AI: "),
                               greeting);
    }
    for (index = 0U; index < snapshot->message_count; ++index) {
        const ai_album_chat_message_t *message = &snapshot->messages[index];
        const char *speaker = ai_album_i18n_text(
            message->role == AI_ALBUM_CHAT_ROLE_USER ? "YOU: " : "AI: ");

        practice_writer_append(&writer, speaker, message->text);
    }
    os_snprintf(g_page.turn_text, sizeof(g_page.turn_text),
                ai_album_i18n_text("TURN: %u"),
                (unsigned)g_page.turn_count);
}

static const char *practice_stage_text(ai_album_chat_stage_t stage)
{
    static const char *const labels[] = {
        "VOICE SERVICE OFFLINE", "CONNECTING TO VOICE SERVICE...",
        "VOICE READY - HOLD M TO SPEAK",
        "LISTENING - RELEASE M TO SEND", "RECOGNIZING VOICE...",
        "PARTNER IS THINKING...", "PARTNER IS SPEAKING...",
        "VOICE SERVICE UNAVAILABLE",
    };

    return stage <= AI_ALBUM_CHAT_STAGE_ERROR ? labels[stage] : "UNKNOWN";
}

static const char *practice_talk_text(
    const ai_album_chat_snapshot_t *snapshot)
{
    if (snapshot->stage == AI_ALBUM_CHAT_STAGE_CONNECTING ||
        snapshot->stage == AI_ALBUM_CHAT_STAGE_OFFLINE) {
        return "WAITING FOR VOICE SERVICE";
    }
    if (snapshot->stage == AI_ALBUM_CHAT_STAGE_ERROR) {
        return "VOICE SERVICE ERROR";
    }
    if (g_page.talk_pressed_at != 0U) {
        return "KEEP HOLDING M";
    }
    if (snapshot->ptt_active) {
        return "RELEASE M TO SEND";
    }
    if (snapshot->stage == AI_ALBUM_CHAT_STAGE_RECOGNIZING ||
        snapshot->stage == AI_ALBUM_CHAT_STAGE_THINKING) {
        return "WAITING FOR AI";
    }
    if (snapshot->stage == AI_ALBUM_CHAT_STAGE_SPEAKING) {
        return "AI SPEAKING";
    }
    return "HOLD M TO SPEAK";
}

static uint32_t practice_stage_color(ai_album_chat_stage_t stage)
{
    if (stage == AI_ALBUM_CHAT_STAGE_ERROR ||
        stage == AI_ALBUM_CHAT_STAGE_OFFLINE) {
        return AI_ALBUM_UI_COLOR_RED;
    }
    if (stage == AI_ALBUM_CHAT_STAGE_LISTENING ||
        stage == AI_ALBUM_CHAT_STAGE_SPEAKING) {
        return AI_ALBUM_UI_COLOR_GREEN;
    }
    return AI_ALBUM_UI_COLOR_ORANGE;
}

static void practice_render(const ai_album_chat_snapshot_t *snapshot)
{
    uint32_t color;
    if (snapshot == NULL || g_page.mode != PRACTICE_MODE_SESSION) {
        return;
    }
    color = practice_stage_color(snapshot->stage);
    practice_format_history(snapshot);
    (void)ai_album_font_set_dynamic_text(
        g_page.conversation,
        ai_album_practice_runtime_language(g_page.selected_language)->brtc_code,
        g_page.history);
    ai_album_ui_common_set_label_raw(g_page.turn, g_page.turn_text);
    practice_set_status(snapshot->error_text[0] != '\0' ?
                        snapshot->error_text :
                        practice_stage_text(snapshot->stage), color);
    ai_album_ui_common_set_label_text(
        g_page.talk_label, practice_talk_text(snapshot));
    ai_album_ui_common_focus(g_page.talk_panel,
                             g_page.talk_pressed_at != 0U ||
                             snapshot->ptt_active, color);
    lv_obj_update_layout(g_page.dialogue_panel);
    lv_obj_scroll_to_y(g_page.dialogue_panel,
                       lv_obj_get_height(g_page.conversation), LV_ANIM_OFF);
}

static void practice_render_current(void)
{
    ai_album_chat_snapshot_t snapshot;
    ai_album_practice_runtime_get_snapshot(&snapshot);
    g_page.observed_sequence = snapshot.sequence;
    practice_render(&snapshot);
}

static void practice_start_voice(void)
{
    ai_album_chat_snapshot_t snapshot;
    int result;
    ai_album_practice_runtime_get_snapshot(&snapshot);
    result = ai_album_practice_runtime_start_ptt();
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_PRACTICE] PTT start failed ret=%d\r\n", result);
    } else if (!snapshot.ptt_active) {
        g_page.turn_count++;
    }
    practice_render_current();
}

static void practice_finish_voice(void)
{
    int result = ai_album_practice_runtime_finish_ptt();
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_PRACTICE] PTT stop failed ret=%d\r\n", result);
    }
    practice_render_current();
}

static void practice_poll(lv_timer_t *timer)
{
    ai_album_chat_snapshot_t snapshot;
    (void)timer;
    if (g_page.screen == NULL || g_page.mode != PRACTICE_MODE_SESSION) {
        return;
    }
    ai_album_practice_runtime_sync();
    ai_album_practice_runtime_get_snapshot(&snapshot);
    if (snapshot.sequence == g_page.observed_sequence) {
        return;
    }
    g_page.observed_sequence = snapshot.sequence;
    practice_render(&snapshot);
}

static void practice_create_language_selector(void)
{
    lv_obj_t *label;
    g_page.language_panel = ai_album_ui_common_panel(
        g_page.selection_view, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_set_pos(g_page.language_panel, 744, 14);
    lv_obj_set_size(g_page.language_panel, 240, 68);
    label = ai_album_ui_common_label(
        g_page.language_panel, "LANGUAGE", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 16, 9);
    g_page.language_label = ai_album_ui_common_label(
        g_page.language_panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.language_label, 16, 34);
}

static void practice_update_selection(void)
{
    const ai_album_practice_scene_t *scene =
        ai_album_practice_runtime_scene(g_page.selected_scene);
    const ai_album_practice_language_t *language =
        ai_album_practice_runtime_language(g_page.selected_language);
    ai_album_ui_common_set_label_text(g_page.selection_title, scene->title);
    ai_album_ui_common_set_label_text(g_page.selection_description,
                                      scene->goal);
    (void)ai_album_font_set_dynamic_text(
        g_page.selection_sample, language->brtc_code,
        ai_album_practice_runtime_sample(g_page.selected_scene,
                                         g_page.selected_language));
    ai_album_ui_common_set_label_text(g_page.language_label,
                                      language->title);
    ai_album_ui_common_set_label_text(
        g_page.selection_status,
        g_page.selection_focus == PRACTICE_FOCUS_SCENE ?
            "SELECTING SCENE: L/R SCENE, M LANGUAGE" :
            "SELECTING LANGUAGE: L/R LANGUAGE, BACK TO SCENE");
    lv_obj_set_style_text_color(g_page.selection_status,
                                lv_color_hex(AI_ALBUM_UI_COLOR_ORANGE),
                                LV_PART_MAIN);
    ai_album_practice_scene_cards_update(
        &g_page.scene_cards, g_page.selected_scene,
        g_page.selection_focus == PRACTICE_FOCUS_SCENE);
    ai_album_ui_common_focus(
        g_page.language_panel,
        g_page.selection_focus == PRACTICE_FOCUS_LANGUAGE,
        AI_ALBUM_UI_COLOR_ORANGE);
}
static void practice_prepare_selected_language(void)
{
    const ai_album_practice_language_t *language =
        ai_album_practice_runtime_language(g_page.selected_language);
    char status[160];
    const char *language_name;
    uint32_t color;
    int result;
    if (language == NULL || g_page.selection_status == NULL) {
        return;
    }
    result = ai_album_practice_runtime_prepare_language(
        g_page.selected_language);
    language_name = ai_album_i18n_text(language->title);
    if (result == BRTC_AGENT_IN_PROGRESS) {
        os_snprintf(status, sizeof(status),
                    ai_album_i18n_text(
                        "PREPARING %s VOICE... SELECT OR OK TO ENTER"),
                    language_name);
        color = AI_ALBUM_UI_COLOR_ORANGE;
    } else if (result == BRTC_AGENT_OK) {
        os_snprintf(status, sizeof(status),
                    ai_album_i18n_text(
                        "%s VOICE LANGUAGE READY - OK TO START"),
                    language_name);
        color = AI_ALBUM_UI_COLOR_ORANGE;
    } else {
        os_snprintf(
            status, sizeof(status), "%s",
            ai_album_i18n_text(
                "VOICE TEMPORARILY UNAVAILABLE - RETRY IN SESSION"));
        color = AI_ALBUM_UI_COLOR_RED;
    }
    ai_album_ui_common_set_label_raw(g_page.selection_status, status);
    lv_obj_set_style_text_color(g_page.selection_status, lv_color_hex(color),
                                LV_PART_MAIN);
}
static void practice_create_selection_view(void)
{
    lv_obj_t *label;
    lv_obj_t *details;
    g_page.selection_view = ai_album_ui_common_plain(g_page.screen);
    lv_obj_set_pos(g_page.selection_view, 0, 48);
    lv_obj_set_size(g_page.selection_view, 1024, 552);
    label = ai_album_ui_common_label(
        g_page.selection_view, "SPEAKING PRACTICE", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(label, 40, 20);
    label = ai_album_ui_common_label(
        g_page.selection_view, "SELECT A SCENE AND LANGUAGE (A1/A2)",
        ai_album_font_ui(), AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 40, 58);
    practice_create_language_selector();
    ai_album_practice_scene_cards_create(&g_page.scene_cards,
                                         g_page.selection_view);
    details = ai_album_ui_common_panel(
        g_page.selection_view, AI_ALBUM_UI_COLOR_WHITE, 16);
    lv_obj_set_pos(details, 40, 306);
    lv_obj_set_size(details, 944, 156);
    g_page.selection_title = ai_album_ui_common_label(
        details, "", &lv_font_montserrat_20, AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.selection_title, 20, 18);
    g_page.selection_description = ai_album_ui_common_label(
        details, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.selection_description, 20, 54);
    g_page.selection_sample = ai_album_ui_common_label(
        details, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(g_page.selection_sample, 20, 92);
    g_page.selection_status = ai_album_ui_common_label(
        details, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_ORANGE);
    lv_obj_align(g_page.selection_status, LV_ALIGN_BOTTOM_RIGHT, -20, -18);
    ai_album_ui_common_footer(
        g_page.selection_view,
        "POWER BACK   M LANGUAGE   L/R SELECT   OK START");
}

static void practice_create_session_header(void)
{
    g_page.session_title = ai_album_ui_common_label(
        g_page.session_view, "", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.session_title, 40, 18);
    g_page.session_chinese_title = ai_album_ui_common_label(
        g_page.session_view, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(g_page.session_chinese_title, 40, 51);
    g_page.status = ai_album_ui_common_label(
        g_page.session_view, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_ORANGE);
    lv_obj_set_pos(g_page.status, 600, 28);
    lv_obj_set_width(g_page.status, 384);
    lv_obj_set_style_text_align(g_page.status, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);
}

static void practice_create_guide_panel(void)
{
    lv_obj_t *panel = ai_album_ui_common_panel(
        g_page.session_view, 0xFFF4E9U, 18);
    lv_obj_t *label;
    lv_obj_set_pos(panel, 40, 82);
    lv_obj_set_size(panel, 300, 328);
    g_page.partner = ai_album_ui_common_label(
        panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_ORANGE);
    lv_obj_set_pos(g_page.partner, 20, 18);
    g_page.goal = ai_album_ui_common_label(
        panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.goal, 20, 55);
    lv_obj_set_width(g_page.goal, 260);
    lv_label_set_long_mode(g_page.goal, LV_LABEL_LONG_WRAP);
    label = ai_album_ui_common_label(
        panel, "参考表达", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 20, 142);
    g_page.sample = ai_album_ui_common_label(
        panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(g_page.sample, 20, 178);
    lv_obj_set_width(g_page.sample, 260);
    lv_label_set_long_mode(g_page.sample, LV_LABEL_LONG_WRAP);
    g_page.turn = ai_album_ui_common_label(
        panel, "当前轮次：0", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_align(g_page.turn, LV_ALIGN_BOTTOM_LEFT, 20, -18);
}

static void practice_create_dialogue_panel(void)
{
    lv_obj_t *heading;
    g_page.dialogue_panel = ai_album_ui_common_panel(
        g_page.session_view, AI_ALBUM_UI_COLOR_WHITE, 18);
    lv_obj_set_pos(g_page.dialogue_panel, 365, 82);
    lv_obj_set_size(g_page.dialogue_panel, 619, 328);
    lv_obj_set_scroll_dir(g_page.dialogue_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_page.dialogue_panel, LV_SCROLLBAR_MODE_AUTO);
    heading = ai_album_ui_common_label(
        g_page.dialogue_panel, "DIALOGUE LOG", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_ORANGE);
    lv_obj_set_pos(heading, 22, 18);
    g_page.conversation = ai_album_ui_common_label(
        g_page.dialogue_panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.conversation, 22, 54);
    lv_obj_set_width(g_page.conversation, 565);
    lv_label_set_long_mode(g_page.conversation, LV_LABEL_LONG_WRAP);
}

static void practice_create_session_controls(void)
{
    lv_obj_t *replay;
    lv_obj_t *label;
    g_page.talk_panel = ai_album_ui_common_panel(
        g_page.session_view, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_set_pos(g_page.talk_panel, 40, 430);
    lv_obj_set_size(g_page.talk_panel, 620, 58);
    g_page.talk_label = ai_album_ui_common_label(
        g_page.talk_panel, "HOLD M TO SPEAK", &lv_font_montserrat_16,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.talk_label, 18, 20);
    replay = ai_album_ui_common_panel(
        g_page.session_view, AI_ALBUM_UI_COLOR_WHITE, 14);
    lv_obj_set_pos(replay, 680, 430);
    lv_obj_set_size(replay, 304, 58);
    label = ai_album_ui_common_label(
        replay, "OK  REPLAY", &lv_font_montserrat_16,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(label, 18, 20);
    ai_album_ui_common_footer(
        g_page.session_view,
        "POWER SCENES   HOLD M TALK   L/R HISTORY   U/D VOLUME");
}

static void practice_create_session_view(void)
{
    g_page.session_view = ai_album_ui_common_plain(g_page.screen);
    lv_obj_set_pos(g_page.session_view, 0, 48);
    lv_obj_set_size(g_page.session_view, 1024, 552);
    practice_create_session_header();
    practice_create_guide_panel();
    practice_create_dialogue_panel();
    practice_create_session_controls();
    lv_obj_add_flag(g_page.session_view, LV_OBJ_FLAG_HIDDEN);
}

static void practice_set_mode(practice_mode_t mode)
{
    g_page.mode = mode;
    if (mode == PRACTICE_MODE_SESSION) {
        lv_obj_add_flag(g_page.selection_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_page.session_view, LV_OBJ_FLAG_HIDDEN);
        if (g_page.timer != NULL) {
            lv_timer_resume(g_page.timer);
        }
        return;
    }
    g_page.selection_focus = PRACTICE_FOCUS_SCENE;
    lv_obj_clear_flag(g_page.selection_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_page.session_view, LV_OBJ_FLAG_HIDDEN);
    if (g_page.timer != NULL) {
        lv_timer_pause(g_page.timer);
    }
}

static void practice_update_session_scene(void)
{
    const ai_album_practice_scene_t *scene =
        ai_album_practice_runtime_scene(g_page.selected_scene);
    const ai_album_practice_language_t *language =
        ai_album_practice_runtime_language(g_page.selected_language);
    ai_album_ui_common_set_label_text(g_page.session_title, scene->title);
    lv_label_set_text_fmt(g_page.session_chinese_title, "%s  |  %s",
                          scene->chinese_title, language->chinese_name);
    ai_album_ui_common_set_label_text(g_page.partner, scene->partner);
    ai_album_ui_common_set_label_text(g_page.goal, scene->goal);
    (void)ai_album_font_set_dynamic_text(
        g_page.sample, language->brtc_code,
        ai_album_practice_runtime_sample(g_page.selected_scene,
                                         g_page.selected_language));
}

static void practice_start_session(void)
{
    int result = ai_album_practice_runtime_start(g_page.selected_scene,
                                                  g_page.selected_language);
    if (result != BRTC_AGENT_OK) {
        ai_album_ui_common_set_label_text(
            g_page.selection_status, "VOICE START FAILED - CHECK NETWORK");
        lv_obj_set_style_text_color(g_page.selection_status,
                                    lv_color_hex(AI_ALBUM_UI_COLOR_RED),
                                    LV_PART_MAIN);
        return;
    }
    g_page.talk_pressed_at = 0U;
    g_page.turn_count = 0U;
    practice_update_session_scene();
    practice_set_mode(PRACTICE_MODE_SESSION);
    practice_render_current();
}
static void practice_return_to_selection(void)
{
    g_page.talk_pressed_at = 0U;
    ai_album_practice_runtime_pause();
    practice_set_mode(PRACTICE_MODE_SCENE_SELECT);
    practice_update_selection();
    practice_prepare_selected_language();
}
int ai_album_practice_page_create(lv_display_t *display)
{
    if (display == NULL || g_page.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_page, 0, sizeof(g_page));
    g_page.screen = ai_album_ui_common_prepare(
        display, "SPEAKING PRACTICE", 0xF7F0E8U);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    practice_create_selection_view();
    practice_create_session_view();
    g_page.timer = lv_timer_create(practice_poll, 150U, NULL);
    if (g_page.timer == NULL) {
        ai_album_practice_page_destroy();
        return RET_ERR;
    }
    lv_timer_pause(g_page.timer);
    practice_update_selection();
    return RET_OK;
}
int ai_album_practice_page_show(void)
{
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    ai_album_practice_runtime_pause();
    g_page.talk_pressed_at = 0U;
    practice_set_mode(PRACTICE_MODE_SCENE_SELECT);
    practice_update_selection();
    practice_prepare_selected_language();
    lv_screen_load(g_page.screen);
    return RET_OK;
}

void ai_album_practice_page_destroy(void)
{
    ai_album_practice_runtime_stop();
    if (g_page.timer != NULL) {
        lv_timer_delete(g_page.timer);
    }
    if (g_page.screen != NULL) {
        lv_obj_delete(g_page.screen);
    }
    memset(&g_page, 0, sizeof(g_page));
}

static void practice_move_selection(int8_t delta)
{
    uint8_t *selection = g_page.selection_focus == PRACTICE_FOCUS_LANGUAGE ?
        &g_page.selected_language : &g_page.selected_scene;
    uint8_t count = g_page.selection_focus == PRACTICE_FOCUS_LANGUAGE ?
        AI_ALBUM_PRACTICE_LANGUAGE_COUNT : AI_ALBUM_PRACTICE_SCENE_COUNT;
    *selection = (uint8_t)((*selection + count + delta) % count);
    practice_update_selection();
    if (g_page.selection_focus == PRACTICE_FOCUS_LANGUAGE) {
        practice_prepare_selected_language();
    }
}

static ai_album_ui_route_t practice_handle_selection(
    ai_album_ui_action_t action)
{
    int8_t delta;
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        if (g_page.selection_focus == PRACTICE_FOCUS_LANGUAGE) {
            g_page.selection_focus = PRACTICE_FOCUS_SCENE;
            practice_update_selection();
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        return AI_ALBUM_UI_ROUTE_HOME;
    }
    if (action == AI_ALBUM_UI_ACTION_MENU) {
        g_page.selection_focus = PRACTICE_FOCUS_LANGUAGE;
        practice_update_selection();
    } else if (action == AI_ALBUM_UI_ACTION_LEFT ||
               action == AI_ALBUM_UI_ACTION_RIGHT) {
        delta = action == AI_ALBUM_UI_ACTION_LEFT ? -1 : 1;
        practice_move_selection(delta);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        practice_start_session();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

static void practice_replay_latest(void)
{
    int result = ai_album_practice_runtime_replay_latest();
    if (result == BRTC_AGENT_OK) {
        practice_set_status("正在重播最近一句...",
                            AI_ALBUM_UI_COLOR_GREEN);
    } else {
        os_printf("[AI_PRACTICE] replay failed ret=%d\r\n", result);
        practice_render_current();
    }
}

static ai_album_ui_route_t practice_handle_session(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        practice_return_to_selection();
    } else if (action == AI_ALBUM_UI_ACTION_UP) {
        (void)ai_album_volume_adjust(1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_DOWN) {
        (void)ai_album_volume_adjust(-1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_LEFT) {
        lv_obj_scroll_by(g_page.dialogue_panel, 0, 110, LV_ANIM_OFF);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        lv_obj_scroll_by(g_page.dialogue_panel, 0, -110, LV_ANIM_OFF);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        practice_replay_latest();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_PRESS) {
        g_page.talk_pressed_at = os_mseconds();
        practice_render_current();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_RELEASE) {
        g_page.talk_pressed_at = 0U;
        practice_finish_voice();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_START) {
        g_page.talk_pressed_at = 0U;
        practice_start_voice();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_STOP) {
        g_page.talk_pressed_at = 0U;
        practice_finish_voice();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

ai_album_ui_route_t ai_album_practice_page_handle_action(
    ai_album_ui_action_t action)
{
    if (g_page.mode == PRACTICE_MODE_SESSION) {
        return practice_handle_session(action);
    }
    return practice_handle_selection(action);
}
