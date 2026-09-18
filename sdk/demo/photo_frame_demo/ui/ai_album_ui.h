#ifndef AI_ALBUM_UI_H
#define AI_ALBUM_UI_H

/*
 * ai_album UI入口。由app_lvgl_init(lvgl_run)在LVGL任务上下文调用,
 * 之后按键动作经ai_album_ui_input_post()投递,由内部lv_timer消费。
 */
void ai_album_ui_bootstrap(void);

#endif
