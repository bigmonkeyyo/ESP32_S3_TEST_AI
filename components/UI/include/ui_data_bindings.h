#pragma once

#include "esp_err.h"

esp_err_t ui_data_bindings_init(void);
void ui_data_bindings_notify_async(void);
void ui_data_bindings_process_pending(void);
