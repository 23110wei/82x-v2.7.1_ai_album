#include "album/ai_album_album_store.h"

#include "album/ai_album_album_perf.h"
#include "album/ai_album_album_storage.h"
#include "basic_include.h"
#include "fs/fatfs/osal_file.h"

#define ALBUM_STORE_SD_ROOT "0:/"
#define ALBUM_STORE_SD_FONT_DIR "0:/ai_album/fonts"
#define ALBUM_STORE_CLEAR_PATH_MAX 256U
#define ALBUM_STORE_CLEAR_MAX_DEPTH 12U

static const uint32_t g_photo_sky[] = {
    0x6E9AB8U, 0xA47D9BU, 0x789C86U, 0xB69872U,
};

static const uint32_t g_photo_ground[] = {
    0x527664U, 0x6C5367U, 0x3D6655U, 0x6D5A45U,
};

static const uint32_t g_photo_accent[] = {
    0xF2C66DU, 0xF2A66FU, 0xD9E79AU, 0xF7D18BU,
};

#define ALBUM_STORE_PSRAM_DATA \
    __attribute__((aligned(4), section(".psram.data")))

static ai_album_album_photo_t
    g_photos[AI_ALBUM_ALBUM_STORE_MAX_PHOTOS] ALBUM_STORE_PSRAM_DATA;
static ai_album_album_photo_t g_refresh_photos[
    AI_ALBUM_ALBUM_STORE_MAX_PHOTOS] ALBUM_STORE_PSRAM_DATA;
static uint8_t g_photo_count;
static uint8_t g_selected_photo;
static uint8_t g_storage_photo_count;
static uint8_t g_store_ready;
static uint32_t g_store_version;
static ai_album_album_store_status_t g_store_status;

static void album_store_apply_photo_palette(ai_album_album_photo_t *photos,
                                            uint8_t count)
{
    uint16_t index;

    for (index = 0U; index < count; ++index) {
        if (photos[index].generated) {
            continue;
        }
        photos[index].sky_color =
            g_photo_sky[index % ARRAY_SIZE(g_photo_sky)];
        photos[index].ground_color =
            g_photo_ground[index % ARRAY_SIZE(g_photo_ground)];
        photos[index].accent_color =
            g_photo_accent[index % ARRAY_SIZE(g_photo_accent)];
    }
}

static uint8_t album_store_is_same_photo(
    const ai_album_album_photo_t *left,
    const ai_album_album_photo_t *right)
{
    if (left->path[0] != '\0' || right->path[0] != '\0') {
        return (uint8_t)(os_strcmp(left->path, right->path) == 0);
    }
    return (uint8_t)(left->generated == right->generated &&
                     os_strcmp(left->name, right->name) == 0);
}

static uint8_t album_store_photo_equal(
    const ai_album_album_photo_t *left,
    const ai_album_album_photo_t *right)
{
    return (uint8_t)(album_store_is_same_photo(left, right) &&
                     os_strcmp(left->name, right->name) == 0 &&
                     os_strcmp(left->origin, right->origin) == 0 &&
                     left->file_size == right->file_size &&
                     left->sky_color == right->sky_color &&
                      left->ground_color == right->ground_color &&
                      left->accent_color == right->accent_color &&
                      left->width == right->width &&
                      left->height == right->height &&
                      left->generated == right->generated &&
                     left->storage_backed == right->storage_backed);
}

static uint8_t album_store_list_equal(
    const ai_album_album_photo_t *left,
    uint8_t left_count,
    const ai_album_album_photo_t *right,
    uint8_t right_count)
{
    uint16_t index;

    if (left_count != right_count) {
        return 0U;
    }
    for (index = 0; index < left_count; ++index) {
        if (!album_store_photo_equal(&left[index], &right[index])) {
            return 0U;
        }
    }
    return 1U;
}

