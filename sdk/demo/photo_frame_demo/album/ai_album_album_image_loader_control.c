#include "ai_album_album_image_loader_control.h"

#include "basic_include.h"

static album_image_loader_control_client_t *control_find_client_locked(
    album_image_loader_control_t *control, void *client)
{
    uint8_t index;

    for (index = 0U; index < ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT;
         ++index) {
        if (control->clients[index].client == client) {
            return &control->clients[index];
        }
    }
    return NULL;
}

static album_image_loader_control_client_t *control_ensure_client_locked(
    album_image_loader_control_t *control, void *client)
{
    album_image_loader_control_client_t *entry;
    uint8_t index;

    entry = control_find_client_locked(control, client);
    if (entry != NULL) return entry;
    for (index = 0U; index < ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT;
         ++index) {
        entry = &control->clients[index];
        if (entry->client == NULL) {
            entry->client = client;
            entry->state = ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY;
            return entry;
        }
    }
    os_printf("[ALBUM_JPEG] client table full\r\n");
    return NULL;
}

static uint8_t control_client_has_slot_locked(
    const album_image_loader_control_t *control, const void *client)
{
    uint8_t slot;

    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        if (control->slot_owner[slot] == client) return 1U;
    }
    return 0U;
}

static uint8_t control_queue_has_client_locked(
    const album_image_loader_control_t *control, const void *client)
{
    uint8_t index;

    for (index = 0U; index < control->queue_count; ++index) {
        uint8_t queue_index = (uint8_t)(
            (control->queue_head + index) %
            ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
        if (control->queue[queue_index].client == client) return 1U;
    }
    return 0U;
}

static uint8_t control_client_has_work_locked(
    const album_image_loader_control_t *control, const void *client)
{
    return (uint8_t)(((control->job_state !=
                       ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE) &&
                      control->active_job.client == client) ||
                     control_queue_has_client_locked(control, client));
}

static uint8_t control_slot_busy_locked(
    const album_image_loader_control_t *control, uint8_t slot)
{
    /* COMPLETION_PENDING deliberately counts as busy: the RGB buffer is
     * still owned by the unconsumed completion. */
    return (uint8_t)(control->job_state !=
                         ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE &&
                     control->active_job.target_slot == slot);
}

static uint8_t control_slot_available_locked(
    album_image_loader_control_t *control, const void *owner, uint8_t slot)
{
    album_image_loader_control_client_t *entry;

    if (owner == NULL || slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT) {
        return 0U;
    }
    entry = control_find_client_locked(control, (void *)owner);
    if (entry != NULL && entry->state !=
        ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY) {
        return 0U;
    }
    return (uint8_t)(!control_slot_busy_locked(control, slot) &&
                     (control->slot_owner[slot] == NULL ||
                      control->slot_owner[slot] == owner));
}

static uint32_t control_next_generation_locked(
    album_image_loader_control_t *control)
{
    control->generation_counter++;
    if (control->generation_counter == 0U) {
        control->generation_counter++;
    }
    return control->generation_counter;
}

static int control_claim_slot_locked(
    album_image_loader_control_t *control, void *owner, uint8_t slot)
{
    album_image_loader_control_client_t *entry;

    if (!control_slot_available_locked(control, owner, slot)) {
        return RET_ERR;
    }
    entry = control_ensure_client_locked(control, owner);
    if (entry == NULL || entry->state !=
        ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY) {
        return RET_ERR;
    }
    control->slot_owner[slot] = owner;
    return RET_OK;
}

static void control_release_slots_locked(
    album_image_loader_control_t *control, void *owner)
{
    uint8_t slot;

    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        if (control->slot_owner[slot] == owner) {
            control->slot_owner[slot] = NULL;
        }
    }
}

static void control_clear_active_locked(
    album_image_loader_control_t *control)
{
    control->job_state = ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE;
    memset(&control->active_job, 0, sizeof(control->active_job));
    control->active_job.target_slot =
        ALBUM_IMAGE_LOADER_CONTROL_INVALID_SLOT;
    memset(&control->completion, 0, sizeof(control->completion));
}

