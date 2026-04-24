#pragma once

#include "esp_err.h"
#include "ui_page_ids.h"

esp_err_t ui_page_stack_reset(void);
esp_err_t ui_page_stack_push(ui_page_id_t id);
esp_err_t ui_page_stack_pop(ui_page_id_t *out_id);
esp_err_t ui_page_stack_peek(ui_page_id_t *out_id);
int ui_page_stack_depth(void);