static void album_store_preserve_generated(
    ai_album_album_photo_t *photos,
    uint8_t *count,
    const ai_album_album_photo_t *old_photos,
    uint8_t old_count)
{
    uint16_t index;

    for (index = 0; index < old_count && *count <
                           AI_ALBUM_ALBUM_STORE_MAX_PHOTOS; ++index) {
        if (!old_photos[index].generated || old_photos[index].storage_backed) {
            continue;
        }
        photos[*count] = old_photos[index];
        (*count)++;
    }
}

static void album_store_restore_selection(
    const ai_album_album_photo_t *old_selected,
    uint8_t old_selected_index,
    const ai_album_album_photo_t *new_photos,
    uint8_t new_count)
{
    uint16_t index;

    if (old_selected == NULL || new_count == 0U) {
        g_selected_photo = 0U;
        return;
    }
    for (index = 0; index < new_count; ++index) {
        if (album_store_is_same_photo(old_selected, &new_photos[index])) {
            g_selected_photo = index;
            return;
        }
    }
    g_selected_photo = old_selected_index < new_count ?
                           old_selected_index : (uint8_t)(new_count - 1U);
}

static int album_store_refresh_internal(void)
{
    ai_album_album_photo_t old_selected;
    const ai_album_album_photo_t *old_selected_ptr = NULL;
    uint8_t old_count = g_photo_count;
    uint8_t old_selected_index = g_selected_photo;
    uint8_t scanned_count = 0U;
    uint8_t next_count = 0U;
    ai_album_album_storage_scan_result_t scan_result;
    ai_album_album_store_status_t old_status = g_store_status;
    ai_album_album_store_status_t next_status;
    uint8_t changed;

    if (old_selected_index < old_count) {
        old_selected = g_photos[old_selected_index];
        old_selected_ptr = &old_selected;
    }
    memset(g_refresh_photos, 0, sizeof(g_refresh_photos));
    scan_result = ai_album_album_storage_scan(
        g_refresh_photos, AI_ALBUM_ALBUM_STORE_MAX_PHOTOS, &scanned_count);
    if (scan_result == AI_ALBUM_ALBUM_STORAGE_SCAN_OK &&
        scanned_count > 0U) {
        album_store_apply_photo_palette(g_refresh_photos, scanned_count);
        next_count = scanned_count;
        g_storage_photo_count = scanned_count;
        next_status = AI_ALBUM_ALBUM_STORE_STATUS_SD_READY;
    } else {
        memset(g_refresh_photos, 0, sizeof(g_refresh_photos));
        g_storage_photo_count = 0U;
        next_status = scan_result == AI_ALBUM_ALBUM_STORAGE_SCAN_EMPTY ?
                          AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY :
                          AI_ALBUM_ALBUM_STORE_STATUS_SD_UNAVAILABLE;
    }
    album_store_preserve_generated(g_refresh_photos, &next_count,
                                   g_photos, old_count);
    changed = (uint8_t)(!album_store_list_equal(
                            g_photos, old_count, g_refresh_photos, next_count) ||
                        old_status != next_status);
    memset(g_photos, 0, sizeof(g_photos));
    memcpy(g_photos, g_refresh_photos, sizeof(g_photos));
    g_photo_count = next_count;
    g_store_status = next_status;
    if (changed) {
        g_store_version++;
    }
    album_store_restore_selection(old_selected_ptr, old_selected_index,
                                  g_photos, g_photo_count);
    return scan_result;
}

void ai_album_album_store_init(void)
{
    if (g_store_ready) {
        return;
    }
    memset(g_photos, 0, sizeof(g_photos));
    g_photo_count = 0U;
    g_selected_photo = 0U;
    g_storage_photo_count = 0U;
    g_store_version = 1U;
    g_store_status = AI_ALBUM_ALBUM_STORE_STATUS_UNSCANNED;
    g_store_ready = 1U;
}

