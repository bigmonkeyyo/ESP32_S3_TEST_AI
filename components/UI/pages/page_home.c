#include "page_home.h"

#include <stdio.h>

#include "app_backend.h"
#include "ui_fonts.h"
#include "ui_page_manager.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_city_weather = NULL;
static lv_obj_t *s_temp_sub = NULL;
static lv_obj_t *s_weather_main = NULL;
static lv_obj_t *s_humidity_label = NULL;
static lv_obj_t *s_wind_label = NULL;
static lv_obj_t *s_aqi_label = NULL;

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

static lv_obj_t *create_pill(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint32_t bg, uint32_t fg,
                             const char *text, lv_obj_t **label_out)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_t *lbl = lv_label_create(pill);

    lv_obj_set_pos(pill, x, y);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, 22);
    lv_obj_set_style_pad_left(pill, 8, 0);
    lv_obj_set_style_pad_right(pill, 8, 0);
    lv_obj_set_style_pad_top(pill, 4, 0);
    lv_obj_set_style_pad_bottom(pill, 4, 0);
    lv_obj_set_style_radius(pill, 999, 0);
    lv_obj_set_style_bg_color(pill, c_hex(bg), 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lbl, text);
    set_label_font_color(lbl, &ui_font_sc_11, fg);
    lv_obj_center(lbl);
    if (label_out != NULL) {
        *label_out = lbl;
    }

    return pill;
}

void page_home_apply_snapshot(const app_backend_snapshot_t *snapshot)
{
    char line[64] = {0};

    if ((s_screen == NULL) || (snapshot == NULL)) {
        return;
    }

    if (s_time_label != NULL) {
        lv_label_set_text(s_time_label, snapshot->now_time[0] != '\0' ? snapshot->now_time : "--:--");
    }

    if (s_city_weather != NULL) {
        snprintf(line, sizeof(line), "%s · %s",
                 snapshot->weather.city[0] != '\0' ? snapshot->weather.city : "上海",
                 snapshot->weather.condition[0] != '\0' ? snapshot->weather.condition : "--");
        lv_label_set_text(s_city_weather, line);
    }

    if (s_temp_sub != NULL) {
        if ((snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTED) && snapshot->weather.valid) {
            lv_label_set_text(s_temp_sub, "联网天气已同步");
        } else if ((snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTED) ||
                   (snapshot->wifi_state == APP_BACKEND_WIFI_CONNECTING) ||
                   (snapshot->wifi_state == APP_BACKEND_WIFI_SCANNING)) {
            lv_label_set_text(s_temp_sub, snapshot->message[0] != '\0' ? snapshot->message : "等待联网同步");
        } else if (snapshot->wifi_state == APP_BACKEND_WIFI_FAILED) {
            lv_label_set_text(s_temp_sub, snapshot->message[0] != '\0' ? snapshot->message : "WiFi 已断开");
        } else {
            lv_label_set_text(s_temp_sub, "WiFi 已断开");
        }
    }

    if (s_weather_main != NULL) {
        snprintf(line, sizeof(line), "%s  %s",
                 snapshot->weather.condition[0] != '\0' ? snapshot->weather.condition : "--",
                 snapshot->weather.temperature[0] != '\0' ? snapshot->weather.temperature : "--");
        lv_label_set_text(s_weather_main, line);
    }

    if (s_humidity_label != NULL) {
        lv_label_set_text(s_humidity_label,
                          snapshot->weather.humidity[0] != '\0' ? snapshot->weather.humidity : "湿度 --");
    }
    if (s_wind_label != NULL) {
        lv_label_set_text(s_wind_label,
                          snapshot->weather.wind[0] != '\0' ? snapshot->weather.wind : "风 --");
    }
    if (s_aqi_label != NULL) {
        lv_label_set_text(s_aqi_label,
                          snapshot->weather.aqi[0] != '\0' ? snapshot->weather.aqi : "AQI --");
    }
}

static void page_home_open_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    (void)ui_page_push(UI_PAGE_SETTINGS, NULL, UI_ANIM_MOVE_LEFT);
}

static void page_home_open_status_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    (void)ui_page_push(UI_PAGE_STATUS, NULL, UI_ANIM_MOVE_LEFT);
}

