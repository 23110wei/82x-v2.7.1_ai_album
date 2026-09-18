#include "album/ai_album_album_image_ai.h"
#include "album/ai_album_album_image_ai_storage.h"

#include "av_mem.h"
#include "basic_include.h"
#include "brtc_agent/brtc_agent.h"
#include "fs/fatfs/osal_file.h"
#include "osal/mutex.h"

#define AI_ALBUM_ALBUM_IMAGE_AI_TIMEOUT_MS (120U * 1000U)
#define AI_ALBUM_ALBUM_IMAGE_AI_JPEG_PREFIX_MAX 16U

typedef struct {
    os_mutex_t lock;
    ai_album_album_image_ai_state_t state;
    ai_album_album_photo_t source;
    ai_album_album_photo_t result;
    uint8_t *source_jpeg;
    uint32_t source_jpeg_len;
    uint8_t *result_jpeg;
    uint32_t result_jpeg_len;
    uint32_t operation;
    uint32_t revision;
    uint64_t started_ms;
    uint8_t style;
    uint8_t initialized;
    uint8_t disable_pending;
    int error;
    uint8_t reason;
    char temporary_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
} image_ai_context_t;

static image_ai_context_t g_image_ai;

static const char *const g_image_ai_styles[AI_ALBUM_ALBUM_STYLE_COUNT] = {
    "WATERCOLOR", "OIL PAINT", "ANIME",
    "ABSTRACT EDITORIAL", "GATHERED ZINE", "HANDCRAFTED", "MINIMAL ZINE",
    "POSTCARD",
};

const char *ai_album_album_image_ai_style_name(uint8_t style)
{
    return style < AI_ALBUM_ALBUM_STYLE_COUNT ? g_image_ai_styles[style] : "";
}

static const char *const g_image_ai_prompts[AI_ALBUM_ALBUM_STYLE_COUNT] = {
    "请基于这张图片生成水彩画风格，保留主体和构图，不要输出文字。",
    "请基于这张图片生成油画风格，保留主体和构图，不要输出文字。",
    "请基于这张图片生成动漫风格，保留主体和构图，不要输出文字。",
    "将原图保留为视觉锚点，在下方创建极简抽象记忆面板，仅保留空间、色彩、结构关系。纯白无纹理底板，一个原创英文标题，衬线体。禁止重绘、滤镜或风格迁移。",
    "保留原图作为真实视觉锚点，用抽象插画场域重新诠释场景元素。压缩密集细节为少数安静形态，添加一个高饱和强调色作为构图结构，保留手工撕纸纤维边缘。微文字仅8个汉字以内。",
    "潦黑笔触、笨拙手型形状、大量留白、以物体为主导的叙事风格。将原图主体重新诠释为手写涂鸦风格的插画，歪扭的手写字体装饰，粗黑线条。",
    "温暖的扫描纸面，70-90%负空间，一个主体微小视觉事件。一个高饱和强调色（钴蓝/深蓝/青色/紫/品红/柠檬黄）主导，整体无边框，丝网/复印颗粒质感，排版如私人笔记。",
    "明信片正面：原图保持原始比例置于上方，下方留白区放置一个手绘主体元素和三个照片色卡。暖白象牙纸，轻微颗粒，手绘水彩/水粉/铅笔/剪纸风格。背面统一布局：薄边框、垂直分隔线、邮资框、地址行、大面积空白留言区。",
};

static const uint32_t g_image_ai_sky[AI_ALBUM_ALBUM_STYLE_COUNT] = {
    0x9CB9E7U, 0xB78D73U, 0x8B79C7U,
    0xF5F0E8U, 0xF5EDE6U, 0x2D2D2DU, 0xF5EDEAU,
    0xF7F4ECU,
};

static const uint32_t g_image_ai_ground[AI_ALBUM_ALBUM_STYLE_COUNT] = {
    0x6E84B8U, 0x725047U, 0x493B83U,
    0xE8E0D0U, 0xE8E0D6U, 0xF5F5F0U, 0x1A1A2EU,
    0xF0EDE4U,
};

