#include "album/ai_album_album_image_ai_storage.h"

#include "album/ai_album_album_jpeg_hw.h"
#include "album/ai_album_album_image_ai.h"
#include "av_mem.h"
#include "basic_include.h"
#include "fs/fatfs/osal_file.h"

#define AI_ALBUM_ALBUM_IMAGE_AI_DIR "0:/AI_GEN"
/* One-sector requests keep retry LBA and source data at the same offset. */
#define AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK 512U
#define AI_ALBUM_ALBUM_IMAGE_AI_PATH_ATTEMPTS 32U

static uint32_t g_image_ai_path_sequence;

static int image_ai_parse_jpeg(const uint8_t *data, uint32_t size,
                               ai_album_album_image_ai_jpeg_info_t *info)
{
    if (info == NULL) {
        return RET_ERR;
    }
    /* Reuse the decoder header parser so a JPEG accepted here is exactly one
     * the display pipeline can render later; the two used to disagree. */
    info->status = AI_ALBUM_ALBUM_IMAGE_AI_JPEG_UNSUPPORTED;
    if (ai_album_album_jpeg_hw_probe(data, size, &info->width,
                                     &info->height) !=
        AI_ALBUM_ALBUM_JPEG_HW_OK) {
        os_printf("[ALBUM_AI] JPEG unsupported by decoder size=%u\r\n",
                  (unsigned)size);
        return RET_ERR;
    }
    info->status = AI_ALBUM_ALBUM_IMAGE_AI_JPEG_OK;
    return RET_OK;
}

int ai_album_album_image_ai_ensure_directory(void)
{
    void *directory = osal_opendir(AI_ALBUM_ALBUM_IMAGE_AI_DIR);

    if (directory != NULL) {
        osal_closedir(directory);
        return RET_OK;
    }
    if (osal_fmkdir(AI_ALBUM_ALBUM_IMAGE_AI_DIR) == 0U) {
        return RET_OK;
    }
    directory = osal_opendir(AI_ALBUM_ALBUM_IMAGE_AI_DIR);
    if (directory == NULL) {
        return RET_ERR;
    }
    osal_closedir(directory);
    return RET_OK;
}

int ai_album_album_image_ai_make_path(char *path, uint32_t capacity,
                                      const char *prefix)
{
    uint8_t attempt;

    if (!path || capacity == 0U || !prefix || prefix[0] == '\0') {
        return RET_ERR;
    }
    path[0] = '\0';
    for (attempt = 0U; attempt < AI_ALBUM_ALBUM_IMAGE_AI_PATH_ATTEMPTS;
         ++attempt) {
        F_FILE *file;
        uint32_t token;
        int length;

        ++g_image_ai_path_sequence;
        if (g_image_ai_path_sequence == 0U) {
            g_image_ai_path_sequence = 1U;
        }
        token = (uint32_t)os_jiffies() ^
                (g_image_ai_path_sequence * 0x9E3779B9U);
        length = os_snprintf(path, capacity, "%s/%s_%08x.JPG",
                             AI_ALBUM_ALBUM_IMAGE_AI_DIR, prefix,
                             (unsigned)token);
        if (length < 0 || (uint32_t)length >= capacity) {
            path[0] = '\0';
            return RET_ERR;
        }
        file = osal_fopen(path, "rb");
        if (file == NULL) {
            return RET_OK;
        }
        (void)osal_fclose(file);
    }
    path[0] = '\0';
    return RET_ERR;
}

int ai_album_album_image_ai_load_file(
    const char *path, uint8_t **data, uint32_t *data_size,
    ai_album_album_image_ai_jpeg_info_t *info)
{
    F_FILE *file;
    uint8_t *buffer;
    uint32_t size;
    uint32_t offset = 0U;

    if (!path || !data || !data_size || !info) {
        return RET_ERR;
    }
    info->status = AI_ALBUM_ALBUM_IMAGE_AI_JPEG_IO_ERROR;
    file = osal_fopen(path, "rb");
    if (file == NULL) {
        return RET_ERR;
    }
    size = osal_fsize(file);
    if (size == 0U || size > AI_ALBUM_ALBUM_IMAGE_AI_MAX_JPEG_SIZE) {
        (void)osal_fclose(file);
        return RET_ERR;
    }
    buffer = av_mem_alloc_psram((size + 3U) & ~3U);
    if (buffer == NULL) {
        (void)osal_fclose(file);
        return RET_ERR;
    }
    while (offset < size) {
        uint32_t chunk = size - offset;

        if (chunk > AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK) {
            chunk = AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK;
        }
        if (osal_fread(buffer + offset, 1U, chunk, file) != chunk) {
            av_mem_free_psram(buffer);
            (void)osal_fclose(file);
            return RET_ERR;
        }
        offset += chunk;
    }
    if (osal_fclose(file) != 0) {
        av_mem_free_psram(buffer);
        return RET_ERR;
    }
    if (image_ai_parse_jpeg(buffer, size, info) != RET_OK) {
        av_mem_free_psram(buffer);
        return RET_ERR;
    }
    *data = buffer;
    *data_size = size;
    return RET_OK;
}

