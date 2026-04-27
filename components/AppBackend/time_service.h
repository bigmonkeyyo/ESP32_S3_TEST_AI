#pragma once

#include <stddef.h>

#include "esp_err.h"

esp_err_t time_service_init(void);
esp_err_t time_service_sync(char *now_time, size_t now_time_size, char *last_sync, size_t last_sync_size);
void time_service_format_current(char *out, size_t out_size);
void time_service_format_uptime(char *out, size_t out_size);
