#ifndef AI_ALBUM_ALBUM_JPEG_HW_H
#define AI_ALBUM_ALBUM_JPEG_HW_H

#include <stdint.h>

typedef enum {
    AI_ALBUM_ALBUM_JPEG_HW_OK = 0,
    AI_ALBUM_ALBUM_JPEG_HW_INVALID = -1,
    AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED = -2,
    AI_ALBUM_ALBUM_JPEG_HW_NO_MEMORY = -3,
    AI_ALBUM_ALBUM_JPEG_HW_UNAVAILABLE = -4,
    AI_ALBUM_ALBUM_JPEG_HW_BUSY = -5,
    AI_ALBUM_ALBUM_JPEG_HW_START_FAILED = -6,
    AI_ALBUM_ALBUM_JPEG_HW_TIMEOUT = -7,
    AI_ALBUM_ALBUM_JPEG_HW_ERROR = -8,
} ai_album_album_jpeg_hw_result_t;

typedef uint8_t (*ai_album_album_jpeg_hw_cancel_cb_t)(void *context);

typedef struct {
    const uint8_t *jpeg_data;
    uint32_t jpeg_data_size;
    uint32_t jpeg_dma_size;
    uint16_t *rgb565;
    uint16_t max_width;
    uint16_t max_height;
    ai_album_album_jpeg_hw_cancel_cb_t cancel_cb;
    void *cancel_context;
} ai_album_album_jpeg_hw_input_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t stride;
    uint32_t data_size;
    uint32_t hardware_us;
    uint32_t convert_us;
} ai_album_album_jpeg_hw_output_t;

ai_album_album_jpeg_hw_result_t ai_album_album_jpeg_hw_decode(
    const ai_album_album_jpeg_hw_input_t *input,
    ai_album_album_jpeg_hw_output_t *output);

/* Header-only probe, so callers can reject a JPEG the decoder cannot handle
 * (progressive, 12-bit, non 4:2:0, ...) before allocating or writing it. */
ai_album_album_jpeg_hw_result_t ai_album_album_jpeg_hw_probe(
    const uint8_t *data, uint32_t size, uint16_t *width, uint16_t *height);

#endif