int ai_album_album_store_refresh(void)
{
    int scan_result;
    uint64 start_us;

    ai_album_album_store_init();
    start_us = os_useconds();
    scan_result = album_store_refresh_internal();
    ai_album_album_perf_record_scan(
        (uint32_t)(os_useconds() - start_us), scan_result);
    return (scan_result == AI_ALBUM_ALBUM_STORAGE_SCAN_OK ||
            scan_result == AI_ALBUM_ALBUM_STORAGE_SCAN_EMPTY) ?
               RET_OK : RET_ERR;
}

uint8_t ai_album_album_store_count(void)
{
    ai_album_album_store_init();
    return g_photo_count;
}

const ai_album_album_photo_t *ai_album_album_store_get(uint8_t index)
{
    ai_album_album_store_init();
    if (index >= g_photo_count) {
        return NULL;
    }
    return &g_photos[index];
}

uint8_t ai_album_album_store_selected(void)
{
    ai_album_album_store_init();
    return g_selected_photo;
}

int ai_album_album_store_select(uint8_t index)
{
    ai_album_album_store_init();
    if (index >= g_photo_count) {
        return RET_ERR;
    }
    g_selected_photo = index;
    return RET_OK;
}

int ai_album_album_store_delete(uint8_t index)
{
    const ai_album_album_photo_t *photo;
    char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];

    ai_album_album_store_init();
    photo = ai_album_album_store_get(index);
    if (photo == NULL || !photo->storage_backed || photo->path[0] == '\0') {
        return RET_ERR;
    }
    os_strncpy(path, photo->path, sizeof(path) - 1U);
    path[sizeof(path) - 1U] = '\0';
    if (osal_unlink(path) != FR_OK) {
        os_printf("ai_album: album delete failed path=%s\r\n", path);
        return RET_ERR;
    }
    os_printf("ai_album: album deleted path=%s\r\n", path);
    return ai_album_album_store_refresh();
}

static int album_store_ensure_directory(const char *path)
{
    void *directory = osal_opendir(path);

    if (directory != NULL) {
        osal_closedir(directory);
        return RET_OK;
    }
    if (osal_fmkdir(path) == FR_OK) {
        return RET_OK;
    }
    directory = osal_opendir(path);
    if (directory == NULL) {
        os_printf("ai_album: storage directory create failed path=%s\r\n",
                  path);
        return RET_ERR;
    }
    osal_closedir(directory);
    return RET_OK;
}

static int album_store_prepare_sd_directories(void)
{
    static const char *const directories[] = {
        AI_ALBUM_ALBUM_STORAGE_DIR,
        AI_ALBUM_ALBUM_STORAGE_SECONDARY_DIR,
        AI_ALBUM_ALBUM_STORAGE_GENERATED_DIR,
    };
    uint8_t index;

    for (index = 0U; index < ARRAY_SIZE(directories); ++index) {
        if (album_store_ensure_directory(directories[index]) != RET_OK) {
            return RET_ERR;
        }
    }
    return RET_OK;
}

typedef struct {
    char path[ALBUM_STORE_CLEAR_PATH_MAX];
    uint16_t length;
    uint32_t deleted_files;
    uint32_t deleted_directories;
} album_store_clear_context_t;

static uint8_t album_store_path_is_protected(const char *path)
{
    return (uint8_t)(os_strcasecmp(path, ALBUM_STORE_SD_FONT_DIR) == 0);
}

static uint8_t album_store_path_is_protected_ancestor(const char *path)
{
    uint32_t path_length = os_strlen(path);
    uint32_t protected_length = os_strlen(ALBUM_STORE_SD_FONT_DIR);

    if (path_length >= protected_length ||
        os_strncasecmp(path, ALBUM_STORE_SD_FONT_DIR, path_length) != 0) {
        return 0U;
    }
    return (uint8_t)(path[path_length - 1U] == '/' ||
                     ALBUM_STORE_SD_FONT_DIR[path_length] == '/');
}

static uint8_t album_store_entry_is_dot(const char *name)
{
    return (uint8_t)(name == NULL || name[0] == '\0' ||
                     (name[0] == '.' && name[1] == '\0') ||
                     (name[0] == '.' && name[1] == '.' &&
                      name[2] == '\0'));
}

