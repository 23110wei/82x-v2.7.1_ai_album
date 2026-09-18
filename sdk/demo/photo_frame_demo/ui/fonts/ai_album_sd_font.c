#include "ai_album_sd_font.h"

#include "basic_include.h"

#include <string.h>

#define AI_SD_FONT_HEADER_SIZE 64U
#define AI_SD_FONT_RECORD_SIZE 12U
#define AI_SD_FONT_BMP_COUNT 0x10000U
#define AI_SD_FONT_SUPPLEMENTARY_PAGE_FIRST 0x100U
#define AI_SD_FONT_SUPPLEMENTARY_PAGE_COUNT 0x1000U
#define AI_SD_FONT_SUPPLEMENTARY_PAGE_SHIFT 8U
#define AI_SD_FONT_INDEX_ENTRY_SIZE 4U
#define AI_SD_FONT_PAGE_ENTRY_COUNT 0x100U
#define AI_SD_FONT_MAX_BITMAP_BYTES 4096U
#define AI_SD_FONT_CACHED_BITMAP_BYTES 256U
#define AI_SD_FONT_DESCRIPTOR_CACHE_SLOTS 64U
#define AI_SD_FONT_BITMAP_CACHE_SLOTS 24U
#define AI_SD_FONT_FILE_SLOTS 2U
#define AI_SD_FONT_RETRY_MS 2000U

typedef enum {
    AI_SD_FONT_UNCHECKED = 0,
    AI_SD_FONT_READY,
    AI_SD_FONT_INVALID,
} ai_sd_font_state_t;

typedef struct {
    uint8_t box_w;
    uint8_t box_h;
    int8_t ofs_x;
    int8_t ofs_y;
    uint16_t adv_w;
    uint16_t stride;
    uint32_t bitmap_size;
} ai_sd_font_record_t;

typedef struct {
    lv_font_t font;
    const char *path;
    uint32_t bmp_index_offset;
    uint32_t supplementary_directory_offset;
    uint32_t supplementary_directory_count;
    uint32_t pages_offset;
    uint32_t records_offset;
    uint32_t file_size;
    uint32_t glyph_count;
    uint32_t retry_at_ms;
    uint8_t family;
    uint8_t state;
} ai_sd_font_context_t;

typedef struct {
    lv_fs_file_t file;
    uint32_t stamp;
    uint8_t family;
    uint8_t open;
} ai_sd_font_file_slot_t;

typedef struct {
    ai_sd_font_record_t record;
    uint32_t codepoint;
    uint32_t record_offset;
    uint32_t stamp;
    uint8_t family;
    uint8_t valid;
    uint8_t found;
} ai_sd_font_descriptor_slot_t;

typedef struct {
    uint8_t bitmap[AI_SD_FONT_CACHED_BITMAP_BYTES];
    uint32_t codepoint;
    uint32_t stamp;
    uint16_t size;
    uint8_t family;
    uint8_t valid;
} ai_sd_font_bitmap_slot_t;

typedef struct {
    ai_sd_font_descriptor_slot_t descriptors[AI_SD_FONT_DESCRIPTOR_CACHE_SLOTS];
    ai_sd_font_bitmap_slot_t bitmaps[AI_SD_FONT_BITMAP_CACHE_SLOTS];
    uint8_t large_bitmap[AI_SD_FONT_MAX_BITMAP_BYTES];
} ai_sd_font_cache_t;

static bool ai_sd_font_get_glyph_dsc(const lv_font_t *font,
                                     lv_font_glyph_dsc_t *out,
                                     uint32_t codepoint,
                                     uint32_t next_codepoint);
/* 本工程LVGL 9.0签名: bitmap回调收font+letter, 返回行打包4bpp位图指针 */
static const uint8_t *ai_sd_font_get_glyph_bitmap(const lv_font_t *font,
                                                   uint32_t codepoint);

