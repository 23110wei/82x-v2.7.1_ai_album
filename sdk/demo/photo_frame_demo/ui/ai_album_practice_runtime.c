#include "ui/ai_album_practice_runtime.h"

#include "basic_include.h"
#include "brtc_agent/brtc_agent.h"
#include "project_config.h"

#define PRACTICE_PROMPT_CAPACITY 256U

typedef struct {
    ai_album_practice_language_t language;
    const char *prompt_name;
} practice_language_config_t;

typedef struct {
    ai_album_practice_scene_t scene;
    const char *role;
    const char *situation;
    const char *samples[AI_ALBUM_PRACTICE_LANGUAGE_COUNT];
    const char *greetings[AI_ALBUM_PRACTICE_LANGUAGE_COUNT];
} practice_scene_config_t;

typedef struct {
    char pre_query[PRACTICE_PROMPT_CAPACITY];
    char post_query[PRACTICE_PROMPT_CAPACITY];
} practice_prompt_t;

typedef struct {
    uint8_t active;
    uint8_t prompt_configured;
    uint8_t scene;
    uint8_t language;
    int setup_error;
} practice_runtime_state_t;

static const practice_language_config_t
    g_practice_languages[AI_ALBUM_PRACTICE_LANGUAGE_COUNT] = {
        {{"ENGLISH", "英语", "en"}, "English"},
        {{"CHINESE", "中文", "zh"}, "Mandarin Chinese"},
        {{"JAPANESE", "日语", "ja"}, "Japanese"},
    };

static const practice_scene_config_t
    g_practice_scenes[AI_ALBUM_PRACTICE_SCENE_COUNT] = {
        {
            {
                "DAILY GREETING",
                "日常问候",
                "PRACTICE WITH: A NEW ACQUAINTANCE",
                "GOAL: GREET, INTRODUCE YOURSELF AND ANSWER SIMPLE QUESTIONS.",
            },
            "friendly new acquaintance",
            "daily greeting conversation",
            {
                "Hi! Nice to meet you. My name is Alex.",
                "你好，很高兴认识你。我叫小李。",
                "こんにちは。はじめまして。わたしはリです。",
            },
            {
                "Hi! Nice to meet you. I'm Emma. What's your name?",
                "你好，很高兴认识你。我叫小美。你叫什么名字？",
                "こんにちは。はじめまして。わたしはエマです。"
                "お名前は何ですか？",
            },
        },
        {
            {
                "TRAVEL",
                "旅行入住",
                "PRACTICE WITH: A HOTEL RECEPTIONIST",
                "GOAL: GIVE BOOKING DETAILS AND CHECK IN.",
            },
            "friendly hotel receptionist",
            "hotel check-in conversation",
            {
                "Hello, I have a reservation under Lee.",
                "你好，我用李这个名字预订了房间。",
                "こんにちは。リーの名前で予約しました。",
            },
            {
                "Hello! Welcome to the Sunrise Hotel. Do you have a "
                "reservation?",
                "你好，欢迎来到阳光酒店。请问您有预订吗？",
                "こんにちは。サンライズホテルへようこそ。"
                "ご予約はありますか？",
            },
        },
        {
            {
                "SHOPPING",
                "购物交流",
                "PRACTICE WITH: A SHOP ASSISTANT",
                "GOAL: ASK ABOUT ITEMS, COLORS OR PRICES AND REPLY.",
            },
            "helpful shop assistant",
            "clothing-store conversation",
            {
                "I'm looking for a blue T-shirt.",
                "我想找一件蓝色的T恤。",
                "青いTシャツを探しています。",
            },
            {
                "Hello! Can I help you find something today?",
                "你好！今天想找什么商品？",
                "いらっしゃいませ。何をお探しですか？",
            },
        },
        {
            {
                "RESTAURANT",
                "餐厅点餐",
                "PRACTICE WITH: A RESTAURANT SERVER",
                "GOAL: READ THE MENU, ORDER AND STATE SIMPLE NEEDS.",
            },
            "friendly restaurant server",
            "restaurant ordering conversation",
            {
                "I'd like the curry and a glass of water, please.",
                "我想要一份咖喱饭和一杯水，谢谢。",
                "カレーと水をお願いします。",
            },
            {
                "Good evening! Here's the menu. Are you ready to order?",
                "晚上好，这是菜单。您准备好点餐了吗？",
                "こんばんは。メニューです。ご注文はお決まりですか？",
            },
        },
        {
            {
                "DIRECTIONS",
                "问路交流",
                "PRACTICE WITH: A HELPFUL LOCAL",
                "GOAL: ASK FOR A PLACE AND FOLLOW SIMPLE DIRECTIONS.",
            },
            "helpful local passerby",
            "asking-for-directions conversation",
            {
                "Excuse me, how can I get to the train station?",
                "请问，去火车站怎么走？",
                "すみません、駅へはどう行けばいいですか？",
            },
            {
                "Hello! Are you looking for somewhere?",
                "你好，你在找什么地方吗？",
                "こんにちは。どこかお探しですか？",
            },
        },
        {
            {
                "TRANSPORT",
                "交通出行",
                "PRACTICE WITH: A STATION CLERK",
                "GOAL: BUY A TICKET AND CONFIRM DESTINATION, TIME OR PRICE.",
            },
            "helpful ticket clerk",
            "public-transport ticket conversation",
            {
                "One ticket to the airport, please.",
                "请给我一张去机场的票。",
                "空港までの切符を一枚お願いします。",
            },
            {
                "Hello! Where would you like to go?",
                "你好，请问您要去哪里？",
                "こんにちは。どちらまで行きますか？",
            },
        },
    };

