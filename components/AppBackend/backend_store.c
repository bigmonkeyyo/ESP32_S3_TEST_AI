#include "backend_store.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "APP_BACKEND_STORE";

static SemaphoreHandle_t s_mutex;
static app_backend_snapshot_t s_snapshot;

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0)) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    strlcpy(dst, src, dst_size);
}

static void snapshot_set_defaults(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    const char *version = (app_desc != NULL && app_desc->version[0] != '\0') ? app_desc->version : "--";

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.wifi_state = APP_BACKEND_WIFI_IDLE;
    s_snapshot.ble_enabled = false;
    s_snapshot.rssi = 0;
    copy_text(s_snapshot.ip, sizeof(s_snapshot.ip), "--");
    copy_text(s_snapshot.now_time, sizeof(s_snapshot.now_time), "--:--");
    copy_text(s_snapshot.uptime, sizeof(s_snapshot.uptime), "00:00:00");
    copy_text(s_snapshot.last_sync, sizeof(s_snapshot.last_sync), "--:--");
    copy_text(s_snapshot.message, sizeof(s_snapshot.message), "WiFi 未连接");
    copy_text(s_snapshot.weather.city, sizeof(s_snapshot.weather.city), "上海");
    copy_text(s_snapshot.weather.condition, sizeof(s_snapshot.weather.condition), "--");
    copy_text(s_snapshot.weather.temperature, sizeof(s_snapshot.weather.temperature), "--");
    copy_text(s_snapshot.weather.humidity, sizeof(s_snapshot.weather.humidity), "--");
    copy_text(s_snapshot.weather.wind, sizeof(s_snapshot.weather.wind), "--");
    copy_text(s_snapshot.weather.aqi, sizeof(s_snapshot.weather.aqi), "AQI --");
    copy_text(s_snapshot.weather.updated_at, sizeof(s_snapshot.weather.updated_at), "--:--");
    s_snapshot.ota.state = APP_BACKEND_OTA_IDLE;
    s_snapshot.ota.progress = 0;
    copy_text(s_snapshot.ota.current_version, sizeof(s_snapshot.ota.current_version), version);
    copy_text(s_snapshot.ota.target_version, sizeof(s_snapshot.ota.target_version), "--");
    copy_text(s_snapshot.ota.message, sizeof(s_snapshot.ota.message), "未检测");
}

esp_err_t backend_store_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        snapshot_set_defaults();
        xSemaphoreGive(s_mutex);
    }

    return ESP_OK;
}

esp_err_t backend_store_get_snapshot(app_backend_snapshot_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(backend_store_init(), TAG, "store init failed");

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *out = s_snapshot;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

void backend_store_set_wifi_state(app_backend_wifi_state_t state, const char *message)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.wifi_state = state;
        if (message != NULL) {
            copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_ble_enabled(bool enabled, const char *message)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.ble_enabled = enabled;
        if (message != NULL) {
            copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_ap_list(const app_backend_wifi_ap_t *aps, size_t count)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        const size_t capped = (count > APP_BACKEND_MAX_APS) ? APP_BACKEND_MAX_APS : count;
        memset(s_snapshot.aps, 0, sizeof(s_snapshot.aps));
        if ((aps != NULL) && (capped > 0)) {
            memcpy(s_snapshot.aps, aps, capped * sizeof(s_snapshot.aps[0]));
        }
        s_snapshot.ap_count = capped;
        s_snapshot.wifi_state = APP_BACKEND_WIFI_IDLE;
        snprintf(s_snapshot.message, sizeof(s_snapshot.message), "扫描完成：找到 %u 个网络", (unsigned)capped);
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_scan_results(const app_backend_wifi_ap_t *aps, size_t count, const char *message)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        const size_t capped = (count > APP_BACKEND_MAX_APS) ? APP_BACKEND_MAX_APS : count;
        memset(s_snapshot.aps, 0, sizeof(s_snapshot.aps));
        if ((aps != NULL) && (capped > 0)) {
            memcpy(s_snapshot.aps, aps, capped * sizeof(s_snapshot.aps[0]));
        }
        s_snapshot.ap_count = capped;
        s_snapshot.wifi_state = APP_BACKEND_WIFI_IDLE;
        if (message != NULL) {
            copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        } else {
            snprintf(s_snapshot.message, sizeof(s_snapshot.message), "扫描完成：找到 %u 个网络", (unsigned)capped);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_connected(const char *ssid, const char *ip, int8_t rssi)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.wifi_state = APP_BACKEND_WIFI_CONNECTED;
        copy_text(s_snapshot.connected_ssid, sizeof(s_snapshot.connected_ssid), ssid);
        copy_text(s_snapshot.ip, sizeof(s_snapshot.ip), ip);
        s_snapshot.rssi = rssi;
        copy_text(s_snapshot.message, sizeof(s_snapshot.message), "连接成功：WiFi 已连接");
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_disconnected(const char *message)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.wifi_state = APP_BACKEND_WIFI_DISCONNECTED;
        s_snapshot.connected_ssid[0] = '\0';
        copy_text(s_snapshot.ip, sizeof(s_snapshot.ip), "--");
        s_snapshot.rssi = 0;
        copy_text(s_snapshot.message, sizeof(s_snapshot.message), message != NULL ? message : "WiFi 已断开");
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_time(const char *now_time, const char *uptime, const char *last_sync)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (now_time != NULL) {
            copy_text(s_snapshot.now_time, sizeof(s_snapshot.now_time), now_time);
        }
        if (uptime != NULL) {
            copy_text(s_snapshot.uptime, sizeof(s_snapshot.uptime), uptime);
        }
        if (last_sync != NULL) {
            copy_text(s_snapshot.last_sync, sizeof(s_snapshot.last_sync), last_sync);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_weather(const app_backend_weather_t *weather, const char *message)
{
    if ((weather == NULL) || (backend_store_init() != ESP_OK)) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.weather = *weather;
        if (message != NULL) {
            copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_ota(app_backend_ota_state_t state,
                           const char *current_version,
                           const char *target_version,
                           int progress,
                           const char *message)
{
    if (backend_store_init() != ESP_OK) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_snapshot.ota.state = state;
        s_snapshot.ota.progress = progress;
        if (current_version != NULL) {
            copy_text(s_snapshot.ota.current_version,
                      sizeof(s_snapshot.ota.current_version),
                      current_version);
        }
        if (target_version != NULL) {
            copy_text(s_snapshot.ota.target_version,
                      sizeof(s_snapshot.ota.target_version),
                      target_version);
        }
        if (message != NULL) {
            copy_text(s_snapshot.ota.message, sizeof(s_snapshot.ota.message), message);
            copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        }
        xSemaphoreGive(s_mutex);
    }
}

void backend_store_set_message(const char *message)
{
    if ((message == NULL) || (backend_store_init() != ESP_OK)) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        copy_text(s_snapshot.message, sizeof(s_snapshot.message), message);
        xSemaphoreGive(s_mutex);
    }
}