static void control_finish_client_locked(
    album_image_loader_control_t *control, void *client)
{
    album_image_loader_control_client_t *entry =
        control_find_client_locked(control, client);

    if (entry == NULL || control_client_has_work_locked(control, client)) {
        return;
    }
    if (entry->state == ALBUM_IMAGE_LOADER_CONTROL_CLIENT_RELEASING) {
        control_release_slots_locked(control, client);
        entry->client = NULL;
        entry->state = ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY;
    } else if (entry->state ==
               ALBUM_IMAGE_LOADER_CONTROL_CLIENT_CANCELLING) {
        entry->state = ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY;
        if (!control_client_has_slot_locked(control, client)) {
            entry->client = NULL;
        }
    } else if (!control_client_has_slot_locked(control, client)) {
        entry->client = NULL;
    }
}

static uint8_t control_active_matches_locked(
    const album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job)
{
    return (uint8_t)(job != NULL &&
                     control->job_state ==
                         ALBUM_IMAGE_LOADER_CONTROL_JOB_RUNNING &&
                     control->active_job.generation == job->generation &&
                     control->active_job.target_slot == job->target_slot &&
                     control->active_job.client == job->client);
}

static uint8_t control_has_job_locked(
    const album_image_loader_control_t *control,
    void *client, uint8_t target_slot)
{
    uint8_t index;

    if (control->job_state != ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE &&
        control->active_job.client == client &&
        control->active_job.target_slot == target_slot) {
        return 1U;
    }
    for (index = 0U; index < control->queue_count; ++index) {
        uint8_t queue_index = (uint8_t)(
            (control->queue_head + index) %
            ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
        if (control->queue[queue_index].client == client &&
            control->queue[queue_index].target_slot == target_slot) {
            return 1U;
        }
    }
    return 0U;
}

static uint8_t control_remove_client_queue_locked(
    album_image_loader_control_t *control, void *client)
{
    uint8_t read_index = control->queue_head;
    uint8_t write_index = control->queue_head;
    uint8_t original_count = control->queue_count;
    uint8_t kept_count = 0U;
    uint8_t removed = 0U;
    uint8_t index;

    for (index = 0U; index < original_count; ++index) {
        album_image_loader_control_job_t job = control->queue[read_index];
        read_index = (uint8_t)((read_index + 1U) %
                               ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
        if (job.client == client) {
            removed = 1U;
            continue;
        }
        control->queue[write_index] = job;
        write_index = (uint8_t)((write_index + 1U) %
                                ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
        kept_count++;
    }
    control->queue_count = kept_count;
    /* Cancellation drops requests, not ownership; release owns the later
     * slot handoff once no active/completion state remains. */
    if (kept_count == 0U) control->queue_head = write_index;
    control->queue_tail = write_index;
    return removed;
}

static void control_discard_completion_locked(
    album_image_loader_control_t *control)
{
    void *client;

    if (control->job_state !=
        ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING) {
        return;
    }
    client = control->active_job.client;
    control_clear_active_locked(control);
    control_finish_client_locked(control, client);
}

static int control_begin_stop_locked(
    album_image_loader_control_t *control,
    void *client,
    uint8_t release,
    uint8_t *wait_for_job)
{
    album_image_loader_control_client_t *entry =
        control_find_client_locked(control, client);

    *wait_for_job = 0U;
    if (entry == NULL && release) {
        entry = control_ensure_client_locked(control, client);
    }
    if (entry == NULL) return RET_ERR;
    if (release) {
        entry->state = ALBUM_IMAGE_LOADER_CONTROL_CLIENT_RELEASING;
    } else if (entry->state ==
               ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY) {
        entry->state = ALBUM_IMAGE_LOADER_CONTROL_CLIENT_CANCELLING;
    }
    (void)control_remove_client_queue_locked(control, client);
    if (control->active_job.client == client) {
        if (control->job_state ==
            ALBUM_IMAGE_LOADER_CONTROL_JOB_RUNNING) {
            *wait_for_job = 1U;
        } else if (control->job_state ==
                   ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING) {
            control_discard_completion_locked(control);
        }
    }
    control_finish_client_locked(control, client);
    return RET_OK;
}

int album_image_loader_control_init(album_image_loader_control_t *control)
{
    if (control == NULL) return RET_ERR;
    memset(control, 0, sizeof(*control));
    control->active_job.target_slot =
        ALBUM_IMAGE_LOADER_CONTROL_INVALID_SLOT;
    if (os_mutex_init(&control->lock) != RET_OK) return RET_ERR;
    control->mutex_ready = 1U;
    return RET_OK;
}

void album_image_loader_control_deinit(album_image_loader_control_t *control)
{
    if (control == NULL) return;
    if (control->mutex_ready) os_mutex_del(&control->lock);
    memset(control, 0, sizeof(*control));
}

int album_image_loader_control_claim_slot(
    album_image_loader_control_t *control, void *owner, uint8_t slot)
{
    int result;

    if (control == NULL || owner == NULL ||
        slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return RET_ERR;
    }
    result = control_claim_slot_locked(control, owner, slot);
    os_mutex_unlock(&control->lock);
    return result;
}

uint8_t album_image_loader_control_slot_available(
    album_image_loader_control_t *control, const void *owner, uint8_t slot)
{
    uint8_t available;

    if (control == NULL || owner == NULL ||
        slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    available = control_slot_available_locked(control, owner, slot);
    os_mutex_unlock(&control->lock);
    return available;
}

int album_image_loader_control_submit(
    album_image_loader_control_t *control,
    const ai_album_album_image_loader_request_t *request,
    uint32_t *generation)
{
    album_image_loader_control_job_t job;

    if (control == NULL || request == NULL || generation == NULL ||
        request->path == NULL || request->completion_cb == NULL ||
        request->client == NULL ||
        request->target_slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return RET_ERR;
    }
    if (!control_slot_available_locked(control, request->client,
                                      request->target_slot) ||
        control_has_job_locked(control, request->client,
                               request->target_slot) ||
        control->queue_count >= ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH ||
        control_claim_slot_locked(control, request->client,
                                  request->target_slot) != RET_OK) {
        os_mutex_unlock(&control->lock);
        return RET_ERR;
    }
    memset(&job, 0, sizeof(job));
    os_strncpy(job.path, request->path, sizeof(job.path) - 1U);
    job.max_width = request->max_width;
    job.max_height = request->max_height;
    job.target_offset = request->target_offset;
    job.target_slot = request->target_slot;
    job.completion_cb = request->completion_cb;
    job.client = request->client;
    job.generation = control_next_generation_locked(control);
    control->queue[control->queue_tail] = job;
    control->queue_tail = (uint8_t)((control->queue_tail + 1U) %
                                    ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
    control->queue_count++;
    *generation = job.generation;
    os_mutex_unlock(&control->lock);
    return RET_OK;
}

int album_image_loader_control_begin_stop(
    album_image_loader_control_t *control,
    void *client,
    uint8_t release,
    uint8_t *wait_for_job)
{
    int result;

    if (control == NULL || client == NULL || wait_for_job == NULL ||
        !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return RET_ERR;
    }
    result = control_begin_stop_locked(control, client, release,
                                       wait_for_job);
    os_mutex_unlock(&control->lock);
    return result;
}

int album_image_loader_control_begin_shutdown(
    album_image_loader_control_t *control)
{
    uint8_t index;

    if (control == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return RET_ERR;
    }
    control->queue_count = 0U;
    control->queue_head = control->queue_tail;
    for (index = 0U; index < ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT;
         ++index) {
        if (control->clients[index].client != NULL) {
            control->clients[index].state =
                ALBUM_IMAGE_LOADER_CONTROL_CLIENT_RELEASING;
        }
    }
    if (control->job_state ==
        ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING) {
        control_discard_completion_locked(control);
    }
    for (index = 0U; index < ALBUM_IMAGE_LOADER_CONTROL_CLIENT_COUNT;
         ++index) {
        if (control->clients[index].client != NULL) {
            control_finish_client_locked(control,
                                         control->clients[index].client);
        }
    }
    os_mutex_unlock(&control->lock);
    return RET_OK;
}

uint8_t album_image_loader_control_client_running(
    album_image_loader_control_t *control, const void *client)
{
    uint8_t running;

    if (control == NULL || client == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    running = (uint8_t)(control->job_state ==
                            ALBUM_IMAGE_LOADER_CONTROL_JOB_RUNNING &&
                        control->active_job.client == client);
    os_mutex_unlock(&control->lock);
    return running;
}

uint8_t album_image_loader_control_take_job(
    album_image_loader_control_t *control,
    album_image_loader_control_job_t *job)
{
    uint8_t available = 0U;

    if (control == NULL || job == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    while (control->job_state == ALBUM_IMAGE_LOADER_CONTROL_JOB_IDLE &&
           control->queue_count > 0U) {
        album_image_loader_control_client_t *entry;
        *job = control->queue[control->queue_head];
        control->queue_head = (uint8_t)((control->queue_head + 1U) %
                                        ALBUM_IMAGE_LOADER_CONTROL_QUEUE_DEPTH);
        control->queue_count--;
        entry = control_find_client_locked(control, job->client);
        if (job->target_slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
            entry == NULL || entry->state !=
                ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY ||
            control->slot_owner[job->target_slot] != job->client ||
            control_slot_busy_locked(control, job->target_slot)) {
            os_printf("[ALBUM_JPEG] drop stale job gen=%u slot=%u\r\n",
                      (unsigned)job->generation,
                      (unsigned)job->target_slot);
            control_finish_client_locked(control, job->client);
            continue;
        }
        control->active_job = *job;
        control->job_state = ALBUM_IMAGE_LOADER_CONTROL_JOB_RUNNING;
        available = 1U;
    }
    os_mutex_unlock(&control->lock);
    return available;
}

uint8_t album_image_loader_control_job_is_current(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job)
{
    album_image_loader_control_client_t *entry;
    uint8_t current = 0U;

    if (control == NULL || job == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    entry = control_find_client_locked(control, job->client);
    current = (uint8_t)(control_active_matches_locked(control, job) &&
                        entry != NULL &&
                        entry->state ==
                            ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY);
    os_mutex_unlock(&control->lock);
    return current;
}

uint8_t album_image_loader_control_publish(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job,
    const ai_album_album_image_loader_completion_t *completion)
{
    album_image_loader_control_client_t *entry;
    uint8_t published = 0U;
    if (control == NULL || job == NULL || completion == NULL ||
        !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    entry = control_find_client_locked(control, job->client);
    if (control_active_matches_locked(control, job) && entry != NULL &&
        entry->state == ALBUM_IMAGE_LOADER_CONTROL_CLIENT_READY) {
        control->completion = *completion;
        control->job_state =
            ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING;
        published = 1U;
    }
    os_mutex_unlock(&control->lock);
    return published;
}

void album_image_loader_control_finish_job(
    album_image_loader_control_t *control,
    const album_image_loader_control_job_t *job)
{
    if (control == NULL || job == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return;
    }
    if (control_active_matches_locked(control, job)) {
        control_clear_active_locked(control);
        control_finish_client_locked(control, job->client);
    }
    os_mutex_unlock(&control->lock);
}

uint8_t album_image_loader_control_take_completion(
    album_image_loader_control_t *control,
    ai_album_album_image_loader_completion_t *completion,
    ai_album_album_image_loader_completion_cb_t *callback,
    void **client)
{
    if (control == NULL || completion == NULL || callback == NULL ||
        client == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    if (control->job_state !=
        ALBUM_IMAGE_LOADER_CONTROL_JOB_COMPLETION_PENDING) {
        os_mutex_unlock(&control->lock);
        return 0U;
    }
    *completion = control->completion;
    *callback = control->active_job.completion_cb;
    *client = control->active_job.client;
    /* This atomic handoff is the completion consumption point.  A callback
     * may immediately enqueue the next generation for the same owner. */
    control_clear_active_locked(control);
    control_finish_client_locked(control, *client);
    os_mutex_unlock(&control->lock);
    return 1U;
}

uint8_t album_image_loader_control_has_queued(
    album_image_loader_control_t *control)
{
    uint8_t has_queued;

    if (control == NULL || !control->mutex_ready ||
        os_mutex_lock(&control->lock, osWaitForever) != RET_OK) {
        return 0U;
    }
    has_queued = (uint8_t)(control->queue_count > 0U);
    os_mutex_unlock(&control->lock);
    return has_queued;
}