static lv_obj_t *page_home_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *weather = NULL;
    lv_obj_t *btn_area = NULL;
    lv_obj_t *lbl = NULL;
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
    lv_obj_set_pos(header, 12, 4);
    lv_obj_set_size(header, 296, 56);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_time_label = lv_label_create(header);
    lv_label_set_text(s_time_label, "--:--");
    set_label_font_color(s_time_label, &ui_font_sc_30, 0x18314F);
    lv_obj_set_pos(s_time_label, 0, 0);

    s_city_weather = lv_label_create(header);
    lv_obj_set_width(s_city_weather, 180);
    lv_obj_set_style_text_align(s_city_weather, LV_TEXT_ALIGN_RIGHT, 0);
    set_label_font_color(s_city_weather, &ui_font_sc_13, 0x214A7A);
    lv_obj_align(s_city_weather, LV_ALIGN_TOP_RIGHT, -6, 4);

    s_temp_sub = lv_label_create(header);
    lv_obj_set_width(s_temp_sub, 180);
    lv_obj_set_style_text_align(s_temp_sub, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(s_temp_sub, LV_LABEL_LONG_CLIP);
    set_label_font_color(s_temp_sub, &ui_font_sc_11, 0x5F738C);
    lv_obj_align(s_temp_sub, LV_ALIGN_TOP_RIGHT, -6, 27);

    weather = lv_obj_create(s_screen);
    lv_obj_set_pos(weather, 12, 62);
    lv_obj_set_size(weather, 296, 106);
    lv_obj_set_style_radius(weather, 12, 0);
    lv_obj_set_style_bg_color(weather, c_hex(0xDFF3FF), 0);
    lv_obj_set_style_bg_grad_color(weather, c_hex(0xFFE9C7), 0);
    lv_obj_set_style_bg_grad_dir(weather, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(weather, 1, 0);
    lv_obj_set_style_border_color(weather, c_hex(0xCFE3F7), 0);
    lv_obj_set_style_pad_all(weather, 0, 0);
    lv_obj_clear_flag(weather, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(weather);
    lv_label_set_text(lbl, "当前天气");
    set_label_font_color(lbl, &ui_font_sc_12, 0x476C95);
    lv_obj_set_pos(lbl, 12, 12);

    s_weather_main = lv_label_create(weather);
    lv_label_set_text(s_weather_main, "--  --");
    set_label_font_color(s_weather_main, &ui_font_sc_24, 0x1A3D64);
    lv_obj_set_pos(s_weather_main, 12, 34);

    create_pill(weather, 12, 77, 0x66C7FF, 0x083B63, "湿度 --", &s_humidity_label);
    create_pill(weather, 86, 77, 0xFFD27A, 0x6A3C00, "风 --", &s_wind_label);
    create_pill(weather, 174, 77, 0x8FE3A9, 0x114D24, "AQI --", &s_aqi_label);

    btn_area = lv_obj_create(s_screen);
    lv_obj_set_pos(btn_area, 12, 172);
    lv_obj_set_size(btn_area, 296, 40);
    lv_obj_set_style_bg_opa(btn_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_area, 0, 0);
    lv_obj_set_style_pad_all(btn_area, 0, 0);
    lv_obj_clear_flag(btn_area, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(btn_area, 0, 0, 144, 40, 0x3D8BFF, "进入设置", &ui_font_sc_13);
    lv_obj_add_event_cb(btn, page_home_open_settings_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(btn_area, 152, 0, 144, 40, 0x7C83FF, "设备状态", &ui_font_sc_13);
    lv_obj_add_event_cb(btn, page_home_open_status_cb, LV_EVENT_CLICKED, NULL);

    lv_label_set_text(s_city_weather, "上海 · --");
    lv_label_set_text(s_temp_sub, "等待联网同步");

    return s_screen;
}

static void page_home_on_destroy(void)
{
    s_screen = NULL;
    s_time_label = NULL;
    s_city_weather = NULL;
    s_temp_sub = NULL;
    s_weather_main = NULL;
    s_humidity_label = NULL;
    s_wind_label = NULL;
    s_aqi_label = NULL;
}

static void page_home_on_show(void *args)
{
    app_backend_snapshot_t snapshot = {0};
    (void)args;

    if (app_backend_get_snapshot(&snapshot) == ESP_OK) {
        page_home_apply_snapshot(&snapshot);
    }
}

const ui_page_t g_page_home = {
    .id = UI_PAGE_HOME,
    .name = "HOME",
    .cache_mode = UI_PAGE_CACHE_KEEP,
    .create = page_home_create,
    .on_show = page_home_on_show,
    .on_hide = NULL,
    .on_destroy = page_home_on_destroy,
};
