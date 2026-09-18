#ifndef AI_ALBUM_ALBUM_IMAGE_AI_STORAGE_H
#define AI_ALBUM_ALBUM_IMAGE_AI_STORAGE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AI_ALBUM_ALBUM_IMAGE_AI_JPEG_OK = 0,
    /* Local decoder cannot handle it: progressive, 12-bit, non 4:2:0, ... */
    AI_ALBUM_ALBUM_IMAGE_AI_JPEG_UNSUPPORTED,
    AI_ALBUM_ALBUM_IMAGE_AI_JPEG_IO_ERROR,
} ai_album_album_image_ai_jpeg_status_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t status; /* ai_album_album_image_ai_jpeg_status_t */
} ai_album_album_image_ai_jpeg_info_t;

int ai_album_album_image_ai_ensure_directory(void);
int ai_album_album_image_ai_make_path(char *path, uint32_t capacity,
                                      const char *prefix);
int ai_album_album_image_ai_load_file(
    const char *path, uint8_t **data, uint32_t *data_size,
    ai_album_album_image_ai_jpeg_info_t *info);
int ai_album_album_image_ai_write_result(
    const char *path, const uint8_t *data, size_t length,
    ai_album_album_image_ai_jpeg_info_t *info);

#endif
