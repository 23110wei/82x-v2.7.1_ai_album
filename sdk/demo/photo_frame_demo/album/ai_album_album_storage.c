#include "album/ai_album_album_storage.h"

#include "basic_include.h"
#include "fs/fatfs/osal_file.h"

typedef struct {
    ai_album_album_photo_t *photos;
    uint8_t capacity;
    uint8_t *count;
    uint8_t opened_directories;
    uint16_t image_candidates;
    uint8_t truncated;
} album_storage_scan_context_t;

static const char *const g_scan_directories[] = {
    AI_ALBUM_ALBUM_STORAGE_DIR,
    AI_ALBUM_ALBUM_STORAGE_SECONDARY_DIR,
    AI_ALBUM_ALBUM_STORAGE_GENERATED_DIR,
};

static uint8_t album_storage_is_image(const char *name)
{
    const char *extension;

    if (name == NULL) {
        return 0U;
    }
    extension = os_strrchr(name, '.');
    if (extension == NULL || extension[1] == '\0') {
        return 0U;
    }
    return (uint8_t)(os_strcasecmp(extension + 1, "jpg") == 0 ||
                     os_strcasecmp(extension + 1, "jpeg") == 0);
}

static uint8_t album_storage_is_hidden(const char *name)
{
    return (uint8_t)(name == NULL || name[0] == '\0' || name[0] == '.');
}

static uint8_t album_storage_path_is_generated(const char *path)
{
    size_t path_length;
    size_t prefix_length;

    if (path == NULL) {
        return 0U;
    }
    path_length = os_strlen(path);
    prefix_length = os_strlen(AI_ALBUM_ALBUM_STORAGE_GENERATED_DIR);
    if (path_length < prefix_length) {
        return 0U;
    }
    return (uint8_t)(os_strncmp(path, AI_ALBUM_ALBUM_STORAGE_GENERATED_DIR,
                                 prefix_length) == 0 &&
                     (path[prefix_length] == '\0' ||
                      path[prefix_length] == '/'));
}

static int album_storage_join_path(char *destination,
                                   uint32_t destination_size,
                                   const char *directory,
                                   const char *name)
{
    uint32_t directory_length;
    const char *separator;
    int path_length;

    if (destination == NULL || destination_size == 0U ||
        directory == NULL || name == NULL || name[0] == '\0') {
        return RET_ERR;
    }
    directory_length = os_strlen(directory);
    separator = (directory_length > 0U &&
                 directory[directory_length - 1U] == '/') ? "" : "/";
    path_length = os_snprintf(destination, destination_size, "%s%s%s",
                              directory, separator, name);
    if (path_length < 0 || (uint32_t)path_length >= destination_size) {
        destination[0] = '\0';
        return RET_ERR;
    }
    return RET_OK;
}

static void album_storage_copy_name(char *destination,
                                    uint32_t destination_size,
                                    const char *path)
{
    const char *filename;
    const char *extension;
    uint32_t name_length;

    if (destination == NULL || destination_size == 0U) {
        return;
    }
    destination[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        os_strncpy(destination, "PHOTO", destination_size - 1U);
        destination[destination_size - 1U] = '\0';
        return;
    }
    filename = os_strrchr(path, '/');
    filename = filename == NULL ? path : filename + 1;
    extension = os_strrchr(filename, '.');
    name_length = extension == NULL ? os_strlen(filename) :
                                      (uint32_t)(extension - filename);
    if (name_length >= destination_size) {
        name_length = destination_size - 1U;
    }
    os_memcpy(destination, filename, name_length);
    destination[name_length] = '\0';
    if (destination[0] == '\0') {
        os_strncpy(destination, "PHOTO", destination_size - 1U);
        destination[destination_size - 1U] = '\0';
    }
}

static int album_storage_fill_photo(ai_album_album_photo_t *photo,
                                    const char *path,
                                    uint32_t file_size,
                                    uint8_t generated)
{
    if (photo == NULL || path == NULL || path[0] == '\0') {
        return RET_ERR;
    }
    memset(photo, 0, sizeof(*photo));
    album_storage_copy_name(photo->name, sizeof(photo->name), path);
    os_strncpy(photo->origin, generated ? "AI GENERATED" : "SD CARD",
               sizeof(photo->origin) - 1U);
    os_strncpy(photo->path, path, sizeof(photo->path) - 1U);
    photo->path[sizeof(photo->path) - 1U] = '\0';
    if (photo->path[0] == '\0' || os_strcmp(photo->path, path) != 0) {
        return RET_ERR;
    }
    photo->file_size = file_size;
    photo->generated = generated;
    photo->storage_backed = 1U;
    return RET_OK;
}

