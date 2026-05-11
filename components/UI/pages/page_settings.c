#include "page_settings.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_backend.h"
#include "ui_fonts.h"
#include "ui_page_manager.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_location_label = NULL;
static lv_obj_t *s_wifi_status_label = NULL;
static lv_obj_t *s_ble_switch = NULL;
static bool s_nav_pending = false;
static bool s_ble_syncing = false;

static char s_location[32] = "上海 · 浦东新区";

typedef enum {
    SETTINGS_NAV_BACK = 0,
    SETTINGS_NAV_WIFI,
    SETTINGS_NAV_STATUS,
    SETTINGS_NAV_GYRO,
} settings_nav_action_t;

static lv_color_t c_hex(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void set_label_font_color(lv_obj_t *label, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c_hex(rgb), 0);
}

static lv_obj_t *create_round_button(
    lv_obj_t *parent,
    lv_coord_t x,
    lv_coord_t y,
    lv_coord_t w,
    lv_coord_t h,
    uint32_t bg,
    const char *text,
    const lv_font_t *font)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, c_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, w);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    return btn;
}

static lv_obj_t *create_header_back_text_btn(lv_obj_t *parent, const char *text, lv_coord_t w)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, 24);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lbl, text);
    set_label_font_color(lbl, &ui_font_sc_13, 0x6A7E95);
    lv_obj_center(lbl);

    return btn;
}

static lv_obj_t *create_settings_row(
    lv_obj_t *parent,
    lv_coord_t y,
    uint32_t dot_color,
    const char *left_text,
    const char *right_text,
    uint32_t right_color,
    lv_obj_t **right_label_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_t *left = lv_label_create(row);
    lv_obj_t *right = lv_label_create(row);

    lv_obj_set_pos(row, 0, y);
    lv_obj_set_size(row, 296, 42);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_border_color(row, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(dot, 8, 8);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_bg_color(dot, c_hex(dot_color), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(left, left_text);
    set_label_font_color(left, &ui_font_sc_13, 0x1C2A3A);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 32, 0);

    lv_label_set_text(right, right_text);
    set_label_font_color(right, &ui_font_sc_13, right_color);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -10, 0);

    if (right_label_out != NULL) {
        *right_label_out = right;
    }

    return row;
}

static lv_obj_t *create_settings_switch_row(
    lv_obj_t *parent,
    lv_coord_t y,
    uint32_t dot_color,
    const char *left_text,
    lv_obj_t **switch_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_t *left = lv_label_create(row);
    lv_obj_t *sw = lv_switch_create(row);

    lv_obj_set_pos(row, 0, y);
    lv_obj_set_size(row, 296, 42);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_border_color(row, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(dot, 8, 8);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_bg_color(dot, c_hex(dot_color), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(left, left_text);
    set_label_font_color(left, &ui_font_sc_13, 0x1C2A3A);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 32, 0);

    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_size(sw, 42, 24);

    if (switch_out != NULL) {
        *switch_out = sw;
    }
    return row;
}

void page_settings_apply_snapshot(const app_backend_snapshot_t *snapshot)
{
    if ((s_screen == NULL) || (snapshot == NULL) || (s_wifi_status_label == NULL)) {
        return;
    }

    if (snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTED) {
        lv_label_set_text(s_wifi_status_label, "已连接");
        lv_obj_set_style_text_color(s_wifi_status_label, c_hex(0x2BC670), 0);
    } else if (snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTING) {
        lv_label_set_text(s_wifi_status_label, "连接中");
        lv_obj_set_style_text_color(s_wifi_status_label, c_hex(0x3D8BFF), 0);
    } else if (snapshot->wifi_state == APP_BACKEND_WIFI_SCANNING) {
        lv_label_set_text(s_wifi_status_label, "扫描中");
        lv_obj_set_style_text_color(s_wifi_status_label, c_hex(0x3D8BFF), 0);
    } else {
        lv_label_set_text(s_wifi_status_label, "点击进入扫描");
        lv_obj_set_style_text_color(s_wifi_status_label, c_hex(0x3D8BFF), 0);
    }

    if (s_ble_switch != NULL) {
        s_ble_syncing = true;
        if (snapshot->ble_enabled) {
            lv_obj_add_state(s_ble_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_ble_switch, LV_STATE_CHECKED);
        }
        s_ble_syncing = false;
    }
}

static void page_settings_nav_async(void *user_data)
{
    const settings_nav_action_t action = (settings_nav_action_t)(uintptr_t)user_data;

    switch (action) {
        case SETTINGS_NAV_BACK:
            (void)ui_page_pop(UI_ANIM_MOVE_RIGHT);
            break;
        case SETTINGS_NAV_WIFI:
            (void)ui_page_push(UI_PAGE_WIFI, NULL, UI_ANIM_MOVE_LEFT);
            break;
        case SETTINGS_NAV_STATUS:
            (void)ui_page_push(UI_PAGE_STATUS, NULL, UI_ANIM_MOVE_LEFT);
            break;
        case SETTINGS_NAV_GYRO:
            (void)ui_page_push(UI_PAGE_GYRO, NULL, UI_ANIM_MOVE_LEFT);
            break;
        default:
            break;
    }

    s_nav_pending = false;
}

static void page_settings_schedule_nav(settings_nav_action_t action)
{
    if (s_nav_pending) {
        return;
    }

    s_nav_pending = true;
    if (lv_async_call(page_settings_nav_async, (void *)(uintptr_t)action) != LV_RES_OK) {
        s_nav_pending = false;
    }
}

static void page_settings_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    page_settings_schedule_nav(SETTINGS_NAV_BACK);
}