static const uint32_t g_image_ai_accent[AI_ALBUM_ALBUM_STYLE_COUNT] = {
    0xF4D58AU, 0xE5B06AU, 0xF3A9D1U,
    0x6B5B4FU, 0xC73E1DU, 0x4A7B3EU, 0xE74C3CU,
    0x5B6E7EU,
};

static void image_ai_bump_revision_locked(void)
{
    ++g_image_ai.revision;
    if (g_image_ai.revision == 0U) {
        g_image_ai.revision = 1U;
    }
}

static uint32_t image_ai_next_operation_locked(void)
{
    ++g_image_ai.operation;
    if (g_image_ai.operation == 0U) {
        g_image_ai.operation = 1U;
    }
    return g_image_ai.operation;
}

static void image_ai_release_source(uint8_t **source)
{
    if (source != NULL && *source != NULL) {
        av_mem_free_psram(*source);
        *source = NULL;
    }
}

static uint8_t image_ai_request_in_flight(void)
{
    uint8_t in_flight;

    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    in_flight = (uint8_t)(g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING ||
                          g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_WAITING ||
                          g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_WRITING);
    os_mutex_unlock(&g_image_ai.lock);
    return in_flight;
}

/* A failure that never reached the agent still has to be visible in the
 * snapshot, otherwise the UI can only show a generic message. */
static void image_ai_record_failure(uint8_t reason)
{
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
    g_image_ai.error = BRTC_AGENT_ERR_ENGINE;
    g_image_ai.reason = reason;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
}

void ai_album_album_image_ai_init(void)
{
    if (g_image_ai.initialized) {
        return;
    }
    os_memset(&g_image_ai, 0, sizeof(g_image_ai));
    if (os_mutex_init(&g_image_ai.lock) != RET_OK) {
        return;
    }
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_IDLE;
    g_image_ai.revision = 1U;
    g_image_ai.initialized = 1U;
}

void ai_album_album_image_ai_deinit(void)
{
    if (!g_image_ai.initialized) {
        return;
    }
    ai_album_album_image_ai_cancel();
    (void)os_mutex_del(&g_image_ai.lock);
    os_memset(&g_image_ai, 0, sizeof(g_image_ai));
}

static int image_ai_start_submit(uint32_t operation, const char *prompt,
                                 uint8_t *jpeg, uint32_t jpeg_len)
{
    int ret = brtc_agent_send_image_generation(jpeg, jpeg_len, prompt);
    uint8_t *stale_source = NULL;
    uint8_t disable_pending = 0U;

    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.operation != operation) {
        os_mutex_unlock(&g_image_ai.lock);
        return RET_ERR;
    }
    if (ret == BRTC_AGENT_OK) {
        if (g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING) {
            g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_WAITING;
            image_ai_bump_revision_locked();
        }
        os_mutex_unlock(&g_image_ai.lock);
        return RET_OK;
    }
    if (g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING) {
        os_mutex_unlock(&g_image_ai.lock);
        return RET_ERR;
    }
    stale_source = g_image_ai.source_jpeg;
    g_image_ai.source_jpeg = NULL;
    g_image_ai.source_jpeg_len = 0U;
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
    g_image_ai.error = ret;
    g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_SERVICE;
    g_image_ai.disable_pending = 1U;
    disable_pending = 1U;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
    image_ai_release_source(&stale_source);
    os_printf("[ALBUM_AI] submit failed ret=%d prompt=%u\r\n", ret,
              (unsigned)(prompt != NULL ? os_strlen(prompt) : 0U));
    if (disable_pending) {
        (void)brtc_agent_exit_image_generation();
    }
    return RET_ERR;
}

