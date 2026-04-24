#include "page_home.h"

#include "ui_page_manager.h"

static lv_obj_t *s_screen = NULL;

static void page_home_open_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    (void)ui_page_push(UI_PAGE_SETTINGS, NULL, UI_ANIM_MOVE_LEFT);
}

static void page_home_open_about_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    (void)ui_page_push(UI_PAGE_ABOUT, NULL, UI_ANIM_MOVE_LEFT);
}

static lv_obj_t *page_home_create(void)
{
    lv_obj_t *title = NULL;
    lv_obj_t *btn_settings = NULL;
    lv_obj_t *btn_about = NULL;
    lv_obj_t *btn_label = NULL;

    s_screen = lv_obj_create(NULL);

    title = lv_label_create(s_screen);
    lv_label_set_text(title, "Home");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    btn_settings = lv_btn_create(s_screen);
    lv_obj_set_size(btn_settings, 180, 48);
    lv_obj_align(btn_settings, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_event_cb(btn_settings, page_home_open_settings_cb, LV_EVENT_CLICKED, NULL);

    btn_label = lv_label_create(btn_settings);
    lv_label_set_text(btn_label, "Go To Settings");
    lv_obj_center(btn_label);

    btn_about = lv_btn_create(s_screen);
    lv_obj_set_size(btn_about, 180, 48);
    lv_obj_align(btn_about, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(btn_about, page_home_open_about_cb, LV_EVENT_CLICKED, NULL);

    btn_label = lv_label_create(btn_about);
    lv_label_set_text(btn_label, "Go To About");
    lv_obj_center(btn_label);

    return s_screen;
}

static void page_home_on_destroy(void)
{
    s_screen = NULL;
}

const ui_page_t g_page_home = {
    .id = UI_PAGE_HOME,
    .name = "HOME",
    .cache_mode = UI_PAGE_CACHE_KEEP,
    .create = page_home_create,
    .on_show = NULL,
    .on_hide = NULL,
    .on_destroy = page_home_on_destroy,
};