static void page_settings_open_wifi_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    page_settings_schedule_nav(SETTINGS_NAV_WIFI);
}

static void page_settings_open_status_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    page_settings_schedule_nav(SETTINGS_NAV_STATUS);
}

static void page_settings_open_gyro_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    page_settings_schedule_nav(SETTINGS_NAV_GYRO);
}

static void page_settings_ble_switch_cb(lv_event_t *e)
{
    bool enabled = false;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (s_ble_syncing || (s_ble_switch == NULL)) {
        return;
    }

    enabled = lv_obj_has_state(s_ble_switch, LV_STATE_CHECKED);
    if (enabled == app_backend_ble_is_enabled()) {
        return;
    }
    (void)app_backend_ble_set_enabled_async(enabled);
}

static lv_obj_t *page_settings_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *scroll_view = NULL;
    lv_obj_t *scroll_content = NULL;
    lv_obj_t *row = NULL;
    lv_obj_t *footer = NULL;
    lv_obj_t *btn = NULL;
    lv_obj_t *tag = NULL;
    lv_obj_t *tag_text = NULL;
    lv_obj_t *scroll_track = NULL;
    lv_obj_t *scroll_thumb = NULL;
    lv_obj_t *lbl = NULL;

    s_nav_pending = false;
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_radius(s_screen, 14, 0);
    lv_obj_set_style_bg_color(s_screen, c_hex(0xE7F4FF), 0);
    lv_obj_set_style_bg_grad_color(s_screen, c_hex(0xFFEBD3), 0);
    lv_obj_set_style_bg_grad_dir(s_screen, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_main_stop(s_screen, 0, 0);
    lv_obj_set_style_bg_grad_stop(s_screen, 100, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 1, 0);
    lv_obj_set_style_border_color(s_screen, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    header = lv_obj_create(s_screen);
    lv_obj_set_pos(header, 12, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_header_back_text_btn(header, "← 返回", 64);
    lv_obj_add_event_cb(btn, page_settings_back_cb, LV_EVENT_CLICKED, NULL);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, "设置");
    set_label_font_color(lbl, &ui_font_sc_18, 0x1C2A3A);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    tag = lv_obj_create(header);
    lv_obj_set_pos(tag, 244, 6);
    lv_obj_set_size(tag, LV_SIZE_CONTENT, 22);
    lv_obj_set_style_radius(tag, 999, 0);
    lv_obj_set_style_bg_color(tag, c_hex(0xE8F2FF), 0);
    lv_obj_set_style_border_width(tag, 0, 0);
    lv_obj_set_style_pad_left(tag, 8, 0);
    lv_obj_set_style_pad_right(tag, 8, 0);
    lv_obj_set_style_pad_top(tag, 4, 0);
    lv_obj_set_style_pad_bottom(tag, 4, 0);
    lv_obj_clear_flag(tag, LV_OBJ_FLAG_SCROLLABLE);

    tag_text = lv_label_create(tag);
    lv_label_set_text(tag_text, "可滚动");
    set_label_font_color(tag_text, &ui_font_sc_12, 0x3F6FA6);
    lv_obj_center(tag_text);

    scroll_view = lv_obj_create(s_screen);
    lv_obj_set_pos(scroll_view, 12, 50);
    lv_obj_set_size(scroll_view, 296, 146);
    lv_obj_set_style_bg_opa(scroll_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_view, 0, 0);
    lv_obj_set_style_pad_all(scroll_view, 0, 0);
    lv_obj_set_scroll_dir(scroll_view, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_view, LV_SCROLLBAR_MODE_OFF);

    scroll_content = lv_obj_create(scroll_view);
    lv_obj_set_pos(scroll_content, 0, 0);
    lv_obj_set_size(scroll_content, 296, 402);
    lv_obj_set_style_bg_opa(scroll_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_content, 0, 0);
    lv_obj_set_style_pad_all(scroll_content, 0, 0);
    lv_obj_clear_flag(scroll_content, LV_OBJ_FLAG_SCROLLABLE);

    row = create_settings_row(scroll_content, 0, 0x3D8BFF, "WiFi连接", "点击进入扫描", 0x3D8BFF, &s_wifi_status_label);
    lv_obj_add_event_cb(row, page_settings_open_wifi_cb, LV_EVENT_CLICKED, NULL);

    row = create_settings_row(scroll_content, 50, 0xFF9F43, "使用地点", s_location, 0x1C2A3A, &s_location_label);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    row = create_settings_row(scroll_content, 100, 0x2BC670, "设备状态", "在线 / 正常", 0x2BC670, NULL);
    lv_obj_add_event_cb(row, page_settings_open_status_cb, LV_EVENT_CLICKED, NULL);

    row = create_settings_row(scroll_content, 150, 0x3D8BFF, "Gyro Verify", "Enter", 0x3D8BFF, NULL);
    lv_obj_add_event_cb(row, page_settings_open_gyro_cb, LV_EVENT_CLICKED, NULL);

    row = create_settings_switch_row(scroll_content, 200, 0x7E57C2, "蓝牙开关", &s_ble_switch);
    if (s_ble_switch != NULL) {
        lv_obj_add_event_cb(s_ble_switch, page_settings_ble_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_clear_state(s_ble_switch, LV_STATE_CHECKED);
    }

    row = create_settings_row(scroll_content, 250, 0x3D8BFF, "天气刷新频率", "每15分钟", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 300, 0x3D8BFF, "语言", "简体中文", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 350, 0x3D8BFF, "时区", "Asia/Shanghai", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    scroll_track = lv_obj_create(s_screen);
    lv_obj_set_pos(scroll_track, 305, 62);
    lv_obj_set_size(scroll_track, 3, 132);
    lv_obj_set_style_radius(scroll_track, 2, 0);
    lv_obj_set_style_bg_color(scroll_track, c_hex(0xD9EAFB), 0);
    lv_obj_set_style_bg_opa(scroll_track, LV_OPA_30, 0);
    lv_obj_set_style_border_width(scroll_track, 0, 0);
    lv_obj_clear_flag(scroll_track, LV_OBJ_FLAG_SCROLLABLE);

    scroll_thumb = lv_obj_create(scroll_track);
    lv_obj_set_pos(scroll_thumb, 0, 8);
    lv_obj_set_size(scroll_thumb, 3, 34);
    lv_obj_set_style_radius(scroll_thumb, 2, 0);
    lv_obj_set_style_bg_color(scroll_thumb, c_hex(0x89B9F0), 0);
    lv_obj_set_style_bg_opa(scroll_thumb, LV_OPA_80, 0);
    lv_obj_set_style_border_width(scroll_thumb, 0, 0);
    lv_obj_clear_flag(scroll_thumb, LV_OBJ_FLAG_SCROLLABLE);

    footer = lv_obj_create(s_screen);
    lv_obj_set_pos(footer, 12, 198);
    lv_obj_set_size(footer, 296, 38);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(footer, 0, 0, 198, 38, 0x3D8BFF, "保存并返回", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_settings_back_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(footer, 206, 0, 90, 38, 0x15B7D9, "WiFi", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_settings_open_wifi_cb, LV_EVENT_CLICKED, NULL);

    if (s_location_label != NULL) {
        lv_label_set_text(s_location_label, s_location);
    }

    return s_screen;
}

static void page_settings_on_destroy(void)
{
    s_screen = NULL;
    s_location_label = NULL;
    s_wifi_status_label = NULL;
    s_ble_switch = NULL;
    s_nav_pending = false;
    s_ble_syncing = false;
}

static void page_settings_on_show(void *args)
{
    app_backend_snapshot_t snapshot = {0};
    (void)args;

    if (app_backend_get_snapshot(&snapshot) == ESP_OK) {
        page_settings_apply_snapshot(&snapshot);
    }
}

const ui_page_t g_page_settings = {
    .id = UI_PAGE_SETTINGS,
    .name = "SETTINGS",
    .cache_mode = UI_PAGE_CACHE_KEEP,
    .create = page_settings_create,
    .on_show = page_settings_on_show,
    .on_hide = NULL,
    .on_destroy = page_settings_on_destroy,
};
