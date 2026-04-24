#pragma once

#include "esp_err.h"
#include "ui_page_ids.h"

esp_err_t ui_init(void);
ui_page_id_t ui_page_current(void);
