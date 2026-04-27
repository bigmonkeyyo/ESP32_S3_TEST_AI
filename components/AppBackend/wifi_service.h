#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t wifi_service_init(void);
esp_err_t wifi_service_scan(void);
esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_disconnect(bool forget_saved);
esp_err_t wifi_service_connect_saved(void);
