#pragma once

#include "esp_err.h"

typedef enum {
    APP_BACKEND_CMD_WIFI_SCAN = 0,
    APP_BACKEND_CMD_WIFI_CONNECT,
    APP_BACKEND_CMD_WIFI_DISCONNECT,
    APP_BACKEND_CMD_WEATHER_REFRESH,
    APP_BACKEND_CMD_IP_READY,
    APP_BACKEND_CMD_OTA_CHECK,
    APP_BACKEND_CMD_OTA_CONFIRM,
    APP_BACKEND_CMD_OTA_RESTART,
} app_backend_cmd_id_t;

void app_backend_notify_changed(void);
esp_err_t app_backend_post_simple(app_backend_cmd_id_t id);