static const char *const g_font_paths[AI_ALBUM_SD_FONT_FAMILY_COUNT] = {
    "S:/ai_album/fonts/NotoSans-16.aif",
    "S:/ai_album/fonts/NotoSansSC-16.aif",
    "S:/ai_album/fonts/NotoSansJP-16.aif",
    "S:/ai_album/fonts/NotoSansKR-16.aif",
    "S:/ai_album/fonts/NotoSansThai-16.aif",
    "S:/ai_album/fonts/NotoSansArabic-16.aif",
    "S:/ai_album/fonts/NotoSansHebrew-16.aif",
    "S:/ai_album/fonts/NotoSansDevanagari-16.aif",
};

static ai_sd_font_context_t g_fonts[AI_ALBUM_SD_FONT_FAMILY_COUNT];
static ai_sd_font_file_slot_t g_file_slots[AI_SD_FONT_FILE_SLOTS];
static ai_sd_font_cache_t *g_cache;
static uint32_t g_cache_clock;
static uint8_t g_initialized;

static uint32_t ai_sd_font_next_stamp(void)
{
    uint8_t index;

    g_cache_clock++;
    if (g_cache_clock != 0U) return g_cache_clock;
    for (index = 0U; index < AI_SD_FONT_FILE_SLOTS; ++index) {
        g_file_slots[index].stamp = 0U;
    }
    if (g_cache != NULL) {
        for (index = 0U; index < AI_SD_FONT_DESCRIPTOR_CACHE_SLOTS; ++index) {
            g_cache->descriptors[index].stamp = 0U;
        }
        for (index = 0U; index < AI_SD_FONT_BITMAP_CACHE_SLOTS; ++index) {
            g_cache->bitmaps[index].stamp = 0U;
        }
    }
    g_cache_clock = 1U;
    return g_cache_clock;
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_i16(const uint8_t *data)
{
    return (int16_t)read_u16(data);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void ai_sd_font_init(void)
{
    uint8_t family;

    if (g_initialized) return;
    memset(g_fonts, 0, sizeof(g_fonts));
    memset(g_file_slots, 0, sizeof(g_file_slots));
    for (family = 0U; family < AI_ALBUM_SD_FONT_FAMILY_COUNT; ++family) {
        ai_sd_font_context_t *context = &g_fonts[family];

        context->path = g_font_paths[family];
        context->family = family;
        context->font.get_glyph_dsc = ai_sd_font_get_glyph_dsc;
        context->font.get_glyph_bitmap = ai_sd_font_get_glyph_bitmap;
        context->font.line_height = 20;
        context->font.base_line = 5;
        context->font.underline_position = -2;
        context->font.underline_thickness = 1;
        /* 9.0无kerning字段(默认无字距调整) */
        context->font.subpx = LV_FONT_SUBPX_NONE;
        /* 9.0无static_bitmap字段 */
        context->font.dsc = context;
    }
    g_initialized = 1U;
}

static ai_sd_font_file_slot_t *ai_sd_font_find_file(uint8_t family)
{
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_FILE_SLOTS; ++index) {
        ai_sd_font_file_slot_t *slot = &g_file_slots[index];

        if (slot->open && slot->family == family) {
            slot->stamp = ai_sd_font_next_stamp();
            return slot;
        }
    }
    return NULL;
}

static void ai_sd_font_close_file(ai_sd_font_file_slot_t *slot)
{
    if (slot == NULL || !slot->open) return;
    (void)lv_fs_close(&slot->file);
    slot->open = 0U;
}

static ai_sd_font_file_slot_t *ai_sd_font_select_file_slot(void)
{
    ai_sd_font_file_slot_t *oldest = &g_file_slots[0];
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_FILE_SLOTS; ++index) {
        ai_sd_font_file_slot_t *slot = &g_file_slots[index];

        if (!slot->open) return slot;
        if (slot->stamp < oldest->stamp) oldest = slot;
    }
    ai_sd_font_close_file(oldest);
    return oldest;
}

