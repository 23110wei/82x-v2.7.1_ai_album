#include "ui/pages/ai_album_settings_wifi_page.h"

#include "basic_include.h"
#include "network/wifi_provision.h"
#include "ui/ai_album_i18n.h"
#include "ui/ai_album_ui_common.h"

#define WIFI_PAGE_MAX_NETWORKS 10U
#define WIFI_PAGE_ROWS_PER_COLUMN 5U
#define WIFI_PAGE_COLUMN_STEP 484
#define WIFI_PAGE_ROW_STEP 70
#define WIFI_PAGE_BUTTON_WIDTH 460
#define WIFI_PAGE_BUTTON_HEIGHT 62
#define WIFI_PAGE_SCAN_PERIOD_MS 250U

typedef struct {
    lv_obj_t *object;
    uint8_t index;
} wifi_focus_item_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *list;
    lv_obj_t *status;
    lv_timer_t *scan_timer;
    wifi_focus_item_t focusables[WIFI_PAGE_MAX_NETWORKS];
    wifi_provision_network_t networks[WIFI_PAGE_MAX_NETWORKS];
    uint8_t network_count;
    uint8_t rendered_count;
    uint8_t focus_count;
    uint8_t focus;
} wifi_page_state_t;

static wifi_page_state_t g_page;
/* The router destroys this page before creating the password page. */
static char g_selected_ssid[33];

static void wifi_page_reset_focus(void)
{
    memset(g_page.focusables, 0, sizeof(g_page.focusables));
    g_page.focus_count = 0U;
    g_page.focus = 0U;
}

static void wifi_page_add_focusable(lv_obj_t *object, uint8_t index)
{
    if (object == NULL || g_page.focus_count >= WIFI_PAGE_MAX_NETWORKS) {
        return;
    }
    g_page.focusables[g_page.focus_count].object = object;
    g_page.focusables[g_page.focus_count].index = index;
    g_page.focus_count++;
}

