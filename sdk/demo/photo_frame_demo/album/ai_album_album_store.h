#ifndef AI_ALBUM_ALBUM_STORE_H
#define AI_ALBUM_ALBUM_STORE_H

#include <stdint.h>

#define AI_ALBUM_ALBUM_STORE_MAX_PHOTOS 255U
#define AI_ALBUM_ALBUM_STYLE_COUNT 8U
#define AI_ALBUM_ALBUM_PHOTO_NAME_MAX 32U
#define AI_ALBUM_ALBUM_PHOTO_ORIGIN_MAX 20U
#define AI_ALBUM_ALBUM_PHOTO_PATH_MAX 96U

typedef struct {
    char name[AI_ALBUM_ALBUM_PHOTO_NAME_MAX];
    char origin[AI_ALBUM_ALBUM_PHOTO_ORIGIN_MAX];
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    uint32_t file_size;
    uint32_t sky_color;
    uint32_t ground_color;
    uint32_t accent_color;
    uint16_t width;
    uint16_t height;
    uint8_t generated;
    uint8_t storage_backed;
} ai_album_album_photo_t;

typedef enum {
    AI_ALBUM_ALBUM_STORE_STATUS_UNSCANNED = 0,
    AI_ALBUM_ALBUM_STORE_STATUS_SD_READY,
    AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY,
    AI_ALBUM_ALBUM_STORE_STATUS_SD_UNAVAILABLE,
} ai_album_album_store_status_t;

typedef enum {
    AI_ALBUM_ALBUM_STORE_FORMAT_SD = 0,
    AI_ALBUM_ALBUM_STORE_FORMAT_FLASH,
    AI_ALBUM_ALBUM_STORE_FORMAT_BOTH,
} ai_album_album_store_format_target_t;

void ai_album_album_store_init(void);
/* Returns RET_OK when SD storage was scanned, even if it was empty. */
int ai_album_album_store_refresh(void);
uint8_t ai_album_album_store_count(void);
const ai_album_album_photo_t *ai_album_album_store_get(uint8_t index);
uint8_t ai_album_album_store_selected(void);
int ai_album_album_store_select(uint8_t index);
/* Delete one storage-backed photo and refresh the in-memory album index. */
int ai_album_album_store_delete(uint8_t index);
/* SD cleanup preserves 0:/ai_album/fonts; Flash targets are formatted. */
uint8_t ai_album_album_store_format_available(
    ai_album_album_store_format_target_t target);
int ai_album_album_store_format(
    ai_album_album_store_format_target_t target);
uint8_t ai_album_album_store_has_storage_photos(void);
uint32_t ai_album_album_store_version(void);
ai_album_album_store_status_t ai_album_album_store_status(void);
int ai_album_album_store_add_generated(const ai_album_album_photo_t *photo,
                                       uint8_t *out_index);

#endif
