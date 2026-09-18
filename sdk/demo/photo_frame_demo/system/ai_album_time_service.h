#ifndef AI_ALBUM_TIME_SERVICE_H
#define AI_ALBUM_TIME_SERVICE_H

#include "typesdef.h"

void ai_album_time_service_update(void);
int ai_album_time_service_get_date(int *year, int *month, int *day);
int ai_album_time_service_format(char *date, uint32 date_size,
                                 char *clock, uint32 clock_size);

#endif