static ai_sd_font_file_slot_t *ai_sd_font_open_file(uint8_t family)
{
    ai_sd_font_file_slot_t *slot = ai_sd_font_find_file(family);

    if (slot != NULL) return slot;
    slot = ai_sd_font_select_file_slot();
    if (lv_fs_open(&slot->file, g_fonts[family].path, LV_FS_MODE_RD) !=
        LV_FS_RES_OK) {
        return NULL;
    }
    slot->family = family;
    slot->stamp = ai_sd_font_next_stamp();
    slot->open = 1U;
    return slot;
}

static bool ai_sd_font_read(uint8_t family, uint32_t offset,
                            void *buffer, uint32_t length)
{
    ai_sd_font_file_slot_t *slot = ai_sd_font_open_file(family);
    uint32_t bytes_read = 0U;

    if (slot == NULL) return false;
    if (lv_fs_seek(&slot->file, offset, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
        ai_sd_font_close_file(slot);
        return false;
    }
    if (lv_fs_read(&slot->file, buffer, length, &bytes_read) != LV_FS_RES_OK) {
        ai_sd_font_close_file(slot);
        return false;
    }
    if (bytes_read != length) {
        ai_sd_font_close_file(slot);
        return false;
    }
    return true;
}

static bool ai_sd_font_get_file_size(uint8_t family, uint32_t *size)
{
    ai_sd_font_file_slot_t *slot = ai_sd_font_open_file(family);

    if (slot == NULL) return false;
    /* 9.0无lv_fs_get_size: seek到末尾用tell取大小 */
    if (lv_fs_seek(&slot->file, 0U, LV_FS_SEEK_END) != LV_FS_RES_OK ||
        lv_fs_tell(&slot->file, size) != LV_FS_RES_OK) {
        ai_sd_font_close_file(slot);
        return false;
    }
    (void)lv_fs_seek(&slot->file, 0U, LV_FS_SEEK_SET);
    return true;
}

static bool ai_sd_font_read_offset(uint8_t family, uint32_t offset,
                                   uint32_t *value)
{
    uint8_t encoded[4];

    if (!ai_sd_font_read(family, offset, encoded, sizeof(encoded))) return false;
    *value = read_u32(encoded);
    return true;
}

static bool ai_sd_font_parse_header(ai_sd_font_context_t *context,
                                    const uint8_t *header)
{
    uint16_t line_height = read_u16(&header[8]);
    int16_t base_line = read_i16(&header[10]);
    int16_t underline_position = read_i16(&header[12]);
    uint16_t underline_thickness = read_u16(&header[14]);
    uint32_t glyph_count = read_u32(&header[16]);
    uint32_t bmp_index_offset = read_u32(&header[20]);
    uint32_t supplementary_directory_offset = read_u32(&header[24]);
    uint32_t supplementary_directory_count = read_u32(&header[28]);
    uint32_t pages_offset = read_u32(&header[32]);
    uint32_t records_offset = read_u32(&header[36]);
    uint32_t file_size = read_u32(&header[40]);
    uint32_t max_bitmap_bytes = read_u32(&header[44]);

    if (memcmp(header, "AIFB", 4U) != 0 || read_u16(&header[4]) != 1U ||
        header[6] != 16U || header[7] != 4U ||
        read_u16(&header[48]) != AI_SD_FONT_RECORD_SIZE ||
        read_u16(&header[50]) != AI_SD_FONT_SUPPLEMENTARY_PAGE_SHIFT ||
        read_u32(&header[52]) != AI_SD_FONT_BMP_COUNT ||
        line_height == 0U || line_height > 64U ||
        base_line < 0 || base_line >= (int16_t)line_height ||
        underline_position < INT8_MIN || underline_position > INT8_MAX ||
        underline_thickness == 0U || underline_thickness > INT8_MAX ||
        glyph_count == 0U || glyph_count > 0x110000U ||
        bmp_index_offset != AI_SD_FONT_HEADER_SIZE ||
        supplementary_directory_offset !=
            bmp_index_offset + AI_SD_FONT_BMP_COUNT *
                                   AI_SD_FONT_INDEX_ENTRY_SIZE ||
        supplementary_directory_count !=
            AI_SD_FONT_SUPPLEMENTARY_PAGE_COUNT ||
        pages_offset != supplementary_directory_offset +
                            supplementary_directory_count *
                                AI_SD_FONT_INDEX_ENTRY_SIZE ||
        records_offset < pages_offset || records_offset >= file_size ||
        max_bitmap_bytes == 0U ||
        max_bitmap_bytes > AI_SD_FONT_MAX_BITMAP_BYTES) {
        return false;
    }
    context->glyph_count = glyph_count;
    context->bmp_index_offset = bmp_index_offset;
    context->supplementary_directory_offset = supplementary_directory_offset;
    context->supplementary_directory_count = supplementary_directory_count;
    context->pages_offset = pages_offset;
    context->records_offset = records_offset;
    context->file_size = file_size;
    context->font.line_height = line_height;
    context->font.base_line = base_line;
    context->font.underline_position = (int8_t)underline_position;
    context->font.underline_thickness = (int8_t)underline_thickness;
    return true;
}

static bool ai_sd_font_prepare(ai_sd_font_context_t *context)
{
    uint8_t header[AI_SD_FONT_HEADER_SIZE];
    uint32_t actual_file_size;

    if (context->state == AI_SD_FONT_READY) {
        return true;
    }
    /* SD 卷挂载可能晚于首次绘制; 失败后限时重试, 避免整族字模永久失效 */
    if (context->state == AI_SD_FONT_INVALID &&
        (uint32_t)(os_mseconds() - context->retry_at_ms) <
            AI_SD_FONT_RETRY_MS) {
        return false;
    }
    if (!ai_sd_font_read(context->family, 0U, header, sizeof(header)) ||
        !ai_sd_font_parse_header(context, header) ||
        !ai_sd_font_get_file_size(context->family, &actual_file_size) ||
        actual_file_size != context->file_size) {
        context->state = AI_SD_FONT_INVALID;
        context->retry_at_ms = (uint32_t)os_mseconds();
        os_printf("[AI_FONT] invalid bitmap path=%s\r\n", context->path);
        return false;
    }
    if (g_cache == NULL) g_cache = os_zalloc_psram(sizeof(*g_cache));
    if (g_cache == NULL) {
        context->state = AI_SD_FONT_INVALID;
        context->retry_at_ms = (uint32_t)os_mseconds();
        os_printf("[AI_FONT] bitmap cache allocation failed\r\n");
        return false;
    }
    context->state = AI_SD_FONT_READY;
    os_printf("[AI_FONT] ready bitmap path=%s glyphs=%u cache_bytes=%u\r\n",
              context->path, (unsigned)context->glyph_count,
              (unsigned)sizeof(*g_cache));
    return true;
}

static bool ai_sd_font_record_offset(ai_sd_font_context_t *context,
                                     uint32_t codepoint,
                                     uint32_t *record_offset)
{
    uint32_t page_table_offset;
    uint32_t page;

    *record_offset = 0U;
    if (codepoint <= 0xffffU) {
        return ai_sd_font_read_offset(
            context->family,
            context->bmp_index_offset +
                codepoint * AI_SD_FONT_INDEX_ENTRY_SIZE,
            record_offset);
    }
    if (codepoint > 0x10ffffU) return true;
    page = codepoint >> 8;
    if (page < AI_SD_FONT_SUPPLEMENTARY_PAGE_FIRST ||
        page - AI_SD_FONT_SUPPLEMENTARY_PAGE_FIRST >=
            context->supplementary_directory_count) {
        return true;
    }
    if (!ai_sd_font_read_offset(
            context->family,
            context->supplementary_directory_offset +
                (page - AI_SD_FONT_SUPPLEMENTARY_PAGE_FIRST) *
                    AI_SD_FONT_INDEX_ENTRY_SIZE,
            &page_table_offset)) {
        return false;
    }
    if (page_table_offset == 0U) return true;
    if (page_table_offset < context->pages_offset ||
        page_table_offset > context->records_offset ||
        context->records_offset - page_table_offset <
            AI_SD_FONT_PAGE_ENTRY_COUNT * AI_SD_FONT_INDEX_ENTRY_SIZE ||
        (page_table_offset - context->pages_offset) %
                (AI_SD_FONT_PAGE_ENTRY_COUNT * AI_SD_FONT_INDEX_ENTRY_SIZE) !=
            0U) {
        return false;
    }
    return ai_sd_font_read_offset(
        context->family,
        page_table_offset +
            (codepoint & 0xffU) * AI_SD_FONT_INDEX_ENTRY_SIZE,
        record_offset);
}

static ai_sd_font_descriptor_slot_t *ai_sd_font_find_descriptor(
    uint8_t family, uint32_t codepoint)
{
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_DESCRIPTOR_CACHE_SLOTS; ++index) {
        ai_sd_font_descriptor_slot_t *slot = &g_cache->descriptors[index];

        if (slot->valid && slot->family == family &&
            slot->codepoint == codepoint) {
            slot->stamp = ai_sd_font_next_stamp();
            return slot;
        }
    }
    return NULL;
}

