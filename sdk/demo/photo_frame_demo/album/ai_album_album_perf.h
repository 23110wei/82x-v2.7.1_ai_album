#ifndef AI_ALBUM_ALBUM_PERF_H
#define AI_ALBUM_ALBUM_PERF_H

#include <stdint.h>

typedef enum {
    AI_ALBUM_ALBUM_PERF_OPEN_ALBUM = 0,
    AI_ALBUM_ALBUM_PERF_OPEN_GALLERY,
    AI_ALBUM_ALBUM_PERF_OPEN_IMAGE_AI,
    AI_ALBUM_ALBUM_PERF_RETURN_HOME,
    AI_ALBUM_ALBUM_PERF_FOCUS,
    AI_ALBUM_ALBUM_PERF_OPERATION_COUNT,
} ai_album_album_perf_operation_t;

void ai_album_album_perf_begin(ai_album_album_perf_operation_t operation);
void ai_album_album_perf_record_route(uint32_t create_ms, uint32_t show_ms);
void ai_album_album_perf_record_scan(uint32_t elapsed_us, int result);
void ai_album_album_perf_record_image_prepare(uint32_t elapsed_us, int result);
void ai_album_album_perf_record_image_draw(uint8_t file_source);
void ai_album_album_perf_poll(void);

#endif
