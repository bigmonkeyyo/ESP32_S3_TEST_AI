#include "ui_page_sim.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "ui_fonts.h"

typedef enum {
    SIM_PAGE_HOME = 0,
    SIM_PAGE_SETTINGS,
    SIM_PAGE_STATUS,
    SIM_PAGE_FIRMWARE,
    SIM_PAGE_OTA_READY,
    SIM_PAGE_OTA_PROGRESS,
    SIM_PAGE_OTA_COMPLETE,
    SIM_PAGE_OTA_FAILED,
    SIM_PAGE_WIFI,
} sim_page_t;

typedef struct {
    const char *ssid;
    const char *password;
    const char *strength;
    uint32_t strength_color;
    uint32_t dot_color;
} wifi_ap_t;

static const wifi_ap_t k_wifi_aps[] = {
    {"Office_2.4G", "12345678", "强", 0x2BC670, 0x2BC670},
    {"Office_5G", "office5g!", "中", 0xFF9F43, 0x3D8BFF},
    {"Guest_WiFi", "guest1234", "弱", 0xF25F5C, 0xFF9F43},
};

static lv_obj_t *s_home = NULL;
static lv_obj_t *s_settings = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_firmware = NULL;
static lv_obj_t *s_ota_ready = NULL;
static lv_obj_t *s_ota_progress = NULL;
static lv_obj_t *s_ota_complete = NULL;
static lv_obj_t *s_ota_failed = NULL;
static lv_obj_t *s_wifi = NULL;

static lv_obj_t *s_home_city = NULL;
static lv_obj_t *s_home_temp_sub = NULL;
static lv_obj_t *s_setting_location = NULL;
static lv_obj_t *s_status_runtime = NULL;
static lv_obj_t *s_status_sync = NULL;

static lv_obj_t *s_wifi_scan_list = NULL;
static lv_obj_t *s_wifi_ssid_ta = NULL;
static lv_obj_t *s_wifi_pwd_ta = NULL;
static lv_obj_t *s_wifi_msg_bar = NULL;
static lv_obj_t *s_wifi_msg_label = NULL;
static lv_obj_t *s_wifi_keyboard = NULL;

static char s_location[32] = "上海 · 浦东新区";
static const wifi_ap_t *s_selected_ap = &k_wifi_aps[0];

static lv_color_t c_hex(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void set_label_font_color(lv_obj_t *label, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c_hex(rgb), 0);
}