static practice_runtime_state_t g_practice_runtime;

const ai_album_practice_scene_t *ai_album_practice_runtime_scene(
    uint8_t scene)
{
    if (scene >= AI_ALBUM_PRACTICE_SCENE_COUNT) {
        return NULL;
    }
    return &g_practice_scenes[scene].scene;
}

const ai_album_practice_language_t *ai_album_practice_runtime_language(
    uint8_t language)
{
    if (language >= AI_ALBUM_PRACTICE_LANGUAGE_COUNT) {
        return NULL;
    }
    return &g_practice_languages[language].language;
}

const char *ai_album_practice_runtime_sample(uint8_t scene, uint8_t language)
{
    if (scene >= AI_ALBUM_PRACTICE_SCENE_COUNT ||
        language >= AI_ALBUM_PRACTICE_LANGUAGE_COUNT) {
        return NULL;
    }
    return g_practice_scenes[scene].samples[language];
}

const char *ai_album_practice_runtime_greeting(uint8_t scene,
                                               uint8_t language)
{
    if (scene >= AI_ALBUM_PRACTICE_SCENE_COUNT ||
        language >= AI_ALBUM_PRACTICE_LANGUAGE_COUNT) {
        return NULL;
    }
    return g_practice_scenes[scene].greetings[language];
}

