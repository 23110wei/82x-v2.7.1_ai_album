#include "ai_album_album_jpeg_hw.h"

#include "av_mem.h" /* 本工程垫片:映射到AV PSRAM/SRAM堆 */
#include "basic_include.h"
#include "chip/txw82x/misc.h"
#include "dev.h"
#include "devid.h"
#include "dev/scale/hgscale.h"
#include "hal/jpeg.h"
#include "hal/scale.h"
#include "lib/lcd/lcd.h"
#include "lib/video/dvp/jpeg/jpg_common.h"
#include "osal/event.h"

#define ALBUM_JPEG_HW_WAIT_MS 3000
#define ALBUM_JPEG_LOCK_WAIT_MS 1000U
#define ALBUM_JPEG_CLOSE_WAIT_MS 1200U
#define ALBUM_JPEG_MAX_OUTPUT_WIDTH 1024U
#define ALBUM_JPEG_MAX_OUTPUT_HEIGHT 600U
#define ALBUM_JPEG_CANCEL_CHECK_ROWS 4U

#define ALBUM_JPEG_EVENT_SCALE_DONE BIT(0)
#define ALBUM_JPEG_EVENT_SCALE_ERROR BIT(1)
#define ALBUM_JPEG_EVENT_JPEG_ERROR BIT(2)
#define ALBUM_JPEG_EVENT_ALL (ALBUM_JPEG_EVENT_SCALE_DONE | \
                              ALBUM_JPEG_EVENT_SCALE_ERROR | \
                              ALBUM_JPEG_EVENT_JPEG_ERROR)

typedef struct {
    uint16_t width;
    uint16_t height;
} album_jpeg_dimensions_t;

typedef struct {
    uint32_t length;
    uint32_t end;
} album_jpeg_segment_t;

typedef struct {
    uint8_t *yuv;
    uint32_t yuv_size;
    uint8_t *line_y;
    uint8_t *line_u;
    uint8_t *line_v;
} album_jpeg_scratch_t;

typedef struct {
    struct jpg_device *jpg;
    struct scale_device *scale;
    const ai_album_album_jpeg_hw_input_t *input;
    const album_jpeg_dimensions_t *source;
    const ai_album_album_jpeg_hw_output_t *output;
    const album_jpeg_scratch_t *scratch;
    uint8_t lock_acquired;
    uint8_t jpg_opened;
    uint8_t scale_touched;
    uint8_t scale_done_irq;
    uint8_t scale_overflow_irq;
    uint8_t scale_error_irq;
    uint8_t jpg_error_irq;
    uint8_t jpg_timeout_irq;
} album_jpeg_session_t;

static os_event_t g_album_jpeg_event;
static uint8_t g_album_jpeg_event_ready;

