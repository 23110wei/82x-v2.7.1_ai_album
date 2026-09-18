#ifndef AI_ALBUM_STORAGE_INFO_H
#define AI_ALBUM_STORAGE_INFO_H

#include "typesdef.h"

typedef enum {
    AI_ALBUM_STORAGE_STATUS_UNKNOWN = 0,
    AI_ALBUM_STORAGE_STATUS_CHECKING,
    AI_ALBUM_STORAGE_STATUS_READY,
    AI_ALBUM_STORAGE_STATUS_NO_MEDIA,
    AI_ALBUM_STORAGE_STATUS_ERROR,
} ai_album_storage_status_t;

typedef struct {
    ai_album_storage_status_t status;
    uint32_t total_mb;
    uint32_t free_mb;
    uint32_t sequence;
} ai_album_storage_snapshot_t;

void ai_album_storage_info_init(void);
void ai_album_storage_info_request(void);
void ai_album_storage_info_get_snapshot(ai_album_storage_snapshot_t *out);

#endif
