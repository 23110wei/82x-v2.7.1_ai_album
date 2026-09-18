#include "storage/ai_album_storage_info.h"
#include "basic_include.h"
#include "fs/fatfs/ff.h"

/* SD 卡容量信息实现 — 通过 FatFS f_getfree 获取总/空闲簇数 */

static ai_album_storage_snapshot_t g_snapshot = {
    .status = AI_ALBUM_STORAGE_STATUS_UNKNOWN,
    .total_mb = 0U,
    .free_mb = 0U,
    .sequence = 0U,
};

void ai_album_storage_info_init(void)
{
    memset(&g_snapshot, 0, sizeof(g_snapshot));
    g_snapshot.status = AI_ALBUM_STORAGE_STATUS_CHECKING;
    g_snapshot.sequence = 1U;
}

void ai_album_storage_info_request(void)
{
    FATFS *fat = NULL;
    DWORD free_clusters = 0U;
    FRESULT res;
    uint32_t cluster_size = 0U;
    uint32_t total_clusters = 0U;

    g_snapshot.status = AI_ALBUM_STORAGE_STATUS_CHECKING;
    g_snapshot.sequence++;

    res = f_getfree("0:/", &free_clusters, &fat);
    if (res != FR_OK || fat == NULL) {
        g_snapshot.status = AI_ALBUM_STORAGE_STATUS_NO_MEDIA;
        g_snapshot.total_mb = 0U;
        g_snapshot.free_mb = 0U;
        return;
    }

    if (fat->ssize == 0U || fat->csize == 0U) {
        g_snapshot.status = AI_ALBUM_STORAGE_STATUS_ERROR;
        return;
    }

    /* 每簇字节 = 扇区大小 * 每簇扇区数 */
    cluster_size = (uint32_t)fat->ssize * (uint32_t)fat->csize;
    /* 总簇数 = FAT 条目数 - 2 (保留簇) */
    total_clusters = (uint32_t)(fat->n_fatent > 2U ? (fat->n_fatent - 2U) : 0U);

    if (total_clusters == 0U) {
        g_snapshot.status = AI_ALBUM_STORAGE_STATUS_ERROR;
        return;
    }

    g_snapshot.total_mb = (uint32_t)((uint64_t)total_clusters *
                                     (uint64_t)cluster_size /
                                     (1024ULL * 1024ULL));
    g_snapshot.free_mb = (uint32_t)((uint64_t)free_clusters *
                                    (uint64_t)cluster_size /
                                    (1024ULL * 1024ULL));
    g_snapshot.status = AI_ALBUM_STORAGE_STATUS_READY;
}

void ai_album_storage_info_get_snapshot(ai_album_storage_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    memcpy(out, &g_snapshot, sizeof(*out));
}
