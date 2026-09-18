#include "album/ai_album_album_image_loader.h"
#include "album/ai_album_album_store.h"
#include "ai_album_album_image_loader_control.h"
#include "ai_album_album_jpeg_hw.h"
#include "av_mem.h" /* 本工程垫片:映射到AV PSRAM/SRAM堆 */
#include "basic_include.h"
#include "fs/fatfs/osal_file.h"
#include "osal/event.h"
#include "osal/task.h"

#define ALBUM_IMAGE_LOADER_MAX_WIDTH \
    AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_WIDTH
#define ALBUM_IMAGE_LOADER_MAX_HEIGHT \
    AI_ALBUM_ALBUM_IMAGE_LOADER_MAX_HEIGHT
#define ALBUM_IMAGE_LOADER_RGB_BYTES \
    AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_BYTES
#define ALBUM_IMAGE_LOADER_MAX_JPEG_SIZE 0xFFFFCU
#define ALBUM_IMAGE_LOADER_READ_CHUNK_SIZE (32U * 1024U)
#define ALBUM_IMAGE_LOADER_TASK_STACK_SIZE (8U * 1024U)
#define ALBUM_IMAGE_LOADER_REQUEST_EVENT BIT(0)
#define ALBUM_IMAGE_LOADER_STOP_EVENT BIT(1)
#define ALBUM_IMAGE_LOADER_EXIT_EVENT BIT(0)
#define ALBUM_IMAGE_LOADER_CANCEL_WAIT_MS 4000U

typedef struct {
    uint8_t *data;
    uint32_t data_size;
    uint32_t dma_size;
} album_image_loader_jpeg_t;

typedef struct {
    album_image_loader_control_t control;
    os_event_t request_event;
    void *task_handle;
    void *task_stack;
    os_event_t exit_event;
    uint16_t *rgb565[AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT];
    uint8_t event_ready;
    uint8_t exit_event_ready;
    volatile uint8_t shutdown_requested;
    uint8_t initialized;
} album_image_loader_state_t;

static album_image_loader_state_t g_album_image_loader;
static void album_image_loader_task(void *arg);

static int album_image_loader_check_buffers(void)
{
    uint8_t slot;

    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        if (g_album_image_loader.rgb565[slot] == NULL) {
            os_printf("[ALBUM_JPEG] loader alloc failed slot=%u bytes=%u\r\n",
                      (unsigned)slot, (unsigned)ALBUM_IMAGE_LOADER_RGB_BYTES);
            return RET_ERR;
        }
    }
    return RET_OK;
}

static void album_image_loader_cleanup_init(void)
{
    uint8_t slot;

    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        if (g_album_image_loader.rgb565[slot] != NULL) {
            av_mem_free_psram(g_album_image_loader.rgb565[slot]);
        }
    }
    if (g_album_image_loader.task_stack != NULL) {
        os_free_psram(g_album_image_loader.task_stack);
    }
    if (g_album_image_loader.event_ready) {
        os_event_del(&g_album_image_loader.request_event);
    }
    if (g_album_image_loader.exit_event_ready) {
        os_event_del(&g_album_image_loader.exit_event);
    }
    album_image_loader_control_deinit(&g_album_image_loader.control);
    memset(&g_album_image_loader, 0, sizeof(g_album_image_loader));
}