static uint16_t album_jpeg_read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static ai_album_album_jpeg_hw_result_t album_jpeg_segment_read(
    const uint8_t *data,
    uint32_t size,
    uint32_t offset,
    album_jpeg_segment_t *segment)
{
    if (offset > size || size - offset < 2U || segment == NULL) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    segment->length = album_jpeg_read_be16(data + offset);
    if (segment->length < 2U || segment->length > size - offset) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    segment->end = offset + segment->length;
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static uint8_t album_jpeg_is_unsupported_sof(uint8_t marker)
{
    return (uint8_t)(marker == 0xC1U || marker == 0xC2U ||
                     marker == 0xC3U ||
                     (marker >= 0xC5U && marker <= 0xC7U) ||
                     (marker >= 0xC9U && marker <= 0xCBU) ||
                     (marker >= 0xCDU && marker <= 0xCFU));
}

static uint8_t album_jpeg_has_eoi(const uint8_t *data,
                                  uint32_t offset,
                                  uint32_t size)
{
    while (offset + 1U < size) {
        if (data[offset] == 0xFFU && data[offset + 1U] == 0xD9U) {
            return 1U;
        }
        offset++;
    }
    return 0U;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_parse_sof0(
    const uint8_t *data,
    uint32_t offset,
    const album_jpeg_segment_t *segment,
    album_jpeg_dimensions_t *dimensions)
{
    uint8_t component_count;
    uint8_t seen_components = 0U;
    uint8_t component_index;
    uint32_t minimum_length;

    if (segment->length < 8U || data[offset + 2U] != 8U) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
    }
    component_count = data[offset + 7U];
    minimum_length = 8U + (uint32_t)component_count * 3U;
    if (component_count != 3U || segment->length != minimum_length) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
    }
    for (component_index = 0U; component_index < component_count;
         ++component_index) {
        const uint8_t *component = data + offset + 8U +
                                   (uint32_t)component_index * 3U;
        uint8_t component_id = component[0];
        uint8_t sampling_factor = component[1];
        uint8_t component_bit;

        if (component_id < 1U || component_id > 3U) {
            return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
        }
        component_bit = (uint8_t)(1U << (component_id - 1U));
        if ((seen_components & component_bit) != 0U ||
            (component_id == 1U ? sampling_factor != 0x22U :
                                  sampling_factor != 0x11U)) {
            return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
        }
        seen_components = (uint8_t)(seen_components | component_bit);
    }
    if (seen_components != 0x07U) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
    }
    dimensions->height = album_jpeg_read_be16(data + offset + 3U);
    dimensions->width = album_jpeg_read_be16(data + offset + 5U);
    if (dimensions->width == 0U || dimensions->height == 0U) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_parse(
    const uint8_t *data,
    uint32_t size,
    album_jpeg_dimensions_t *dimensions)
{
    uint32_t offset = 2U;
    uint8_t found_sof0 = 0U;

    if (data == NULL || dimensions == NULL || size < 4U ||
        data[0] != 0xFFU || data[1] != 0xD8U) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    while (offset + 1U < size) {
        album_jpeg_segment_t segment;
        ai_album_album_jpeg_hw_result_t result;
        uint8_t marker;

        if (data[offset] != 0xFFU) return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
        while (offset < size && data[offset] == 0xFFU) offset++;
        if (offset >= size) return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
        marker = data[offset++];
        if (marker == 0x00U || marker == 0xD8U || marker == 0xD9U ||
            marker == 0x01U || (marker >= 0xD0U && marker <= 0xD7U)) {
            return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
        }
        if (album_jpeg_is_unsupported_sof(marker)) {
            return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
        }
        result = album_jpeg_segment_read(data, size, offset, &segment);
        if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
        if (marker == 0xC0U) {
            if (found_sof0) return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
            result = album_jpeg_parse_sof0(data, offset, &segment, dimensions);
            if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
            found_sof0 = 1U;
        }
        if (marker == 0xDAU) {
            return found_sof0 && segment.length >= 6U &&
                           album_jpeg_has_eoi(data, segment.end, size) ?
                       AI_ALBUM_ALBUM_JPEG_HW_OK :
                       AI_ALBUM_ALBUM_JPEG_HW_INVALID;
        }
        offset = segment.end;
    }
    return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
}

ai_album_album_jpeg_hw_result_t ai_album_album_jpeg_hw_probe(
    const uint8_t *data, uint32_t size, uint16_t *width, uint16_t *height)
{
    album_jpeg_dimensions_t dimensions;
    ai_album_album_jpeg_hw_result_t result;

    if (width == NULL || height == NULL) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    result = album_jpeg_parse(data, size, &dimensions);
    if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
        *width = dimensions.width;
        *height = dimensions.height;
    }
    return result;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_fit_dimensions(
    const album_jpeg_dimensions_t *source,
    uint16_t max_width,
    uint16_t max_height,
    ai_album_album_jpeg_hw_output_t *output)
{
    uint32_t width = (uint32_t)max_width & ~3U;
    uint32_t height = (uint32_t)max_height & ~1U;

    if (max_width > ALBUM_JPEG_MAX_OUTPUT_WIDTH ||
        max_height > ALBUM_JPEG_MAX_OUTPUT_HEIGHT ||
        width < 4U || height < 2U) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    if ((uint64_t)source->width * height >
        (uint64_t)source->height * width) {
        height = (uint32_t)((uint64_t)source->height * width /
                            source->width) & ~1U;
    } else {
        width = (uint32_t)((uint64_t)source->width * height /
                           source->height) & ~3U;
    }
    if (width < 4U || height < 2U) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
    }
    /* SCALE硬件缩小比上限约1/16:超过会触发错误中断(DMA_STA1错误)。
     * 此类高分辨率源无法在盒子内合规解码(放大目标会超出调用方缓冲区,
     * 造成堆越界,严禁),直接判不支持,由UI显示解码失败卡片 */
    if (source->width / 16U > width || source->height / 16U > height) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNSUPPORTED;
    }
    output->width = (uint16_t)width;
    output->height = (uint16_t)height;
    output->stride = width * 2U;
    output->data_size = output->stride * height;
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static void album_jpeg_scratch_free(album_jpeg_scratch_t *scratch)
{
    if (scratch->line_v != NULL) av_mem_free_sram(scratch->line_v);
    if (scratch->line_u != NULL) av_mem_free_sram(scratch->line_u);
    if (scratch->line_y != NULL) av_mem_free_sram(scratch->line_y);
    /* yuv为持久缓冲(scratch->yuv指向全局persistent_yuv),此处不释放 */
    memset(scratch, 0, sizeof(*scratch));
}