static lv_obj_t *create_base_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_radius(scr, 14, 0);
    lv_obj_set_style_bg_color(scr, c_hex(0xE7F4FF), 0);
    lv_obj_set_style_bg_grad_color(scr, c_hex(0xFFEBD3), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_main_stop(scr, 0, 0);
    lv_obj_set_style_bg_grad_stop(scr, 100, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 1, 0);
    lv_obj_set_style_border_color(scr, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static void open_page(sim_page_t page, lv_scr_load_anim_t anim)
{
    lv_obj_t *target = NULL;

    switch (page) {
        case SIM_PAGE_HOME:
            target = s_home;
            break;
        case SIM_PAGE_SETTINGS:
            target = s_settings;
            break;
        case SIM_PAGE_STATUS:
            target = s_status;
            break;
        case SIM_PAGE_FIRMWARE:
            target = s_firmware;
            break;
        case SIM_PAGE_OTA_READY:
            target = s_ota_ready;
            break;
        case SIM_PAGE_OTA_PROGRESS:
            target = s_ota_progress;
            break;
        case SIM_PAGE_OTA_COMPLETE:
            target = s_ota_complete;
            break;
        case SIM_PAGE_OTA_FAILED:
            target = s_ota_failed;
            break;
        case SIM_PAGE_WIFI:
            target = s_wifi;
            break;
        default:
            break;
    }

    if (target != NULL) {
        if (anim == LV_SCR_LOAD_ANIM_NONE) {
            lv_scr_load(target);
        } else {
            lv_scr_load_anim(target, anim, 220, 0, false);
        }
    }
}

void ui_page_sim_open(ui_page_sim_id_t page_id)
{
    switch (page_id) {
        case UI_PAGE_SIM_HOME:
            open_page(SIM_PAGE_HOME, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_SETTINGS:
            open_page(SIM_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_STATUS:
            open_page(SIM_PAGE_STATUS, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_FIRMWARE:
            open_page(SIM_PAGE_FIRMWARE, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_OTA_READY:
            open_page(SIM_PAGE_OTA_READY, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_OTA_PROGRESS:
            open_page(SIM_PAGE_OTA_PROGRESS, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_OTA_COMPLETE:
            open_page(SIM_PAGE_OTA_COMPLETE, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_OTA_FAILED:
            open_page(SIM_PAGE_OTA_FAILED, LV_SCR_LOAD_ANIM_NONE);
            break;
        case UI_PAGE_SIM_WIFI:
            open_page(SIM_PAGE_WIFI, LV_SCR_LOAD_ANIM_NONE);
            break;
        default:
            open_page(SIM_PAGE_HOME, LV_SCR_LOAD_ANIM_NONE);
            break;
    }
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
    lv_coord_t radius = 10;

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, radius, 0);
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

static lv_obj_t *create_pill(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint32_t bg, uint32_t fg, const char *text)
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
    return pill;
}

static void sync_dynamic_texts(void)
{
    lv_label_set_text(s_home_city, "上海 · 多云");
    lv_label_set_text(s_home_temp_sub, "26℃  体感28℃");
    lv_label_set_text(s_setting_location, s_location);
    lv_label_set_text(s_status_runtime, "03:27:51");
    lv_label_set_text(s_status_sync, "14:20:33");
}

static void home_to_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

static void home_to_status_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_STATUS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

static void settings_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void settings_to_wifi_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_WIFI, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

static void settings_to_status_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_STATUS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

static void status_to_home_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void status_refresh_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_label_set_text(s_status_runtime, "03:27:52");
        lv_label_set_text(s_status_sync, "14:28:00");
    }
}

static void wifi_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void wifi_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_page(SIM_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void wifi_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_READY) || (code == LV_EVENT_CANCEL)) {
        lv_keyboard_set_textarea(s_wifi_keyboard, NULL);
        lv_obj_add_flag(s_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_ta_focus_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if ((code == LV_EVENT_CLICKED) || (code == LV_EVENT_FOCUSED)) {
        lv_keyboard_set_textarea(s_wifi_keyboard, ta);
        lv_obj_clear_flag(s_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_show_msg(const char *msg, uint32_t text_color, uint32_t bg_color, uint32_t border_color)
{
    lv_label_set_text(s_wifi_msg_label, msg);
    lv_obj_set_style_text_color(s_wifi_msg_label, c_hex(text_color), 0);
    lv_obj_set_style_bg_color(s_wifi_msg_bar, c_hex(bg_color), 0);
    lv_obj_set_style_border_color(s_wifi_msg_bar, c_hex(border_color), 0);
    lv_obj_clear_flag(s_wifi_msg_bar, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_wifi_ap_row(lv_obj_t *parent, const wifi_ap_t *ap)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_t *name = lv_label_create(row);
    lv_obj_t *lvl = lv_label_create(row);

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
    lv_obj_set_style_bg_color(dot, c_hex(ap->dot_color), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(name, ap->ssid);
    set_label_font_color(name, &ui_font_sc_13, 0x1C2A3A);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 32, 0);

    lv_label_set_text(lvl, ap->strength);
    set_label_font_color(lvl, &ui_font_sc_13, ap->strength_color);
    lv_obj_align(lvl, LV_ALIGN_RIGHT_MID, -10, 0);

    return row;
}

static void wifi_select_ap_cb(lv_event_t *e)
{
    const wifi_ap_t *ap = (const wifi_ap_t *)lv_event_get_user_data(e);

    if ((lv_event_get_code(e) != LV_EVENT_CLICKED) || (ap == NULL)) {
        return;
    }

    s_selected_ap = ap;
    lv_textarea_set_text(s_wifi_ssid_ta, ap->ssid);
    lv_textarea_set_text(s_wifi_pwd_ta, "");
    lv_obj_add_flag(s_wifi_msg_bar, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_fill_scan_list(const wifi_ap_t *aps, size_t ap_cnt)
{
    size_t i;

    lv_obj_clean(s_wifi_scan_list);
    for (i = 0; i < ap_cnt; i++) {
        lv_obj_t *row = create_wifi_ap_row(s_wifi_scan_list, &aps[i]);
        lv_obj_add_event_cb(row, wifi_select_ap_cb, LV_EVENT_CLICKED, (void *)&aps[i]);
    }
}

static void wifi_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    wifi_fill_scan_list(k_wifi_aps, sizeof(k_wifi_aps) / sizeof(k_wifi_aps[0]));
    lv_obj_add_flag(s_wifi_msg_bar, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_connect_cb(lv_event_t *e)
{
    const char *pwd;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    pwd = lv_textarea_get_text(s_wifi_pwd_ta);
    if ((pwd == NULL) || (pwd[0] == '\0')) {
        wifi_show_msg("连接失败：请输入密码", 0xF25F5C, 0xFFE5E5, 0xF5B6B6);
        return;
    }

    if ((s_selected_ap != NULL) && (strcmp(pwd, s_selected_ap->password) == 0)) {
        wifi_show_msg("连接成功：WiFi 已连接", 0x17743D, 0xDDF8E8, 0xBEEBCF);
    } else {
        wifi_show_msg("连接失败：密码错误，请重新输入", 0xF25F5C, 0xFFE5E5, 0xF5B6B6);
    }
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

static lv_obj_t *create_firmware_row(
    lv_obj_t *parent,
    lv_coord_t y,
    uint32_t dot_color,
    const char *left_text,
    const char *right_text,
    uint32_t right_color)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_t *left = lv_label_create(row);
    lv_obj_t *right = lv_label_create(row);

    lv_obj_set_pos(row, 0, y);
    lv_obj_set_size(row, 296, 42);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_border_color(row, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(dot, 8, 8);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 9, 0);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_bg_color(dot, c_hex(dot_color), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(left, left_text);
    set_label_font_color(left, &ui_font_sc_13, 0x1C2A3A);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 25, 0);

    lv_label_set_text(right, right_text);
    lv_obj_set_width(right, 200);
    lv_obj_set_style_text_align(right, LV_TEXT_ALIGN_RIGHT, 0);
    set_label_font_color(right, &ui_font_sc_12, right_color);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -10, 0);

    return row;
}

static void create_sim_header(lv_obj_t *screen, const char *back_text, const char *title, bool ota_tag)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_t *lbl = NULL;
    lv_obj_t *tag = NULL;

    lv_obj_set_pos(header, 11, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, back_text);
    set_label_font_color(lbl, &ui_font_sc_13, 0x5F738C);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, title);
    set_label_font_color(lbl, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    if (ota_tag) {
        tag = lv_obj_create(header);
        lv_obj_set_pos(tag, 248, 6);
        lv_obj_set_size(tag, LV_SIZE_CONTENT, 22);
        lv_obj_set_style_radius(tag, 999, 0);
        lv_obj_set_style_bg_color(tag, c_hex(0xE0F0FF), 0);
        lv_obj_set_style_border_width(tag, 0, 0);
        lv_obj_set_style_pad_left(tag, 8, 0);
        lv_obj_set_style_pad_right(tag, 8, 0);
        lv_obj_set_style_pad_top(tag, 4, 0);
        lv_obj_set_style_pad_bottom(tag, 4, 0);
        lv_obj_clear_flag(tag, LV_OBJ_FLAG_SCROLLABLE);

        lbl = lv_label_create(tag);
        lv_label_set_text(lbl, "OTA");
        set_label_font_color(lbl, &ui_font_sc_12, 0x3D8BFF);
        lv_obj_center(lbl);
    }
}

static void create_progress_bar(lv_obj_t *parent,
                                lv_coord_t x,
                                lv_coord_t y,
                                lv_coord_t w,
                                lv_coord_t h,
                                int progress,
                                uint32_t color)
{
    lv_coord_t fill_w = (lv_coord_t)((w * progress) / 100);
    lv_obj_t *track = lv_obj_create(parent);
    lv_obj_t *fill = lv_obj_create(track);

    if (progress > 0 && fill_w < 4) {
        fill_w = 4;
    } else if (progress <= 0) {
        fill_w = 0;
    }
    if (fill_w > w) {
        fill_w = w;
    }

    lv_obj_set_pos(track, x, y);
    lv_obj_set_size(track, w, h);
    lv_obj_set_style_radius(track, h / 2, 0);
    lv_obj_set_style_bg_color(track, c_hex(0xE8F2FF), 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, fill_w, h);
    lv_obj_set_style_radius(fill, h / 2, 0);
    lv_obj_set_style_bg_color(fill, c_hex(color), 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_home(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *t = NULL;
    lv_obj_t *weather = NULL;
    lv_obj_t *btn_area = NULL;

    s_home = create_base_screen();

    header = lv_obj_create(s_home);
    lv_obj_set_pos(header, 12, 4);
    lv_obj_set_size(header, 296, 56);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    t = lv_label_create(header);
    lv_label_set_text(t, "14:28");
    set_label_font_color(t, &ui_font_sc_30, 0x18314F);
    lv_obj_set_pos(t, 0, 0);

    s_home_city = lv_label_create(header);
    lv_obj_set_width(s_home_city, 180);
    lv_obj_set_style_text_align(s_home_city, LV_TEXT_ALIGN_RIGHT, 0);
    set_label_font_color(s_home_city, &ui_font_sc_13, 0x214A7A);
    lv_obj_align(s_home_city, LV_ALIGN_TOP_RIGHT, -6, 4);

    s_home_temp_sub = lv_label_create(header);
    lv_obj_set_width(s_home_temp_sub, 180);
    lv_obj_set_style_text_align(s_home_temp_sub, LV_TEXT_ALIGN_RIGHT, 0);
    set_label_font_color(s_home_temp_sub, &ui_font_sc_11, 0x5F738C);
    lv_obj_align(s_home_temp_sub, LV_ALIGN_TOP_RIGHT, -6, 27);

    weather = lv_obj_create(s_home);
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

    t = lv_label_create(weather);
    lv_label_set_text(t, "当前天气");
    set_label_font_color(t, &ui_font_sc_12, 0x476C95);
    lv_obj_set_pos(t, 12, 12);

    t = lv_label_create(weather);
    lv_label_set_text(t, "多云转晴  26°C");
    set_label_font_color(t, &ui_font_sc_24, 0x1A3D64);
    lv_obj_set_pos(t, 12, 34);

    create_pill(weather, 12, 77, 0x66C7FF, 0x083B63, "湿度 68%");
    create_pill(weather, 86, 77, 0xFFD27A, 0x6A3C00, "东南风 2级");
    create_pill(weather, 174, 77, 0x8FE3A9, 0x114D24, "AQI 良");

    btn_area = lv_obj_create(s_home);
    lv_obj_set_pos(btn_area, 12, 172);
    lv_obj_set_size(btn_area, 296, 40);
    lv_obj_set_style_bg_opa(btn_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_area, 0, 0);
    lv_obj_set_style_pad_all(btn_area, 0, 0);
    lv_obj_clear_flag(btn_area, LV_OBJ_FLAG_SCROLLABLE);

    t = create_round_button(btn_area, 0, 0, 144, 40, 0x3D8BFF, "进入设置", &ui_font_sc_13);
    lv_obj_add_event_cb(t, home_to_settings_cb, LV_EVENT_CLICKED, NULL);
    t = create_round_button(btn_area, 152, 0, 144, 40, 0x7C83FF, "设备状态", &ui_font_sc_13);
    lv_obj_add_event_cb(t, home_to_status_cb, LV_EVENT_CLICKED, NULL);

    /* Bottom hint removed by request to maximize visible UI space. */
}

static void create_firmware(void)
{
    lv_obj_t *scroll_view = NULL;
    lv_obj_t *scroll_content = NULL;
    lv_obj_t *scroll_track = NULL;
    lv_obj_t *scroll_thumb = NULL;

    s_firmware = create_base_screen();
    create_sim_header(s_firmware, "← 状态", "固件版本", true);

    scroll_view = lv_obj_create(s_firmware);
    lv_obj_set_pos(scroll_view, 11, 53);
    lv_obj_set_size(scroll_view, 296, 144);
    lv_obj_set_style_bg_opa(scroll_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_view, 0, 0);
    lv_obj_set_style_pad_all(scroll_view, 0, 0);
    lv_obj_set_scroll_dir(scroll_view, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_view, LV_SCROLLBAR_MODE_OFF);

    scroll_content = lv_obj_create(scroll_view);
    lv_obj_set_pos(scroll_content, 0, 0);
    lv_obj_set_size(scroll_content, 296, 252);
    lv_obj_set_style_bg_opa(scroll_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_content, 0, 0);
    lv_obj_set_style_pad_all(scroll_content, 0, 0);
    lv_obj_clear_flag(scroll_content, LV_OBJ_FLAG_SCROLLABLE);

    create_firmware_row(scroll_content, 0, 0x3D8BFF, "当前版本", "v1.2.9", 0x1C2A3A);
    create_firmware_row(scroll_content, 50, 0xFF9F43, "最新版本", "待检测", 0x5F738C);
    create_firmware_row(scroll_content, 100, 0x2BC670, "升级状态", "当前正常", 0x2BC670);
    create_firmware_row(scroll_content, 150, 0x3D8BFF, "上次检测", "--:--", 0x5F738C);
    create_firmware_row(scroll_content, 200, 0xFF9F43, "更新方式", "手动检查", 0x5F738C);

    scroll_track = lv_obj_create(s_firmware);
    lv_obj_set_pos(scroll_track, 304, 65);
    lv_obj_set_size(scroll_track, 3, 124);
    lv_obj_set_style_radius(scroll_track, 2, 0);
    lv_obj_set_style_bg_color(scroll_track, c_hex(0xD9EAFB), 0);
    lv_obj_set_style_bg_opa(scroll_track, LV_OPA_50, 0);
    lv_obj_set_style_border_width(scroll_track, 0, 0);
    lv_obj_clear_flag(scroll_track, LV_OBJ_FLAG_SCROLLABLE);

    scroll_thumb = lv_obj_create(scroll_track);
    lv_obj_set_pos(scroll_thumb, 0, 10);
    lv_obj_set_size(scroll_thumb, 3, 36);
    lv_obj_set_style_radius(scroll_thumb, 2, 0);
    lv_obj_set_style_bg_color(scroll_thumb, c_hex(0x89B9F0), 0);
    lv_obj_set_style_bg_opa(scroll_thumb, LV_OPA_80, 0);
    lv_obj_set_style_border_width(scroll_thumb, 0, 0);
    lv_obj_clear_flag(scroll_thumb, LV_OBJ_FLAG_SCROLLABLE);

    create_round_button(s_firmware, 11, 205, 296, 34, 0x3D8BFF, "检查更新", &ui_font_sc_14_bold);
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

static lv_obj_t *create_ota_popup_base(lv_obj_t **box_out, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *screen = create_base_screen();
    lv_obj_t *overlay = NULL;
    lv_obj_t *box = NULL;

    create_sim_header(screen, "← 固件", "固件版本", true);

    overlay = lv_obj_create(screen);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, 320, 240);
    lv_obj_set_style_bg_color(overlay, c_hex(0x0E1825), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    box = lv_obj_create(overlay);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_border_color(box, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_shadow_width(box, 18, 0);
    lv_obj_set_style_shadow_color(box, c_hex(0x1A2E47), 0);
    lv_obj_set_style_shadow_opa(box, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(box, 8, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    if (box_out != NULL) {
        *box_out = box;
    }
    return screen;
}

static void label_center(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, w);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    set_label_font_color(lbl, font, color);
    lv_obj_set_pos(lbl, 0, y);
}

static void create_version_compare(lv_obj_t *parent,
                                   lv_coord_t x,
                                   lv_coord_t y,
                                   const char *current,
                                   const char *target)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *pill = NULL;
    lv_obj_t *lbl = NULL;

    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, 216, 28);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_color(row, c_hex(0xEBF6FF), 0);
    lv_obj_set_style_border_color(row, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, current);
    lv_obj_set_width(lbl, 74);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    set_label_font_color(lbl, &ui_font_sc_12_heavy, 0x1C2A3A);
    lv_obj_set_pos(lbl, 16, 6);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, "→");
    lv_obj_set_width(lbl, 18);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    set_label_font_color(lbl, &ui_font_sc_12_heavy, 0x5F738C);
    lv_obj_set_pos(lbl, 92, 6);

    pill = lv_obj_create(row);
    lv_obj_set_pos(pill, 120, 5);
    lv_obj_set_size(pill, 82, 18);
    lv_obj_set_style_radius(pill, 9, 0);
    lv_obj_set_style_bg_color(pill, c_hex(0x3D8BFF), 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(pill);
    lv_label_set_text(lbl, target);
    lv_obj_set_width(lbl, 82);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    set_label_font_color(lbl, &ui_font_sc_12_heavy, 0xFFFFFF);
    lv_obj_center(lbl);
}

static void create_ota_ready(void)
{
    lv_obj_t *box = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn = NULL;

    s_ota_ready = create_ota_popup_base(&box, 33, 41, 252, 158);
    label_center(box, "发现新版本", &ui_font_sc_18_heavy, 0x1C2A3A, 14, 252);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "检测到固件 v1.3.0");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(lbl, 27, 47);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "建议保持供电和 WiFi 连接");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 27, 69);

    create_version_compare(box, 18, 91, "v1.2.9", "v1.3.0");

    create_round_button(box, 19, 122, 96, 30, 0x7C83FF, "稍后", &ui_font_sc_14_bold);
    create_round_button(box, 135, 122, 96, 30, 0x3D8BFF, "更新", &ui_font_sc_14_bold);
}

static void create_ota_progress(void)
{
    lv_obj_t *box = NULL;
    lv_obj_t *lbl = NULL;

    s_ota_progress = create_ota_popup_base(&box, 33, 49, 252, 138);
    label_center(box, "OTA升级中", &ui_font_sc_18_heavy, 0x1C2A3A, 14, 252);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "正在下载升级包");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(lbl, 27, 47);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "请勿断电或重启设备");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 27, 69);

    create_progress_bar(box, 27, 98, 174, 8, 68, 0x3D8BFF);
    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "68%");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x3F6FA6);
    lv_obj_set_pos(lbl, 213, 93);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "1.15 MB / 1.69 MB");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 27, 115);
}

static void create_ota_complete(void)
{
    lv_obj_t *box = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn = NULL;

    s_ota_complete = create_ota_popup_base(&box, 27, 37, 264, 164);
    label_center(box, "下载完成", &ui_font_sc_18_heavy, 0x1C2A3A, 16, 264);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "升级包已准备完成");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(lbl, 29, 52);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "重启后将切换到新版本 v1.3.0");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 29, 75);

    create_progress_bar(box, 29, 104, 178, 8, 100, 0x2BC670);
    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "100%");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x2BC670);
    lv_obj_set_pos(lbl, 219, 99);

    btn = create_round_button(box, 21, 129, 104, 30, 0xD6E5F5, "稍后重启", &ui_font_sc_14_bold);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), c_hex(0x5F738C), 0);
    create_round_button(box, 137, 129, 104, 30, 0x3D8BFF, "立即重启", &ui_font_sc_14_bold);
}

