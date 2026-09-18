#ifndef AI_ALBUM_ALBUM_STORAGE_H
#define AI_ALBUM_ALBUM_STORAGE_H

#include <stdint.h>

#include "album/ai_album_album_store.h"

#define AI_ALBUM_ALBUM_STORAGE_DIR "0:/IMG"
#define AI_ALBUM_ALBUM_STORAGE_SECONDARY_DIR "0:/DCIM"
#define AI_ALBUM_ALBUM_STORAGE_GENERATED_DIR "0:/AI_GEN"
#define AI_ALBUM_ALBUM_STORAGE_ROOT_DIR "0:/"
#define AI_ALBUM_ALBUM_STORAGE_MAX_DEPTH 3U

typedef enum {
    AI_ALBUM_ALBUM_STORAGE_SCAN_OK = 0,
    AI_ALBUM_ALBUM_STORAGE_SCAN_EMPTY = 1,
    AI_ALBUM_ALBUM_STORAGE_SCAN_NOT_READY = -2,
    AI_ALBUM_ALBUM_STORAGE_SCAN_INVALID = -3,
} ai_album_album_storage_scan_result_t;

/* Scan standard SD photo directories without touching LVGL state. */
ai_album_album_storage_scan_result_t ai_album_album_storage_scan(
    ai_album_album_photo_t *photos,
    uint8_t capacity,
    uint8_t *count);

#endif
