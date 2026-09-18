#include "ui/pages/ai_album_ai_chat_page.h"

#include "audio/ai_album_volume.h"
#include "basic_include.h"
#include "ui/ai_album_chat_runtime.h"
#include "ui/ai_album_font_manager.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_language.h"
#include "ui/ai_album_ui_common.h"

#include <string.h>

#define AI_CHAT_PERSONA_COUNT 6U
#define AI_CHAT_PERSONA_VISIBLE_COUNT 3U
#define AI_CHAT_HISTORY_CAPACITY 1800U

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *persona_title;
    lv_obj_t *persona_description;
    lv_obj_t *status;
    lv_obj_t *dialogue_panel;
    lv_obj_t *conversation;
    lv_obj_t *persona_buttons[AI_CHAT_PERSONA_VISIBLE_COUNT];
    lv_obj_t *talk_button;
    lv_obj_t *talk_label;
    lv_timer_t *timer;
    uint32_t observed_sequence;
    uint64_t talk_pressed_at;
    char history[AI_CHAT_HISTORY_CAPACITY];
} ai_chat_page_state_t;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} ai_chat_writer_t;

typedef struct {
    const char *title;
    const char *description;
    const char *greeting_en;
    const char *greeting;
    const char *pre_query_en;
    const char *pre_query;
    const char *post_query_en;
    const char *post_query;
} ai_chat_persona_t;

static ai_chat_page_state_t g_page;

static const ai_chat_persona_t g_personas[AI_CHAT_PERSONA_COUNT] = {
    {
        "LIFE PARTNER",
        "A warm companion for everyday conversation",
        "Hello, I am your life companion. Anything happy, troubling, or worth solving together today?",
        "你好，我是你的生活伙伴。今天有什么开心、烦恼，或想一起解决的事情？",
        "You are the user's warm and sincere life companion. First understand the user's feelings, then respond naturally and offer practical help. The user says:",
        "你是用户温暖真诚的生活伙伴。先理解用户的感受，再自然回应并提供实际帮助。用户说：",
        "Stay in the life companion role, answer in concise spoken English, and continue with a relevant question.",
        "请保持生活伙伴身份，用简洁中文口语回答，并用一个贴合话题的问题继续交流。",
    },
    {
        "TRAVELER",
        "A practical guide for your next journey",
        "Hello, I am your travel guide. Where would you like to go, or shall we talk about routes, food, or budget first?",
        "你好，我是你的旅行向导。你想去哪里，还是想先聊路线、美食或旅行预算？",
        "You are an experienced, practical travel guide. Answer around destinations, routes, food, culture, safety, and budget. The user says:",
        "你是经验丰富、务实可靠的旅行向导。围绕目的地、路线、美食、文化、安全和预算回答。用户说：",
        "Stay in the travel guide role, give concrete advice, and ask a relevant question to help the user detail the trip.",
        "请保持旅行向导身份，给出具体建议，并用一个相关问题引导用户补充行程。",
    },
    {
        "STORYTELLER",
        "An imaginative companion for family stories",
        "Hello, I am the storyteller. Would you like an adventure, a fairy tale, or shall we pick the hero and opening together?",
        "你好，我是故事讲述者。你想听冒险、童话，还是一起决定主角和故事开头？",
        "You are an imaginative storyteller suitable for family listening. Build a story from the theme or characters the user gives. The user says:",
        "你是富有想象力、适合家庭聆听的故事讲述者。根据用户给出的主题或角色展开故事。用户说：",
        "Stay in the storyteller role, keep content warm, safe and vivid, and invite the user to decide the next step of the story.",
        "请保持故事讲述者身份，内容温暖安全、语言生动，并邀请用户决定故事下一步。",
    },
    {
        "STUDY MENTOR",
        "A patient guide for study plans and explanations",
        "Hello, I am your study mentor. Want to master a concept, plan your study, or solve a problem together?",
        "你好，我是你的学习导师。你想弄懂一个知识点、制定学习计划，还是一起解决一道题？",
        "You are a patient and clear study mentor. First find where the user is stuck, then explain knowledge, methods, or plans step by step. The user says:",
        "你是耐心清晰的学习导师。先判断用户真正卡住的地方，再分步骤讲解知识、方法或学习计划。用户说：",
        "Stay in the study mentor role, explain concisely in English, and ask a small question to check understanding.",
        "请保持学习导师身份，用简洁中文解释，并用一个小问题确认用户是否理解。",
    },
    {
        "FAMILY CHEF",
        "A friendly helper for everyday home cooking",
        "Hello, I am your family chef. Tell me the ingredients, headcount, or flavors you like, and let's plan this meal.",
        "你好，我是你的家庭厨师。告诉我现有食材、人数或想吃的口味，我们一起安排这顿饭。",
        "You are a family chef familiar with home cooking. Give practical advice based on ingredients, headcount, time, tools, and taste. The user says:",
        "你是熟悉家常菜的家庭厨师。结合用户的食材、人数、时间、厨具和口味给出实用建议。用户说：",
        "Stay in the family chef role, keep steps and amounts clear, and remind about heat control, allergies, or food safety.",
        "请保持家庭厨师身份，步骤和用量要清楚，并提醒关键火候、过敏或食品安全事项。",
    },
    {
        "WELLNESS COACH",
        "A gentle coach for routines, movement and mood",
        "Hello, I am your wellness coach. Want better routines, some exercise, or just talk about today's mood?",
        "你好，我是你的活力教练。你想改善作息、开始运动，还是聊聊今天的心情？",
        "You are a gentle, practical wellness coach. Offer small actionable suggestions around routines, daily exercise, and emotional support. The user says:",
        "你是温和务实的生活健康教练，围绕作息、日常运动和情绪支持提供可执行的小建议。用户说：",
        "Stay in the wellness coach role, never diagnose; suggest professional consultation for serious or persistent symptoms.",
        "请保持生活健康教练身份，不做医疗诊断；遇到严重或持续症状时建议用户咨询专业人员。",
    },
};

