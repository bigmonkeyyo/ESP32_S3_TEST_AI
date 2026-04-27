#include "page_status.h"

#include <stdbool.h>
#include <stdio.h>

#include "app_backend.h"
#include "esp_log.h"
#include "ui_fonts.h"
#include "ui_page_manager.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_wifi_label = NULL;
static lv_obj_t *s_ip_label = NULL;
static lv_obj_t *s_runtime_label = NULL;
static lv_obj_t *s_sync_label = NULL;
static lv_obj_t *s_online_label = NULL;
static bool s_back_home_pending = false;

static const char *TAG = "PAGE_STATUS";

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

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, c_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, c_hex(0xCFE2F8), 0);

    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    return btn;
}

static lv_obj_t *create_status_row(
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

void page_status_apply_snapshot(const app_backend_snapshot_t *snapshot)
{
    char line[32] = {0};

    if ((s_screen == NULL) || (snapshot == NULL)) {
        return;
    }

    if (s_online_label != NULL) {
        lv_label_set_text(s_online_label,
                          snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTED ? "在线" : "离线");
    }
    if (s_wifi_label != NULL) {
        if (snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTED) {
            snprintf(line, sizeof(line), "%d dBm", (int)snapshot->rssi);
        } else {
            snprintf(line, sizeof(line), "--");
        }
        lv_label_set_text(s_wifi_label, line);
    }
    if (s_ip_label != NULL) {
        lv_label_set_text(s_ip_label, snapshot->ip[0] != '\0' ? snapshot->ip : "--");
    }
    if (s_runtime_label != NULL) {
        lv_label_set_text(s_runtime_label, snapshot->uptime[0] != '\0' ? snapshot->uptime : "00:00:00");
    }
    if (s_sync_label != NULL) {
        lv_label_set_text(s_sync_label, snapshot->last_sync[0] != '\0' ? snapshot->last_sync : "--:--");
    }
}

static void page_status_back_home_async(void *user_data)
{
    (void)user_data;

    if (ui_page_back_to_root(UI_ANIM_MOVE_RIGHT) != ESP_OK) {
        ESP_LOGW(TAG, "ui_page_back_to_root failed");
    }

    s_back_home_pending = false;
}

static void page_status_home_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_back_home_pending) {
        return;
    }

    s_back_home_pending = true;
    if (lv_async_call(page_status_back_home_async, NULL) != LV_RES_OK) {
        s_back_home_pending = false;
        ESP_LOGW(TAG, "lv_async_call failed");
    }
}

static void page_status_refresh_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (app_backend_weather_refresh_async() != ESP_OK) {
        ESP_LOGW(TAG, "weather refresh enqueue failed");
    }

}

static lv_obj_t *page_status_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *tag = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *scroll_view = NULL;
    lv_obj_t *scroll_content = NULL;
    lv_obj_t *scroll_track = NULL;
    lv_obj_t *scroll_thumb = NULL;
    lv_obj_t *footer = NULL;
    lv_obj_t *btn = NULL;

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

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, "设备状态");
    set_label_font_color(lbl, &ui_font_sc_18, 0x1C2A3A);
    lv_obj_set_pos(lbl, 0, 4);

    tag = lv_obj_create(header);
    lv_obj_set_pos(tag, 252, 6);
    lv_obj_set_size(tag, LV_SIZE_CONTENT, 22);
    lv_obj_set_style_radius(tag, 999, 0);
    lv_obj_set_style_bg_color(tag, c_hex(0xDDF8E8), 0);
    lv_obj_set_style_border_width(tag, 0, 0);
    lv_obj_set_style_pad_left(tag, 8, 0);
    lv_obj_set_style_pad_right(tag, 8, 0);
    lv_obj_set_style_pad_top(tag, 4, 0);
    lv_obj_set_style_pad_bottom(tag, 4, 0);
    lv_obj_clear_flag(tag, LV_OBJ_FLAG_SCROLLABLE);

    s_online_label = lv_label_create(tag);
    lv_label_set_text(s_online_label, "离线");
    set_label_font_color(s_online_label, &ui_font_sc_12, 0x17743D);
    lv_obj_center(s_online_label);

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
    lv_obj_set_size(scroll_content, 296, 352);
    lv_obj_set_style_bg_opa(scroll_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_content, 0, 0);
    lv_obj_set_style_pad_all(scroll_content, 0, 0);
    lv_obj_clear_flag(scroll_content, LV_OBJ_FLAG_SCROLLABLE);

    create_status_row(scroll_content, 0, 0xFF9F43, "供电状态", "USB 5V / 0.42A", 0x1C2A3A, NULL);
    create_status_row(scroll_content, 50, 0x2BC670, "WiFi信号", "--", 0x2BC670, &s_wifi_label);
    create_status_row(scroll_content, 100, 0x3D8BFF, "IP地址", "--", 0x1C2A3A, &s_ip_label);
    create_status_row(scroll_content, 150, 0xFF9F43, "固件版本", "v1.2.7", 0x1C2A3A, NULL);
    create_status_row(scroll_content, 200, 0x2BC670, "运行时长", "00:00:00", 0x1C2A3A, &s_runtime_label);
    create_status_row(scroll_content, 250, 0x3D8BFF, "内存占用", "63%", 0x5F738C, NULL);
    create_status_row(scroll_content, 300, 0x3D8BFF, "上次同步", "--:--", 0x5F738C, &s_sync_label);

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

    btn = create_round_button(footer, 0, 0, 144, 38, 0x15B7D9, "刷新状态", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_status_refresh_cb, LV_EVENT_CLICKED, NULL);
    btn = create_round_button(footer, 152, 0, 144, 38, 0x3D8BFF, "返回首页", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_status_home_cb, LV_EVENT_CLICKED, NULL);

    return s_screen;
}

static void page_status_on_destroy(void)
{
    s_screen = NULL;
    s_wifi_label = NULL;
    s_ip_label = NULL;
    s_runtime_label = NULL;
    s_sync_label = NULL;
    s_online_label = NULL;
    s_back_home_pending = false;
}

static void page_status_on_show(void *args)
{
    app_backend_snapshot_t snapshot = {0};
    (void)args;

    if (app_backend_get_snapshot(&snapshot) == ESP_OK) {
        page_status_apply_snapshot(&snapshot);
    }
}

const ui_page_t g_page_status = {
    .id = UI_PAGE_STATUS,
    .name = "STATUS",
    .cache_mode = UI_PAGE_CACHE_NONE,
    .create = page_status_create,
    .on_show = page_status_on_show,
    .on_hide = NULL,
    .on_destroy = page_status_on_destroy,
};