int ai_album_album_image_ai_start(const ai_album_album_photo_t *source,
                                  uint8_t style)
{
    ai_album_album_image_ai_jpeg_info_t info;
    uint8_t *jpeg = NULL;
    uint32_t jpeg_len = 0U;
    uint32_t operation;
    char temporary_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    int ret;

    if (!source || !source->storage_backed || source->path[0] == '\0' ||
        style >= AI_ALBUM_ALBUM_STYLE_COUNT) {
        return RET_ERR;
    }
    ai_album_album_image_ai_init();
    if (!g_image_ai.initialized) {
        return RET_ERR;
    }
    /* One generation at a time: the agent protocol carries no request id, so a
     * second submit would race the first response with no way to tell them
     * apart.  Callers keep showing the in-flight progress instead. */
    if (image_ai_request_in_flight()) {
        return AI_ALBUM_ALBUM_IMAGE_AI_BUSY;
    }
    if (ai_album_album_image_ai_ensure_directory() != RET_OK) {
        return RET_ERR;
    }
    ai_album_album_image_ai_cancel();
    ret = ai_album_album_image_ai_load_file(source->path, &jpeg, &jpeg_len,
                                            &info);
    if (ret != RET_OK) {
        image_ai_release_source(&jpeg);
        image_ai_record_failure(
            info.status == AI_ALBUM_ALBUM_IMAGE_AI_JPEG_UNSUPPORTED ?
                AI_ALBUM_ALBUM_IMAGE_AI_REASON_FORMAT :
                AI_ALBUM_ALBUM_IMAGE_AI_REASON_WRITE);
        return RET_ERR;
    }
    if (ai_album_album_image_ai_make_path(temporary_path,
                                          sizeof(temporary_path), "TMP") !=
        RET_OK) {
        image_ai_release_source(&jpeg);
        image_ai_record_failure(AI_ALBUM_ALBUM_IMAGE_AI_REASON_WRITE);
        return RET_ERR;
    }
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    g_image_ai.source = *source;
    g_image_ai.source_jpeg = jpeg;
    g_image_ai.source_jpeg_len = jpeg_len;
    g_image_ai.result_jpeg = NULL;
    g_image_ai.result_jpeg_len = 0U;
    g_image_ai.style = style;
    g_image_ai.error = 0;
    g_image_ai.result = (ai_album_album_photo_t){0};
    os_strncpy(g_image_ai.temporary_path, temporary_path,
               sizeof(g_image_ai.temporary_path) - 1U);
    g_image_ai.temporary_path[sizeof(g_image_ai.temporary_path) - 1U] = '\0';
    operation = image_ai_next_operation_locked();
    g_image_ai.started_ms = os_mseconds();
    g_image_ai.disable_pending = 0U;
    g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE;
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
    os_printf("[ALBUM_AI] submit source=%s size=%u input=%ux%u style=%s\r\n",
              source->path, (unsigned)jpeg_len, (unsigned)info.width,
              (unsigned)info.height, g_image_ai_styles[style]);
    return image_ai_start_submit(operation, g_image_ai_prompts[style], jpeg,
                                 jpeg_len);
}

