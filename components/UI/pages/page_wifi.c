#include "page_wifi.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "ui_fonts.h"
#include "ui_page_manager.h"

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

typedef struct {
    lv_obj_t *scan_list;
    lv_obj_t *ta_ssid;
    lv_obj_t *ta_pwd;
    lv_obj_t *msg_bar;
    lv_obj_t *msg_label;
    lv_obj_t *keyboard;
    const wifi_ap_t *selected_ap;
} wifi_page_ctx_t;

static lv_obj_t *s_screen = NULL;
static wifi_page_ctx_t s_ctx = {0};
static bool s_nav_pending = false;

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

static void wifi_show_msg(const char *msg, uint32_t text_color, uint32_t bg_color, uint32_t border_color)
{
    lv_label_set_text(s_ctx.msg_label, msg);
    lv_obj_set_style_text_color(s_ctx.msg_label, c_hex(text_color), 0);
    lv_obj_set_style_bg_color(s_ctx.msg_bar, c_hex(bg_color), 0);
    lv_obj_set_style_border_color(s_ctx.msg_bar, c_hex(border_color), 0);
    lv_obj_clear_flag(s_ctx.msg_bar, LV_OBJ_FLAG_HIDDEN);
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

static void page_wifi_pop_async(void *user_data)
{
    (void)user_data;

    if (s_ctx.keyboard != NULL) {
        lv_keyboard_set_textarea(s_ctx.keyboard, NULL);
        lv_obj_add_flag(s_ctx.keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    (void)ui_page_pop(UI_ANIM_MOVE_RIGHT);
    s_nav_pending = false;
}

static void page_wifi_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_nav_pending) {
        return;
    }

    s_nav_pending = true;
    if (lv_async_call(page_wifi_pop_async, NULL) != LV_RES_OK) {
        s_nav_pending = false;
    }
}

static void page_wifi_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if ((code == LV_EVENT_READY) || (code == LV_EVENT_CANCEL)) {
        lv_keyboard_set_textarea(s_ctx.keyboard, NULL);
        lv_obj_add_flag(s_ctx.keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void page_wifi_ta_focus_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if ((code == LV_EVENT_CLICKED) || (code == LV_EVENT_FOCUSED)) {
        lv_keyboard_set_textarea(s_ctx.keyboard, ta);
        lv_obj_clear_flag(s_ctx.keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void page_wifi_select_ap_cb(lv_event_t *e)
{
    const wifi_ap_t *ap = (const wifi_ap_t *)lv_event_get_user_data(e);

    if ((lv_event_get_code(e) != LV_EVENT_CLICKED) || (ap == NULL)) {
        return;
    }

    s_ctx.selected_ap = ap;
    lv_textarea_set_text(s_ctx.ta_ssid, ap->ssid);
    lv_textarea_set_text(s_ctx.ta_pwd, "");
    lv_obj_add_flag(s_ctx.msg_bar, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_fill_scan_list(const wifi_ap_t *aps, size_t ap_cnt)
{
    size_t i;

    lv_obj_clean(s_ctx.scan_list);
    for (i = 0; i < ap_cnt; i++) {
        lv_obj_t *row = create_wifi_ap_row(s_ctx.scan_list, &aps[i]);
        lv_obj_add_event_cb(row, page_wifi_select_ap_cb, LV_EVENT_CLICKED, (void *)&aps[i]);
    }
}

static void page_wifi_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    wifi_fill_scan_list(k_wifi_aps, sizeof(k_wifi_aps) / sizeof(k_wifi_aps[0]));
    lv_obj_add_flag(s_ctx.msg_bar, LV_OBJ_FLAG_HIDDEN);
}

static void page_wifi_connect_cb(lv_event_t *e)
{
    const char *pwd;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    pwd = lv_textarea_get_text(s_ctx.ta_pwd);
    if ((pwd == NULL) || (pwd[0] == '\0')) {
        wifi_show_msg("连接失败：请输入密码", 0xF25F5C, 0xFFE5E5, 0xF5B6B6);
        return;
    }

    if ((s_ctx.selected_ap != NULL) && (strcmp(pwd, s_ctx.selected_ap->password) == 0)) {
        wifi_show_msg("连接成功：WiFi 已连接", 0x17743D, 0xDDF8E8, 0xBEEBCF);
    } else {
        wifi_show_msg("连接失败：密码错误，请重新输入", 0xF25F5C, 0xFFE5E5, 0xF5B6B6);
    }
}

static lv_obj_t *page_wifi_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn = NULL;
    lv_obj_t *scroll_page = NULL;
    lv_obj_t *content = NULL;
    lv_obj_t *card = NULL;
    lv_obj_t *footer = NULL;

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.selected_ap = &k_wifi_aps[0];
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

    btn = create_header_back_text_btn(header, "← 设置", 64);
    lv_obj_add_event_cb(btn, page_wifi_back_cb, LV_EVENT_CLICKED, NULL);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, "WiFi连接");
    set_label_font_color(lbl, &ui_font_sc_18, 0x1C2A3A);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    btn = create_round_button(header, 234, 0, 62, 34, 0x3D8BFF, "扫描", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_wifi_scan_cb, LV_EVENT_CLICKED, NULL);

    scroll_page = lv_obj_create(s_screen);
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

    s_ctx.scan_list = lv_obj_create(card);
    lv_obj_set_pos(s_ctx.scan_list, 0, 24);
    lv_obj_set_size(s_ctx.scan_list, 296, 58);
    lv_obj_set_style_bg_opa(s_ctx.scan_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ctx.scan_list, 0, 0);
    lv_obj_set_style_pad_all(s_ctx.scan_list, 0, 0);
    lv_obj_set_style_pad_row(s_ctx.scan_list, 6, 0);
    lv_obj_set_flex_flow(s_ctx.scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_ctx.scan_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_ctx.scan_list, LV_SCROLLBAR_MODE_OFF);

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

    s_ctx.ta_ssid = lv_textarea_create(card);
    lv_obj_set_pos(s_ctx.ta_ssid, 10, 28);
    lv_obj_set_size(s_ctx.ta_ssid, 276, 30);
    lv_obj_set_style_radius(s_ctx.ta_ssid, 8, 0);
    lv_obj_set_style_bg_color(s_ctx.ta_ssid, c_hex(0xF7FBFF), 0);
    lv_obj_set_style_border_width(s_ctx.ta_ssid, 1, 0);
    lv_obj_set_style_border_color(s_ctx.ta_ssid, c_hex(0xCDE0F7), 0);
    lv_obj_set_style_text_font(s_ctx.ta_ssid, &ui_font_sc_12, 0);
    lv_obj_set_style_text_color(s_ctx.ta_ssid, c_hex(0x32557D), 0);
    lv_obj_add_event_cb(s_ctx.ta_ssid, page_wifi_ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ctx.ta_ssid, page_wifi_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_textarea_set_one_line(s_ctx.ta_ssid, true);
    lv_textarea_set_text(s_ctx.ta_ssid, k_wifi_aps[0].ssid);

    s_ctx.ta_pwd = lv_textarea_create(card);
    lv_obj_set_pos(s_ctx.ta_pwd, 10, 64);
    lv_obj_set_size(s_ctx.ta_pwd, 276, 30);
    lv_obj_set_style_radius(s_ctx.ta_pwd, 8, 0);
    lv_obj_set_style_bg_color(s_ctx.ta_pwd, c_hex(0xF7FBFF), 0);
    lv_obj_set_style_border_width(s_ctx.ta_pwd, 1, 0);
    lv_obj_set_style_border_color(s_ctx.ta_pwd, c_hex(0xCDE0F7), 0);
    lv_obj_set_style_text_font(s_ctx.ta_pwd, &ui_font_sc_12, 0);
    lv_obj_set_style_text_color(s_ctx.ta_pwd, c_hex(0x32557D), 0);
    lv_obj_add_event_cb(s_ctx.ta_pwd, page_wifi_ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ctx.ta_pwd, page_wifi_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_textarea_set_one_line(s_ctx.ta_pwd, true);
    lv_textarea_set_password_mode(s_ctx.ta_pwd, true);
    lv_textarea_set_text(s_ctx.ta_pwd, "");

    footer = lv_obj_create(content);
    lv_obj_set_pos(footer, 0, 210);
    lv_obj_set_size(footer, 296, 38);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_round_button(footer, 0, 0, 144, 38, 0x2BC670, "连接WiFi", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_wifi_connect_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(footer, 152, 0, 144, 38, 0x7C83FF, "取消", &ui_font_sc_14);
    lv_obj_add_event_cb(btn, page_wifi_back_cb, LV_EVENT_CLICKED, NULL);

    s_ctx.msg_bar = lv_obj_create(content);
    lv_obj_set_pos(s_ctx.msg_bar, 0, 258);
    lv_obj_set_size(s_ctx.msg_bar, 296, 30);
    lv_obj_set_style_radius(s_ctx.msg_bar, 8, 0);
    lv_obj_set_style_bg_color(s_ctx.msg_bar, c_hex(0xFFE5E5), 0);
    lv_obj_set_style_border_color(s_ctx.msg_bar, c_hex(0xF5B6B6), 0);
    lv_obj_set_style_border_width(s_ctx.msg_bar, 1, 0);
    lv_obj_clear_flag(s_ctx.msg_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ctx.msg_bar, LV_OBJ_FLAG_HIDDEN);

    s_ctx.msg_label = lv_label_create(s_ctx.msg_bar);
    lv_label_set_text(s_ctx.msg_label, "连接失败：密码错误，请重新输入");
    set_label_font_color(s_ctx.msg_label, &ui_font_sc_12, 0xF25F5C);
    lv_obj_center(s_ctx.msg_label);

    s_ctx.keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_ctx.keyboard, 320, 106);
    lv_obj_align(s_ctx.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_ctx.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ctx.keyboard, page_wifi_kb_event_cb, LV_EVENT_ALL, NULL);

    return s_screen;
}

static void page_wifi_on_destroy(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_screen = NULL;
    s_nav_pending = false;
}

const ui_page_t g_page_wifi = {
    .id = UI_PAGE_WIFI,
    .name = "WIFI",
    .cache_mode = UI_PAGE_CACHE_NONE,
    .create = page_wifi_create,
    .on_show = NULL,
    .on_hide = NULL,
    .on_destroy = page_wifi_on_destroy,
};
