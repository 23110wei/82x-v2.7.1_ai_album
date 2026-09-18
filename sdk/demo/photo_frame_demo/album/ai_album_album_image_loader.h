#ifndef AI_ALBUM_ALBUM_IMAGE_LOADER_H
#define AI_ALBUM_ALBUM_IMAGE_LOADER_H

#include <stdint.h>

enum {
    /* Two full-size slots cover the displayed image and the next image. */
    AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT = 2U,
    AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_WIDTH = 1024U,
    AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_HEIGHT = 600U,
    AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_BYTES =
        AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_WIDTH *
        AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_HEIGHT * 2U,
    AI_ALBUM_ALBUM_IMAGE_LOADER_TARGET_ALIGNMENT = 32U,
};

typedef enum {
    AI_ALBUM_ALBUM_IMAGE_LOADER_OK = 0,
    AI_ALBUM_ALBUM_IMAGE_LOADER_FILE_UNAVAILABLE = -1,
    AI_ALBUM_ALBUM_IMAGE_LOADER_FORMAT_UNSUPPORTED = -2,
    AI_ALBUM_ALBUM_IMAGE_LOADER_DECODE_FAILED = -3,
} ai_album_album_image_loader_result_t;

typedef struct {
    ai_album_album_image_loader_result_t result;
    const uint8_t *data;
    uint32_t data_size;
    uint32_t stride;
    uint16_t width;
    uint16_t height;
    uint32_t generation;
    uint32_t target_offset;
    uint8_t target_slot;
} ai_album_album_image_loader_completion_t;

typedef void (*ai_album_album_image_loader_completion_cb_t)(
    const ai_album_album_image_loader_completion_t *completion,
    void *client);

typedef struct {
    const char *path;
    uint16_t max_width;
    uint16_t max_height;
    uint32_t target_offset;
    uint8_t target_slot;
    ai_album_album_image_loader_completion_cb_t completion_cb;
    void *client;
} ai_album_album_image_loader_request_t;

int ai_album_album_image_loader_submit(
    const ai_album_album_image_loader_request_t *request,
    uint32_t *generation);
/* Claim a complete physical RGB565 slot for one logical owner. */
int ai_album_album_image_loader_claim_slot(void *owner, uint8_t slot);
uint8_t ai_album_album_image_loader_slot_available(
    const void *owner, uint8_t slot);
/* Release removes queued work immediately; an active job keeps the owner and
 * slot blocked until its hardware session has fully returned. */
void ai_album_album_image_loader_release_owner(void *owner);
/* Cancellation is a barrier: submissions from this client are rejected until
 * the active job has exited (or the timeout leaves the client blocked). */
void ai_album_album_image_loader_cancel(void *client);
/* Stop the worker and release its AV PSRAM buffers after the task exits. */
void ai_album_album_image_loader_deinit(void);
/* Poll atomically consumes at most one completion before invoking its
 * callback, then wakes the FIFO worker.  Completion data points into the
 * target slot; the client must coordinate any later reuse of that slot. */
void ai_album_album_image_loader_poll(void);

#endif
