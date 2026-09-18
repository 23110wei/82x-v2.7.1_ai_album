#ifndef AI_ALBUM_ALBUM_IMAGE_LOADER_CONTROL_H
#define AI_ALBUM_ALBUM_IMAGE_LOADER_CONTROL_H

#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_store.h"
#include "typesdef.h"
#include "osal/mutex.h"

#define ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH 8U
#define ALBUM_IMAGE_LOADER_CONTROL_INVALID_SLOT 0xFFU
/* Every live client owns at least one slot; one spare entry covers a
 * release request that arrives before a first claim. */
#define ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT \
    (AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT + 1U)

typedef enum {
    ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY = 0U,
    ALBUM_IMAGE_LOADER_CONTROL_CLIENT_CANCELLING,
    ALBUM_IMAGE_LOADER_CONTROL_CLIENT_RELEASING,
} album_image_loader_control_client_state_t;

typedef enum {
    ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE = 0U,
    ALBUM_IMAGE_LOADER_CONTROL_JOB_RUNNING,
    ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING,
} album_image_loader_control_job_state_t;

/* All transitions and ownership checks happen under lock.  The intended
 * transitions are READY -> CANCELLING/RELEASING -> READY/removed and
 * IDLE -> RUNNING -> COMPLETION_PENDING -> IDLE (or RUNNING -> IDLE after
 * cancellation).  A non-ready client cannot enqueue or claim; a non-idle
 * job keeps its target slot busy. */

typedef struct {
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    uint16_t max_width;
    uint16_t max_height;
    uint32_t generation;
    uint32_t target_offset;
    uint8_t target_slot;
    ai_album_album_image_loader_completion_cb_t completion_cb;
    void *client;
} album_image_loader_control_job_t;

typedef struct {
    void *client;
    album_image_loader_control_client_state_t state;
} album_image_loader_control_client_t;

typedef struct {
    os_mutex_t lock;
    album_image_loader_control_job_t queue[
        ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH];
    album_image_loader_control_job_t active_job;
    ai_album_album_image_loader_completion_t completion;
    void *slot_owner[AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT];
    album_image_loader_control_client_t clients[
        ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT];
    uint32_t generation_counter;
    album_image_loader_control_job_state_t job_state;
    uint8_t queue_head;
    uint8_t queue_tail;
    uint8_t queue_count;
    uint8_t mutex_ready;
} album_image_loader_control_t;

int album_image_loader_control_init(album_image_loader_control_t *control);
void album_image_loader_control_deinit(album_image_loader_control_t *control);
int album_image_loader_control_submit(
    album_image_loader_control_t *control,
    const ai_album_album_image_loader_request_t *request,
    uint32_t *generation);
int album_image_loader_control_claim_slot(
    album_image_loader_control_t *control, void *owner, uint8_t slot);
uint8_t album_image_loader_control_slot_available(
    album_image_loader_control_t *control, const void *owner, uint8_t slot);
int album_image_loader_control_begin_stop(
    album_image_loader_control_t *control,
    void *client,
    uint8_t release,
    uint8_t *wait_for_job);
/* Stop every client and discard queued work.  A running job is left intact
 * until its worker observes cancellation and finishes normally. */
int album_image_loader_control_begin_shutdown(
    album_image_loader_control_t *control);
uint8_t album_image_loader_control_client_running(
    album_image_loader_control_t *control, const void *client);
uint8_t album_image_loader_control_take_job(
    album_image_loader_control_t *control,
    album_image_loader_control_job_t *job);
uint8_t album_image_loader_control_job_is_current(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job);
uint8_t album_image_loader_control_publish(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job,
    const ai_album_album_image_loader_completion_t *completion);
void album_image_loader_control_finish_job(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job);
uint8_t album_image_loader_control_take_completion(
    album_image_loader_control_t *control,
    ai_album_album_image_loader_completion_t *completion,
    ai_album_album_image_loader_completion_cb_t *callback,
    void **client);
uint8_t album_image_loader_control_has_queued(
    album_image_loader_control_t *control);

#endif