static ai_sd_font_descriptor_slot_t *ai_sd_font_select_descriptor(void)
{
    ai_sd_font_descriptor_slot_t *oldest = &g_cache->descriptors[0];
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_DESCRIPTOR_CACHE_SLOTS; ++index) {
        ai_sd_font_descriptor_slot_t *slot = &g_cache->descriptors[index];

        if (!slot->valid) return slot;
        if (slot->stamp < oldest->stamp) oldest = slot;
    }
    return oldest;
}

static bool ai_sd_font_decode_record(ai_sd_font_context_t *context,
                                     uint32_t offset,
                                     ai_sd_font_record_t *record)
{
    uint8_t encoded[AI_SD_FONT_RECORD_SIZE];

    if (offset < context->records_offset ||
        offset > context->file_size - AI_SD_FONT_RECORD_SIZE ||
        !ai_sd_font_read(context->family, offset, encoded, sizeof(encoded))) {
        return false;
    }
    record->box_w = encoded[0];
    record->box_h = encoded[1];
    record->ofs_x = (int8_t)encoded[2];
    record->ofs_y = (int8_t)encoded[3];
    record->adv_w = read_u16(&encoded[4]);
    record->stride = read_u16(&encoded[6]);
    record->bitmap_size = read_u32(&encoded[8]);
    if (record->bitmap_size > AI_SD_FONT_MAX_BITMAP_BYTES ||
        record->bitmap_size != (uint32_t)record->stride * record->box_h ||
        offset + AI_SD_FONT_RECORD_SIZE + record->bitmap_size >
            context->file_size) {
        return false;
    }
    return record->box_w == 0U ||
           record->stride == ((uint16_t)record->box_w + 1U) / 2U;
}