void ai_album_album_image_ai_cancel(void)
{
    uint8_t *source = NULL;
    uint8_t *result = NULL;
    char temporary_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX] = {0};
    ai_album_album_image_ai_state_t state;
    uint8_t disable_pending;

    if (!g_image_ai.initialized) {
        return;
    }
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    state = g_image_ai.state;
    if (state == AI_ALBUM_ALBUM_IMAGE_AI_SAVING ||
        (state == AI_ALBUM_ALBUM_IMAGE_AI_WRITING &&
         g_image_ai.result_jpeg == NULL)) {
        os_mutex_unlock(&g_image_ai.lock);
        return;
    }
    disable_pending = g_image_ai.disable_pending;
    source = g_image_ai.source_jpeg;
    g_image_ai.source_jpeg = NULL;
    g_image_ai.source_jpeg_len = 0U;
    result = g_image_ai.result_jpeg;
    g_image_ai.result_jpeg = NULL;
    g_image_ai.result_jpeg_len = 0U;
    os_strncpy(temporary_path, g_image_ai.temporary_path,
               sizeof(temporary_path) - 1U);
    g_image_ai.temporary_path[0] = '\0';
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_IDLE;
    g_image_ai.error = 0;
    g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE;
    g_image_ai.started_ms = 0U;
    g_image_ai.disable_pending = 0U;
    os_memset(&g_image_ai.result, 0, sizeof(g_image_ai.result));
    image_ai_next_operation_locked();
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
    image_ai_release_source(&source);
    image_ai_release_source(&result);
    if (disable_pending ||
        (state != AI_ALBUM_ALBUM_IMAGE_AI_IDLE &&
         state != AI_ALBUM_ALBUM_IMAGE_AI_SAVED)) {
        (void)brtc_agent_exit_image_generation();
    }
    if (temporary_path[0] != '\0' &&
        state != AI_ALBUM_ALBUM_IMAGE_AI_SAVED) {
        (void)osal_unlink(temporary_path);
    }
}

static void image_ai_fill_result_locked(
    const ai_album_album_image_ai_jpeg_info_t *info,
                                        uint32_t length)
{
    os_snprintf(g_image_ai.result.name, sizeof(g_image_ai.result.name),
                "%s AI", g_image_ai.source.name);
    os_strncpy(g_image_ai.result.origin, g_image_ai_styles[g_image_ai.style],
               sizeof(g_image_ai.result.origin) - 1U);
    os_strncpy(g_image_ai.result.path, g_image_ai.temporary_path,
               sizeof(g_image_ai.result.path) - 1U);
    g_image_ai.result.file_size = length;
    g_image_ai.result.width = info->width;
    g_image_ai.result.height = info->height;
    g_image_ai.result.sky_color = g_image_ai_sky[g_image_ai.style];
    g_image_ai.result.ground_color = g_image_ai_ground[g_image_ai.style];
    g_image_ai.result.accent_color = g_image_ai_accent[g_image_ai.style];
    g_image_ai.result.generated = 1U;
    g_image_ai.result.storage_backed = 1U;
}

static void image_ai_handle_agent_error(const brtc_agent_event_t *event);

static int image_ai_find_jpeg_bounds(const uint8_t *data, uint32_t length,
                                     uint32_t *offset, uint32_t *jpeg_length)
{
    uint32_t start = 0U;
    uint32_t end = length;

    if (!data || !offset || !jpeg_length || length < 4U) {
        return RET_ERR;
    }
    while (start <= AI_ALBUM_ALBUM_IMAGE_AI_JPEG_PREFIX_MAX &&
           start + 1U < length) {
        if (data[start] == 0xFFU && data[start + 1U] == 0xD8U) {
            break;
        }
        if (data[start] != 0U) {
            return RET_ERR;
        }
        ++start;
    }
    if (start > AI_ALBUM_ALBUM_IMAGE_AI_JPEG_PREFIX_MAX ||
        start + 1U >= length || data[start] != 0xFFU ||
        data[start + 1U] != 0xD8U) {
        return RET_ERR;
    }
    while (end > start + 1U) {
        if (data[end - 2U] == 0xFFU && data[end - 1U] == 0xD9U) {
            *offset = start;
            *jpeg_length = end - start;
            return RET_OK;
        }
        --end;
    }
    return RET_ERR;
}

