#ifndef AI_ALBUM_LV_IMAGE_CACHE_COMPAT_H
#define AI_ALBUM_LV_IMAGE_CACHE_COMPAT_H

/*
 * 移植垫片:原工程LVGL 9.5的实例缓存接口 → 本工程LVGL 9.0内置图片缓存。
 * lv_image_cache_drop(dsc) 语义 = 让指定图片描述符的缓存条目失效,
 * 对应9.0的 lv_img_cache_invalidate_src(dsc)。
 */
#include "lvgl.h"

#define lv_image_cache_drop(src) lv_img_cache_invalidate_src((const void *)(src))

#endif
