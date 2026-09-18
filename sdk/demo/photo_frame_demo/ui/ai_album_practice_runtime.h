#ifndef AI_ALBUM_PRACTICE_RUNTIME_H
#define AI_ALBUM_PRACTICE_RUNTIME_H

#include "ui/ai_album_chat_runtime.h"

#include <stdint.h>

#define AI_ALBUM_PRACTICE_SCENE_COUNT 6U
#define AI_ALBUM_PRACTICE_LANGUAGE_COUNT 3U

typedef struct {
    const char *title;
    const char *chinese_name;
    const char *brtc_code;
} ai_album_practice_language_t;

typedef struct {
    const char *title;
    const char *chinese_title;
    const char *partner;     /* i18n 源文案(英文) */
    const char *goal;        /* i18n 源文案(英文) */
} ai_album_practice_scene_t;

const ai_album_practice_scene_t *ai_album_practice_runtime_scene(
    uint8_t scene);
const ai_album_practice_language_t *ai_album_practice_runtime_language(
    uint8_t language);
const char *ai_album_practice_runtime_sample(uint8_t scene,
                                             uint8_t language);
const char *ai_album_practice_runtime_greeting(uint8_t scene,
                                                uint8_t language);
int ai_album_practice_runtime_prepare_language(uint8_t language);
int ai_album_practice_runtime_start(uint8_t scene, uint8_t language);
void ai_album_practice_runtime_pause(void);
void ai_album_practice_runtime_stop(void);
void ai_album_practice_runtime_sync(void);
void ai_album_practice_runtime_get_snapshot(ai_album_chat_snapshot_t *out);
int ai_album_practice_runtime_start_ptt(void);
int ai_album_practice_runtime_finish_ptt(void);
int ai_album_practice_runtime_replay_latest(void);

#endif
