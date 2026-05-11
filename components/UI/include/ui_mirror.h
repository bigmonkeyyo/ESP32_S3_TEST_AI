#pragma once

#include "esp_err.h"

esp_err_t ui_mirror_init(void);
void ui_mirror_notify_page_changed(void);
void ui_mirror_notify_backend_changed(void);
void ui_mirror_process_pending(void);