static int image_ai_verify_written_file(const char *path,
                                        const uint8_t *expected,
                                        uint32_t expected_size)
{
    F_FILE *file;
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t offset = 0U;
    int close_result;

    file = osal_fopen(path, "rb");
    if (file == NULL) {
        os_printf("[ALBUM_AI] verify open failed path=%s\r\n", path);
        return RET_ERR;
    }
    if (osal_fsize(file) != expected_size) {
        os_printf("[ALBUM_AI] verify size mismatch path=%s expected=%u "
                  "actual=%u\r\n",
                  path, (unsigned)expected_size, (unsigned)osal_fsize(file));
        (void)osal_fclose(file);
        return RET_ERR;
    }
    buffer_size = expected_size < AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK ?
                      expected_size : AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK;
    buffer = av_mem_alloc_psram((buffer_size + 3U) & ~3U);
    if (buffer == NULL) {
        os_printf("[ALBUM_AI] verify allocation failed size=%u\r\n",
                  (unsigned)buffer_size);
        (void)osal_fclose(file);
        return RET_ERR;
    }
    while (offset < expected_size) {
        uint32_t chunk = expected_size - offset;
        uint32_t read_size;

        if (chunk > buffer_size) {
            chunk = buffer_size;
        }
        read_size = osal_fread(buffer, 1U, chunk, file);
        if (read_size != chunk ||
            os_memcmp(buffer, expected + offset, chunk) != 0) {
            os_printf("[ALBUM_AI] verify data mismatch path=%s offset=%u "
                      "read=%u expected=%u\r\n",
                      path, (unsigned)offset, (unsigned)read_size,
                      (unsigned)chunk);
            break;
        }
        offset += chunk;
    }
    close_result = osal_fclose(file);
    av_mem_free_psram(buffer);
    if (offset != expected_size || close_result != 0) {
        return RET_ERR;
    }
    return RET_OK;
}

static uint32_t image_ai_write_safe_chunks(F_FILE *file,
                                           const uint8_t *data,
                                           uint32_t size)
{
    uint32_t offset = 0U;

    while (offset < size) {
        uint32_t chunk = size - offset;
        uint32_t written;

        if (chunk > AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK) {
            chunk = AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK;
        }
        written = osal_fwrite((void *)(data + offset), 1U, chunk, file);
        offset += written;
        if (written != chunk) {
            break;
        }
    }
    return offset;
}

int ai_album_album_image_ai_write_result(
    const char *path, const uint8_t *data, size_t length,
    ai_album_album_image_ai_jpeg_info_t *info)
{
    F_FILE *file;
    FRESULT sync_result;
    int close_result;
    uint32_t written;

    if (!path || !data || length == 0U || !info) {
        return RET_ERR;
    }
    info->status = AI_ALBUM_ALBUM_IMAGE_AI_JPEG_IO_ERROR;
    if (length > AI_ALBUM_ALBUM_IMAGE_AI_MAX_JPEG_SIZE ||
        image_ai_parse_jpeg(data, (uint32_t)length, info) != RET_OK) {
        return RET_ERR;
    }
    file = osal_fopen(path, "wb");
    if (file == NULL) {
        return RET_ERR;
    }
    written = image_ai_write_safe_chunks(file, data, (uint32_t)length);
    sync_result = osal_fsync(file);
    close_result = osal_fclose(file);
    if (written != (uint32_t)length || sync_result != FR_OK ||
        close_result != 0) {
        os_printf("[ALBUM_AI] write failed path=%s written=%u expected=%u "
                  "sync=%d close=%d\r\n",
                  path, (unsigned)written, (unsigned)length,
                  (int)sync_result, close_result);
        return RET_ERR;
    }
    if (image_ai_verify_written_file(path, data, (uint32_t)length) != RET_OK) {
        os_printf("[ALBUM_AI] write verification failed path=%s size=%u\r\n",
                  path, (unsigned)length);
        return RET_ERR;
    }
    info->status = AI_ALBUM_ALBUM_IMAGE_AI_JPEG_OK;
    os_printf("[ALBUM_AI] write verified path=%s size=%u jpeg=%ux%u "
              "io_chunk=%u\r\n",
              path, (unsigned)length, (unsigned)info->width,
              (unsigned)info->height,
              (unsigned)AI_ALBUM_ALBUM_IMAGE_AI_SAFE_IO_CHUNK);
    return RET_OK;
}