static uint8_t ai_chat_is_chinese(void)
{
    return ai_album_language_get() == AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED;
}

static const char *ai_chat_greeting(uint8_t persona)
{
    return ai_chat_is_chinese() ? g_personas[persona].greeting :
                                  g_personas[persona].greeting_en;
}

static const char *ai_chat_pre_query(uint8_t persona)
{
    return ai_chat_is_chinese() ? g_personas[persona].pre_query :
                                  g_personas[persona].pre_query_en;
}

static const char *ai_chat_post_query(uint8_t persona)
{
    return ai_chat_is_chinese() ? g_personas[persona].post_query :
                                  g_personas[persona].post_query_en;
}

static const char *ai_chat_stage_text(ai_album_chat_stage_t stage)
{
    static const char *const labels[] = {
        "OFFLINE", "CONNECTING", "READY", "LISTENING - OK TO SEND",
        "RECOGNIZING", "THINKING", "SPEAKING", "ERROR"
    };

    return stage <= AI_ALBUM_CHAT_STAGE_ERROR ? labels[stage] : "UNKNOWN";
}

static const char *ai_chat_talk_text(const ai_album_chat_snapshot_t *snapshot)
{
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
    return "HOLD M TO TALK";
}

static uint32_t ai_chat_stage_color(ai_album_chat_stage_t stage)
{
    if (stage == AI_ALBUM_CHAT_STAGE_ERROR ||
        stage == AI_ALBUM_CHAT_STAGE_OFFLINE) {
        return AI_ALBUM_UI_COLOR_RED;
    }
    if (stage == AI_ALBUM_CHAT_STAGE_LISTENING ||
        stage == AI_ALBUM_CHAT_STAGE_SPEAKING) {
        return AI_ALBUM_UI_COLOR_GREEN;
    }
    return AI_ALBUM_UI_COLOR_PURPLE;
}

