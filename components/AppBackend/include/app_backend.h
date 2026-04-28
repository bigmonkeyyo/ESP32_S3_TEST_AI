#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "app_backend_types.h"

typedef void (*app_backend_update_cb_t)(void *user_ctx);

esp_err_t app_backend_start(void);
esp_err_t app_backend_register_update_callback(app_backend_update_cb_t cb, void *user_ctx);
esp_err_t app_backend_wifi_scan_async(void);
esp_err_t app_backend_wifi_connect_async(const char *ssid, const char *password);
esp_err_t app_backend_wifi_disconnect_async(bool forget_saved);
esp_err_t app_backend_weather_refresh_async(void);
esp_err_t app_backend_ota_confirm_async(void);
esp_err_t app_backend_get_snapshot(app_backend_snapshot_t *out);
