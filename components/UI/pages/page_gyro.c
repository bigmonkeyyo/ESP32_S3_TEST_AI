#include "page_gyro.h"

#include <stdbool.h>

#include "ui_fonts.h"
#include "ui_page_manager.h"

#define GYRO_FIELD_W 296
#define GYRO_FIELD_H 182
#define GYRO_BALL_SIZE 14

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_ball = NULL;
static lv_timer_t *s_ball_timer = NULL;
static volatile float s_pending_x_norm = 0.0f;
static volatile float s_pending_y_norm = 0.0f;
static volatile bool s_ball_pending = false;

static lv_color_t c_hex(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void set_label_font_color(lv_obj_t *label, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c_hex(rgb), 0);
}

static lv_coord_t clamp_coord(lv_coord_t value, lv_coord_t min_value, lv_coord_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float clamp_norm(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void page_gyro_apply_ball_norm(float x_norm, float y_norm)
{
    const lv_coord_t x_max = GYRO_FIELD_W - GYRO_BALL_SIZE;
    const lv_coord_t y_max = GYRO_FIELD_H - GYRO_BALL_SIZE;
    lv_coord_t x;
    lv_coord_t y;

    if (s_ball == NULL) {
        return;
    }

    x = (lv_coord_t)(((clamp_norm(x_norm) + 1.0f) * (float)x_max) * 0.5f);
    y = (lv_coord_t)(((clamp_norm(y_norm) + 1.0f) * (float)y_max) * 0.5f);
    lv_obj_set_pos(s_ball, clamp_coord(x, 0, x_max), clamp_coord(y, 0, y_max));
}

static void page_gyro_ball_timer_cb(lv_timer_t *timer)
{
    float x_norm;
    float y_norm;

    (void)timer;

    if (!s_ball_pending) {
        return;
    }

    x_norm = s_pending_x_norm;
    y_norm = s_pending_y_norm;
    s_ball_pending = false;
    page_gyro_apply_ball_norm(x_norm, y_norm);
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

static lv_obj_t *create_header_back_text_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 72, 24);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lbl, "← 返回");
    set_label_font_color(lbl, &ui_font_sc_13, 0x5F738C);
    lv_obj_center(lbl);

    return btn;
}

static void page_gyro_back_async(void *user_data)
{
    (void)user_data;
    (void)ui_page_pop(UI_ANIM_MOVE_RIGHT);
}

static void page_gyro_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        (void)lv_async_call(page_gyro_back_async, NULL);
    }
}

static void create_corner_anchor(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *dot = lv_obj_create(parent);

    lv_obj_set_pos(dot, x, y);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_color(dot, c_hex(0xEAF3FF), 0);
    lv_obj_set_style_border_color(dot, c_hex(0xCFE4FA), 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *page_gyro_create(void)
{
    lv_obj_t *header = NULL;
    lv_obj_t *btn = NULL;
    lv_obj_t *field = NULL;

    s_screen = create_base_screen();

    header = lv_obj_create(s_screen);
    lv_obj_set_pos(header, 12, 8);
    lv_obj_set_size(header, 296, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    btn = create_header_back_text_btn(header);
    lv_obj_add_event_cb(btn, page_gyro_back_cb, LV_EVENT_CLICKED, NULL);

    field = lv_obj_create(s_screen);
    lv_obj_set_pos(field, 12, 46);
    lv_obj_set_size(field, GYRO_FIELD_W, GYRO_FIELD_H);
    lv_obj_set_style_radius(field, 10, 0);
    lv_obj_set_style_bg_color(field, c_hex(0xFDFEFF), 0);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(field, c_hex(0xD0E5F9), 0);
    lv_obj_set_style_border_width(field, 1, 0);
    lv_obj_set_style_pad_all(field, 0, 0);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(field, true, 0);

    create_corner_anchor(field, 8, 8);
    create_corner_anchor(field, GYRO_FIELD_W - 14, 8);
    create_corner_anchor(field, 8, GYRO_FIELD_H - 14);
    create_corner_anchor(field, GYRO_FIELD_W - 14, GYRO_FIELD_H - 14);

    s_ball = lv_obj_create(field);
    lv_obj_set_size(s_ball, GYRO_BALL_SIZE, GYRO_BALL_SIZE);
    lv_obj_set_style_radius(s_ball, GYRO_BALL_SIZE / 2, 0);
    lv_obj_set_style_bg_color(s_ball, c_hex(0x3D8BFF), 0);
    lv_obj_set_style_bg_opa(s_ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ball, 0, 0);
    lv_obj_set_style_pad_all(s_ball, 0, 0);
    lv_obj_clear_flag(s_ball, LV_OBJ_FLAG_SCROLLABLE);

    page_gyro_apply_ball_norm(0.0f, 0.0f);
    s_ball_timer = lv_timer_create(page_gyro_ball_timer_cb, 20, NULL);

    return s_screen;
}

void page_gyro_set_ball_norm(float x_norm, float y_norm)
{
    s_pending_x_norm = clamp_norm(x_norm);
    s_pending_y_norm = clamp_norm(y_norm);
    s_ball_pending = true;
}

static void page_gyro_on_destroy(void)
{
    if (s_ball_timer != NULL) {
        lv_timer_del(s_ball_timer);
        s_ball_timer = NULL;
    }
    s_screen = NULL;
    s_ball = NULL;
    s_ball_pending = false;
}

const ui_page_t g_page_gyro = {
    .id = UI_PAGE_GYRO,
    .name = "GYRO",
    .cache_mode = UI_PAGE_CACHE_NONE,
    .create = page_gyro_create,
    .on_show = NULL,
    .on_hide = NULL,
    .on_destroy = page_gyro_on_destroy,
};