static void wifi_page_update_focus(void)
{
    uint8_t index;

    for (index = 0U; index < g_page.focus_count; ++index) {
        ai_album_ui_common_focus(
            g_page.focusables[index].object, index == g_page.focus,
            AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void wifi_page_set_status(const char *text, uint32_t color)
{
    if (g_page.status == NULL || text == NULL) {
        return;
    }
    ai_album_ui_common_set_label_text(g_page.status, text);
    lv_obj_set_style_text_color(g_page.status, lv_color_hex(color),
                                LV_PART_MAIN);
}

static void wifi_page_update_status(void)
{
    wifi_provision_status_t status;
    char text[128];

    if (g_page.network_count == 0U) {
        wifi_page_set_status("SCANNING...", AI_ALBUM_UI_COLOR_BLUE);
        return;
    }
    wifi_provision_get_status(&status);
    if (status.state == WIFI_PROVISION_STATE_ONLINE) {
        os_snprintf(text, sizeof(text),
                    ai_album_i18n_text("CONNECTED: %s (%s)"),
                    status.ssid, status.ip);
    } else if (status.state == WIFI_PROVISION_STATE_CONNECTING) {
        os_snprintf(text, sizeof(text),
                    ai_album_i18n_text("CONNECTING: %s ..."), status.ssid);
    } else {
        os_snprintf(text, sizeof(text),
                    ai_album_i18n_text("FOUND %u NETWORKS"),
                    (unsigned)g_page.network_count);
    }
    wifi_page_set_status(text, AI_ALBUM_UI_COLOR_BLUE);
}

static uint8_t wifi_page_read_networks(void)
{
    g_page.network_count = (uint8_t)wifi_provision_get_networks(
        g_page.networks, WIFI_PAGE_MAX_NETWORKS);
    return g_page.network_count;
}

static void wifi_page_render(void)
{
    uint8_t index;

    if (g_page.list == NULL) {
        return;
    }
    wifi_page_read_networks();
    wifi_page_reset_focus();
    lv_obj_clean(g_page.list);
    for (index = 0U; index < g_page.network_count; ++index) {
        char subtitle[64];
        const char *security = g_page.networks[index].encrypted ?
                                   "SECURED" : "OPEN";
        const char *strength = g_page.networks[index].rssi > -60 ?
                                   "STRONG" :
                                   (g_page.networks[index].rssi > -75 ?
                                        "MEDIUM" : "WEAK");
        lv_obj_t *button;

        os_snprintf(subtitle, sizeof(subtitle),
                    ai_album_i18n_text("%s %s RSSI:%d"),
                    ai_album_i18n_text(security),
                    ai_album_i18n_text(strength),
                    g_page.networks[index].rssi);
        button = ai_album_ui_common_button(
            g_page.list, g_page.networks[index].ssid, subtitle);

        lv_obj_set_pos(
            button,
            (int32_t)(index / WIFI_PAGE_ROWS_PER_COLUMN) *
                WIFI_PAGE_COLUMN_STEP,
            (int32_t)(index % WIFI_PAGE_ROWS_PER_COLUMN) *
                WIFI_PAGE_ROW_STEP);
        lv_obj_set_size(button, WIFI_PAGE_BUTTON_WIDTH,
                        WIFI_PAGE_BUTTON_HEIGHT);
        wifi_page_add_focusable(button, index);
    }
    g_page.rendered_count = g_page.network_count;
    wifi_page_update_status();
    wifi_page_update_focus();
}

static void wifi_page_scan_timer_cb(lv_timer_t *timer)
{
    uint8_t count;

    (void)timer;
    if (g_page.screen == NULL) {
        return;
    }
    count = wifi_page_read_networks();
    if (count != g_page.rendered_count) {
        wifi_page_render();
    } else {
        wifi_page_update_status();
    }
}

static void wifi_page_move_focus(ai_album_ui_action_t action)
{
    uint8_t column;
    int16_t row;
    int16_t next;
    uint8_t target;

    if (g_page.focus_count == 0U) {
        return;
    }
    column = g_page.focus / WIFI_PAGE_ROWS_PER_COLUMN;
    row = g_page.focus % WIFI_PAGE_ROWS_PER_COLUMN;
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        if (column > 0U) {
            target = (uint8_t)(g_page.focus - WIFI_PAGE_ROWS_PER_COLUMN);
            if (target < g_page.focus_count) {
                g_page.focus = target;
            }
        }
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        if (column == 0U) {
            target = (uint8_t)(g_page.focus + WIFI_PAGE_ROWS_PER_COLUMN);
            if (target < g_page.focus_count) {
                g_page.focus = target;
            }
        }
    } else if (action == AI_ALBUM_UI_ACTION_UP ||
               action == AI_ALBUM_UI_ACTION_DOWN) {
        next = row + (action == AI_ALBUM_UI_ACTION_UP ? -1 : 1);
        if (next < 0) {
            next = 0;
        } else if (next >= WIFI_PAGE_ROWS_PER_COLUMN) {
            next = WIFI_PAGE_ROWS_PER_COLUMN - 1;
        }
        target = (uint8_t)(column * WIFI_PAGE_ROWS_PER_COLUMN + next);
        if (target >= g_page.focus_count) {
            target = (uint8_t)(g_page.focus_count - 1U);
        }
        g_page.focus = target;
    }
    wifi_page_update_focus();
}

int ai_album_settings_wifi_page_create(lv_display_t *display)
{
    if (display == NULL || g_page.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_page, 0, sizeof(g_page));
    g_page.screen = ai_album_ui_common_prepare(
        display, "SETTINGS / WI-FI", 0xEAF1EFU);
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    {
        lv_obj_t *label = ai_album_ui_common_label(
            g_page.screen, "WI-FI NETWORKS", &lv_font_montserrat_24,
            AI_ALBUM_UI_COLOR_TEXT);
        lv_obj_set_pos(label, 40, 72);
        label = ai_album_ui_common_label(
            g_page.screen, "SELECT A NETWORK TO CONNECT",
            &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_MUTED);
        lv_obj_set_pos(label, 40, 108);
    }
    g_page.status = ai_album_ui_common_label(
        g_page.screen, "SCANNING...", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_BLUE);
    lv_obj_set_pos(g_page.status, 40, 130);
    g_page.list = ai_album_ui_common_plain(g_page.screen);
    lv_obj_set_pos(g_page.list, 40, 155);
    lv_obj_set_size(g_page.list, 944, 350);
    g_page.rendered_count = 0xFFU;
    wifi_provision_scan_start();
    wifi_page_render();
    g_page.scan_timer = lv_timer_create(
        wifi_page_scan_timer_cb, WIFI_PAGE_SCAN_PERIOD_MS, NULL);
    if (g_page.scan_timer == NULL) {
        ai_album_settings_wifi_page_destroy();
        return RET_ERR;
    }
    ai_album_ui_common_footer(
        g_page.screen, "POWER BACK   ARROWS SELECT   OK CONNECT   M RESCAN");
    return RET_OK;
}

int ai_album_settings_wifi_page_show(void)
{
    if (g_page.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_page.screen);
    wifi_page_update_focus();
    return RET_OK;
}

void ai_album_settings_wifi_page_destroy(void)
{
    if (g_page.scan_timer != NULL) {
        lv_timer_delete(g_page.scan_timer);
    }
    if (g_page.screen != NULL) {
        lv_obj_delete(g_page.screen);
    }
    memset(&g_page, 0, sizeof(g_page));
}

const char *ai_album_settings_wifi_page_selected_ssid(void)
{
    return g_selected_ssid;
}

ai_album_ui_route_t ai_album_settings_wifi_page_handle_action(
    ai_album_ui_action_t action)
{
    uint8_t network_index;

    if (action == AI_ALBUM_UI_ACTION_BACK) {
        return AI_ALBUM_UI_ROUTE_SETTINGS;
    }
    if (action == AI_ALBUM_UI_ACTION_MENU) {
        /* 手动重扫:手机热点后开等场景,立即触发新一轮全信道扫描。
         * 清空缓存计数让状态栏回到SCANNING...,列表由扫描定时器刷新 */
        g_page.network_count = 0U;
        g_page.rendered_count = 0xFFU;
        wifi_page_render();
        wifi_provision_scan_start();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_RIGHT ||
        action == AI_ALBUM_UI_ACTION_UP ||
        action == AI_ALBUM_UI_ACTION_DOWN) {
        wifi_page_move_focus(action);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action != AI_ALBUM_UI_ACTION_OK) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_page.focus >= g_page.focus_count) {
        wifi_page_set_status("WAITING FOR NETWORK SCAN",
                             AI_ALBUM_UI_COLOR_ORANGE);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    network_index = g_page.focusables[g_page.focus].index;
    if (network_index >= g_page.network_count) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    os_memset(g_selected_ssid, 0, sizeof(g_selected_ssid));
    os_strncpy(g_selected_ssid, g_page.networks[network_index].ssid,
               sizeof(g_selected_ssid) - 1U);
    return AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD;
}
