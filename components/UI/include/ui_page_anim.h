#pragma once

#include "lvgl.h"
#include "ui_types.h"

lv_scr_load_anim_t ui_page_anim_to_lv(ui_anim_t anim);
void ui_page_anim_load(lv_obj_t *screen, ui_anim_t anim);