static int album_image_loader_init(void)
{
    uint8_t slot;

    if (g_album_image_loader.initialized) return RET_OK;
    if (album_image_loader_control_init(&g_album_image_loader.control) !=
        RET_OK) {
        return RET_ERR;
    }
    if (os_event_init(&g_album_image_loader.request_event) != RET_OK) {
        album_image_loader_cleanup_init();
        return RET_ERR;
    }
    g_album_image_loader.event_ready = 1U;
    if (os_event_init(&g_album_image_loader.exit_event) != RET_OK) {
        album_image_loader_cleanup_init();
        return RET_ERR;
    }
    g_album_image_loader.exit_event_ready = 1U;
    for (slot = 0U; slot < AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT; ++slot) {
        g_album_image_loader.rgb565[slot] =
            av_mem_alloc_psram(ALBUM_IMAGE_LOADER_RGB_BYTES);
    }
    g_album_image_loader.task_stack =
        os_malloc_psram(ALBUM_IMAGE_LOADER_TASK_STACK_SIZE);
    if (album_image_loader_check_buffers() != RET_OK ||
        g_album_image_loader.task_stack == NULL) {
        if (g_album_image_loader.task_stack == NULL) {
            os_printf("[ALBUM_JPEG] loader stack alloc failed bytes=%u\r\n",
                      (unsigned)ALBUM_IMAGE_LOADER_TASK_STACK_SIZE);
        }
        album_image_loader_cleanup_init();
        return RET_ERR;
    }
    g_album_image_loader.task_handle = os_task_create(
        "album_jpeg", album_image_loader_task, NULL,
        OS_TASK_PRIORITY_BELOW_NORMAL, 0, g_album_image_loader.task_stack,
        ALBUM_IMAGE_LOADER_TASK_STACK_SIZE);
    if (g_album_image_loader.task_handle == NULL) {
        album_image_loader_cleanup_init();
        return RET_ERR;
    }
    g_album_image_loader.initialized = 1U;
    os_printf("ai_album: JPEG loader ready slots=%u max=%ux%u slot_bytes=%u "
              "total=%u stack=%u\r\n",
              (unsigned)AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT,
              (unsigned)ALBUM_IMAGE_LOADER_MAX_WIDTH,
              (unsigned)ALBUM_IMAGE_LOADER_MAX_HEIGHT,
              (unsigned)ALBUM_IMAGE_LOADER_RGB_BYTES,
              (unsigned)(ALBUM_IMAGE_LOADER_RGB_BYTES *
                         AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT),
              (unsigned)ALBUM_IMAGE_LOADER_TASK_STACK_SIZE);
    return RET_OK;
}

static uint8_t album_image_loader_request_valid(
    const ai_album_album_image_loader_request_t *request)
{
    uint32_t maximum_output_size;
    uint32_t path_length;

    if (request == NULL || request->path == NULL ||
        request->completion_cb == NULL || request->client == NULL ||
        request->target_slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        request->max_width < 4U || request->max_height < 2U ||
        request->max_width > ALBUM_IMAGE_LOADER_MAX_WIDTH ||
        request->max_height > ALBUM_IMAGE_LOADER_MAX_HEIGHT) {
        return 0U;
    }
    maximum_output_size = (uint32_t)request->max_width *
                          request->max_height * 2U;
    if ((request->target_offset &
         (AI_ALBUM_ALBUM_IMAGE_LOADER_TARGET_ALIGNMENT - 1U)) != 0U ||
        request->target_offset > ALBUM_IMAGE_LOADER_RGB_BYTES ||
        maximum_output_size > ALBUM_IMAGE_LOADER_RGB_BYTES -
                                  request->target_offset) {
        return 0U;
    }
    path_length = os_strlen(request->path);
    return (uint8_t)(path_length > 0U &&
                     path_length < AI_ALBUM_ALBUM_PHOTO_PATH_MAX);
}

static uint8_t album_image_loader_wait_for_stop(void *client)
{
    uint32_t waited_ms;

    for (waited_ms = 0U;
         waited_ms < ALBUM_IMAGE_LOADER_CANCEL_WAIT_MS; ++waited_ms) {
        if (!album_image_loader_control_client_running(
                &g_album_image_loader.control, client)) {
            return 0U;
        }
        os_sleep_ms(1U);
    }
    os_printf("[ALBUM_JPEG] stop wait timeout\r\n");
    return 1U;
}

static void album_image_loader_kick_worker(void)
{
    if (!album_image_loader_control_has_queued(
            &g_album_image_loader.control)) {
        return;
    }
    (void)os_event_set(&g_album_image_loader.request_event,
                       ALBUM_IMAGE_LOADER_REQUEST_EVENT, NULL);
}

int ai_album_album_image_loader_claim_slot(void *owner, uint8_t slot)
{
    int result;

    if (owner == NULL || slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT ||
        album_image_loader_init() != RET_OK) {
        return RET_ERR;
    }
    result = album_image_loader_control_claim_slot(
        &g_album_image_loader.control, owner, slot);
    if (result != RET_OK) {
        os_printf("[ALBUM_JPEG] slot unavailable slot=%u\r\n",
                  (unsigned)slot);
    }
    return result;
}