static int album_store_clear_path_append(album_store_clear_context_t *context,
                                         const char *name)
{
    uint32_t name_length = os_strlen(name);
    uint8_t add_separator = (uint8_t)(
        context->length > 0U && context->path[context->length - 1U] != '/');
    uint32_t required = context->length + add_separator + name_length + 1U;

    if (required > sizeof(context->path)) {
        os_printf("ai_album: SD clear path too long parent=%s name=%s\r\n",
                  context->path, name);
        return RET_ERR;
    }
    if (add_separator) context->path[context->length++] = '/';
    os_memcpy(&context->path[context->length], name, name_length + 1U);
    context->length = (uint16_t)(context->length + name_length);
    return RET_OK;
}

static int album_store_clear_directory(album_store_clear_context_t *context,
                                       uint8_t depth)
{
    void *directory;
    void *entry;
    int result = RET_OK;

    if (depth > ALBUM_STORE_CLEAR_MAX_DEPTH) {
        os_printf("ai_album: SD clear depth exceeded path=%s\r\n",
                  context->path);
        return RET_ERR;
    }
    directory = osal_opendir(context->path);
    if (directory == NULL) {
        os_printf("ai_album: SD clear open failed path=%s\r\n",
                  context->path);
        return RET_ERR;
    }
    while ((entry = osal_readdir(directory)) != NULL) {
        const char *name = osal_dirent_name(entry);
        uint16_t parent_length = context->length;
        uint8_t is_directory;

        if (album_store_entry_is_dot(name)) continue;
        is_directory = (uint8_t)(osal_dirent_isdir(entry) != 0);
        if (album_store_clear_path_append(context, name) != RET_OK) {
            result = RET_ERR;
            break;
        }
        if (album_store_path_is_protected(context->path)) {
            context->path[parent_length] = '\0';
            context->length = parent_length;
            continue;
        }
        if (is_directory &&
            album_store_clear_directory(context, (uint8_t)(depth + 1U)) !=
                RET_OK) {
            result = RET_ERR;
        } else if (!is_directory ||
                   !album_store_path_is_protected_ancestor(context->path)) {
            if (osal_unlink(context->path) != FR_OK) {
                os_printf("ai_album: SD clear delete failed path=%s\r\n",
                          context->path);
                result = RET_ERR;
            } else if (is_directory) {
                context->deleted_directories++;
            } else {
                context->deleted_files++;
            }
        }
        context->path[parent_length] = '\0';
        context->length = parent_length;
        if (result != RET_OK) break;
    }
    osal_closedir(directory);
    return result;
}

static uint8_t album_store_font_directory_available(void)
{
    void *directory = osal_opendir(ALBUM_STORE_SD_FONT_DIR);

    if (directory == NULL) return 0U;
    osal_closedir(directory);
    return 1U;
}

static int album_store_verify_font_directory(void)
{
    if (!album_store_font_directory_available()) {
        os_printf("ai_album: protected font directory missing path=%s\r\n",
                  ALBUM_STORE_SD_FONT_DIR);
        return RET_ERR;
    }
    return RET_OK;
}

static int album_store_clear_sd_data(void)
{
#if !FS_EN
    os_printf("ai_album: SD clear unavailable without FATFS\r\n");
    return RET_ERR;
#else
    album_store_clear_context_t context;

    if (album_store_verify_font_directory() != RET_OK) {
        return RET_ERR;
    }
    memset(&context, 0, sizeof(context));
    os_strncpy(context.path, ALBUM_STORE_SD_ROOT,
               sizeof(context.path) - 1U);
    context.length = (uint16_t)os_strlen(context.path);
    if (album_store_clear_directory(&context, 0U) != RET_OK) {
        return RET_ERR;
    }
    if (album_store_verify_font_directory() != RET_OK ||
        album_store_prepare_sd_directories() != RET_OK) {
        return RET_ERR;
    }
    os_printf("ai_album: SD data cleared files=%u dirs=%u fonts=preserved\r\n",
              (unsigned)context.deleted_files,
              (unsigned)context.deleted_directories);
    return RET_OK;
#endif
}

