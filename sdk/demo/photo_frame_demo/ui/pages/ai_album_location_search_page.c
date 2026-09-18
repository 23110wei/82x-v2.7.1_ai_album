#include "ui/pages/ai_album_location_search_page.h"

#include "basic_include.h"
#include "network/ai_album_weather_service.h"
#include "ui/ai_album_ui_common.h"

#include <string.h>

#define SEARCH_KEY_ROW_COUNT 4U
#define SEARCH_KEY_SHIFT "SHIFT"
#define SEARCH_KEY_SPACE "SPACE"
#define SEARCH_KEY_BACKSPACE "BKSP"
#define SEARCH_KEY_SYMBOLS "SYM"

typedef enum {
    SEARCH_VIEW_ENTRY = 0,
    SEARCH_VIEW_SEARCHING,
    SEARCH_VIEW_RESULTS,
} location_search_view_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *body;
    lv_obj_t *status;
    lv_obj_t *query_label;
    lv_obj_t *keyboard;
    lv_obj_t *result_buttons[AI_ALBUM_WEATHER_CITY_RESULT_COUNT];
    lv_timer_t *timer;
    ai_album_weather_city_search_t results;
    char query[AI_ALBUM_WEATHER_LOCATION_NAME_LEN + 1U];
    uint32 observed_sequence;
    location_search_view_t view;
    uint8 key_row;
    uint8 key_column;
    uint8 result_focus;
    uint8 shift_on;   /* 0=lowercase, 1=uppercase */
    uint8 kb_mode;    /* 0=letters, 1=symbols */
} location_search_page_t;

static location_search_page_t g_search;

/* Phone-style virtual keyboard layout - iOS/Android style */
static const char *const g_keyboard_default[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    SEARCH_KEY_SHIFT, "z", "x", "c", "v", "b", "n", "m", ",", ".", "\n",
    "123", SEARCH_KEY_SPACE, SEARCH_KEY_BACKSPACE, SEARCH_KEY_SYMBOLS,
    "SEARCH", ""
};

static const char *const g_keyboard_symbols[] = {
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "_", "=", "+", "[", "]", "{", "}", "|", "\\", "\n",
    "ABC", SEARCH_KEY_SPACE, SEARCH_KEY_BACKSPACE, "SEARCH", ""
};

static const char *const g_keyboard_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    SEARCH_KEY_SHIFT, "Z", "X", "C", "V", "B", "N", "M", ",", ".", "\n",
    "123", SEARCH_KEY_SPACE, SEARCH_KEY_BACKSPACE, SEARCH_KEY_SYMBOLS,
    "SEARCH", ""
};

/* Letter rows: 10, 9, 10, 5; symbol row: 10, 9, 10, 4. */
static const uint8 g_row_start[SEARCH_KEY_ROW_COUNT] = {0U, 10U, 19U, 29U};
static const uint8 g_letter_row_length[SEARCH_KEY_ROW_COUNT] =
    {10U, 9U, 10U, 5U};
static const uint8 g_symbol_row_length[SEARCH_KEY_ROW_COUNT] =
    {10U, 9U, 10U, 4U};

static const char *const *search_get_keyboard_map(void)
{
    if (g_search.kb_mode == 1) {
        return g_keyboard_symbols;
    }
    if (g_search.shift_on) {
        return g_keyboard_upper;
    }
    return g_keyboard_default;
}

static uint8 search_key_id(void)
{
    return (uint8)(g_row_start[g_search.key_row] + g_search.key_column);
}

static uint8 search_row_length(uint8 row)
{
    return g_search.kb_mode == 1U ? g_symbol_row_length[row] :
                                    g_letter_row_length[row];
}

static void search_set_status(const char *text, uint32 color)
{
    ai_album_ui_common_set_label_text(g_search.status, text);
    lv_obj_set_style_text_color(g_search.status, lv_color_hex(color),
                                LV_PART_MAIN);
}

static void search_update_query_label(void)
{
    lv_label_set_text_fmt(g_search.query_label, "%s_",
                          g_search.query[0] == '\0' ? "" : g_search.query);
}

