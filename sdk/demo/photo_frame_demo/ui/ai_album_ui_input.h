#ifndef AI_ALBUM_UI_INPUT_H
#define AI_ALBUM_UI_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AI_ALBUM_UI_ACTION_LEFT = 0,
    AI_ALBUM_UI_ACTION_RIGHT,
    AI_ALBUM_UI_ACTION_OK,
    AI_ALBUM_UI_ACTION_OK_LONG,
    AI_ALBUM_UI_ACTION_MENU,
    AI_ALBUM_UI_ACTION_UP,
    AI_ALBUM_UI_ACTION_DOWN,
    AI_ALBUM_UI_ACTION_BACK,
    AI_ALBUM_UI_ACTION_TALK_START,
    AI_ALBUM_UI_ACTION_TALK_STOP,
    AI_ALBUM_UI_ACTION_TALK_PRESS,
    AI_ALBUM_UI_ACTION_TALK_RELEASE,
    AI_ALBUM_UI_ACTION_POWER_OFF,
    AI_ALBUM_UI_ACTION_COUNT,
} ai_album_ui_action_t;

#define AI_ALBUM_UI_TALK_HOLD_MS 500U

int ai_album_ui_input_init(void);
void ai_album_ui_input_deinit(void);
int ai_album_ui_input_post(ai_album_ui_action_t action);

#ifdef __cplusplus
}
#endif

#endif
