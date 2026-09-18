#include "ui/pages/ai_album_settings_wifi_password.h"

#include "basic_include.h"
#include "network/wifi_credentials.h"
#include "network/wifi_provision.h"
#include "ui/ai_album_ui_common.h"
#include "ui/ai_album_i18n.h"

#define WIFI_PASSWORD_MAX_LEN 63U
#define WIFI_PASSWORD_ROW_COUNT 4U
#define WIFI_PASSWORD_STATUS_POLL_MS 250U
#define WIFI_PASSWORD_KEY_SHIFT "SHIFT"
#define WIFI_PASSWORD_KEY_SPACE "SPACE"
#define WIFI_PASSWORD_KEY_BACKSPACE "BKSP"
#define WIFI_PASSWORD_KEY_SYMBOLS "SYM"

typedef enum {
    WIFI_PASSWORD_FOCUS_KEYBOARD = 0,
    WIFI_PASSWORD_FOCUS_ACTIONS,
} wifi_password_focus_area_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *password_label;
    lv_obj_t *keyboard;
    lv_obj_t *status;
    lv_obj_t *connect_button;
    lv_obj_t *cancel_button;
    wifi_password_focus_area_t focus_area;
    uint8_t action_focus;
    uint8_t key_row;
    uint8_t key_column;
    uint8_t shift_on;
    uint8_t symbols_on;
    uint8_t connection_pending;
    uint64 next_status_poll_ms;
    char ssid[33];
    char password[WIFI_PASSWORD_MAX_LEN + 1U];
} wifi_password_page_t;

static wifi_password_page_t g_password;

static const char *const g_keyboard_letters[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    WIFI_PASSWORD_KEY_SHIFT, "z", "x", "c", "v", "b", "n", "m", ",", ".", "\n",
    "123", WIFI_PASSWORD_KEY_SPACE, WIFI_PASSWORD_KEY_BACKSPACE,
    WIFI_PASSWORD_KEY_SYMBOLS, ""
};

static const char *const g_keyboard_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    WIFI_PASSWORD_KEY_SHIFT, "Z", "X", "C", "V", "B", "N", "M", ",", ".", "\n",
    "123", WIFI_PASSWORD_KEY_SPACE, WIFI_PASSWORD_KEY_BACKSPACE,
    WIFI_PASSWORD_KEY_SYMBOLS, ""
};

static const char *const g_keyboard_symbols[] = {
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "_", "=", "+", "[", "]", "{", "}", "|", "\\", "\n",
    "ABC", WIFI_PASSWORD_KEY_SPACE, WIFI_PASSWORD_KEY_BACKSPACE, ""
};

static const uint8_t g_row_start[WIFI_PASSWORD_ROW_COUNT] = {
    0U, 10U, 19U, 29U,
};

static const uint8_t g_letter_row_length[WIFI_PASSWORD_ROW_COUNT] = {
    10U, 9U, 10U, 4U,
};

static const uint8_t g_symbol_row_length[WIFI_PASSWORD_ROW_COUNT] = {
    10U, 9U, 10U, 3U,
};

static const char *const *wifi_password_keyboard_map(void)
{
    if (g_password.symbols_on) {
        return g_keyboard_symbols;
    }
    if (g_password.shift_on) {
        return g_keyboard_upper;
    }
    return g_keyboard_letters;
}

static uint8_t wifi_password_row_length(uint8_t row)
{
    return g_password.symbols_on ? g_symbol_row_length[row] :
                                    g_letter_row_length[row];
}

static uint8_t wifi_password_key_id(void)
{
    return (uint8_t)(g_row_start[g_password.key_row] +
                     g_password.key_column);
}

static void wifi_password_set_status(const char *text, uint32_t color)
{
    if (g_password.status != NULL) {
        ai_album_ui_common_set_label_text(g_password.status, text);
        lv_obj_set_style_text_color(g_password.status, lv_color_hex(color),
                                    LV_PART_MAIN);
    }
}