static ai_sd_font_descriptor_slot_t *ai_sd_font_load_descriptor(
    ai_sd_font_context_t *context, uint32_t codepoint)
{
    ai_sd_font_descriptor_slot_t *slot =
        ai_sd_font_find_descriptor(context->family, codepoint);
    uint32_t offset;

    if (slot != NULL) return slot;
    slot = ai_sd_font_select_descriptor();
    memset(slot, 0, sizeof(*slot));
    if (!ai_sd_font_record_offset(context, codepoint, &offset)) return NULL;
    slot->family = context->family;
    slot->codepoint = codepoint;
    slot->stamp = ai_sd_font_next_stamp();
    slot->valid = 1U;
    if (offset == 0U) return slot;
    if (!ai_sd_font_decode_record(context, offset, &slot->record)) {
        slot->valid = 0U;
        return NULL;
    }
    slot->record_offset = offset;
    slot->found = 1U;
    return slot;
}

static bool ai_sd_font_fallback_has_glyph(const lv_font_t *font,
                                          uint32_t codepoint)
{
    lv_font_glyph_dsc_t descriptor;

    if (font->fallback == NULL ||
        font->fallback->get_glyph_dsc == NULL) return false;
    memset(&descriptor, 0, sizeof(descriptor));
    return font->fallback->get_glyph_dsc(
        font->fallback, &descriptor, codepoint, 0U);
}

