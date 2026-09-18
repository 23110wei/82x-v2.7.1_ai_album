#ifndef AI_ALBUM_UI_MODEL_H
#define AI_ALBUM_UI_MODEL_H

#include "typesdef.h"

typedef struct {
    const char *greeting;
    const char *time;
    const char *date;
    const char *lunar_date;
    const char *weather;
    const char *temperature;
    const char *weather_details;
    const char *forecast[4];
} ai_album_home_model_t;

#endif