uint8_t ai_album_album_image_loader_slot_available(
    const void *owner, uint8_t slot)
{
    if (owner == NULL || slot >= AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT) {
        return 0U;
    }
    if (!g_album_image_loader.initialized) return 1U;
    return album_image_loader_control_slot_available(
        &g_album_image_loader.control, owner, slot);
}

void ai_album_album_image_loader_release_owner(void *owner)
{
    uint8_t wait_for_job;
    uint8_t timed_out;

    if (owner == NULL || !g_album_image_loader.initialized) return;
    if (album_image_loader_control_begin_stop(
            &g_album_image_loader.control, owner, 1U, &wait_for_job) !=
        RET_OK) {
        return;
    }
    timed_out = wait_for_job ? album_image_loader_wait_for_stop(owner) : 0U;
    if (!timed_out) album_image_loader_kick_worker();
}

void ai_album_album_image_loader_cancel(void *client)
{
    uint8_t wait_for_job;
    uint8_t timed_out;

    if (client == NULL || !g_album_image_loader.initialized) return;
    if (album_image_loader_control_begin_stop(
            &g_album_image_loader.control, client, 0U, &wait_for_job) !=
        RET_OK) {
        return;
    }
    timed_out = wait_for_job ? album_image_loader_wait_for_stop(client) : 0U;
    if (!timed_out) album_image_loader_kick_worker();
}

int ai_album_album_image_loader_submit(
    const ai_album_album_image_loader_request_t *request,
    uint32_t *generation)
{
    if (!album_image_loader_request_valid(request) || generation == NULL ||
        album_image_loader_init() != RET_OK) {
        return RET_ERR;
    }
    if (album_image_loader_control_submit(
            &g_album_image_loader.control, request, generation) != RET_OK) {
        os_printf("[ALBUM_JPEG] submit rejected slot=%u\r\n",
                  (unsigned)request->target_slot);
        return RET_ERR;
    }
    if (os_event_set(&g_album_image_loader.request_event,
                     ALBUM_IMAGE_LOADER_REQUEST_EVENT, NULL) != RET_OK) {
        /* The FIFO item is committed; poll() or a later submit retries the kick. */
        os_printf("[ALBUM_JPEG] worker kick deferred gen=%u\r\n",
                  (unsigned)*generation);
    }
    return RET_OK;
}

static uint8_t album_image_loader_job_is_current(
    const album_image_loader_control_job_t *job)
{
    if (g_album_image_loader.shutdown_requested) return 0U;
    return album_image_loader_control_job_is_current(
        &g_album_image_loader.control, job);
}

static uint8_t album_image_loader_job_cancelled(void *context)
{
    if (context == NULL) return 1U;
    return (uint8_t)!album_image_loader_job_is_current(
        (const album_image_loader_control_job_t *)context);
}

