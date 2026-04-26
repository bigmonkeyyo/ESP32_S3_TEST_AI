#include "ui_page_anim.h"

lv_scr_load_anim_t ui_page_anim_to_lv(ui_anim_t anim)
{
    switch (anim) {
    case UI_ANIM_MOVE_LEFT:
        return LV_SCR_LOAD_ANIM_MOVE_LEFT;
    case UI_ANIM_MOVE_RIGHT:
        return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    case UI_ANIM_FADE:
        return LV_SCR_LOAD_ANIM_FADE_ON;
    case UI_ANIM_NONE:
    default:
        return LV_SCR_LOAD_ANIM_NONE;
    }
}

void ui_page_anim_load(lv_obj_t *screen, ui_anim_t anim)
{
    if (anim == UI_ANIM_NONE) {
        /* Ensure "none" is truly immediate to avoid overlapping scr_load animations. */
        lv_scr_load(screen);
        return;
    }

    lv_scr_load_anim(screen, ui_page_anim_to_lv(anim), 220, 0, false);
}