static void image_ai_handle_video(const brtc_agent_event_t *event)
{
    brtc_agent_event_t error_event = {
        .code = BRTC_AGENT_ERR_ENGINE,
    };
    uint8_t *result;
    uint32_t jpeg_offset;
    uint32_t raw_length;
    uint32_t result_length;

    if (!event->data || event->data_len == 0U ||
        event->data_len > AI_ALBUM_ALBUM_IMAGE_AI_MAX_JPEG_SIZE) {
        os_printf("[ALBUM_AI] invalid JPEG envelope length=%u\r\n",
                  (unsigned)event->data_len);
        image_ai_handle_agent_error(&error_event);
        return;
    }
    raw_length = (uint32_t)event->data_len;
    if (image_ai_find_jpeg_bounds(event->data, raw_length, &jpeg_offset,
                                  &result_length) != RET_OK ||
        result_length > AI_ALBUM_ALBUM_IMAGE_AI_MAX_JPEG_SIZE) {
        os_printf("[ALBUM_AI] invalid JPEG boundaries raw=%u\r\n",
                  (unsigned)raw_length);
        image_ai_handle_agent_error(&error_event);
        return;
    }
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING &&
        g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_WAITING) {
        os_mutex_unlock(&g_image_ai.lock);
        return;
    }
    os_mutex_unlock(&g_image_ai.lock);
    result = av_mem_alloc_psram((result_length + 3U) & ~3U);
    if (result == NULL) {
        error_event.code = BRTC_AGENT_ERR_NO_MEMORY;
        image_ai_handle_agent_error(&error_event);
        return;
    }
    os_memcpy(result, event->data + jpeg_offset, result_length);
    os_printf("[ALBUM_AI] JPEG normalized raw=%u prefix=%u jpeg=%u "
              "trailing=%u\r\n",
              (unsigned)raw_length, (unsigned)jpeg_offset,
              (unsigned)result_length,
              (unsigned)(raw_length - jpeg_offset - result_length));
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING &&
        g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_WAITING) {
        os_mutex_unlock(&g_image_ai.lock);
        image_ai_release_source(&result);
        return;
    }
    g_image_ai.result_jpeg = result;
    g_image_ai.result_jpeg_len = result_length;
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_WRITING;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
}

static void image_ai_process_pending_result(void)
{
    ai_album_album_image_ai_jpeg_info_t info;
    uint8_t *result = NULL;
    uint8_t *source = NULL;
    uint32_t result_length;
    uint32_t operation;
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX] = {0};
    int write_result;

    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_WRITING ||
        g_image_ai.result_jpeg == NULL || g_image_ai.result_jpeg_len == 0U) {
        os_mutex_unlock(&g_image_ai.lock);
        return;
    }
    operation = g_image_ai.operation;
    result = g_image_ai.result_jpeg;
    result_length = g_image_ai.result_jpeg_len;
    g_image_ai.result_jpeg = NULL;
    g_image_ai.result_jpeg_len = 0U;
    os_strncpy(path, g_image_ai.temporary_path, sizeof(path) - 1U);
    os_mutex_unlock(&g_image_ai.lock);

    write_result = ai_album_album_image_ai_write_result(
        path, result, result_length, &info);
    image_ai_release_source(&result);
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.operation != operation ||
        g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_WRITING) {
        os_mutex_unlock(&g_image_ai.lock);
        if (path[0] != '\0') {
            (void)osal_unlink(path);
        }
        return;
    }
    source = g_image_ai.source_jpeg;
    g_image_ai.source_jpeg = NULL;
    g_image_ai.source_jpeg_len = 0U;
    if (write_result == RET_OK) {
        image_ai_fill_result_locked(&info, result_length);
        g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_READY;
        g_image_ai.error = 0;
        g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE;
    } else {
        g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
        g_image_ai.error = BRTC_AGENT_ERR_ENGINE;
        g_image_ai.reason =
            info.status == AI_ALBUM_ALBUM_IMAGE_AI_JPEG_UNSUPPORTED ?
                AI_ALBUM_ALBUM_IMAGE_AI_REASON_FORMAT :
                AI_ALBUM_ALBUM_IMAGE_AI_REASON_WRITE;
    }
    g_image_ai.disable_pending = 1U;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
    image_ai_release_source(&source);
    if (write_result != RET_OK && path[0] != '\0') {
        (void)osal_unlink(path);
    }
}

