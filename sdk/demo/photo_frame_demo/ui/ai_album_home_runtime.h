#ifndef AI_ALBUM_HOME_RUNTIME_H
#define AI_ALBUM_HOME_RUNTIME_H

#include "ui/ai_album_ui_model.h"

const ai_album_home_model_t *ai_album_home_runtime_prepare(void);
int ai_album_home_runtime_start(void);
void ai_album_home_runtime_refresh(void);

#endif