static void ai_chat_writer_append(ai_chat_writer_t *writer,
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

static void ai_chat_format_history(const ai_album_chat_snapshot_t *snapshot)
{
    ai_chat_writer_t writer = {
        .buffer = g_page.history,
        .capacity = sizeof(g_page.history),
    };
    uint8_t index;

    g_page.history[0] = '\0';
    if (snapshot->message_count == 0U) {
        ai_chat_writer_append(&writer, ai_album_i18n_text("AI: "),
                              ai_chat_greeting(snapshot->persona));
        return;
    }
    for (index = 0U; index < snapshot->message_count; ++index) {
        const ai_album_chat_message_t *message = &snapshot->messages[index];
        const char *speaker = ai_album_i18n_text(
            message->role == AI_ALBUM_CHAT_ROLE_USER ? "YOU: " : "AI: ");

        ai_chat_writer_append(&writer, speaker, message->text);
    }
}

static void ai_chat_update_persona(uint8_t persona)
{
    uint8_t first_persona;
    uint8_t slot;

    ai_album_ui_common_set_label_text(
        g_page.persona_title, g_personas[persona].title);
    ai_album_ui_common_set_label_text(
        g_page.persona_description, g_personas[persona].description);
    first_persona = (uint8_t)(persona -
        persona % AI_CHAT_PERSONA_VISIBLE_COUNT);
    for (slot = 0U; slot < AI_CHAT_PERSONA_VISIBLE_COUNT; ++slot) {
        uint8_t visible_persona = (uint8_t)(
            (first_persona + slot) % AI_CHAT_PERSONA_COUNT);
        lv_obj_t *label = lv_obj_get_child(g_page.persona_buttons[slot], 0);

        ai_album_ui_common_set_label_text(
            label, g_personas[visible_persona].title);
        ai_album_ui_common_focus(g_page.persona_buttons[slot],
                                 visible_persona == persona,
                                 AI_ALBUM_UI_COLOR_PURPLE);
    }
}

static void ai_chat_render(const ai_album_chat_snapshot_t *snapshot)
{
    uint32_t color = ai_chat_stage_color(snapshot->stage);

    ai_chat_update_persona(snapshot->persona);
    ai_chat_format_history(snapshot);
    (void)ai_album_font_set_dynamic_text(g_page.conversation, "zh",
                                         g_page.history);
    ai_album_ui_common_set_label_text(
        g_page.status, snapshot->error_text[0] != '\0' ?
                           snapshot->error_text :
                           ai_chat_stage_text(snapshot->stage));
    lv_obj_set_style_text_color(g_page.status, lv_color_hex(color),
                                LV_PART_MAIN);
    ai_album_ui_common_set_label_text(
        g_page.talk_label, ai_chat_talk_text(snapshot));
    ai_album_ui_common_focus(g_page.talk_button,
                             g_page.talk_pressed_at != 0U ||
                             snapshot->ptt_active,
                             color);
    lv_obj_update_layout(g_page.dialogue_panel);
    lv_obj_scroll_to_y(g_page.dialogue_panel,
                       lv_obj_get_height(g_page.conversation), LV_ANIM_OFF);
}

static void ai_chat_render_current(void)
{
    ai_album_chat_snapshot_t snapshot;

    ai_album_chat_runtime_get_snapshot(&snapshot);
    g_page.observed_sequence = snapshot.sequence;
    ai_chat_render(&snapshot);
}

static void ai_chat_poll(lv_timer_t *timer)
{
    ai_album_chat_snapshot_t snapshot;

    (void)timer;
    ai_album_chat_runtime_sync_agent_state();
    ai_album_chat_runtime_get_snapshot(&snapshot);
    if (snapshot.sequence == g_page.observed_sequence) {
        return;
    }
    g_page.observed_sequence = snapshot.sequence;
    ai_chat_render(&snapshot);
}

static lv_obj_t *ai_chat_create_persona(const char *name, int32_t x)
{
    lv_obj_t *button = ai_album_ui_common_button(g_page.screen, name, NULL);

    lv_obj_set_pos(button, x, 455);
    lv_obj_set_size(button, 190, 58);
    return button;
}

static void ai_chat_create_persona_buttons(void)
{
    static const int32_t positions[AI_CHAT_PERSONA_VISIBLE_COUNT] = {
        40, 245, 450
    };
    uint8_t slot;

    for (slot = 0U; slot < AI_CHAT_PERSONA_VISIBLE_COUNT; ++slot) {
        g_page.persona_buttons[slot] = ai_chat_create_persona(
            g_personas[slot].title, positions[slot]);
    }
}

static void ai_chat_create_character(void)
{
    lv_obj_t *panel = ai_album_ui_common_panel(g_page.screen, 0xDDD5F0U, 22);
    lv_obj_t *face;
    lv_obj_t *label;

    lv_obj_set_pos(panel, 40, 150);
    lv_obj_set_size(panel, 310, 275);
    face = ai_album_ui_common_panel(panel, 0x8B63D7U, 60);
    lv_obj_set_pos(face, 95, 48);
    lv_obj_set_size(face, 120, 120);
    label = ai_album_ui_common_label(
        face, "AI", &lv_font_montserrat_24, AI_ALBUM_UI_COLOR_WHITE);
    lv_obj_center(label);
    label = ai_album_ui_common_label(
        panel, "VOICE COMPANION", &lv_font_montserrat_16,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -42);
}

static void ai_chat_create_dialogue(void)
{
    lv_obj_t *heading;

    g_page.dialogue_panel = ai_album_ui_common_panel(
        g_page.screen, AI_ALBUM_UI_COLOR_WHITE, 22);
    lv_obj_set_pos(g_page.dialogue_panel, 380, 150);
    lv_obj_set_size(g_page.dialogue_panel, 604, 275);
    lv_obj_set_scroll_dir(g_page.dialogue_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_page.dialogue_panel, LV_SCROLLBAR_MODE_AUTO);
    heading = ai_album_ui_common_label(
        g_page.dialogue_panel, "CONVERSATION", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_PURPLE);
    lv_obj_set_pos(heading, 22, 18);
    g_page.conversation = ai_album_ui_common_label(
        g_page.dialogue_panel, "", ai_album_font_ui(),
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.conversation, 22, 52);
    lv_obj_set_width(g_page.conversation, 545);
    lv_label_set_long_mode(g_page.conversation, LV_LABEL_LONG_WRAP);
}

static void ai_chat_create_header(void)
{
    g_page.persona_title = ai_album_ui_common_label(
        g_page.screen, "", &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(g_page.persona_title, 40, 72);
    g_page.persona_description = ai_album_ui_common_label(
        g_page.screen, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(g_page.persona_description, 40, 108);
    g_page.status = ai_album_ui_common_label(
        g_page.screen, "OFFLINE", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_PURPLE);
    lv_obj_set_pos(g_page.status, 620, 103);
    lv_obj_set_width(g_page.status, 364);
    lv_obj_set_style_text_align(g_page.status, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);
}

int ai_album_ai_chat_page_create(lv_display_t *display)
{
    ai_album_chat_snapshot_t snapshot;

    if (display == NULL || g_page.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_page, 0, sizeof(g_page));
    g_page.screen = ai_album_ui_common_prepare(display, "AI CHAT", 0xF0EDF7U);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    ai_album_chat_runtime_init();
    ai_chat_create_header();
    ai_chat_create_character();
    ai_chat_create_dialogue();
    ai_chat_create_persona_buttons();
    g_page.talk_button = ai_chat_create_persona("TALK", 755);
    lv_obj_set_width(g_page.talk_button, 229);
    g_page.talk_label = lv_obj_get_child(g_page.talk_button, 0);
    ai_album_ui_common_footer(
        g_page.screen,
        "POWER BACK   HOLD M TALK   L/R PERSONA   U/D VOLUME");
    ai_album_chat_runtime_get_snapshot(&snapshot);
    g_page.observed_sequence = snapshot.sequence;
    ai_chat_render(&snapshot);
    g_page.timer = lv_timer_create(ai_chat_poll, 150U, NULL);
    if (g_page.timer == NULL) {
        ai_album_ai_chat_page_destroy();
        return RET_ERR;
    }
    return RET_OK;
}

int ai_album_ai_chat_page_show(void)
{
    ai_album_chat_snapshot_t snapshot;

    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    g_page.talk_pressed_at = 0U;
    ai_album_chat_runtime_get_snapshot(&snapshot);
    (void)ai_album_chat_runtime_activate_persona(
        snapshot.persona,
        ai_chat_pre_query(snapshot.persona),
        ai_chat_post_query(snapshot.persona),
        ai_chat_greeting(snapshot.persona));
    ai_chat_poll(NULL);
    lv_screen_load(g_page.screen);
    return RET_OK;
}

void ai_album_ai_chat_page_destroy(void)
{
    ai_album_chat_runtime_leave();
    ai_album_chat_runtime_clear_persona();
    if (g_page.timer != NULL) {
        lv_timer_delete(g_page.timer);
    }
    if (g_page.screen != NULL) {
        lv_obj_delete(g_page.screen);
    }
    memset(&g_page, 0, sizeof(g_page));
}

static void ai_chat_change_persona(int8_t delta)
{
    ai_album_chat_snapshot_t snapshot;
    uint8_t persona;

    ai_album_chat_runtime_get_snapshot(&snapshot);
    persona = (uint8_t)((snapshot.persona + AI_CHAT_PERSONA_COUNT + delta) %
                        AI_CHAT_PERSONA_COUNT);
    (void)ai_album_chat_runtime_activate_persona(
        persona, ai_chat_pre_query(persona),
        ai_chat_post_query(persona), ai_chat_greeting(persona));
    ai_chat_poll(NULL);
}

ai_album_ui_route_t ai_album_ai_chat_page_handle_action(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        g_page.talk_pressed_at = 0U;
        ai_album_chat_runtime_leave();
        ai_album_chat_runtime_clear_persona();
        return AI_ALBUM_UI_ROUTE_HOME;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        ai_chat_change_persona(-1);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        ai_chat_change_persona(1);
    } else if (action == AI_ALBUM_UI_ACTION_UP) {
        (void)ai_album_volume_adjust(1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_DOWN) {
        (void)ai_album_volume_adjust(-1);
        ai_album_ui_common_refresh_volume();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_PRESS) {
        g_page.talk_pressed_at = os_mseconds();
        ai_chat_render_current();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_RELEASE) {
        g_page.talk_pressed_at = 0U;
        (void)ai_album_chat_runtime_finish_ptt();
        ai_chat_render_current();
    } else if (action == AI_ALBUM_UI_ACTION_TALK_START) {
        g_page.talk_pressed_at = 0U;
        (void)ai_album_chat_runtime_start_ptt();
        ai_chat_poll(NULL);
    } else if (action == AI_ALBUM_UI_ACTION_TALK_STOP) {
        g_page.talk_pressed_at = 0U;
        (void)ai_album_chat_runtime_finish_ptt();
        ai_chat_poll(NULL);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