static ai_album_album_jpeg_hw_result_t album_jpeg_scratch_allocate(
    const ai_album_album_jpeg_hw_output_t *output,
    album_jpeg_scratch_t *scratch)
{
    uint32_t line_y_size = 0x20U + output->width +
                           17U * 4U * SRAMBUF_WLEN;
    uint32_t line_uv_size = 0x12U + output->width / 2U +
                            9U * 4U * SRAMBUF_WLEN;

    memset(scratch, 0, sizeof(*scratch));
    /* 硬件按MCU对齐行写入YUV(fitted高度非16倍数时会多写几行),
     * 必须按MCU对齐尺寸+4KB余量分配,否则越界写坏AV堆元数据
     * (实测导致timer_task野指针死机)。
     * 且暂存区为持久分配(首次按最大盒1024x608分配,此后复用):
     * 每次解码 alloc/free 大块会在 AV 堆中碎片化,导致后续
     * 满屏 YUV 分配失败、大图切换失效 */
    {
        static uint8_t *persistent_yuv = NULL;
        static uint32_t persistent_size = 0U;
        uint32_t mcu_w = ((output->width + 15U) & ~15U);
        uint32_t mcu_h = ((output->height + 15U) & ~15U);
        uint32_t need = mcu_w * mcu_h + mcu_w * mcu_h / 2U + 4096U;
        uint32_t max_need = (1024U * 608U) + (1024U * 608U) / 2U + 4096U;

        if (persistent_yuv == NULL) {
            persistent_yuv = av_mem_alloc_psram((int)max_need);
            if (persistent_yuv != NULL) {
                persistent_size = max_need;
            }
        }
        if (persistent_yuv == NULL || persistent_size < need) {
            return AI_ALBUM_ALBUM_JPEG_HW_NO_MEMORY;
        }
        scratch->yuv = persistent_yuv;
        scratch->yuv_size = need;
    }
    scratch->line_y = av_mem_alloc_sram(line_y_size);
    scratch->line_u = av_mem_alloc_sram(line_uv_size);
    scratch->line_v = av_mem_alloc_sram(line_uv_size);
    if (scratch->yuv == NULL || scratch->line_y == NULL ||
        scratch->line_u == NULL || scratch->line_v == NULL) {
        album_jpeg_scratch_free(scratch);
        return AI_ALBUM_ALBUM_JPEG_HW_NO_MEMORY;
    }
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static int32 album_jpeg_scale_done_isr(uint32 irq_flags,
                                       uint32 irq_data,
                                       uint32 param)
{
    (void)irq_flags;
    (void)param;
    return os_event_set((os_event_t *)(uintptr_t)irq_data,
                        ALBUM_JPEG_EVENT_SCALE_DONE, NULL);
}

static int32 album_jpeg_scale_error_isr(uint32 irq_flags,
                                        uint32 irq_data,
                                        uint32 param)
{
    (void)irq_flags;
    (void)param;
    return os_event_set((os_event_t *)(uintptr_t)irq_data,
                        ALBUM_JPEG_EVENT_SCALE_ERROR, NULL);
}

static int32 album_jpeg_error_isr(uint32 irq_flags,
                                  uint32 irq_data,
                                  uint32 param1,
                                  uint32 param2)
{
    (void)irq_flags;
    (void)param1;
    (void)param2;
    return os_event_set((os_event_t *)(uintptr_t)irq_data,
                        ALBUM_JPEG_EVENT_JPEG_ERROR, NULL);
}

static ai_album_album_jpeg_hw_result_t album_jpeg_event_init(void)
{
    if (g_album_jpeg_event_ready) return AI_ALBUM_ALBUM_JPEG_HW_OK;
    if (os_event_init(&g_album_jpeg_event) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNAVAILABLE;
    }
    g_album_jpeg_event_ready = 1U;
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_lock(
    album_jpeg_session_t *session)
{
    uint32_t waited_ms;

    for (waited_ms = 0U; waited_ms < ALBUM_JPEG_LOCK_WAIT_MS; ++waited_ms) {
        if (jpg_mutex_lock(JPGID1, JPG_LOCK_DECODE, NULL) == RET_OK) {
            session->lock_acquired = 1U;
            return AI_ALBUM_ALBUM_JPEG_HW_OK;
        }
        os_sleep_ms(1);
    }
    return AI_ALBUM_ALBUM_JPEG_HW_BUSY;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_configure_scale(
    album_jpeg_session_t *session)
{
    uint32_t pixels = (uint32_t)session->output->width *
                      session->output->height;

    if (scale_close(session->scale) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    }
    session->scale_touched = 1U;
    if (scale_set_input_stream(session->scale, MJPEG_DEC) != RET_OK ||
        scale_set_output_sram_or_frame(session->scale, 1U) != RET_OK ||
        scale_set_in_out_size(session->scale, session->source->width,
                              session->source->height,
                              session->output->width,
                              session->output->height) != RET_OK ||
        scale_set_step(session->scale, session->source->width,
                       session->source->height, session->output->width,
                       session->output->height) != RET_OK ||
        scale_set_start_addr(session->scale, 0U, 0U) != RET_OK ||
        scale_set_out_yaddr(session->scale,
                            (uint32_t)(uintptr_t)session->scratch->yuv) != RET_OK ||
        scale_set_out_uaddr(session->scale,
                            (uint32_t)(uintptr_t)(session->scratch->yuv +
                                                  pixels)) != RET_OK ||
        scale_set_out_vaddr(session->scale,
                            (uint32_t)(uintptr_t)(session->scratch->yuv +
                                                  pixels + pixels / 4U)) != RET_OK ||
        scale_set_srambuf_wlen(session->scale, SRAMBUF_WLEN) != RET_OK ||
        scale_linebuf_yuv_addr(
            session->scale,
            (uint32_t)(uintptr_t)session->scratch->line_y,
            (uint32_t)(uintptr_t)session->scratch->line_u,
            (uint32_t)(uintptr_t)session->scratch->line_v) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    }
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_register_irqs(
    album_jpeg_session_t *session)
{
    uint32_t event_address = (uint32_t)(uintptr_t)&g_album_jpeg_event;

    if (scale_request_irq(session->scale, FRAME_END,
                          album_jpeg_scale_done_isr,
                          event_address) != RET_OK) return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    session->scale_done_irq = 1U;
    if (scale_request_irq(session->scale, INBUF_OV,
                          album_jpeg_scale_error_isr,
                          event_address) != RET_OK) return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    session->scale_overflow_irq = 1U;
    if (scale_request_irq(session->scale, ERROR_PEND,
                          album_jpeg_scale_error_isr,
                          event_address) != RET_OK) return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    session->scale_error_irq = 1U;
    if (jpg_request_irq(session->jpg, album_jpeg_error_isr,
                        JPG_IRQ_FLAG_ERROR,
                        &g_album_jpeg_event) != RET_OK) return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    session->jpg_error_irq = 1U;
    if (jpg_request_irq(session->jpg, album_jpeg_error_isr,
                        JPG_IRQ_FLAG_TIME_OUT,
                        &g_album_jpeg_event) != RET_OK) return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    session->jpg_timeout_irq = 1U;
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static void album_jpeg_close_device(album_jpeg_session_t *session)
{
    uint32_t waited_ms;

    if (!session->jpg_opened) return;
    jpg_close(session->jpg);
    for (waited_ms = 0U; waited_ms < ALBUM_JPEG_CLOSE_WAIT_MS; ++waited_ms) {
        if (jpg_is_online(session->jpg) <= 0) return;
        os_sleep_ms(1);
    }
    os_printf("ai_album: JPEG close timeout\r\n");
}

static void album_jpeg_session_close(album_jpeg_session_t *session)
{
    if (session->scale_done_irq) scale_release_irq(session->scale, FRAME_END);
    if (session->scale_overflow_irq) scale_release_irq(session->scale, INBUF_OV);
    if (session->scale_error_irq) scale_release_irq(session->scale, ERROR_PEND);
    if (session->jpg_error_irq) jpg_release_irq(session->jpg, JPG_IRQ_FLAG_ERROR);
    if (session->jpg_timeout_irq) jpg_release_irq(session->jpg, JPG_IRQ_FLAG_TIME_OUT);
    if (session->scale_touched) scale_close(session->scale);
    album_jpeg_close_device(session);
    if (session->lock_acquired) {
        jpg_mutex_unlock(JPGID1, JPG_LOCK_DECODE);
    }
}

static ai_album_album_jpeg_hw_result_t album_jpeg_start(
    album_jpeg_session_t *session)
{
    ai_album_album_jpeg_hw_result_t result;

    session->jpg = (struct jpg_device *)dev_get(HG_JPG1_DEVID);
    session->scale = (struct scale_device *)dev_get(HG_SCALE2_DEVID);
    if (session->jpg == NULL || session->scale == NULL) {
        return AI_ALBUM_ALBUM_JPEG_HW_UNAVAILABLE;
    }
    result = album_jpeg_lock(session);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    if (jpg_open(session->jpg) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    }
    session->jpg_opened = 1U;
    result = album_jpeg_configure_scale(session);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    result = album_jpeg_register_irqs(session);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    os_event_clear(&g_album_jpeg_event, ALBUM_JPEG_EVENT_ALL, NULL);
    sys_dcache_clean_invalid_range_unaligned(
        (uint32_t *)(uintptr_t)session->scratch->yuv,
        (int32_t)session->scratch->yuv_size);
    sys_dcache_clean_range_unaligned(
        (uint32_t *)(uintptr_t)session->input->jpeg_data,
        (int32_t)session->input->jpeg_dma_size);
    if (scale_open(session->scale) != RET_OK ||
        jpg_decode_target(session->jpg, 1U) != RET_OK ||
        jpg_decode_photo(session->jpg,
                         (uint32_t)(uintptr_t)session->input->jpeg_data,
                         session->input->jpeg_dma_size) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_START_FAILED;
    }
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_wait(void)
{
    uint32_t flags = 0U;

    if (os_event_wait(&g_album_jpeg_event, ALBUM_JPEG_EVENT_ALL, &flags,
                      OS_EVENT_WMODE_OR | OS_EVENT_WMODE_CLEAR,
                      ALBUM_JPEG_HW_WAIT_MS) != RET_OK) {
        return AI_ALBUM_ALBUM_JPEG_HW_TIMEOUT;
    }
    if ((flags & (ALBUM_JPEG_EVENT_SCALE_ERROR |
                  ALBUM_JPEG_EVENT_JPEG_ERROR)) != 0U) {
        return AI_ALBUM_ALBUM_JPEG_HW_ERROR;
    }
    return (flags & ALBUM_JPEG_EVENT_SCALE_DONE) != 0U ?
               AI_ALBUM_ALBUM_JPEG_HW_OK : AI_ALBUM_ALBUM_JPEG_HW_ERROR;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_run_hardware(
    album_jpeg_session_t *session,
    uint32_t *elapsed_us)
{
    ai_album_album_jpeg_hw_result_t result;
    uint64 start_us = os_useconds();

    result = album_jpeg_event_init();
    if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
        result = album_jpeg_start(session);
    }
    if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
        result = album_jpeg_wait();
    }
    album_jpeg_session_close(session);
    /* 硬件错误/超时常为上次解码的状态残留(实测重试即成功):
     * 完整关闭后自动重试一次 */
    if (result == AI_ALBUM_ALBUM_JPEG_HW_ERROR ||
        result == AI_ALBUM_ALBUM_JPEG_HW_TIMEOUT) {
        os_sleep_ms(2);
        result = album_jpeg_event_init();
        if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
            result = album_jpeg_start(session);
        }
        if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
            result = album_jpeg_wait();
        }
        album_jpeg_session_close(session);
    }
    *elapsed_us = (uint32_t)(os_useconds() - start_us);
    return result;
}

static uint8_t album_jpeg_clamp8(int32_t value)
{
    if (value < 0) return 0U;
    return value > 255 ? 255U : (uint8_t)value;
}

static ai_album_album_jpeg_hw_result_t album_jpeg_yuv420_to_rgb565(
    const album_jpeg_scratch_t *scratch,
    const ai_album_album_jpeg_hw_output_t *output,
    const ai_album_album_jpeg_hw_input_t *input)
{
    uint32_t width = output->width;
    uint32_t height = output->height;
    uint32_t pixels = width * height;
    const uint8_t *plane_y = scratch->yuv;
    const uint8_t *plane_u = plane_y + pixels;
    const uint8_t *plane_v = plane_u + pixels / 4U;
    uint32_t row;

    for (row = 0U; row < height; ++row) {
        const uint8_t *line_y = plane_y + row * width;
        const uint8_t *line_u = plane_u + (row / 2U) * (width / 2U);
        const uint8_t *line_v = plane_v + (row / 2U) * (width / 2U);
        uint16_t *line_rgb = input->rgb565 + row * width;
        uint32_t column;

        if (input->cancel_cb != NULL &&
            (row % ALBUM_JPEG_CANCEL_CHECK_ROWS) == 0U &&
            input->cancel_cb(input->cancel_context)) {
            return AI_ALBUM_ALBUM_JPEG_HW_ERROR;
        }

        for (column = 0U; column < width; ++column) {
            int32_t y = line_y[column];
            int32_t u = (int32_t)line_u[column / 2U] - 128;
            int32_t v = (int32_t)line_v[column / 2U] - 128;
            uint8_t red = album_jpeg_clamp8(y + ((1436 * v) >> 10));
            uint8_t green = album_jpeg_clamp8(
                y - ((352 * u + 731 * v) >> 10));
            uint8_t blue = album_jpeg_clamp8(y + ((1812 * u) >> 10));

            line_rgb[column] = (uint16_t)(((red & 0xF8U) << 8) |
                                          ((green & 0xFCU) << 3) |
                                          (blue >> 3));
        }
    }
    return AI_ALBUM_ALBUM_JPEG_HW_OK;
}

ai_album_album_jpeg_hw_result_t ai_album_album_jpeg_hw_decode(
    const ai_album_album_jpeg_hw_input_t *input,
    ai_album_album_jpeg_hw_output_t *output)
{
    album_jpeg_dimensions_t source;
    album_jpeg_scratch_t scratch;
    album_jpeg_session_t session;
    ai_album_album_jpeg_hw_result_t result;
    uint64 convert_start_us;

    if (input == NULL || output == NULL || input->jpeg_data == NULL ||
        input->rgb565 == NULL || input->jpeg_data_size == 0U ||
        input->jpeg_dma_size < input->jpeg_data_size ||
        (input->jpeg_dma_size & 3U) != 0U) {
        return AI_ALBUM_ALBUM_JPEG_HW_INVALID;
    }
    memset(output, 0, sizeof(*output));
    result = album_jpeg_parse(input->jpeg_data, input->jpeg_data_size, &source);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    result = album_jpeg_fit_dimensions(&source, input->max_width,
                                       input->max_height, output);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    result = album_jpeg_scratch_allocate(output, &scratch);
    if (result != AI_ALBUM_ALBUM_JPEG_HW_OK) return result;
    memset(&session, 0, sizeof(session));
    session.input = input;
    session.source = &source;
    session.output = output;
    session.scratch = &scratch;
    result = album_jpeg_run_hardware(&session, &output->hardware_us);
    if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
        sys_dcache_invalid_range_unaligned(
            (uint32_t *)(uintptr_t)scratch.yuv, (int32_t)scratch.yuv_size);
        convert_start_us = os_useconds();
        result = album_jpeg_yuv420_to_rgb565(&scratch, output, input);
        output->convert_us = (uint32_t)(os_useconds() - convert_start_us);
        if (result == AI_ALBUM_ALBUM_JPEG_HW_OK) {
            sys_dcache_clean_range_unaligned(
                (uint32_t *)(uintptr_t)input->rgb565,
                (int32_t)output->data_size);
        }
    }
    album_jpeg_scratch_free(&scratch);
    return result;
}
