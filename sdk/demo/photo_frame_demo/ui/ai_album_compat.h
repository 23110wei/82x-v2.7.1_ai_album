#ifndef AI_ALBUM_COMPAT_H
#define AI_ALBUM_COMPAT_H

/*
 * ai_album UI 按 LVGL 9.5 编写,本SDK内置LVGL为9.0.0-dev(旧命名)。
 * UI内容保持不变,仅在此做API映射:
 *   lv_display_t                        -> lv_disp_t
 *   lv_display_get_screen_active(disp)  -> lv_scr_act()
 *   lv_screen_load(scr)                 -> lv_scr_load(scr)
 *   lv_obj_get_display(obj)             -> lv_obj_get_disp(obj)
 */
#include "lvgl.h"

typedef lv_disp_t lv_display_t;
typedef lv_img_dsc_t lv_image_dsc_t;
/* 9.0无图像专用对齐类型,用通用lv_align_t代替(仅桩函数形参透传) */
typedef lv_align_t lv_image_align_t;

#define lv_display_get_screen_active(disp) lv_scr_act()
#define lv_screen_load(scr)                lv_scr_load(scr)
#define lv_obj_get_display(obj)            lv_obj_get_disp(obj)
#define lv_display_get_default()           lv_disp_get_default()

/* 9.1 del->delete/cnt->count 改名 */
#define lv_obj_delete(obj)                 lv_obj_del(obj)
#define lv_timer_delete(timer)             lv_timer_del(timer)
#define lv_obj_get_child_count(obj)        lv_obj_get_child_cnt(obj)

/* 9.1 btnmatrix->buttonmatrix 改名 */
#define lv_buttonmatrix_create(parent)     lv_btnmatrix_create(parent)
#define lv_buttonmatrix_set_map(obj, map)  lv_btnmatrix_set_map(obj, map)
#define lv_buttonmatrix_set_button_ctrl(obj, id, ctrl) \
        lv_btnmatrix_set_btn_ctrl(obj, id, ctrl)
/* 注意:_ctrl_all两个变体是两参(对象+属性),不要与三参的单按钮版本混淆 */
#define lv_buttonmatrix_set_button_ctrl_all(obj, ctrl) \
        lv_btnmatrix_set_btn_ctrl_all(obj, ctrl)
#define lv_buttonmatrix_clear_button_ctrl_all(obj, ctrl) \
        lv_btnmatrix_clear_btn_ctrl_all(obj, ctrl)
#define lv_buttonmatrix_get_button_text(obj, id) \
        lv_btnmatrix_get_btn_text(obj, id)
#define lv_buttonmatrix_set_selected_button(obj, id) \
        lv_btnmatrix_set_selected_btn(obj, id)
#define LV_BUTTONMATRIX_CTRL_CHECKABLE     LV_BTNMATRIX_CTRL_CHECKABLE
#define LV_BUTTONMATRIX_CTRL_CHECKED       LV_BTNMATRIX_CTRL_CHECKED

/* 9.1 img->image 改名(对齐类型仅桩函数形参透传,CONTAIN为占位值) */
#define lv_image_set_src(obj, src)         lv_img_set_src(obj, src)
#define LV_IMAGE_ALIGN_CONTAIN             0

/* 9.0用add/clear两个函数,set_flag三参形式展开 */
#define lv_obj_set_flag(obj, flag, en) \
        ((en) ? (void)lv_obj_add_flag(obj, flag) \
              : (void)lv_obj_clear_flag(obj, flag))

/* ---- 相册引擎(image_view.c)所需的9.5->9.0映射 ---- */
#define lv_event_get_target_obj(event)   lv_event_get_target(event)
#define lv_image_create(parent)          lv_img_create(parent)
#define lv_image_get_src(obj)            lv_img_get_src(obj)
#define lv_image_set_src(obj, src)       lv_img_set_src(obj, src)
#define lv_image_set_antialias(obj, en)  lv_img_set_antialias(obj, en)
#define lv_obj_remove_flag(obj, flag)    lv_obj_clear_flag(obj, flag)
/* 9.0的lv_malloc不带清零,包一层memset */
static inline void *lv_malloc_zeroed(size_t size) {
    void *p = lv_malloc(size);
    if (p != NULL) {
        memset(p, 0, size);
    }
    return p;
}
/* 9.0无inner_align概念,size_mode=REAL等效CONTAIN(源尺寸即显示尺寸) */
#define lv_image_set_inner_align(obj, align) \
        lv_img_set_size_mode(obj, LV_IMG_SIZE_MODE_REAL)
#define LV_IMAGE_ALIGN_COVER  1
/* 9.0无源图宽高getter,用decoder_get_info实现 */
#define lv_image_get_src_width(obj) \
        ai_album_img_src_dim(obj, 1)
#define lv_image_get_src_height(obj) \
        ai_album_img_src_dim(obj, 0)
int32_t ai_album_img_src_dim(lv_obj_t *obj, uint8 want_width);
uint8_t ai_album_img_src_is_file(const void *src);

#endif
