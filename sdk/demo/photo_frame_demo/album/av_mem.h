#ifndef AI_ALBUM_AV_MEM_H
#define AI_ALBUM_AV_MEM_H

/*
 * 移植垫片:原工程av_mem.h(component)映射到本工程AV堆。
 * AV PSRAM堆: video_psram_init 分配(当前4MB,含loader的2x1.2MB解码槽位)
 * AV SRAM堆:  video_sram_init 分配(当前100KB,含scale行缓冲)
 */
#include "basic_include.h"
#include "lib/heap/av_heap.h"
#include "lib/heap/av_psram_heap.h"

#define av_mem_alloc_psram(size)  av_psram_malloc((int)(size))
#define av_mem_free_psram(ptr)    av_psram_free((ptr))
#define av_mem_alloc_sram(size)   av_malloc((int)(size))
#define av_mem_free_sram(ptr)     av_free((ptr))

#endif
