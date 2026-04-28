#include "page_firmware.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_backend.h"
#include "esp_log.h"
#include "ui_fonts.h"
#include "ui_page_manager.h"

static const char *TAG = "PAGE_FIRMWARE";

static lv_obj_t *s_screen;
static lv_obj_t *s_current_label;
static lv_obj_t *s_latest_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_last_check_label;
static lv_obj_t *s_check_btn;
static lv_obj_t *s_check_btn_label;
static lv_obj_t *s_modal_overlay;
static lv_obj_t *s_modal_box;
static app_backend_ota_state_t s_dismissed_state = APP_BACKEND_OTA_IDLE;
static app_backend_ota_state_t s_visible_state = APP_BACKEND_OTA_IDLE;
static int s_visible_progress = -1;
static char s_dismissed_target[APP_BACKEND_OTA_VERSION_LEN];
static char s_visible_target[APP_BACKEND_OTA_VERSION_LEN];
static char s_visible_message[APP_BACKEND_OTA_MESSAGE_LEN];

static lv_color_t c_hex(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void set_label_font_color(lv_obj_t *label, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c_hex(rgb), 0);
}

static const char *version_or_dash(const char *version)
{
    return ((version != NULL) && (version[0] != '\0')) ? version : "--";
}

static void format_version(char *out, size_t out_size, const char *version)
{
    if ((out == NULL) || (out_size == 0)) {
        return;
    }
    if ((version == NULL) || (version[0] == '\0') || (strcmp(version, "--") == 0)) {
        snprintf(out, out_size, "--");
    } else if ((version[0] == 'v') || (version[0] == 'V')) {
        snprintf(out, out_size, "%s", version);
    } else {
        snprintf(out, out_size, "v%s", version);
    }
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

static lv_obj_t *create_header_text_btn(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 64, 24);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lbl, text);
    set_label_font_color(lbl, &ui_font_sc_13, 0x5F738C);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *create_round_button(lv_obj_t *parent,
                                     lv_coord_t x,
                                     lv_coord_t y,
                                     lv_coord_t w,
                                     lv_coord_t h,
                                     uint32_t bg,
                                     const char *text,
                                     lv_obj_t **label_out)
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
    lv_obj_set_style_text_font(lbl, &ui_font_sc_14_bold, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    if (label_out != NULL) {
        *label_out = lbl;
    }
    return btn;
}

static lv_obj_t *create_row(lv_obj_t *parent,
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

    if (right_label_out != NULL) {
        *right_label_out = right;
    }
    return row;
}

static void close_modal(void)
{
    if (s_modal_overlay != NULL) {
        lv_obj_del(s_modal_overlay);
        s_modal_overlay = NULL;
        s_modal_box = NULL;
    }
    s_visible_state = APP_BACKEND_OTA_IDLE;
    s_visible_progress = -1;
    s_visible_target[0] = '\0';
    s_visible_message[0] = '\0';
}

static void dismiss_modal(app_backend_ota_state_t state, const char *target)
{
    s_dismissed_state = state;
    snprintf(s_dismissed_target, sizeof(s_dismissed_target), "%s", version_or_dash(target));
    close_modal();
}

static bool modal_is_dismissed(app_backend_ota_state_t state, const char *target)
{
    return (s_dismissed_state == state) &&
           (strcmp(s_dismissed_target, version_or_dash(target)) == 0);
}

static lv_obj_t *create_progress_bar(lv_obj_t *parent,
                                     lv_coord_t x,
                                     lv_coord_t y,
                                     lv_coord_t w,
                                     lv_coord_t h,
                                     int progress,
                                     uint32_t fill_color)
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
    lv_obj_set_style_bg_color(fill, c_hex(fill_color), 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    return track;
}