static int practice_build_prompt(uint8_t scene, uint8_t language,
                                 practice_prompt_t *prompt)
{
    const practice_scene_config_t *scene_config = &g_practice_scenes[scene];
    const char *language_name =
        g_practice_languages[language].prompt_name;
    int written;

    written = os_snprintf(
        prompt->pre_query, sizeof(prompt->pre_query),
        "You are a %s in a %s speaking exercise. Stay in a %s with a "
        "CEFR A1-A2 learner. The learner says: ",
        scene_config->role, language_name, scene_config->situation);
    if (written < 0 || (size_t)written >= sizeof(prompt->pre_query)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    written = os_snprintf(
        prompt->post_query, sizeof(prompt->post_query),
        "Reply in one or two short, natural A1-A2 %s sentences. Ask one "
        "simple scene question. If needed, model a better phrase naturally. "
        "Use only %s; no labels, scores, translations, explanations, or "
        "Markdown.",
        language_name, language_name);
    return written < 0 || (size_t)written >= sizeof(prompt->post_query) ?
        BRTC_AGENT_ERR_CONFIG : BRTC_AGENT_OK;
}

static int practice_apply_prompt(void)
{
    practice_prompt_t prompt;
    const char *greeting;
    int result;

    result = practice_build_prompt(g_practice_runtime.scene,
                                   g_practice_runtime.language, &prompt);
    if (result != BRTC_AGENT_OK) {
        g_practice_runtime.setup_error = result;
        return result;
    }
    greeting = ai_album_practice_runtime_greeting(
        g_practice_runtime.scene, g_practice_runtime.language);
    result = ai_album_chat_runtime_activate_persona(
        g_practice_runtime.scene, prompt.pre_query, prompt.post_query,
        greeting);
    if (result == BRTC_AGENT_OK) {
        g_practice_runtime.prompt_configured = 1U;
        os_printf(
            "[AI_PRACTICE] prompt_applied t=%u scene=%u language=%s\r\n",
            (unsigned)os_mseconds(), (unsigned)g_practice_runtime.scene,
            g_practice_languages[g_practice_runtime.language].language.brtc_code);
    } else {
        g_practice_runtime.setup_error = result;
    }
    return result;
}

static int practice_switch_language(uint8_t language)
{
    const ai_album_practice_language_t *selected_language =
        ai_album_practice_runtime_language(language);
    int result;

    if (selected_language == NULL) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    result = brtc_agent_switch_language(selected_language->brtc_code);
    os_printf(
        "[AI_PRACTICE] language_switch_requested t=%u language=%s ret=%d\r\n",
        (unsigned)os_mseconds(), selected_language->brtc_code, result);
    return result;
}

static void practice_clear_session(void)
{
    if (!g_practice_runtime.active) {
        return;
    }
    ai_album_chat_runtime_leave();
    ai_album_chat_runtime_clear_persona();
    ai_album_chat_runtime_clear_translation();
    memset(&g_practice_runtime, 0, sizeof(g_practice_runtime));
}

int ai_album_practice_runtime_prepare_language(uint8_t language)
{
    if (language >= AI_ALBUM_PRACTICE_LANGUAGE_COUNT) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if (g_practice_runtime.active) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    ai_album_chat_runtime_init();
    return practice_switch_language(language);
}

int ai_album_practice_runtime_start(uint8_t scene, uint8_t language)
{
    int result;

    if (scene >= AI_ALBUM_PRACTICE_SCENE_COUNT ||
        language >= AI_ALBUM_PRACTICE_LANGUAGE_COUNT) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    os_printf(
        "[AI_PRACTICE] practice_session_requested t=%u scene=%u language=%s\r\n",
        (unsigned)os_mseconds(), (unsigned)scene,
        g_practice_languages[language].language.brtc_code);
    practice_clear_session();
    ai_album_chat_runtime_init();
    memset(&g_practice_runtime, 0, sizeof(g_practice_runtime));
    g_practice_runtime.active = 1U;
    g_practice_runtime.scene = scene;
    g_practice_runtime.language = language;
    result = practice_switch_language(language);
    if (result < BRTC_AGENT_OK) {
        g_practice_runtime.active = 0U;
        os_printf("[AI_PRACTICE] language=%s switch failed ret=%d\r\n",
                  g_practice_languages[language].language.brtc_code, result);
        return result;
    }
    if (result == BRTC_AGENT_IN_PROGRESS) {
        os_printf(
            "[AI_PRACTICE] scene=%u language=%s switching\r\n",
            (unsigned)scene,
            g_practice_languages[language].language.brtc_code);
        return BRTC_AGENT_OK;
    }
    result = practice_apply_prompt();
    if (result != BRTC_AGENT_OK) {
        ai_album_practice_runtime_stop();
        return result;
    }
    os_printf(
        "[AI_PRACTICE] scene=%u language=%s started\r\n", (unsigned)scene,
        g_practice_languages[language].language.brtc_code);
    return BRTC_AGENT_OK;
}

void ai_album_practice_runtime_pause(void)
{
    if (!g_practice_runtime.active) {
        return;
    }
    practice_clear_session();
    os_printf("[AI_PRACTICE] session paused t=%u, language retained\r\n",
              (unsigned)os_mseconds());
}

void ai_album_practice_runtime_stop(void)
{
    int result;

    practice_clear_session();
    if (brtc_agent_get_state() == BRTC_AGENT_STATE_UNINITIALIZED) {
        return;
    }
    result = brtc_agent_switch_language(AI_ALBUM_BRTC_LANGUAGE);
    if (result < BRTC_AGENT_OK) {
        os_printf("[AI_PRACTICE] restore language=%s failed ret=%d\r\n",
                  AI_ALBUM_BRTC_LANGUAGE, result);
    } else {
        os_printf("[AI_PRACTICE] stopped, restoring language=%s\r\n",
                  AI_ALBUM_BRTC_LANGUAGE);
    }
}

void ai_album_practice_runtime_sync(void)
{
    const ai_album_practice_language_t *selected_language;
    char current_language[16];
    int result;

    ai_album_chat_runtime_sync_agent_state();
    if (!g_practice_runtime.active ||
        g_practice_runtime.prompt_configured ||
        g_practice_runtime.setup_error != 0) {
        return;
    }
    selected_language = ai_album_practice_runtime_language(
        g_practice_runtime.language);
    result = brtc_agent_get_language(current_language,
                                     sizeof(current_language));
    if (result != BRTC_AGENT_OK) {
        g_practice_runtime.setup_error = result;
        return;
    }
    if (os_strcmp(current_language, selected_language->brtc_code) != 0) {
        return;
    }
    os_printf("[AI_PRACTICE] language_configured t=%u language=%s\r\n",
              (unsigned)os_mseconds(), selected_language->brtc_code);
    result = practice_apply_prompt();
    if (result != BRTC_AGENT_OK) {
        os_printf("[AI_PRACTICE] deferred prompt failed ret=%d\r\n",
                  result);
        return;
    }
    ai_album_chat_runtime_sync_agent_state();
}

void ai_album_practice_runtime_get_snapshot(ai_album_chat_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    ai_album_chat_runtime_get_snapshot(out);
    if (!g_practice_runtime.active) {
        return;
    }
    if (g_practice_runtime.setup_error != 0) {
        out->stage = AI_ALBUM_CHAT_STAGE_ERROR;
        out->error_code = g_practice_runtime.setup_error;
        os_snprintf(out->error_text, sizeof(out->error_text),
                    "Practice setup failed (%d)",
                    g_practice_runtime.setup_error);
    } else if (!g_practice_runtime.prompt_configured) {
        out->stage = AI_ALBUM_CHAT_STAGE_CONNECTING;
        out->ptt_active = 0U;
        out->speaking = 0U;
        out->error_code = 0;
        out->error_text[0] = '\0';
    }
}

int ai_album_practice_runtime_start_ptt(void)
{
    if (!g_practice_runtime.active ||
        !g_practice_runtime.prompt_configured) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    return ai_album_chat_runtime_start_ptt();
}

int ai_album_practice_runtime_finish_ptt(void)
{
    if (!g_practice_runtime.active ||
        !g_practice_runtime.prompt_configured) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    return ai_album_chat_runtime_finish_ptt();
}

static const char *practice_latest_ai_text(
    const ai_album_chat_snapshot_t *snapshot)
{
    int index;

    for (index = (int)snapshot->message_count - 1; index >= 0; --index) {
        const ai_album_chat_message_t *message = &snapshot->messages[index];

        if (message->role == AI_ALBUM_CHAT_ROLE_ASSISTANT &&
            message->final && message->text[0] != '\0') {
            return message->text;
        }
    }
    return ai_album_practice_runtime_greeting(
        g_practice_runtime.scene, g_practice_runtime.language);
}

int ai_album_practice_runtime_replay_latest(void)
{
    ai_album_chat_snapshot_t snapshot;
    const char *text;

    if (!g_practice_runtime.active ||
        !g_practice_runtime.prompt_configured) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    ai_album_chat_runtime_get_snapshot(&snapshot);
    if (snapshot.stage != AI_ALBUM_CHAT_STAGE_READY) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    text = practice_latest_ai_text(&snapshot);
    return brtc_agent_speak_text(text);
}