static ai_album_album_image_loader_result_t album_image_loader_read_file(
    const album_image_loader_control_job_t *job,
    album_image_loader_jpeg_t *jpeg,
    uint32_t *elapsed_us)
{
    F_FILE *file;
    uint32_t bytes_read;
    uint32_t offset = 0U;
    uint32_t remaining;
    uint64 start_us = os_useconds();
    memset(jpeg, 0, sizeof(*jpeg));
    file = osal_fopen(job->path, "rb");
    if (file == NULL) {
        *elapsed_us = (uint32_t)(os_useconds() - start_us);
        return AI_ALBUM_ALBUM_IMAGE_LOADER_FILE_UNAVAILABLE;
    }
    jpeg->data_size = osal_fsize(file);
    if (jpeg->data_size == 0U ||
        jpeg->data_size > ALBUM_IMAGE_LOADER_MAX_JPEG_SIZE) {
        osal_fclose(file);
        *elapsed_us = (uint32_t)(os_useconds() - start_us);
        return AI_ALBUM_ALBUM_IMAGE_LOADER_FORMAT_UNSUPPORTED;
    }
    jpeg->dma_size = (jpeg->data_size + 3U) & ~3U;
    jpeg->data = av_mem_alloc_psram(jpeg->dma_size);
    if (jpeg->data == NULL) {
        osal_fclose(file);
        *elapsed_us = (uint32_t)(os_useconds() - start_us);
        return AI_ALBUM_ALBUM_IMAGE_LOADER_DECODE_FAILED;
    }
    remaining = jpeg->data_size;
    while (remaining > 0U) {
        uint32_t chunk = remaining > ALBUM_IMAGE_LOADER_READ_CHUNK_SIZE ?
                         ALBUM_IMAGE_LOADER_READ_CHUNK_SIZE : remaining;
        if (!album_image_loader_job_is_current(job)) {
            osal_fclose(file);
            av_mem_free_psram(jpeg->data);
            memset(jpeg, 0, sizeof(*jpeg));
            *elapsed_us = (uint32_t)(os_useconds() - start_us);
            return AI_ALBUM_ALBUM_IMAGE_LOADER_DECODE_FAILED;
        }
        bytes_read = osal_fread(jpeg->data + offset, 1U, chunk, file);
        if (bytes_read != chunk) {
            osal_fclose(file);
            av_mem_free_psram(jpeg->data);
            memset(jpeg, 0, sizeof(*jpeg));
            *elapsed_us = (uint32_t)(os_useconds() - start_us);
            return AI_ALBUM_ALBUM_IMAGE_LOADER_FILE_UNAVAILABLE;
        }
        offset += bytes_read;
        remaining -= bytes_read;
    }
    osal_fclose(file);
    if (jpeg->dma_size > jpeg->data_size) {
        memset(jpeg->data + jpeg->data_size, 0,
               jpeg->dma_size - jpeg->data_size);
    }
    *elapsed_us = (uint32_t)(os_useconds() - start_us);
    return AI_ALBUM_ALBUM_IMAGE_LOADER_OK;
}
static ai_album_album_image_loader_result_t album_image_loader_map_hw_result(
    ai_album_album_jpeg_hw_result_t result)
{
    if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
        return AI_ALBUM_ALBUM_IMAGE_LOADER_OK;
    }
    if (result == AI_ALBUM_ALBUM_JPEG_HW_INVALID ||
        result == AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED) {
        return AI_ALBUM_ALBUM_IMAGE_LOADER_FORMAT_UNSUPPORTED;
    }
    return AI_ALBUM_ALBUM_IMAGE_LOADER_DECODE_FAILED;
}
static ai_album_album_image_loader_result_t album_image_loader_decode(
    const album_image_loader_control_job_t *job,
    const album_image_loader_jpeg_t *jpeg,
    ai_album_album_image_loader_completion_t *completion,
    ai_album_album_jpeg_hw_output_t *hardware_output)
{
    uint8_t *slot_base =
        (uint8_t *)g_album_image_loader.rgb565[job->target_slot];
    uint16_t *target = (uint16_t *)(slot_base + job->target_offset);
    ai_album_album_jpeg_hw_input_t input = {
        .jpeg_data = jpeg->data,
        .jpeg_data_size = jpeg->data_size,
        .jpeg_dma_size = jpeg->dma_size,
        .rgb565 = target,
        .max_width = job->max_width,
        .max_height = job->max_height,
        .cancel_cb = album_image_loader_job_cancelled,
        .cancel_context = (void *)job,
    };
    ai_album_album_jpeg_hw_result_t hardware_result =
        ai_album_album_jpeg_hw_decode(&input, hardware_output);
    ai_album_album_image_loader_result_t result =
        album_image_loader_map_hw_result(hardware_result);
    if (result == AI_ALBUM_ALBUM_IMAGE_LOADER_OK) {
        completion->data = (const uint8_t *)target;
        completion->data_size = hardware_output->data_size;
        completion->stride = hardware_output->stride;
        completion->width = hardware_output->width;
        completion->height = hardware_output->height;
    }
    return result;
}
static void album_image_loader_process_job(
    const album_image_loader_control_job_t *job)
{
    album_image_loader_jpeg_t jpeg;
    ai_album_album_jpeg_hw_output_t hardware_output;
    ai_album_album_image_loader_completion_t completion;
    ai_album_album_image_loader_result_t result;
    uint32_t file_us = 0U;
    uint64 start_us = os_useconds();

    memset(&completion, 0, sizeof(completion));
    memset(&hardware_output, 0, sizeof(hardware_output));
    completion.generation = job->generation;
    completion.target_offset = job->target_offset;
    completion.target_slot = job->target_slot;
    result = album_image_loader_read_file(job, &jpeg, &file_us);
    if (result == AI_ALBUM_ALBUM_IMAGE_LOADER_OK &&
        album_image_loader_job_is_current(job)) {
        result = album_image_loader_decode(job, &jpeg, &completion,
                                           &hardware_output);
    }
    if (jpeg.data != NULL) av_mem_free_psram(jpeg.data);
    if (!album_image_loader_job_is_current(job)) {
        /* Even a cancelled decode must retire RUNNING, otherwise its slot
         * remains permanently busy and a release cannot complete. */
        album_image_loader_control_finish_job(
            &g_album_image_loader.control, job);
        return;
    }

    completion.result = result;
    os_printf("[ALBUM_JPEG] gen=%u slot=%u offset=%u result=%d file=%uus "
              "hw=%uus rgb=%uus total=%uus out=%ux%u path=%s\r\n",
              (unsigned)job->generation, (unsigned)job->target_slot,
              (unsigned)job->target_offset, (int)result, (unsigned)file_us,
              (unsigned)hardware_output.hardware_us,
              (unsigned)hardware_output.convert_us,
              (unsigned)(os_useconds() - start_us),
              (unsigned)completion.width, (unsigned)completion.height,
              job->path);
    if (!album_image_loader_control_publish(
            &g_album_image_loader.control, job, &completion)) {
        /* Cancellation may win between the last check and publish. */
        album_image_loader_control_finish_job(
            &g_album_image_loader.control, job);
    }
}