static void image_ai_handle_agent_error(const brtc_agent_event_t *event)
{
    uint8_t *source = NULL;
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX] = {0};
    uint8_t active = 0U;

    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING ||
        g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_WAITING) {
        active = 1U;
        source = g_image_ai.source_jpeg;
        g_image_ai.source_jpeg = NULL;
        g_image_ai.source_jpeg_len = 0U;
        os_strncpy(path, g_image_ai.temporary_path, sizeof(path) - 1U);
        g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
        g_image_ai.error = event->code != 0 ?
                               event->code : BRTC_AGENT_ERR_ENGINE;
        g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_SERVICE;
        g_image_ai.disable_pending = 1U;
        image_ai_next_operation_locked();
        image_ai_bump_revision_locked();
    }
    os_mutex_unlock(&g_image_ai.lock);
    if (active) {
        image_ai_release_source(&source);
        if (path[0] != '\0') {
            (void)osal_unlink(path);
        }
    }
}

void ai_album_album_image_ai_handle_event(const brtc_agent_event_t *event)
{
    if (!event || !g_image_ai.initialized) {
        return;
    }
    if (event->type == BRTC_AGENT_EVENT_VIDEO_DATA &&
        event->media_type == BRTC_AGENT_MEDIA_JPEG) {
        image_ai_handle_video(event);
    } else if (event->type == BRTC_AGENT_EVENT_ERROR) {
        image_ai_handle_agent_error(event);
    } else if (event->type == BRTC_AGENT_EVENT_MEDIA_GENERATE_RESULT ||
               event->type == BRTC_AGENT_EVENT_MEDIA_GENERATE_ACK) {
        os_printf("[ALBUM_AI] media event type=%d text=%.*s\r\n",
                  (int)event->type, (int)event->text_len,
                  event->text ? event->text : "");
    }
}

void ai_album_album_image_ai_poll(void)
{
    uint8_t timed_out = 0U;
    uint8_t disable_pending = 0U;
    uint8_t *source = NULL;
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX] = {0};

    if (!g_image_ai.initialized) {
        return;
    }
    image_ai_process_pending_result();
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if ((g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SUBMITTING ||
         g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_WAITING) &&
        os_mseconds() - g_image_ai.started_ms >
            AI_ALBUM_ALBUM_IMAGE_AI_TIMEOUT_MS) {
        timed_out = 1U;
        source = g_image_ai.source_jpeg;
        g_image_ai.source_jpeg = NULL;
        os_strncpy(path, g_image_ai.temporary_path, sizeof(path) - 1U);
        g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
        g_image_ai.error = BRTC_AGENT_ERR_HTTP;
        g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_TIMEOUT;
        g_image_ai.disable_pending = 1U;
        image_ai_next_operation_locked();
        image_ai_bump_revision_locked();
    }
    disable_pending = g_image_ai.disable_pending;
    g_image_ai.disable_pending = 0U;
    os_mutex_unlock(&g_image_ai.lock);
    if (timed_out) {
        image_ai_release_source(&source);
        (void)osal_unlink(path);
        os_printf("[ALBUM_AI] generation timeout\r\n");
    }
    if (disable_pending) {
        (void)brtc_agent_exit_image_generation();
    }
}

int ai_album_album_image_ai_get_snapshot(
    ai_album_album_image_ai_snapshot_t *snapshot)
{
    if (!snapshot || !g_image_ai.initialized ||
        os_mutex_lock(&g_image_ai.lock, osWaitForever) != RET_OK) {
        return RET_ERR;
    }
    snapshot->state = g_image_ai.state;
    snapshot->revision = g_image_ai.revision;
    snapshot->error = g_image_ai.error;
    snapshot->reason = g_image_ai.reason;
    snapshot->style = g_image_ai.style;
    snapshot->result = g_image_ai.result;
    os_mutex_unlock(&g_image_ai.lock);
    return RET_OK;
}

