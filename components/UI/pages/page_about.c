#include "page_about.h"

#include "ui_page_manager.h"

static lv_obj_t *s_screen = NULL;

static void page_about_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    (void)ui_page_pop(UI_ANIM_MOVE_RIGHT);
}

static lv_obj_t *page_about_create(void)
{
    lv_obj_t *title = NULL;
    lv_obj_t *desc = NULL;
    lv_obj_t *btn_back = NULL;
    lv_obj_t *btn_label = NULL;

    s_screen = lv_obj_create(NULL);

    title = lv_label_create(s_screen);
    lv_label_set_text(title, "About");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    desc = lv_label_create(s_screen);
    lv_label_set_text(desc, "About page sample");
    lv_obj_align(desc, LV_ALIGN_CENTER, 0, -20);

    btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 140, 46);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_add_event_cb(btn_back, page_about_back_cb, LV_EVENT_CLICKED, NULL);

    btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "Back");
    lv_obj_center(btn_label);

    return s_screen;
}

static void page_about_on_destroy(void)
{
    s_screen = NULL;
}

const ui_page_t g_page_about = {
    .id = UI_PAGE_ABOUT,
    .name = "ABOUT",
    .cache_mode = UI_PAGE_CACHE_NONE,
    .create = page_about_create,
    .on_show = NULL,
    .on_hide = NULL,
    .on_destroy = page_about_on_destroy,
};