static void album_image_loader_task(void *arg)
{
    (void)arg;
    for (;;) {
        album_image_loader_control_job_t job;
        if (os_event_wait(&g_album_image_loader.request_event,
                          ALBUM_IMAGE_LOADER_REQUEST_EVENT |
                              ALBUM_IMAGE_LOADER_STOP_EVENT, NULL,
                          OS_EVENT_WMODE_OR | OS_EVENT_WMODE_CLEAR,
                          osWaitForever) != RET_OK) {
            continue;
        }
        if (g_album_image_loader.shutdown_requested) break;
        while (album_image_loader_control_take_job(
            &g_album_image_loader.control, &job)) {
            album_image_loader_process_job(&job);
            if (g_album_image_loader.shutdown_requested) break;
        }
    }
    (void)os_event_set(&g_album_image_loader.exit_event,
                       ALBUM_IMAGE_LOADER_EXIT_EVENT, NULL);
}

void ai_album_album_image_loader_deinit(void)
{
    if (!g_album_image_loader.initialized) return;
    g_album_image_loader.shutdown_requested = 1U;
    if (album_image_loader_control_begin_shutdown(
            &g_album_image_loader.control) != RET_OK) {
        os_printf("[ALBUM_JPEG] shutdown control failed\r\n");
        return;
    }
    (void)os_event_set(&g_album_image_loader.request_event,
                       ALBUM_IMAGE_LOADER_STOP_EVENT, NULL);
    if (os_event_wait(&g_album_image_loader.exit_event,
                      ALBUM_IMAGE_LOADER_EXIT_EVENT, NULL,
                      OS_EVENT_WMODE_OR | OS_EVENT_WMODE_CLEAR,
                      ALBUM_IMAGE_LOADER_CANCEL_WAIT_MS) != RET_OK) {
        os_printf("[ALBUM_JPEG] worker exit timeout; buffers retained\r\n");
        return;
    }
    if (g_album_image_loader.task_handle != NULL) {
        (void)os_task_destroy(g_album_image_loader.task_handle);
    }
    album_image_loader_cleanup_init();
    os_printf("[ALBUM_JPEG] loader deinitialized\r\n");
}

void ai_album_album_image_loader_poll(void)
{
    ai_album_album_image_loader_completion_t completion;
    ai_album_album_image_loader_completion_cb_t callback = NULL;
    void *client = NULL;

    memset(&completion, 0, sizeof(completion));
    if (!g_album_image_loader.initialized) {
        return;
    }
    if (album_image_loader_control_take_completion(
            &g_album_image_loader.control, &completion, &callback,
            &client) && callback != NULL && client != NULL) {
        callback(&completion, client);
    }
    album_image_loader_kick_worker();
}