static void album_storage_scan_directory(const char *directory_path,
                                         uint8_t depth,
                                         uint8_t generated,
                                         album_storage_scan_context_t *context)
{
    void *directory;

    if (directory_path == NULL || context == NULL || context->truncated) {
        return;
    }
    directory = osal_opendir(directory_path);
    if (directory == NULL) {
        os_printf("ai_album: album scan open_failed path=%s\r\n",
                  directory_path);
        return;
    }
    context->opened_directories++;
    while (!context->truncated) {
        void *entry = osal_readdir(directory);
        const char *filename;
        char path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
        uint8_t photo_generated;

        if (entry == NULL) {
            break;
        }
        filename = osal_dirent_name(entry);
        if (album_storage_is_hidden(filename)) {
            continue;
        }
        if (osal_dirent_isdir(entry)) {
            if (depth < AI_ALBUM_ALBUM_STORAGE_MAX_DEPTH &&
                album_storage_join_path(path, sizeof(path), directory_path,
                                        filename) == RET_OK) {
                album_storage_scan_directory(path, (uint8_t)(depth + 1U),
                                             generated, context);
            }
            continue;
        }
        if (!album_storage_is_image(filename)) {
            continue;
        }
        if (album_storage_join_path(path, sizeof(path), directory_path,
                                    filename) != RET_OK) {
            os_printf("ai_album: album skip path_too_long dir=%s name=%s\r\n",
                      directory_path, filename);
            continue;
        }
        photo_generated = (uint8_t)(generated ||
                                    album_storage_path_is_generated(path));
        if (photo_generated && os_strncasecmp(filename, "TMP_", 4U) == 0) {
            os_printf("ai_album: album skip temporary path=%s\r\n", path);
            continue;
        }
        context->image_candidates++;
        if (*context->count >= context->capacity) {
            context->truncated = 1U;
            break;
        }
        if (album_storage_fill_photo(&context->photos[*context->count],
                                     path, osal_dirent_size(entry),
                                     photo_generated) != RET_OK) {
            os_printf("ai_album: album skip path=%s reason=path_too_long\r\n",
                      path);
            continue;
        }
        os_printf("ai_album: album photo path=%s size=%u\r\n",
                  path, (unsigned)osal_dirent_size(entry));
        (*context->count)++;
    }
    osal_closedir(directory);
}

static void album_storage_sort_photos(ai_album_album_photo_t *photos,
                                      uint8_t count)
{
    uint16_t left;

    for (left = 0U; left < count; ++left) {
        uint16_t right;

        for (right = (uint8_t)(left + 1U); right < count; ++right) {
            if (os_strcasecmp(photos[left].path, photos[right].path) > 0) {
                ai_album_album_photo_t temporary = photos[left];
                photos[left] = photos[right];
                photos[right] = temporary;
            }
        }
    }
}

ai_album_album_storage_scan_result_t ai_album_album_storage_scan(
    ai_album_album_photo_t *photos,
    uint8_t capacity,
    uint8_t *count)
{
    album_storage_scan_context_t context;
    uint8_t directory_index;

    if (photos == NULL || count == NULL || capacity == 0U) {
        return AI_ALBUM_ALBUM_STORAGE_SCAN_INVALID;
    }
    *count = 0U;
    memset(&context, 0, sizeof(context));
    context.photos = photos;
    context.capacity = capacity;
    context.count = count;
    os_printf("ai_album: album scan begin primary=%s secondary=%s root=%s\r\n",
              AI_ALBUM_ALBUM_STORAGE_DIR,
              AI_ALBUM_ALBUM_STORAGE_SECONDARY_DIR,
              AI_ALBUM_ALBUM_STORAGE_ROOT_DIR);
    for (directory_index = 0U;
         directory_index < ARRAY_SIZE(g_scan_directories) &&
         !context.truncated; ++directory_index) {
        album_storage_scan_directory(g_scan_directories[directory_index], 0U,
                                      album_storage_path_is_generated(
                                          g_scan_directories[directory_index]),
                                      &context);
    }
    if (*count == 0U && !context.truncated) {
        os_printf("ai_album: album scan fallback root=%s\r\n",
                  AI_ALBUM_ALBUM_STORAGE_ROOT_DIR);
        album_storage_scan_directory(AI_ALBUM_ALBUM_STORAGE_ROOT_DIR, 0U,
                                     0U, &context);
    }
    album_storage_sort_photos(photos, *count);
    os_printf("ai_album: album scan result dirs=%u candidates=%u photos=%u\r\n",
              (unsigned)context.opened_directories,
              (unsigned)context.image_candidates, (unsigned)*count);
    if (context.opened_directories == 0U) {
        return AI_ALBUM_ALBUM_STORAGE_SCAN_NOT_READY;
    }
    if (*count == 0U) {
        return AI_ALBUM_ALBUM_STORAGE_SCAN_EMPTY;
    }
    if (context.truncated) {
        os_printf("ai_album: album scan limit=%u reached\r\n",
                  (unsigned)capacity);
    }
    return AI_ALBUM_ALBUM_STORAGE_SCAN_OK;
}
