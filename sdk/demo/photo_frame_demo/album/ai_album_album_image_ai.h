#ifndef AI_ALBUM_ALBUM_IMAGE_AI_H
#define AI_ALBUM_ALBUM_IMAGE_AI_H

#include <stdint.h>

#include "album/ai_album_album_store.h"
#include "brtc_agent/brtc_agent_types.h"

#define AI_ALBUM_ALBUM_IMAGE_AI_MAX_JPEG_SIZE 0xFFFFCU
/* ai_album_album_image_ai_start() returns this while a request is in flight. */
#define AI_ALBUM_ALBUM_IMAGE_AI_BUSY 1

typedef enum {
    AI_ALBUM_ALBUM_IMAGE_AI_IDLE = 0,
    AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING,
    AI_ALBUM_ALBUM_IMAGE_AI_WAITING,
    AI_ALBUM_ALBUM_IMAGE_AI_WRITING,
    AI_ALBUM_ALBUM_IMAGE_AI_READY,
    AI_ALBUM_ALBUM_IMAGE_AI_SAVING,
    AI_ALBUM_ALBUM_IMAGE_AI_SAVED,
    AI_ALBUM_ALBUM_IMAGE_AI_ERROR,
} ai_album_album_image_ai_state_t;

typedef enum {
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE = 0,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_SERVICE,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_FORMAT,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_WRITE,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_SAVE,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_FULL,
    AI_ALBUM_ALBUM_IMAGE_AI_REASON_TIMEOUT,
} ai_album_album_image_ai_reason_t;

typedef struct {
    ai_album_album_image_ai_state_t state;
    uint32_t revision;
    int error;
    uint8_t reason; /* ai_album_album_image_ai_reason_t */
    uint8_t style;
    ai_album_album_photo_t result;
} ai_album_album_image_ai_snapshot_t;

void ai_album_album_image_ai_init(void);
void ai_album_album_image_ai_deinit(void);
const char *ai_album_album_image_ai_style_name(uint8_t style);
int ai_album_album_image_ai_start(const ai_album_album_photo_t *source,
                                  uint8_t style);
void ai_album_album_image_ai_cancel(void);
void ai_album_album_image_ai_poll(void);
void ai_album_album_image_ai_handle_event(const brtc_agent_event_t *event);
int ai_album_album_image_ai_get_snapshot(
    ai_album_album_image_ai_snapshot_t *snapshot);
int ai_album_album_image_ai_save(uint8_t *out_index, uint8_t *out_reason);

#endif