static void create_ota_failed(void)
{
    lv_obj_t *box = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn = NULL;

    s_ota_failed = create_ota_popup_base(&box, 33, 41, 252, 158);
    label_center(box, "升级失败", &ui_font_sc_18_heavy, 0x1C2A3A, 14, 252);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "下载中断，请稍后重试");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(lbl, 27, 47);

    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "当前版本仍保持 v1.2.9");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 27, 69);

    create_progress_bar(box, 27, 98, 174, 8, 41, 0xF04542);
    lbl = lv_label_create(box);
    lv_label_set_text(lbl, "41%");
    set_label_font_color(lbl, &ui_font_sc_14_bold, 0xF04542);
    lv_obj_set_pos(lbl, 213, 93);

    btn = create_round_button(box, 19, 122, 96, 28, 0xD6E5F5, "稍后", &ui_font_sc_14_bold);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), c_hex(0x5F738C), 0);
    create_round_button(box, 135, 122, 96, 28, 0x3D8BFF, "重试", &ui_font_sc_14_bold);
}

static void create_settings(void)
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

    s_settings = create_base_screen();

    header = lv_obj_create(s_settings);
    lv_obj_set_pos(header, 12, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_header_back_text_btn(header, "← 返回", 64);
    lv_obj_add_event_cb(btn, settings_back_cb, LV_EVENT_CLICKED, NULL);

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

    scroll_view = lv_obj_create(s_settings);
    lv_obj_set_pos(scroll_view, 12, 50);
    lv_obj_set_size(scroll_view, 296, 146);
    lv_obj_set_style_bg_opa(scroll_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_view, 0, 0);
    lv_obj_set_style_pad_all(scroll_view, 0, 0);
    lv_obj_set_scroll_dir(scroll_view, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_view, LV_SCROLLBAR_MODE_OFF);

    scroll_content = lv_obj_create(scroll_view);
    lv_obj_set_pos(scroll_content, 0, 0);
    lv_obj_set_size(scroll_content, 296, 302);
    lv_obj_set_style_bg_opa(scroll_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_content, 0, 0);
    lv_obj_set_style_pad_all(scroll_content, 0, 0);
    lv_obj_clear_flag(scroll_content, LV_OBJ_FLAG_SCROLLABLE);

    row = create_settings_row(scroll_content, 0, 0x3D8BFF, "WiFi连接", "点击进入扫描", 0x3D8BFF, NULL);
    lv_obj_add_event_cb(row, settings_to_wifi_cb, LV_EVENT_CLICKED, NULL);

    row = create_settings_row(scroll_content, 50, 0xFF9F43, "使用地点", s_location, 0x1C2A3A, &s_setting_location);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    row = create_settings_row(scroll_content, 100, 0x2BC670, "设备状态", "在线 / 正常", 0x2BC670, NULL);
    lv_obj_add_event_cb(row, settings_to_status_cb, LV_EVENT_CLICKED, NULL);

    row = create_settings_row(scroll_content, 150, 0x3D8BFF, "天气刷新频率", "每15分钟", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 200, 0x3D8BFF, "语言", "简体中文", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 250, 0x3D8BFF, "时区", "Asia/Shanghai", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    scroll_track = lv_obj_create(s_settings);
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

    footer = lv_obj_create(s_settings);
    lv_obj_set_pos(footer, 12, 198);
    lv_obj_set_size(footer, 296, 38);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(footer, 0, 0, 198, 38, 0x3D8BFF, "保存并返回", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, settings_back_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(footer, 206, 0, 90, 38, 0x15B7D9, "WiFi", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, settings_to_wifi_cb, LV_EVENT_CLICKED, NULL);
}

static void create_status(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *tag = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *scroll_view = NULL;
    lv_obj_t *scroll_content = NULL;
    lv_obj_t *scroll_track = NULL;
    lv_obj_t *scroll_thumb = NULL;
    lv_obj_t *row = NULL;
    lv_obj_t *footer = NULL;
    lv_obj_t *btn = NULL;

    s_status = create_base_screen();

    header = lv_obj_create(s_status);
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

    lbl = lv_label_create(tag);
    lv_label_set_text(lbl, "在线");
    set_label_font_color(lbl, &ui_font_sc_12, 0x17743D);
    lv_obj_center(lbl);

    scroll_view = lv_obj_create(s_status);
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

    row = create_settings_row(scroll_content, 0, 0xFF9F43, "供电状态", "USB 5V / 0.42A", 0x1C2A3A, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 50, 0x2BC670, "WiFi信号", "-54 dBm", 0x2BC670, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 100, 0x3D8BFF, "IP地址", "192.168.1.43", 0x1C2A3A, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 150, 0xFF9F43, "固件版本", "v1.2.7", 0x1C2A3A, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 200, 0x2BC670, "运行时长", "03:27:51", 0x1C2A3A, &s_status_runtime);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 250, 0x3D8BFF, "内存占用", "63%", 0x5F738C, NULL);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    row = create_settings_row(scroll_content, 300, 0x3D8BFF, "上次同步", "14:20:33", 0x5F738C, &s_status_sync);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    scroll_track = lv_obj_create(s_status);
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

    footer = lv_obj_create(s_status);
    lv_obj_set_pos(footer, 12, 198);
    lv_obj_set_size(footer, 296, 38);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(footer, 0, 0, 144, 38, 0x15B7D9, "刷新状态", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, status_refresh_cb, LV_EVENT_CLICKED, NULL);
    btn = create_round_button(footer, 152, 0, 144, 38, 0x3D8BFF, "返回首页", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, status_to_home_cb, LV_EVENT_CLICKED, NULL);
}

static void create_wifi(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn_scan = NULL;
    lv_obj_t *scroll_page = NULL;
    lv_obj_t *content = NULL;
    lv_obj_t *card = NULL;
    lv_obj_t *footer = NULL;
    lv_obj_t *btn = NULL;

    s_wifi = create_base_screen();

    header = lv_obj_create(s_wifi);
    lv_obj_set_pos(header, 12, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_header_back_text_btn(header, "← 设置", 64);
    lv_obj_add_event_cb(btn, wifi_back_cb, LV_EVENT_CLICKED, NULL);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, "WiFi连接");
    set_label_font_color(lbl, &ui_font_sc_18, 0x1C2A3A);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    btn_scan = create_round_button(header, 234, 0, 62, 34, 0x3D8BFF, "扫描", &ui_font_sc_14);
    lv_obj_add_event_cb(btn_scan, wifi_scan_cb, LV_EVENT_CLICKED, NULL);

    scroll_page = lv_obj_create(s_wifi);
    lv_obj_set_pos(scroll_page, 12, 50);
    lv_obj_set_size(scroll_page, 296, 184);
    lv_obj_set_style_bg_opa(scroll_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_page, 0, 0);
    lv_obj_set_style_pad_all(scroll_page, 0, 0);
    lv_obj_set_scroll_dir(scroll_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_page, LV_SCROLLBAR_MODE_OFF);

    content = lv_obj_create(scroll_page);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, 296, 300);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    card = lv_obj_create(content);
    lv_obj_set_pos(card, 0, 0);
    lv_obj_set_size(card, 296, 90);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(card);
    lv_label_set_text(lbl, "扫描结果");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 10, 6);

    s_wifi_scan_list = lv_obj_create(card);
    lv_obj_set_pos(s_wifi_scan_list, 0, 24);
    lv_obj_set_size(s_wifi_scan_list, 296, 58);
    lv_obj_set_style_bg_opa(s_wifi_scan_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_wifi_scan_list, 0, 0);
    lv_obj_set_style_pad_all(s_wifi_scan_list, 0, 0);
    lv_obj_set_style_pad_row(s_wifi_scan_list, 6, 0);
    lv_obj_set_flex_flow(s_wifi_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_wifi_scan_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_wifi_scan_list, LV_SCROLLBAR_MODE_OFF);

    wifi_fill_scan_list(k_wifi_aps, 1);

    card = lv_obj_create(content);
    lv_obj_set_pos(card, 0, 98);
    lv_obj_set_size(card, 296, 104);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(card);
    lv_label_set_text(lbl, "连接信息");
    set_label_font_color(lbl, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(lbl, 10, 6);

    s_wifi_ssid_ta = lv_textarea_create(card);
    lv_obj_set_pos(s_wifi_ssid_ta, 10, 28);
    lv_obj_set_size(s_wifi_ssid_ta, 276, 30);
    lv_obj_set_style_radius(s_wifi_ssid_ta, 8, 0);
    lv_obj_set_style_bg_color(s_wifi_ssid_ta, c_hex(0xF7FBFF), 0);
    lv_obj_set_style_border_width(s_wifi_ssid_ta, 1, 0);
    lv_obj_set_style_border_color(s_wifi_ssid_ta, c_hex(0xCDE0F7), 0);
    lv_obj_set_style_text_font(s_wifi_ssid_ta, &ui_font_sc_12, 0);
    lv_obj_set_style_text_color(s_wifi_ssid_ta, c_hex(0x32557D), 0);
    lv_obj_add_event_cb(s_wifi_ssid_ta, wifi_ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_wifi_ssid_ta, wifi_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_textarea_set_one_line(s_wifi_ssid_ta, true);
    lv_textarea_set_text(s_wifi_ssid_ta, k_wifi_aps[0].ssid);

    s_wifi_pwd_ta = lv_textarea_create(card);
    lv_obj_set_pos(s_wifi_pwd_ta, 10, 64);
    lv_obj_set_size(s_wifi_pwd_ta, 276, 30);
    lv_obj_set_style_radius(s_wifi_pwd_ta, 8, 0);
    lv_obj_set_style_bg_color(s_wifi_pwd_ta, c_hex(0xF7FBFF), 0);
    lv_obj_set_style_border_width(s_wifi_pwd_ta, 1, 0);
    lv_obj_set_style_border_color(s_wifi_pwd_ta, c_hex(0xCDE0F7), 0);
    lv_obj_set_style_text_font(s_wifi_pwd_ta, &ui_font_sc_12, 0);
    lv_obj_set_style_text_color(s_wifi_pwd_ta, c_hex(0x32557D), 0);
    lv_obj_add_event_cb(s_wifi_pwd_ta, wifi_ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_wifi_pwd_ta, wifi_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_textarea_set_one_line(s_wifi_pwd_ta, true);
    lv_textarea_set_password_mode(s_wifi_pwd_ta, true);
    lv_textarea_set_text(s_wifi_pwd_ta, "");

    footer = lv_obj_create(content);
    lv_obj_set_pos(footer, 0, 210);
    lv_obj_set_size(footer, 296, 38);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(footer, 0, 0, 144, 38, 0x2BC670, "连接WiFi", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, wifi_connect_cb, LV_EVENT_CLICKED, NULL);
    btn = create_round_button(footer, 152, 0, 144, 38, 0x7C83FF, "取消", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, wifi_cancel_cb, LV_EVENT_CLICKED, NULL);

    s_wifi_msg_bar = lv_obj_create(content);
    lv_obj_set_pos(s_wifi_msg_bar, 0, 258);
    lv_obj_set_size(s_wifi_msg_bar, 296, 30);
    lv_obj_set_style_radius(s_wifi_msg_bar, 8, 0);
    lv_obj_set_style_bg_color(s_wifi_msg_bar, c_hex(0xFFE5E5), 0);
    lv_obj_set_style_border_color(s_wifi_msg_bar, c_hex(0xF5B6B6), 0);
    lv_obj_set_style_border_width(s_wifi_msg_bar, 1, 0);
    lv_obj_clear_flag(s_wifi_msg_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_wifi_msg_bar, LV_OBJ_FLAG_HIDDEN);

    s_wifi_msg_label = lv_label_create(s_wifi_msg_bar);
    lv_label_set_text(s_wifi_msg_label, "连接失败：密码错误，请重新输入");
    set_label_font_color(s_wifi_msg_label, &ui_font_sc_12, 0xF25F5C);
    lv_obj_center(s_wifi_msg_label);

    s_wifi_keyboard = lv_keyboard_create(s_wifi);
    lv_obj_set_size(s_wifi_keyboard, 320, 106);
    lv_obj_align(s_wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_wifi_keyboard, wifi_kb_event_cb, LV_EVENT_ALL, NULL);
}

void ui_page_sim_create(void)
{
    create_home();
    create_settings();
    create_status();
    create_firmware();
    create_ota_ready();
    create_ota_progress();
    create_ota_complete();
    create_ota_failed();
    create_wifi();
    sync_dynamic_texts();
    open_page(SIM_PAGE_HOME, LV_SCR_LOAD_ANIM_NONE);
}
