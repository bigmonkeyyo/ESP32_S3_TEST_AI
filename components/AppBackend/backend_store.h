#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "app_backend_types.h"

esp_err_t backend_store_init(void);
esp_err_t backend_store_get_snapshot(app_backend_snapshot_t *out);
void backend_store_set_wifi_state(app_backend_wifi_state_t state, const char *message);
void backend_store_set_ap_list(const app_backend_wifi_ap_t *aps, size_t count);
void backend_store_set_scan_results(const app_backend_wifi_ap_t *aps, size_t count, const char *message);
void backend_store_set_connected(const char *ssid, const char *ip, int8_t rssi);
void backend_store_set_disconnected(const char *message);
void backend_store_set_time(const char *now_time, const char *uptime, const char *last_sync);
void backend_store_set_weather(const app_backend_weather_t *weather, const char *message);
void backend_store_set_ota(app_backend_ota_state_t state,
                           const char *current_version,
                           const char *target_version,
                           int progress,
                           const char *message);
void backend_store_set_message(const char *message);