static int album_store_format_flash(void)
{
#if defined(FLASHDISK_EN) && FLASHDISK_EN
    return flash_fatfs_format() == 0 ? RET_OK : RET_ERR;
#else
    os_printf("ai_album: Flash format unavailable\r\n");
    return RET_ERR;
#endif
}

uint8_t ai_album_album_store_format_available(
    ai_album_album_store_format_target_t target)
{
    uint8_t sd_available;
    uint8_t flash_available;

    if (target > AI_ALBUM_ALBUM_STORE_FORMAT_BOTH) {
        return 0U;
    }
    ai_album_album_store_init();
    sd_available = (uint8_t)(
        (g_store_status == AI_ALBUM_ALBUM_STORE_STATUS_SD_READY ||
         g_store_status == AI_ALBUM_ALBUM_STORE_STATUS_SD_EMPTY) &&
        album_store_font_directory_available());
#if defined(FLASHDISK_EN) && FLASHDISK_EN
    flash_available = 1U;
#else
    flash_available = 0U;
#endif
    if (target == AI_ALBUM_ALBUM_STORE_FORMAT_SD) {
        return sd_available;
    }
    if (target == AI_ALBUM_ALBUM_STORE_FORMAT_FLASH) {
        return flash_available;
    }
    return (uint8_t)(sd_available && flash_available);
}

int ai_album_album_store_format(
    ai_album_album_store_format_target_t target)
{
    uint8_t clear_sd;
    uint8_t format_flash;
    int result = RET_OK;

    if (target > AI_ALBUM_ALBUM_STORE_FORMAT_BOTH) {
        return RET_ERR;
    }
    clear_sd = (uint8_t)(target == AI_ALBUM_ALBUM_STORE_FORMAT_SD ||
                         target == AI_ALBUM_ALBUM_STORE_FORMAT_BOTH);
    format_flash = (uint8_t)(target == AI_ALBUM_ALBUM_STORE_FORMAT_FLASH ||
                             target == AI_ALBUM_ALBUM_STORE_FORMAT_BOTH);
    if (clear_sd && ai_album_album_store_refresh() != RET_OK) {
        os_printf("ai_album: storage reset rejected because SD is unavailable\r\n");
        return RET_ERR;
    }
    if (!ai_album_album_store_format_available(target)) {
        os_printf("ai_album: format target unavailable target=%u\r\n",
                  (unsigned)target);
        return RET_ERR;
    }
    if (clear_sd && album_store_clear_sd_data() != RET_OK) {
        return RET_ERR;
    }
    if (format_flash && album_store_format_flash() != RET_OK) {
        result = RET_ERR;
    }
    if (clear_sd && ai_album_album_store_refresh() != RET_OK) {
        result = RET_ERR;
    }
    return result;
}

uint8_t ai_album_album_store_has_storage_photos(void)
{
    ai_album_album_store_init();
    return (uint8_t)(g_storage_photo_count > 0U);
}

uint32_t ai_album_album_store_version(void)
{
    ai_album_album_store_init();
    return g_store_version;
}

ai_album_album_store_status_t ai_album_album_store_status(void)
{
    ai_album_album_store_init();
    return g_store_status;
}

int ai_album_album_store_add_generated(const ai_album_album_photo_t *photo,
                                       uint8_t *out_index)
{
    ai_album_album_store_init();
    if (photo == NULL || !photo->generated || !photo->storage_backed ||
        photo->path[0] == '\0' || photo->file_size == 0U ||
        g_photo_count >= AI_ALBUM_ALBUM_STORE_MAX_PHOTOS) {
        return RET_ERR;
    }
    g_photos[g_photo_count] = *photo;
    if (out_index != NULL) {
        *out_index = g_photo_count;
    }
    g_selected_photo = g_photo_count;
    g_photo_count++;
    g_storage_photo_count++;
    g_store_version++;
    return RET_OK;
}