static bool ai_sd_font_get_glyph_dsc(const lv_font_t *font,
                                     lv_font_glyph_dsc_t *out,
                                     uint32_t codepoint,
                                     uint32_t next_codepoint)
{
    ai_sd_font_context_t *context = (ai_sd_font_context_t *)font->dsc;
    ai_sd_font_descriptor_slot_t *slot;

    (void)next_codepoint;
    if (ai_sd_font_fallback_has_glyph(font, codepoint)) return false;
    slot = ai_sd_font_load_descriptor(context, codepoint);
    if (slot == NULL || !slot->found) return false;
    /* AIF stores 1/16 px; LVGL 9 font callbacks return whole pixels. */
    out->adv_w = (uint16_t)((slot->record.adv_w + 8U) >> 4);
    out->box_w = slot->record.box_w;
    out->box_h = slot->record.box_h;
    out->ofs_x = slot->record.ofs_x;
    out->ofs_y = slot->record.ofs_y;
    /* bpp=4,无stride/format/gid字段;位图必须是位连续流(w*4bit/行,
     * 行间不补字节),由get_glyph_bitmap提供(repack已从AIF行对齐布局转换) */
    out->bpp = 4U;
    out->is_placeholder = false;
    return true;
}

static ai_sd_font_bitmap_slot_t *ai_sd_font_find_bitmap(uint8_t family,
                                                         uint32_t codepoint)
{
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_BITMAP_CACHE_SLOTS; ++index) {
        ai_sd_font_bitmap_slot_t *slot = &g_cache->bitmaps[index];

        if (slot->valid && slot->family == family &&
            slot->codepoint == codepoint) {
            slot->stamp = ai_sd_font_next_stamp();
            return slot;
        }
    }
    return NULL;
}

static ai_sd_font_bitmap_slot_t *ai_sd_font_select_bitmap(void)
{
    ai_sd_font_bitmap_slot_t *oldest = &g_cache->bitmaps[0];
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_BITMAP_CACHE_SLOTS; ++index) {
        ai_sd_font_bitmap_slot_t *slot = &g_cache->bitmaps[index];

        if (!slot->valid) return slot;
        if (slot->stamp < oldest->stamp) oldest = slot;
    }
    return oldest;
}

/* AIF磁盘位图每行按字节对齐(stride=(w+1)/2,高nibble在前);本工程渲染器
 * lv_draw_sw_letter按"位连续流"寻址(width_bit=w*4,行间不补字节,见其
 * draw_letter_normal的map_p跨行续算)。偶数宽字形两种布局逐字节一致;
 * 奇数宽(如16px级CJK大量w=15)每行错4bit → 整字斜切乱码。
 * 加载后原地重排为位连续流:目标尺寸(ceil(w*h/2))<=源尺寸(stride*h),
 * 且目标字节下标恒<=源字节下标(仅首行相等,首行写回的nibble取自同一
 * 字节),原地前移安全。 */
static void ai_sd_font_put_nibble(uint8_t *bitmap, uint32_t index, uint8_t nibble)
{
    uint8_t *byte = &bitmap[index >> 1];

    if ((index & 1U) == 0U) {
        *byte = (uint8_t)((*byte & 0x0FU) | (uint8_t)(nibble << 4));
    } else {
        *byte = (uint8_t)((*byte & 0xF0U) | (nibble & 0x0FU));
    }
}

