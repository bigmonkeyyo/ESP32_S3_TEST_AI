#pragma once

#include "esp_err.h"
#include "ui_page_ids.h"
#include "ui_types.h"

esp_err_t ui_page_push(ui_page_id_t id, void *args, ui_anim_t anim);
esp_err_t ui_page_pop(ui_anim_t anim);
esp_err_t ui_page_replace(ui_page_id_t id, void *args, ui_anim_t anim);
esp_err_t ui_page_back_to_root(ui_anim_t anim);
ui_page_id_t ui_page_current(void);
void ui_page_dump_stack(void);
