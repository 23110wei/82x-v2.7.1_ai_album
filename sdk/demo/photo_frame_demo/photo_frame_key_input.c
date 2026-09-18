#include "basic_include.h"
#include "keyWork.h"
#include "keyScan.h"
#include "hardware/power_ctrl.h"
#include "ui/ai_album_ui_input.h"

/*
 * 实体按键 -> ai_album UI动作桥接。
 * 按键阶梯(PA15,实测mV,见adkey.c的PHOTO_FRAME_DEMO键表):
 *   电源0 / OK 670 / 右1620 / M(菜单)1843 / 左2226 / 上2600 / 下2929
 * 语义与原工程app_key_handler一致:
 *   电源短按=BACK 长按=POWER_OFF;OK短按=OK 长按=OK_LONG;
 *   M短按=MENU 长按/松开=TALK_START/TALK_STOP(语音页用,当前无消费方)。
 * keyvalue编码:key = keyvalue >> 8, event = keyvalue & 0xff。
 */

static uint8 key_talk_active;
static uint32 g_last_key_raw;
/* 按键开机的"开机按住"尚未松开:此时电源键事件(keyWork把开机长按
 * 识别为LDOWN)是给电源锁存用的,不是UI动作,全部吞掉直到首次松开 */
static uint8 key_power_boot_hold;

static void photo_frame_key_post(uint32 key, uint32 event,
                                 ai_album_ui_action_t action)
{
    if (action != AI_ALBUM_UI_ACTION_TALK_STOP) {
        os_printf(KERN_DEBUG "key id=%u event=%u raw=%u -> action=%u\r\n",
                  (unsigned)key, (unsigned)event,
                  (unsigned)g_last_key_raw, (unsigned)action);
    }
    ai_album_ui_input_post(action);
}

static uint32_t photo_frame_key_callback(
    struct key_callback_list_s *callback_list, uint32_t keyvalue,
    uint32_t extern_value)
{
    uint32_t key   = keyvalue >> 8;
    uint32_t event = keyvalue & 0xff;

    (void)callback_list;
    /* extern_value为AD采样原始值(0~2047),用于键表校准 */
    g_last_key_raw = extern_value;

    if (event != KEY_EVENT_SUP && event != KEY_EVENT_LDOWN &&
        event != KEY_EVENT_LUP) {
        return 0;
    }

    switch (key) {
        case AD_PRESS: /* OK */
            if (event == KEY_EVENT_LDOWN) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_OK_LONG);
            } else if (event == KEY_EVENT_SUP) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_OK);
            }
            break;
        case AD_RIGHT:
            if (event == KEY_EVENT_SUP) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_RIGHT);
            }
            break;
        case AD_LEFT:
            if (event == KEY_EVENT_SUP) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_LEFT);
            }
            break;
        case AD_UP:
            if (event == KEY_EVENT_SUP) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_UP);
            }
            break;
        case AD_DOWN:
            if (event == KEY_EVENT_SUP) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_DOWN);
            }
            break;
        case AD_B: /* M 菜单/通话键 */
            if (event == KEY_EVENT_SUP && !key_talk_active) {
                photo_frame_key_post(key, event, AI_ALBUM_UI_ACTION_MENU);
            } else if (event == KEY_EVENT_LDOWN) {
                key_talk_active = 1;
                photo_frame_key_post(key, event,
                                     AI_ALBUM_UI_ACTION_TALK_START);
            } else if ((event == KEY_EVENT_LUP || event == KEY_EVENT_SUP) &&
                       key_talk_active) {
                key_talk_active = 0;
                photo_frame_key_post(key, event,
                                     AI_ALBUM_UI_ACTION_TALK_STOP);
            }
            break;
        case AD_A: /* 电源键 */
            if (event == KEY_EVENT_LDOWN) {
                if (key_power_boot_hold) {
                    /* 开机按住的延续:keyWork长按判定,非UI动作,吞掉 */
                    os_printf(KERN_DEBUG "key: power boot-hold swallowed\r\n");
                    break;
                }
                photo_frame_key_post(key, event,
                                     AI_ALBUM_UI_ACTION_POWER_OFF);
            } else if (event == KEY_EVENT_LUP ||
                       event == KEY_EVENT_SUP) {
                if (key_power_boot_hold) {
                    /* 开机按住首次松开:重新武装,此次松开也不算BACK */
                    os_printf(KERN_DEBUG "key: power boot-hold released\r\n");
                    key_power_boot_hold = 0;
                    break;
                }
                /* 正常路径仅短按松开(SUP)=BACK;长按松开(LUP)不发
                 * 动作,否则长按弹出的关机弹窗会被松手瞬间关掉 */
                if (event == KEY_EVENT_SUP) {
                    photo_frame_key_post(key, event,
                                         AI_ALBUM_UI_ACTION_BACK);
                }
            }
            break;
        default:
            break;
    }
    return 0;
}

void photo_frame_key_input_init(void)
{
    key_power_boot_hold = power_ctrl_booted_with_key();
    add_keycallback(photo_frame_key_callback, NULL);
}