static void create_version_compare(lv_obj_t *parent,
                                   lv_coord_t x,
                                   lv_coord_t y,
                                   const char *current,
                                   const char *target)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *pill = NULL;
    lv_obj_t *label = NULL;

    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, 216, 28);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_color(row, c_hex(0xEBF6FF), 0);
    lv_obj_set_style_border_color(row, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(row);
    lv_label_set_text(label, current);
    lv_obj_set_width(label, 74);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    set_label_font_color(label, &ui_font_sc_12_heavy, 0x1C2A3A);
    lv_obj_set_pos(label, 16, 6);

    label = lv_label_create(row);
    lv_label_set_text(label, "→");
    lv_obj_set_width(label, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    set_label_font_color(label, &ui_font_sc_12_heavy, 0x5F738C);
    lv_obj_set_pos(label, 92, 6);

    pill = lv_obj_create(row);
    lv_obj_set_pos(pill, 120, 5);
    lv_obj_set_size(pill, 82, 18);
    lv_obj_set_style_radius(pill, 9, 0);
    lv_obj_set_style_bg_color(pill, c_hex(0x3D8BFF), 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(pill);
    lv_label_set_text(label, target);
    lv_obj_set_width(label, 82);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    set_label_font_color(label, &ui_font_sc_12_heavy, 0xFFFFFF);
    lv_obj_center(label);
}

static lv_obj_t *create_modal_box(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    s_modal_overlay = lv_obj_create(s_screen);
    lv_obj_set_pos(s_modal_overlay, 0, 0);
    lv_obj_set_size(s_modal_overlay, 320, 240);
    lv_obj_set_style_bg_color(s_modal_overlay, c_hex(0x0E1825), 0);
    lv_obj_set_style_bg_opa(s_modal_overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_modal_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_modal_overlay, 0, 0);
    lv_obj_clear_flag(s_modal_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_modal_box = lv_obj_create(s_modal_overlay);
    lv_obj_set_pos(s_modal_box, x, y);
    lv_obj_set_size(s_modal_box, w, h);
    lv_obj_set_style_radius(s_modal_box, 14, 0);
    lv_obj_set_style_bg_color(s_modal_box, lv_color_white(), 0);
    lv_obj_set_style_border_color(s_modal_box, c_hex(0xD7E7F8), 0);
    lv_obj_set_style_border_width(s_modal_box, 1, 0);
    lv_obj_set_style_shadow_width(s_modal_box, 18, 0);
    lv_obj_set_style_shadow_color(s_modal_box, c_hex(0x1A2E47), 0);
    lv_obj_set_style_shadow_opa(s_modal_box, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(s_modal_box, 8, 0);
    lv_obj_set_style_pad_all(s_modal_box, 0, 0);
    lv_obj_clear_flag(s_modal_box, LV_OBJ_FLAG_SCROLLABLE);
    return s_modal_box;
}

static void ota_later_cb(lv_event_t *e)
{
    (void)lv_event_get_user_data(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    dismiss_modal(s_visible_state, s_visible_target);
}

static void ota_update_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (app_backend_ota_confirm_async() != ESP_OK) {
        ESP_LOGW(TAG, "OTA confirm enqueue failed");
    }
    close_modal();
}

static void ota_retry_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (app_backend_ota_check_async() != ESP_OK) {
        ESP_LOGW(TAG, "OTA check enqueue failed");
    }
    close_modal();
}

static void ota_restart_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (app_backend_ota_restart_async() != ESP_OK) {
        ESP_LOGW(TAG, "OTA restart enqueue failed");
    }
}

static void create_ready_modal(const app_backend_snapshot_t *snapshot)
{
    char current[32] = {0};
    char target[32] = {0};
    char body[80] = {0};
    lv_obj_t *box = create_modal_box(33, 41, 252, 158);
    lv_obj_t *label = NULL;
    lv_obj_t *btn = NULL;

    format_version(current, sizeof(current), snapshot->ota.current_version);
    format_version(target, sizeof(target), snapshot->ota.target_version);

    label = lv_label_create(box);
    lv_label_set_text(label, "发现新版本");
    set_label_font_color(label, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_set_width(label, 252);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 14);

    snprintf(body, sizeof(body), "检测到固件 %s", target);
    label = lv_label_create(box);
    lv_label_set_text(label, body);
    set_label_font_color(label, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(label, 27, 47);

    label = lv_label_create(box);
    lv_label_set_text(label, "建议保持供电和 WiFi 连接");
    set_label_font_color(label, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(label, 27, 69);

    create_version_compare(box, 18, 91, current, target);

    btn = create_round_button(box, 19, 122, 96, 30, 0x7C83FF, "稍后", NULL);
    lv_obj_add_event_cb(btn, ota_later_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(box, 135, 122, 96, 30, 0x3D8BFF, "更新", NULL);
    lv_obj_add_event_cb(btn, ota_update_cb, LV_EVENT_CLICKED, NULL);
}

static void create_progress_modal(const app_backend_snapshot_t *snapshot)
{
    char pct[8] = {0};
    int progress = snapshot->ota.progress;
    lv_obj_t *box = create_modal_box(33, 49, 252, 138);
    lv_obj_t *label = NULL;

    if (progress < 0) {
        progress = 0;
    } else if (progress > 100) {
        progress = 100;
    }

    label = lv_label_create(box);
    lv_label_set_text(label, "OTA升级中");
    set_label_font_color(label, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_set_width(label, 252);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 14);

    label = lv_label_create(box);
    lv_label_set_text(label, snapshot->ota.message[0] != '\0' ? snapshot->ota.message : "正在下载升级包");
    set_label_font_color(label, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(label, 27, 47);

    label = lv_label_create(box);
    lv_label_set_text(label, "请勿断电或重启设备");
    set_label_font_color(label, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(label, 27, 69);

    create_progress_bar(box, 27, 98, 174, 8, progress, 0x3D8BFF);

    snprintf(pct, sizeof(pct), "%d%%", progress);
    label = lv_label_create(box);
    lv_label_set_text(label, pct);
    set_label_font_color(label, &ui_font_sc_14_bold, 0x3F6FA6);
    lv_obj_set_pos(label, 213, 93);

    label = lv_label_create(box);
    lv_label_set_text(label, progress >= 100 ? "升级包校验完成" : "下载进度更新中");
    set_label_font_color(label, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(label, 27, 115);
}

static void create_done_modal(const app_backend_snapshot_t *snapshot)
{
    char target[32] = {0};
    char body[64] = {0};
    lv_obj_t *box = create_modal_box(27, 37, 264, 164);
    lv_obj_t *label = NULL;
    lv_obj_t *btn = NULL;

    format_version(target, sizeof(target), snapshot->ota.target_version);

    label = lv_label_create(box);
    lv_label_set_text(label, "下载完成");
    set_label_font_color(label, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_set_width(label, 264);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 16);

    label = lv_label_create(box);
    lv_label_set_text(label, "升级包已准备完成");
    set_label_font_color(label, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(label, 29, 52);

    snprintf(body, sizeof(body), "重启后将切换到新版本 %s", target);
    label = lv_label_create(box);
    lv_label_set_text(label, body);
    set_label_font_color(label, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(label, 29, 75);

    create_progress_bar(box, 29, 104, 178, 8, 100, 0x2BC670);

    label = lv_label_create(box);
    lv_label_set_text(label, "100%");
    set_label_font_color(label, &ui_font_sc_14_bold, 0x2BC670);
    lv_obj_set_pos(label, 219, 99);

    btn = create_round_button(box, 21, 129, 104, 30, 0xD6E5F5, "稍后重启", NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), c_hex(0x5F738C), 0);
    lv_obj_add_event_cb(btn, ota_later_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(box, 137, 129, 104, 30, 0x3D8BFF, "立即重启", NULL);
    lv_obj_add_event_cb(btn, ota_restart_cb, LV_EVENT_CLICKED, NULL);
}

static void create_failed_modal(const app_backend_snapshot_t *snapshot)
{
    char pct[8] = {0};
    char current[32] = {0};
    char body[64] = {0};
    int progress = snapshot->ota.progress;
    lv_obj_t *box = create_modal_box(33, 41, 252, 158);
    lv_obj_t *label = NULL;
    lv_obj_t *btn = NULL;

    if (progress < 0) {
        progress = 0;
    } else if (progress > 100) {
        progress = 100;
    }
    format_version(current, sizeof(current), snapshot->ota.current_version);

    label = lv_label_create(box);
    lv_label_set_text(label, "升级失败");
    set_label_font_color(label, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_set_width(label, 252);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 14);

    label = lv_label_create(box);
    lv_label_set_text(label, snapshot->ota.message[0] != '\0' ? snapshot->ota.message : "下载中断，请稍后重试");
    set_label_font_color(label, &ui_font_sc_14_bold, 0x1C2A3A);
    lv_obj_set_pos(label, 27, 47);

    snprintf(body, sizeof(body), "当前版本仍保持 %s", current);
    label = lv_label_create(box);
    lv_label_set_text(label, body);
    set_label_font_color(label, &ui_font_sc_12, 0x5F738C);
    lv_obj_set_pos(label, 27, 69);

    create_progress_bar(box, 27, 98, 174, 8, progress, 0xF04542);

    snprintf(pct, sizeof(pct), "%d%%", progress);
    label = lv_label_create(box);
    lv_label_set_text(label, pct);
    set_label_font_color(label, &ui_font_sc_14_bold, 0xF04542);
    lv_obj_set_pos(label, 213, 93);

    btn = create_round_button(box, 19, 122, 96, 28, 0xD6E5F5, "稍后", NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), c_hex(0x5F738C), 0);
    lv_obj_add_event_cb(btn, ota_later_cb, LV_EVENT_CLICKED, NULL);

    btn = create_round_button(box, 135, 122, 96, 28, 0x3D8BFF, "重试", NULL);
    lv_obj_add_event_cb(btn, ota_retry_cb, LV_EVENT_CLICKED, NULL);
}

static void update_modal(const app_backend_snapshot_t *snapshot)
{
    const char *target = NULL;
    const char *message = NULL;

    if ((s_screen == NULL) || (snapshot == NULL)) {
        return;
    }

    target = version_or_dash(snapshot->ota.target_version);
    message = snapshot->ota.message;

    if (modal_is_dismissed(snapshot->ota.state, target)) {
        close_modal();
        return;
    }

    if ((s_modal_overlay != NULL) &&
        (s_visible_state == snapshot->ota.state) &&
        (s_visible_progress == snapshot->ota.progress) &&
        (strcmp(s_visible_target, target) == 0) &&
        (strcmp(s_visible_message, message) == 0)) {
        return;
    }

    close_modal();
    s_visible_state = snapshot->ota.state;
    s_visible_progress = snapshot->ota.progress;
    snprintf(s_visible_target, sizeof(s_visible_target), "%s", target);
    snprintf(s_visible_message, sizeof(s_visible_message), "%s", message);
    switch (snapshot->ota.state) {
    case APP_BACKEND_OTA_READY:
        create_ready_modal(snapshot);
        break;
    case APP_BACKEND_OTA_DOWNLOADING:
    case APP_BACKEND_OTA_APPLYING:
        create_progress_modal(snapshot);
        break;
    case APP_BACKEND_OTA_DONE:
        create_done_modal(snapshot);
        break;
    case APP_BACKEND_OTA_FAILED:
        create_failed_modal(snapshot);
        break;
    default:
        break;
    }
}

static const char *ota_state_text(app_backend_ota_state_t state, const char *message)
{
    if ((message != NULL) && (message[0] != '\0')) {
        return message;
    }
    switch (state) {
    case APP_BACKEND_OTA_CHECKING:
        return "正在检测";
    case APP_BACKEND_OTA_READY:
        return "发现新版本";
    case APP_BACKEND_OTA_DOWNLOADING:
        return "正在升级";
    case APP_BACKEND_OTA_APPLYING:
        return "正在写入";
    case APP_BACKEND_OTA_DONE:
        return "等待重启";
    case APP_BACKEND_OTA_FAILED:
        return "升级失败";
    default:
        return "当前正常";
    }
}

void page_firmware_apply_snapshot(const app_backend_snapshot_t *snapshot)
{
    char version[32] = {0};

    if ((s_screen == NULL) || (snapshot == NULL)) {
        return;
    }

    format_version(version, sizeof(version), snapshot->ota.current_version);
    if (s_current_label != NULL) {
        lv_label_set_text(s_current_label, version);
    }

    if (s_latest_label != NULL) {
        if ((snapshot->ota.target_version[0] != '\0') &&
            (strcmp(snapshot->ota.target_version, "--") != 0) &&
            (snapshot->ota.state != APP_BACKEND_OTA_IDLE)) {
            format_version(version, sizeof(version), snapshot->ota.target_version);
            lv_label_set_text(s_latest_label, version);
            lv_obj_set_style_text_color(s_latest_label, c_hex(0x1C2A3A), 0);
        } else if ((snapshot->ota.message[0] != '\0') &&
                   (strcmp(snapshot->ota.message, "当前已是最新版本") == 0)) {
            lv_label_set_text(s_latest_label, "无新版本");
            lv_obj_set_style_text_color(s_latest_label, c_hex(0x2BC670), 0);
        } else {
            lv_label_set_text(s_latest_label, "待检测");
            lv_obj_set_style_text_color(s_latest_label, c_hex(0x5F738C), 0);
        }
    }

    if (s_state_label != NULL) {
        lv_label_set_text(s_state_label, ota_state_text(snapshot->ota.state, snapshot->ota.message));
        lv_obj_set_style_text_color(s_state_label,
                                    snapshot->ota.state == APP_BACKEND_OTA_FAILED ? c_hex(0xF04542) :
                                    snapshot->ota.state == APP_BACKEND_OTA_READY ? c_hex(0x3D8BFF) :
                                    snapshot->ota.state == APP_BACKEND_OTA_DONE ? c_hex(0x2BC670) :
                                    c_hex(0x2BC670),
                                    0);
    }

    if (s_last_check_label != NULL) {
        lv_label_set_text(s_last_check_label,
                          snapshot->ota.state == APP_BACKEND_OTA_IDLE ? "--:--" :
                          (snapshot->now_time[0] != '\0' ? snapshot->now_time : "--:--"));
    }

    if ((s_check_btn_label != NULL) && (s_check_btn != NULL)) {
        if (snapshot->ota.state == APP_BACKEND_OTA_CHECKING) {
            lv_label_set_text(s_check_btn_label, "检测中");
            lv_obj_clear_flag(s_check_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_check_btn, c_hex(0x89B9F0), 0);
        } else {
            lv_label_set_text(s_check_btn_label, "检查更新");
            lv_obj_add_flag(s_check_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_check_btn, c_hex(0x3D8BFF), 0);
        }
    }

    update_modal(snapshot);
}

static void page_firmware_back_async(void *user_data)
{
    (void)user_data;
    if (ui_page_pop(UI_ANIM_MOVE_RIGHT) != ESP_OK) {
        ESP_LOGW(TAG, "ui_page_pop failed");
    }
}

static void page_firmware_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        (void)lv_async_call(page_firmware_back_async, NULL);
    }
}

static void page_firmware_check_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    s_dismissed_state = APP_BACKEND_OTA_IDLE;
    s_dismissed_target[0] = '\0';
    if (app_backend_ota_check_async() != ESP_OK) {
        ESP_LOGW(TAG, "OTA check enqueue failed");
    }
}

static lv_obj_t *page_firmware_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *tag = NULL;
    lv_obj_t *lbl = NULL;
    lv_obj_t *btn = NULL;
    lv_obj_t *scroll_view = NULL;
    lv_obj_t *scroll_content = NULL;
    lv_obj_t *scroll_track = NULL;
    lv_obj_t *scroll_thumb = NULL;

    s_screen = create_base_screen();

    header = lv_obj_create(s_screen);
    lv_obj_set_pos(header, 11, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_header_text_btn(header, "← 状态");
    lv_obj_add_event_cb(btn, page_firmware_back_cb, LV_EVENT_CLICKED, NULL);

    lbl = lv_label_create(header);
    lv_label_set_text(lbl, "固件版本");
    set_label_font_color(lbl, &ui_font_sc_18_heavy, 0x1C2A3A);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

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

    scroll_view = lv_obj_create(s_screen);
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

    create_row(scroll_content, 0, 0x3D8BFF, "当前版本", "--", 0x1C2A3A, &s_current_label);
    create_row(scroll_content, 50, 0xFF9F43, "最新版本", "待检测", 0x5F738C, &s_latest_label);
    create_row(scroll_content, 100, 0x2BC670, "升级状态", "当前正常", 0x2BC670, &s_state_label);
    create_row(scroll_content, 150, 0x3D8BFF, "上次检测", "--:--", 0x5F738C, &s_last_check_label);
    create_row(scroll_content, 200, 0xFF9F43, "更新方式", "手动检查", 0x5F738C, NULL);

    scroll_track = lv_obj_create(s_screen);
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

    s_check_btn = create_round_button(s_screen, 11, 205, 296, 34, 0x3D8BFF, "检查更新", &s_check_btn_label);
    lv_obj_add_event_cb(s_check_btn, page_firmware_check_cb, LV_EVENT_CLICKED, NULL);

    return s_screen;
}

static void page_firmware_on_destroy(void)
{
    close_modal();
    s_screen = NULL;
    s_current_label = NULL;
    s_latest_label = NULL;
    s_state_label = NULL;
    s_last_check_label = NULL;
    s_check_btn = NULL;
    s_check_btn_label = NULL;
}

static void page_firmware_on_show(void *args)
{
    app_backend_snapshot_t snapshot = {0};
    (void)args;

    if (app_backend_get_snapshot(&snapshot) == ESP_OK) {
        page_firmware_apply_snapshot(&snapshot);
    }
}

const ui_page_t g_page_firmware = {
    .id = UI_PAGE_FIRMWARE,
    .name = "FIRMWARE",
    .cache_mode = UI_PAGE_CACHE_NONE,
    .create = page_firmware_create,
    .on_show = page_firmware_on_show,
    .on_hide = NULL,
    .on_destroy = page_firmware_on_destroy,
};