static void wifi_password_update_key_focus(void)
{
    uint8_t id;

    if (g_password.keyboard == NULL) {
        return;
    }
    id = wifi_password_key_id();
    lv_buttonmatrix_clear_button_ctrl_all(
        g_password.keyboard, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        g_password.keyboard, id,
        LV_BUTTONMATRIX_CTRL_CHECKABLE | LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(g_password.keyboard, id);
}

static void wifi_password_update_action_focus(void)
{
    ai_album_ui_common_focus(g_password.connect_button,
                             g_password.action_focus == 0U,
                             AI_ALBUM_UI_COLOR_GREEN);
    ai_album_ui_common_focus(g_password.cancel_button,
                             g_password.action_focus == 1U,
                             AI_ALBUM_UI_COLOR_GREEN);
}

static void wifi_password_update_focus(void)
{
    if (g_password.focus_area == WIFI_PASSWORD_FOCUS_KEYBOARD) {
        wifi_password_update_key_focus();
        ai_album_ui_common_focus(g_password.connect_button, 0U,
                                 AI_ALBUM_UI_COLOR_GREEN);
        ai_album_ui_common_focus(g_password.cancel_button, 0U,
                                 AI_ALBUM_UI_COLOR_GREEN);
    } else {
        if (g_password.keyboard != NULL) {
            lv_buttonmatrix_clear_button_ctrl_all(
                g_password.keyboard, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
        wifi_password_update_action_focus();
    }
}

static void wifi_password_update_label(void)
{
    char masked[WIFI_PASSWORD_MAX_LEN + 1U];
    uint32_t length = os_strlen(g_password.password);
    uint32_t index;

    for (index = 0U; index < length; ++index) {
        masked[index] = '*';
    }
    masked[length] = '\0';
    lv_label_set_text(g_password.password_label,
                      length == 0U ? "_" : masked);
}

static void wifi_password_move_key(int8_t row_delta, int8_t column_delta)
{
    int16_t row = (int16_t)g_password.key_row + row_delta;
    int16_t column = (int16_t)g_password.key_column + column_delta;
    uint8_t row_length;

    if (row < 0) {
        row = WIFI_PASSWORD_ROW_COUNT - 1;
    } else if (row >= WIFI_PASSWORD_ROW_COUNT) {
        row = 0;
    }
    g_password.key_row = (uint8_t)row;
    row_length = wifi_password_row_length(g_password.key_row);
    if (column < 0) {
        column = row_length - 1U;
    } else if (column >= row_length) {
        column = column_delta == 0 ? row_length - 1U : 0;
    }
    g_password.key_column = (uint8_t)column;
    wifi_password_update_key_focus();
}

static void wifi_password_append(const char *text)
{
    uint32_t current_length;
    uint32_t text_length;

    if (text == NULL) {
        return;
    }
    current_length = os_strlen(g_password.password);
    text_length = os_strlen(text);
    if (current_length + text_length > WIFI_PASSWORD_MAX_LEN) {
        wifi_password_set_status("PASSWORD IS TOO LONG",
                                 AI_ALBUM_UI_COLOR_RED);
        return;
    }
    os_memcpy(g_password.password + current_length, text, text_length + 1U);
    wifi_password_update_label();
}

static void wifi_password_delete(void)
{
    uint32_t length = os_strlen(g_password.password);

    if (length > 0U) {
        g_password.password[length - 1U] = '\0';
        wifi_password_update_label();
    }
}

static void wifi_password_activate_key(void)
{
    const char *key;

    if (g_password.key_row >= WIFI_PASSWORD_ROW_COUNT ||
        g_password.key_column >=
            wifi_password_row_length(g_password.key_row)) {
        return;
    }
    /* LVGL button IDs omit map line separators, so resolve text through LVGL. */
    key = lv_buttonmatrix_get_button_text(g_password.keyboard,
                                          wifi_password_key_id());

    if (key == NULL || key[0] == '\0' || key[0] == '\n') {
        return;
    }
    if (os_strcmp(key, WIFI_PASSWORD_KEY_SHIFT) == 0) {
        g_password.shift_on = (uint8_t)!g_password.shift_on;
        lv_buttonmatrix_set_map(g_password.keyboard,
                                wifi_password_keyboard_map());
        wifi_password_update_key_focus();
    } else if (os_strcmp(key, WIFI_PASSWORD_KEY_SYMBOLS) == 0 ||
               os_strcmp(key, "123") == 0) {
        g_password.symbols_on = 1U;
        g_password.shift_on = 0U;
        g_password.key_row = 0U;
        g_password.key_column = 0U;
        lv_buttonmatrix_set_map(g_password.keyboard,
                                wifi_password_keyboard_map());
        wifi_password_update_key_focus();
    } else if (os_strcmp(key, "ABC") == 0) {
        g_password.symbols_on = 0U;
        g_password.shift_on = 1U;
        g_password.key_row = 0U;
        g_password.key_column = 0U;
        lv_buttonmatrix_set_map(g_password.keyboard,
                                wifi_password_keyboard_map());
        wifi_password_update_key_focus();
    } else if (os_strcmp(key, WIFI_PASSWORD_KEY_SPACE) == 0) {
        wifi_password_append(" ");
    } else if (os_strcmp(key, WIFI_PASSWORD_KEY_BACKSPACE) == 0) {
        wifi_password_delete();
    } else {
        wifi_password_append(key);
    }
}

static void wifi_password_connect(void)
{
    int result;

    if (os_strlen(g_password.password) < 8U) {
        wifi_password_set_status("PASSWORD MUST BE >= 8 CHARACTERS",
                                 AI_ALBUM_UI_COLOR_RED);
        return;
    }
    result = wifi_provision_connect(g_password.ssid, g_password.password);
    if (result != RET_OK) {
        char error[64];

        g_password.connection_pending = 0U;
        os_snprintf(error, sizeof(error),
                    ai_album_i18n_text("CONNECT FAILED: %d"), result);
        wifi_password_set_status(error, AI_ALBUM_UI_COLOR_RED);
        return;
    }
    g_password.connection_pending = 1U;
    g_password.next_status_poll_ms = 0U;
    if (wifi_credentials_save(g_password.ssid, g_password.password) !=
        RET_OK) {
        wifi_password_set_status("CONNECTING  ·  SAVE FAILED",
                                 AI_ALBUM_UI_COLOR_ORANGE);
        return;
    }
    wifi_password_set_status("CONNECTING...", AI_ALBUM_UI_COLOR_BLUE);
}

static void wifi_password_move_action(int8_t delta)
{
    int16_t next = (int16_t)g_password.action_focus + delta;

    if (next < 0) {
        next = 1;
    } else if (next > 1) {
        next = 0;
    }
    g_password.action_focus = (uint8_t)next;
    wifi_password_update_action_focus();
}

static void wifi_password_handle_arrow(ai_album_ui_action_t action)
{
    if (g_password.focus_area == WIFI_PASSWORD_FOCUS_KEYBOARD) {
        if (action == AI_ALBUM_UI_ACTION_LEFT) {
            wifi_password_move_key(0, -1);
        } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
            wifi_password_move_key(0, 1);
        } else if (action == AI_ALBUM_UI_ACTION_UP) {
            wifi_password_move_key(-1, 0);
        } else if (g_password.key_row == WIFI_PASSWORD_ROW_COUNT - 1U) {
            g_password.focus_area = WIFI_PASSWORD_FOCUS_ACTIONS;
            g_password.action_focus = 0U;
            wifi_password_update_focus();
        } else {
            wifi_password_move_key(1, 0);
        }
    } else if (action == AI_ALBUM_UI_ACTION_LEFT ||
               action == AI_ALBUM_UI_ACTION_RIGHT) {
        wifi_password_move_action(action == AI_ALBUM_UI_ACTION_LEFT ? -1 : 1);
    } else if (action == AI_ALBUM_UI_ACTION_UP) {
        g_password.focus_area = WIFI_PASSWORD_FOCUS_KEYBOARD;
        g_password.key_row = WIFI_PASSWORD_ROW_COUNT - 1U;
        wifi_password_update_focus();
    }
}

static int wifi_password_build(lv_display_t *display)
{
    lv_obj_t *label;
    lv_obj_t *password_box;

    g_password.screen = ai_album_ui_common_prepare(
        display, "WI-FI PASSWORD", 0xEAF1EFU);
    if (g_password.screen == NULL) {
        return RET_ERR;
    }
    label = ai_album_ui_common_label(g_password.screen, "ENTER PASSWORD",
                                     &lv_font_montserrat_24,
                                     AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_set_pos(label, 40, 66);
    label = ai_album_ui_common_label(g_password.screen, g_password.ssid,
                                     &lv_font_montserrat_14,
                                     AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 40, 100);
    password_box = ai_album_ui_common_panel(
        g_password.screen, AI_ALBUM_UI_COLOR_WHITE, 10);
    lv_obj_set_pos(password_box, 40, 122);
    lv_obj_set_size(password_box, 944, 45);
    g_password.password_label = ai_album_ui_common_label(
        password_box, "_", &lv_font_montserrat_20,
        AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_align(g_password.password_label, LV_ALIGN_LEFT_MID, 18, 0);
    g_password.keyboard = lv_buttonmatrix_create(g_password.screen);
    lv_buttonmatrix_set_map(g_password.keyboard,
                            wifi_password_keyboard_map());
    lv_buttonmatrix_set_button_ctrl_all(
        g_password.keyboard, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_obj_set_pos(g_password.keyboard, 40, 175);
    lv_obj_set_size(g_password.keyboard, 944, 285);
    lv_obj_set_style_bg_color(g_password.keyboard, lv_color_hex(0xD1D3D9),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_password.keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_password.keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_password.keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_password.keyboard, lv_color_hex(0xFFFFFF),
                              LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_password.keyboard, lv_color_hex(0x000000),
                                LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_password.keyboard, &lv_font_montserrat_16,
                               LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_password.keyboard,
                              lv_color_hex(AI_ALBUM_UI_COLOR_GREEN),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    g_password.connect_button = ai_album_ui_common_button(
        g_password.screen, "CONNECT", NULL);
    lv_obj_set_pos(g_password.connect_button, 40, 470);
    lv_obj_set_size(g_password.connect_button, 460, 48);
    g_password.cancel_button = ai_album_ui_common_button(
        g_password.screen, "CANCEL", NULL);
    lv_obj_set_pos(g_password.cancel_button, 524, 470);
    lv_obj_set_size(g_password.cancel_button, 460, 48);
    g_password.status = ai_album_ui_common_label(
        g_password.screen, "ARROWS MOVE   OK TYPE   DOWN ACTIONS",
        &lv_font_montserrat_14, AI_ALBUM_UI_COLOR_GREEN);
    lv_obj_set_pos(g_password.status, 40, 528);
    ai_album_ui_common_footer(g_password.screen,
                              "POWER BACK   ARROWS SELECT   OK ACTION");
    wifi_password_update_label();
    wifi_password_update_focus();
    return RET_OK;
}

int ai_album_settings_wifi_password_create(lv_display_t *display,
                                            const char *ssid)
{
    int result;

    if (display == NULL || ssid == NULL || ssid[0] == '\0' ||
        g_password.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_password, 0, sizeof(g_password));
    os_strncpy(g_password.ssid, ssid, sizeof(g_password.ssid) - 1U);
    g_password.ssid[sizeof(g_password.ssid) - 1U] = '\0';
    g_password.focus_area = WIFI_PASSWORD_FOCUS_KEYBOARD;
    g_password.shift_on = 1U;
    result = wifi_password_build(display);
    if (result != RET_OK) {
        ai_album_settings_wifi_password_destroy();
    }
    return result;
}

int ai_album_settings_wifi_password_show(void)
{
    if (g_password.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_password.screen);
    wifi_password_update_focus();
    return RET_OK;
}

ai_album_ui_route_t ai_album_settings_wifi_password_poll(void)
{
    wifi_provision_status_t status;
    uint64 now_ms;

    if (g_password.screen == NULL || !g_password.connection_pending) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    now_ms = os_mseconds();
    if (now_ms < g_password.next_status_poll_ms) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    g_password.next_status_poll_ms = now_ms + WIFI_PASSWORD_STATUS_POLL_MS;
    wifi_provision_get_status(&status);
    if (status.state == WIFI_PROVISION_STATE_ONLINE) {
        g_password.connection_pending = 0U;
        os_printf("ai_album: WiFi connected to '%s', leave password page\r\n",
                  status.ssid);
        return AI_ALBUM_UI_ROUTE_SETTINGS_WIFI;
    }
    if (status.state == WIFI_PROVISION_STATE_FAILED) {
        g_password.connection_pending = 0U;
        wifi_password_set_status("CONNECT FAILED - TRY AGAIN",
                                 AI_ALBUM_UI_COLOR_RED);
    } else if (status.state == WIFI_PROVISION_STATE_DHCP) {
        wifi_password_set_status("CONNECTED - OBTAINING IP...",
                                 AI_ALBUM_UI_COLOR_BLUE);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

void ai_album_settings_wifi_password_destroy(void)
{
    if (g_password.screen != NULL) {
        lv_obj_delete(g_password.screen);
    }
    memset(&g_password, 0, sizeof(g_password));
}

ai_album_ui_route_t ai_album_settings_wifi_password_handle_action(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        return AI_ALBUM_UI_ROUTE_SETTINGS_WIFI;
    }
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_RIGHT ||
        action == AI_ALBUM_UI_ACTION_UP ||
        action == AI_ALBUM_UI_ACTION_DOWN) {
        wifi_password_handle_arrow(action);
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (action != AI_ALBUM_UI_ACTION_OK) {
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_password.focus_area == WIFI_PASSWORD_FOCUS_KEYBOARD) {
        wifi_password_activate_key();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_password.action_focus == 0U) {
        wifi_password_connect();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    return AI_ALBUM_UI_ROUTE_SETTINGS_WIFI;
}