static void search_update_key_focus(void)
{
    uint8 id = search_key_id();

    lv_buttonmatrix_clear_button_ctrl_all(
        g_search.keyboard, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        g_search.keyboard, id,
        LV_BUTTONMATRIX_CTRL_CHECKABLE | LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(g_search.keyboard, id);
}

static void search_apply_keyboard_style(void)
{
    /* Main keyboard background - dark gray like iOS */
    lv_obj_set_style_bg_color(g_search.keyboard,
                              lv_color_hex(0xD1D3D9), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_search.keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_search.keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_search.keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_search.keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_search.keyboard, 2, LV_PART_MAIN);

    /* Key style - white background like iOS */
    lv_obj_set_style_bg_color(g_search.keyboard,
                              lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(g_search.keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(g_search.keyboard, 5, LV_PART_ITEMS);
    lv_obj_set_style_border_width(g_search.keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_search.keyboard,
                                lv_color_hex(0x000000), LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_search.keyboard, &lv_font_montserrat_16,
                               LV_PART_ITEMS);
    lv_obj_set_style_pad_all(g_search.keyboard, 0, LV_PART_ITEMS);

    /* Checked/pressed state - light gray */
    lv_obj_set_style_bg_color(g_search.keyboard,
                              lv_color_hex(0xAEB3BD),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
}

static void search_create_heading(const char *title, const char *subtitle)
{
    lv_obj_t *label = ai_album_ui_common_label(
        g_search.body, title, &lv_font_montserrat_24,
        AI_ALBUM_UI_COLOR_TEXT);

    lv_obj_set_pos(label, 40, 14);
    label = ai_album_ui_common_label(
        g_search.body, subtitle, &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_MUTED);
    lv_obj_set_pos(label, 40, 48);
}

static void search_render_entry(void)
{
    lv_obj_t *input;

    lv_obj_clean(g_search.body);
    g_search.view = SEARCH_VIEW_ENTRY;
    g_search.key_row = 0U;
    g_search.key_column = 0U;
    search_create_heading("SEARCH CITY", "ENTER AN ENGLISH CITY NAME");
    input = ai_album_ui_common_panel(
        g_search.body, AI_ALBUM_UI_COLOR_WHITE, 12);
    lv_obj_set_pos(input, 40, 78);
    lv_obj_set_size(input, 944, 58);
    g_search.query_label = ai_album_ui_common_label(
        input, "_", &lv_font_montserrat_20, AI_ALBUM_UI_COLOR_TEXT);
    lv_obj_align(g_search.query_label, LV_ALIGN_LEFT_MID, 18, 0);
    g_search.keyboard = lv_buttonmatrix_create(g_search.body);
    lv_buttonmatrix_set_map(g_search.keyboard, search_get_keyboard_map());
    lv_buttonmatrix_set_button_ctrl_all(
        g_search.keyboard, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_obj_set_pos(g_search.keyboard, 40, 145);
    lv_obj_set_size(g_search.keyboard, 944, 340);
    search_apply_keyboard_style();
    lv_obj_set_style_text_font(g_search.keyboard, &lv_font_montserrat_16,
                               LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_search.keyboard,
                              lv_color_hex(AI_ALBUM_UI_COLOR_GREEN),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    search_update_query_label();
    search_update_key_focus();
    search_set_status("ARROWS MOVE   OK TYPE   SELECT SEARCH WHEN READY",
                      AI_ALBUM_UI_COLOR_GREEN);
}

static void search_update_result_focus(void)
{
    uint8 index;

    for (index = 0U; index < g_search.results.count; ++index) {
        ai_album_ui_common_focus(g_search.result_buttons[index],
                                 index == g_search.result_focus,
                                 AI_ALBUM_UI_COLOR_GREEN);
    }
}

static void search_create_result_button(uint8 index)
{
    const ai_album_weather_city_result_t *city = &g_search.results.cities[index];
    char subtitle[80];

    os_snprintf(subtitle, sizeof(subtitle), "%s%s%s",
                city->admin1,
                city->admin1[0] != '\0' && city->country[0] != '\0' ?
                    " / " : "",
                city->country);
    g_search.result_buttons[index] = ai_album_ui_common_button(
        g_search.body, city->location.name, subtitle);
    lv_obj_set_pos(g_search.result_buttons[index], 40,
                   90 + (int32_t)index * 76);
    lv_obj_set_size(g_search.result_buttons[index], 944, 66);
}

static const char *search_error_text(uint8 error)
{
    if (error == AI_ALBUM_WEATHER_SEARCH_ERROR_OFFLINE) {
        return "NETWORK UNAVAILABLE - POWER BACK AND RETRY";
    }
    if (error == AI_ALBUM_WEATHER_SEARCH_ERROR_INVALID_QUERY) {
        return "ENTER AT LEAST TWO CHARACTERS";
    }
    if (error == AI_ALBUM_WEATHER_SEARCH_ERROR_NO_RESULTS) {
        return "NO MATCHING CITY - POWER BACK TO EDIT";
    }
    return "CITY SEARCH FAILED - POWER BACK TO RETRY";
}

static void search_render_results(void)
{
    uint8 index;

    lv_obj_clean(g_search.body);
    g_search.view = SEARCH_VIEW_RESULTS;
    g_search.result_focus = 0U;
    memset(g_search.result_buttons, 0, sizeof(g_search.result_buttons));
    search_create_heading("CITY RESULTS", g_search.query);
    for (index = 0U; index < g_search.results.count; ++index) {
        search_create_result_button(index);
    }
    if (g_search.results.count > 0U) {
        search_update_result_focus();
        search_set_status("ARROWS SELECT   OK APPLY   POWER EDIT QUERY",
                          AI_ALBUM_UI_COLOR_GREEN);
    } else {
        search_set_status(search_error_text(g_search.results.error),
                          AI_ALBUM_UI_COLOR_RED);
    }
}

static void search_poll(lv_timer_t *timer)
{
    ai_album_weather_city_search_t results;

    (void)timer;
    if (g_search.view != SEARCH_VIEW_SEARCHING) {
        return;
    }
    ai_album_weather_service_get_city_search(&results);
    if (results.sequence == g_search.observed_sequence || results.searching) {
        return;
    }
    g_search.observed_sequence = results.sequence;
    g_search.results = results;
    search_render_results();
}

int ai_album_location_search_page_create(lv_display_t *display)
{
    if (display == NULL || g_search.screen != NULL) {
        return RET_ERR;
    }
    memset(&g_search, 0, sizeof(g_search));
    g_search.screen = ai_album_ui_common_prepare(
        display, "SETTINGS / LOCATION SEARCH", 0xEAF1EFU);
    if (g_search.screen == NULL) {
        return RET_ERR;
    }
    g_search.body = ai_album_ui_common_plain(g_search.screen);
    lv_obj_set_pos(g_search.body, 0, 48);
    lv_obj_set_size(g_search.body, 1024, 480);
    g_search.shift_on = 1U;  /* Start uppercase */
    g_search.kb_mode = 0U;  /* Letters mode */
    g_search.status = ai_album_ui_common_label(
        g_search.screen, "", &lv_font_montserrat_14,
        AI_ALBUM_UI_COLOR_GREEN);
    lv_obj_set_pos(g_search.status, 40, 525);
    ai_album_ui_common_footer(
        g_search.screen, "POWER BACK   ARROWS SELECT   OK CONFIRM");
    search_render_entry();
    g_search.timer = lv_timer_create(search_poll, 250U, NULL);
    if (g_search.timer == NULL) {
        ai_album_location_search_page_destroy();
        return RET_ERR;
    }
    return RET_OK;
}

int ai_album_location_search_page_show(void)
{
    if (g_search.screen == NULL) {
        return RET_ERR;
    }
    lv_screen_load(g_search.screen);
    return RET_OK;
}

void ai_album_location_search_page_destroy(void)
{
    if (g_search.timer != NULL) {
        lv_timer_delete(g_search.timer);
    }
    if (g_search.screen != NULL) {
        lv_obj_delete(g_search.screen);
    }
    memset(&g_search, 0, sizeof(g_search));
}

static void search_move_key(int8 row_delta, int8 column_delta)
{
    int16 row = (int16)g_search.key_row + row_delta;
    int16 column;

    if (row < 0) {
        row = SEARCH_KEY_ROW_COUNT - 1;
    } else if (row >= SEARCH_KEY_ROW_COUNT) {
        row = 0;
    }
    g_search.key_row = (uint8)row;
    column = (int16)g_search.key_column + column_delta;
    if (column < 0) {
        column = search_row_length(g_search.key_row) - 1;
    } else if (column >= search_row_length(g_search.key_row)) {
        column = column_delta == 0 ?
                     search_row_length(g_search.key_row) - 1 : 0;
    }
    g_search.key_column = (uint8)column;
    if (g_search.key_column >= search_row_length(g_search.key_row)) {
        g_search.key_column = search_row_length(g_search.key_row) - 1U;
    }
    search_update_key_focus();
}

static void search_append_text(const char *text)
{
    uint32 query_length = (uint32)strlen(g_search.query);
    uint32 text_length = (uint32)strlen(text);

    if (query_length + text_length > AI_ALBUM_WEATHER_LOCATION_NAME_LEN) {
        search_set_status("CITY NAME IS TOO LONG", AI_ALBUM_UI_COLOR_RED);
        return;
    }
    memcpy(g_search.query + query_length, text, text_length + 1U);
    search_update_query_label();
}

static void search_delete_character(void)
{
    uint32 length = (uint32)strlen(g_search.query);

    if (length > 0U) {
        g_search.query[length - 1U] = '\0';
        search_update_query_label();
    }
}

static void search_start_request(void)
{
    ai_album_weather_city_search_t current;

    if (ai_album_weather_service_search_city(g_search.query) != RET_OK) {
        search_set_status("ENTER AT LEAST TWO CHARACTERS",
                          AI_ALBUM_UI_COLOR_RED);
        return;
    }
    ai_album_weather_service_get_city_search(&current);
    g_search.observed_sequence = current.sequence;
    g_search.view = SEARCH_VIEW_SEARCHING;
    search_set_status("SEARCHING OPEN-METEO...", AI_ALBUM_UI_COLOR_BLUE);
}

static void search_activate_key(void)
{
    /* LVGL button IDs omit map line separators, so resolve text through LVGL. */
    const char *key = lv_buttonmatrix_get_button_text(g_search.keyboard,
                                                      search_key_id());

    if (key == NULL) {
        return;
    }

    if (os_strcmp(key, SEARCH_KEY_SHIFT) == 0) {
        g_search.shift_on = !g_search.shift_on;
        lv_buttonmatrix_set_map(g_search.keyboard, search_get_keyboard_map());
        search_update_key_focus();
    } else if (os_strcmp(key, SEARCH_KEY_SYMBOLS) == 0 ||
               os_strcmp(key, "123") == 0) {
        g_search.kb_mode = 1U;
        g_search.shift_on = 0U;
        g_search.key_row = 0U;
        g_search.key_column = 0U;
        lv_buttonmatrix_set_map(g_search.keyboard, search_get_keyboard_map());
        search_update_key_focus();
    } else if (os_strcmp(key, "ABC") == 0) {
        g_search.kb_mode = 0U;
        g_search.shift_on = 1U;
        g_search.key_row = 0U;
        g_search.key_column = 0U;
        lv_buttonmatrix_set_map(g_search.keyboard, search_get_keyboard_map());
        search_update_key_focus();
    } else if (os_strcmp(key, SEARCH_KEY_SPACE) == 0) {
        search_append_text(" ");
    } else if (os_strcmp(key, SEARCH_KEY_BACKSPACE) == 0) {
        search_delete_character();
    } else if (os_strcmp(key, "SEARCH") == 0) {
        search_start_request();
    } else if (key[0] != '\0' && key[0] != '\n') {
        /* Regular character key */
        search_append_text(key);
    }
}

static void search_move_result(int8 delta)
{
    if (g_search.results.count == 0U) {
        return;
    }
    g_search.result_focus = (uint8)(
        (g_search.result_focus + g_search.results.count + delta) %
        g_search.results.count);
    search_update_result_focus();
}

static ai_album_ui_route_t search_handle_entry(ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT) {
        search_move_key(0, -1);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT) {
        search_move_key(0, 1);
    } else if (action == AI_ALBUM_UI_ACTION_UP) {
        search_move_key(-1, 0);
    } else if (action == AI_ALBUM_UI_ACTION_DOWN) {
        search_move_key(1, 0);
    } else if (action == AI_ALBUM_UI_ACTION_OK) {
        search_activate_key();
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

static ai_album_ui_route_t search_handle_results(ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_LEFT ||
        action == AI_ALBUM_UI_ACTION_UP) {
        search_move_result(-1);
    } else if (action == AI_ALBUM_UI_ACTION_RIGHT ||
               action == AI_ALBUM_UI_ACTION_DOWN) {
        search_move_result(1);
    } else if (action == AI_ALBUM_UI_ACTION_OK &&
               g_search.results.count > 0U) {
        if (ai_album_weather_service_select_city_result(
                g_search.result_focus) != RET_OK) {
            search_set_status("CITY ACTIVE - PERSISTENCE FAILED",
                              AI_ALBUM_UI_COLOR_RED);
            return AI_ALBUM_UI_ROUTE_NONE;
        }
        return AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION;
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}

ai_album_ui_route_t ai_album_location_search_page_handle_action(
    ai_album_ui_action_t action)
{
    if (action == AI_ALBUM_UI_ACTION_BACK) {
        if (g_search.view == SEARCH_VIEW_ENTRY) {
            return AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION;
        }
        search_render_entry();
        return AI_ALBUM_UI_ROUTE_NONE;
    }
    if (g_search.view == SEARCH_VIEW_ENTRY) {
        return search_handle_entry(action);
    }
    if (g_search.view == SEARCH_VIEW_RESULTS) {
        return search_handle_results(action);
    }
    return AI_ALBUM_UI_ROUTE_NONE;
}
