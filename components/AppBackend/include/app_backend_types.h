#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_BACKEND_MAX_APS              16
#define APP_BACKEND_SSID_MAX_LEN         32
#define APP_BACKEND_PASSWORD_MAX_LEN     64
#define APP_BACKEND_IP_MAX_LEN           16
#define APP_BACKEND_TIME_MAX_LEN         16
#define APP_BACKEND_UPTIME_MAX_LEN       16
#define APP_BACKEND_WEATHER_CITY_LEN     16
#define APP_BACKEND_WEATHER_TEXT_LEN     24
#define APP_BACKEND_WEATHER_VALUE_LEN    16
#define APP_BACKEND_WEATHER_WIND_LEN     24
#define APP_BACKEND_MESSAGE_MAX_LEN      64
#define APP_BACKEND_OTA_VERSION_LEN      24
#define APP_BACKEND_OTA_MESSAGE_LEN      72

typedef enum {
    APP_BACKEND_WIFI_IDLE = 0,
    APP_BACKEND_WIFI_SCANNING,
    APP_BACKEND_WIFI_CONNECTING,
    APP_BACKEND_WIFI_CONNECTED,
    APP_BACKEND_WIFI_FAILED,
    APP_BACKEND_WIFI_DISCONNECTED,
} app_backend_wifi_state_t;

typedef struct {
    char ssid[APP_BACKEND_SSID_MAX_LEN + 1];
    int8_t rssi;
    uint8_t authmode;
    bool selected;
} app_backend_wifi_ap_t;

typedef struct {
    char city[APP_BACKEND_WEATHER_CITY_LEN];
    char condition[APP_BACKEND_WEATHER_TEXT_LEN];
    char temperature[APP_BACKEND_WEATHER_VALUE_LEN];
    char humidity[APP_BACKEND_WEATHER_VALUE_LEN];
    char wind[APP_BACKEND_WEATHER_WIND_LEN];
    char aqi[APP_BACKEND_WEATHER_VALUE_LEN];
    char updated_at[APP_BACKEND_TIME_MAX_LEN];
    bool valid;
} app_backend_weather_t;

typedef enum {
    APP_BACKEND_OTA_IDLE = 0,
    APP_BACKEND_OTA_CHECKING,
    APP_BACKEND_OTA_READY,
    APP_BACKEND_OTA_DOWNLOADING,
    APP_BACKEND_OTA_APPLYING,
    APP_BACKEND_OTA_DONE,
    APP_BACKEND_OTA_FAILED,
} app_backend_ota_state_t;

typedef struct {
    app_backend_ota_state_t state;
    char current_version[APP_BACKEND_OTA_VERSION_LEN];
    char target_version[APP_BACKEND_OTA_VERSION_LEN];
    int progress;
    char message[APP_BACKEND_OTA_MESSAGE_LEN];
} app_backend_ota_t;

typedef struct {
    app_backend_wifi_state_t wifi_state;
    bool ble_enabled;
    app_backend_wifi_ap_t aps[APP_BACKEND_MAX_APS];
    size_t ap_count;
    char connected_ssid[APP_BACKEND_SSID_MAX_LEN + 1];
    char ip[APP_BACKEND_IP_MAX_LEN];
    int8_t rssi;
    char now_time[APP_BACKEND_TIME_MAX_LEN];
    char uptime[APP_BACKEND_UPTIME_MAX_LEN];
    char last_sync[APP_BACKEND_TIME_MAX_LEN];
    app_backend_weather_t weather;
    app_backend_ota_t ota;
    char message[APP_BACKEND_MESSAGE_MAX_LEN];
} app_backend_snapshot_t;