static void ai_sd_font_repack_bitmap(uint8_t *bitmap, uint16_t box_w,
                                     uint16_t box_h)
{
    uint32_t stride = ((uint32_t)box_w + 1U) / 2U;
    uint32_t nibble_index = 0U;
    uint16_t row;
    uint16_t x;

    if (box_w == 0U || (box_w & 1U) == 0U) return;
    for (row = 0U; row < box_h; ++row) {
        const uint8_t *src = &bitmap[(uint32_t)row * stride];
        for (x = 0U; x < box_w; ++x) {
            uint8_t byte = src[x >> 1];
            uint8_t nibble = ((x & 1U) == 0U) ? (uint8_t)(byte >> 4)
                                              : (uint8_t)(byte & 0x0FU);
            ai_sd_font_put_nibble(bitmap, nibble_index, nibble);
            nibble_index++;
        }
    }
}

static ai_sd_font_bitmap_slot_t *ai_sd_font_load_bitmap(
    ai_sd_font_context_t *context, ai_sd_font_descriptor_slot_t *descriptor)
{
    ai_sd_font_bitmap_slot_t *slot =
        ai_sd_font_find_bitmap(context->family, descriptor->codepoint);

    if (descriptor->record.bitmap_size > AI_SD_FONT_CACHED_BITMAP_BYTES) {
        return NULL;
    }
    if (slot != NULL) return slot;
    slot = ai_sd_font_select_bitmap();
    slot->valid = 0U;
    if (!ai_sd_font_read(context->family,
                         descriptor->record_offset + AI_SD_FONT_RECORD_SIZE,
                         slot->bitmap, descriptor->record.bitmap_size)) {
        return NULL;
    }
    ai_sd_font_repack_bitmap(slot->bitmap, descriptor->record.box_w,
                             descriptor->record.box_h);
    slot->family = context->family;
    slot->codepoint = descriptor->codepoint;
    slot->size = (uint16_t)descriptor->record.bitmap_size;
    slot->stamp = ai_sd_font_next_stamp();
    slot->valid = 1U;
    return slot;
}

/* 返回缓存的位连续4bpp位图指针(加载时已由repack从AIF行对齐布局转换) */

static const uint8_t *ai_sd_font_get_glyph_bitmap(const lv_font_t *font,
                                                   uint32_t codepoint)
{
    ai_sd_font_context_t *context = (ai_sd_font_context_t *)font->dsc;
    ai_sd_font_descriptor_slot_t *descriptor;
    ai_sd_font_bitmap_slot_t *bitmap;

    descriptor = ai_sd_font_load_descriptor(context, codepoint);
    if (descriptor == NULL || !descriptor->found ||
        descriptor->record.bitmap_size == 0U) {
        return NULL;
    }
    if (descriptor->record.bitmap_size <= AI_SD_FONT_CACHED_BITMAP_BYTES) {
        bitmap = ai_sd_font_load_bitmap(context, descriptor);
        if (bitmap == NULL || bitmap->size != descriptor->record.bitmap_size) {
            return NULL;
        }
        return bitmap->bitmap;
    }
    if (!ai_sd_font_read(
            context->family,
            descriptor->record_offset + AI_SD_FONT_RECORD_SIZE,
            g_cache->large_bitmap, descriptor->record.bitmap_size)) {
        return NULL;
    }
    ai_sd_font_repack_bitmap(g_cache->large_bitmap,
                             descriptor->record.box_w,
                             descriptor->record.box_h);
    return g_cache->large_bitmap;
}

const lv_font_t *ai_album_sd_font_get(ai_album_sd_font_family_t family,
                                      const lv_font_t *fallback)
{
    ai_sd_font_context_t *context;

    if (family >= AI_ALBUM_SD_FONT_FAMILY_COUNT || fallback == NULL) return NULL;
    ai_sd_font_init();
    context = &g_fonts[family];
    context->font.fallback = fallback;
    if (!ai_sd_font_prepare(context)) return NULL;
    return &context->font;
}

void ai_album_sd_font_close_files(void)
{
    uint8_t index;

    for (index = 0U; index < AI_SD_FONT_FILE_SLOTS; ++index) {
        ai_sd_font_close_file(&g_file_slots[index]);
    }
}