int ai_album_album_image_ai_save(uint8_t *out_index, uint8_t *out_reason)
{
    ai_album_album_photo_t photo;
    char temporary_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX] = {0};
    char final_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
    uint32_t operation;
    uint8_t index = 0U;
    uint8_t renamed = 0U;
    uint8_t result_available = 1U;
    int store_result = RET_ERR;

    if (!out_index || !out_reason || !g_image_ai.initialized) {
        return RET_ERR;
    }
    *out_reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_NONE;
    /* The album index cannot grow past its table, and the ready result stays
     * usable, so report it instead of throwing the result away. */
    if (ai_album_album_store_count() >= AI_ALBUM_ALBUM_STORE_MAX_PHOTOS) {
        *out_reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_FULL;
        return RET_ERR;
    }
    if (ai_album_album_image_ai_ensure_directory() != RET_OK ||
        ai_album_album_image_ai_make_path(final_path, sizeof(final_path),
                                          "AI") != RET_OK) {
        *out_reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_SAVE;
        return RET_ERR;
    }
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.state != AI_ALBUM_ALBUM_IMAGE_AI_READY) {
        os_mutex_unlock(&g_image_ai.lock);
        return RET_ERR;
    }
    operation = g_image_ai.operation;
    os_strncpy(temporary_path, g_image_ai.temporary_path,
               sizeof(temporary_path) - 1U);
    if (temporary_path[0] == '\0' || g_image_ai.result.path[0] == '\0') {
        os_mutex_unlock(&g_image_ai.lock);
        return RET_ERR;
    }
    photo = g_image_ai.result;
    g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_SAVING;
    image_ai_bump_revision_locked();
    os_mutex_unlock(&g_image_ai.lock);
    os_strncpy(photo.path, final_path, sizeof(photo.path) - 1U);
    photo.path[sizeof(photo.path) - 1U] = '\0';
    if (osal_rename(temporary_path, final_path) == FR_OK) {
        renamed = 1U;
        store_result = ai_album_album_store_add_generated(&photo, &index);
    }
    if (!renamed || store_result != RET_OK) {
        if (renamed) {
            if (osal_rename(final_path, temporary_path) != FR_OK) {
                (void)osal_unlink(final_path);
                result_available = 0U;
            }
        }
        os_mutex_lock(&g_image_ai.lock, osWaitForever);
        if (g_image_ai.operation == operation &&
            g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SAVING) {
            /* Keep the result usable so the user can retry (free space, delete
             * a photo) instead of losing a generated image to a failed save. */
            g_image_ai.state = result_available ?
                                   AI_ALBUM_ALBUM_IMAGE_AI_READY :
                                   AI_ALBUM_ALBUM_IMAGE_AI_ERROR;
            g_image_ai.error = result_available ? 0 : BRTC_AGENT_ERR_ENGINE;
            g_image_ai.reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_SAVE;
            g_image_ai.disable_pending = 1U;
            image_ai_bump_revision_locked();
        }
        os_mutex_unlock(&g_image_ai.lock);
        *out_reason = AI_ALBUM_ALBUM_IMAGE_AI_REASON_SAVE;
        return RET_ERR;
    }
    os_mutex_lock(&g_image_ai.lock, osWaitForever);
    if (g_image_ai.operation == operation &&
        g_image_ai.state == AI_ALBUM_ALBUM_IMAGE_AI_SAVING) {
        g_image_ai.result = photo;
        g_image_ai.state = AI_ALBUM_ALBUM_IMAGE_AI_SAVED;
        g_image_ai.disable_pending = 1U;
        os_strncpy(g_image_ai.temporary_path, final_path,
                   sizeof(g_image_ai.temporary_path) - 1U);
        image_ai_bump_revision_locked();
    }
    os_mutex_unlock(&g_image_ai.lock);
    *out_index = index;
    return RET_OK;
}
