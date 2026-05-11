#include "app_backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "app_backend_internal.h"
#include "backend_store.h"
#include "mqtt_service.h"
#include "ota_service.h"
#include "time_service.h"
#include "weather_service.h"
#include "wifi_service.h"
#include "gyro_ble.h"

#define APP_BACKEND_QUEUE_LEN      8
#define APP_BACKEND_TASK_STACK     8192
#define APP_BACKEND_TASK_PRIORITY  5
#define APP_BACKEND_ONLINE_TASK_STACK 8192
#define APP_BACKEND_ONLINE_TASK_PRIORITY 4
#define APP_BACKEND_ONLINE_RETRY_MS 30000
#define APP_BACKEND_IP_READY_SETTLE_MS 1500
#define APP_BACKEND_OTA_CHECK_DEBOUNCE_MS 1200
#define APP_BACKEND_UPDATE_CB_MAX 4

typedef struct {
    app_backend_cmd_id_t id;
    char ssid[APP_BACKEND_SSID_MAX_LEN + 1];
    char password[APP_BACKEND_PASSWORD_MAX_LEN + 1];
    bool forget_saved;
    bool ble_enabled;
} app_backend_cmd_t;

static const char *TAG = "APP_BACKEND";

static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static bool s_started;
static app_backend_update_cb_t s_update_cbs[APP_BACKEND_UPDATE_CB_MAX];
static void *s_update_user_ctxs[APP_BACKEND_UPDATE_CB_MAX];
static volatile bool s_online_retry_pending;
static volatile TickType_t s_next_online_retry_tick;
static volatile bool s_ip_ready_task_running;
static bool s_ble_enabled;
static TickType_t s_last_ota_check_post_tick;

static void app_backend_log_heap(const char *label)
{
    ESP_LOGI(TAG, "heap %s: internal free=%u largest=%u psram free=%u largest=%u",
             label,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

static void app_backend_schedule_online_retry(uint32_t delay_ms)
{
    s_online_retry_pending = true;
    s_next_online_retry_tick = xTaskGetTickCount() + pdMS_TO_TICKS(delay_ms);
}

static bool app_backend_wifi_is_connected(void)
{
    app_backend_snapshot_t snapshot = {0};

    if (backend_store_get_snapshot(&snapshot) != ESP_OK) {
        return false;
    }

    return snapshot.wifi_state == APP_BACKEND_WIFI_CONNECTED;
}

static void app_backend_update_time_snapshot(void)
{
    char now_time[APP_BACKEND_TIME_MAX_LEN] = {0};
    char uptime[APP_BACKEND_UPTIME_MAX_LEN] = {0};

    time_service_format_current(now_time, sizeof(now_time));
    time_service_format_uptime(uptime, sizeof(uptime));
    backend_store_set_time(now_time, uptime, NULL);
}

static void app_backend_prepare_ble_stack(void)
{
    esp_err_t err;

    if (s_ble_enabled) {
        return;
    }

    app_backend_log_heap("before_ble_prepare");
    (void)gyro_ble_set_streaming_enabled(false);
    (void)gyro_ble_set_advertising_enabled(false);
    err = gyro_ble_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BLE stack prepare failed: %s", esp_err_to_name(err));
        app_backend_log_heap("ble_prepare_failed");
        return;
    }

    ESP_LOGI(TAG, "BLE stack prepared");
    app_backend_log_heap("after_ble_prepare");
}

static void app_backend_refresh_weather_with_diag(void)
{
    app_backend_weather_t weather = {0};

    if (weather_service_fetch_shanghai(&weather) == ESP_OK) {
        backend_store_set_weather(&weather, "天气同步完成");
        s_online_retry_pending = false;
    } else {
        backend_store_set_message("天气同步失败");
        app_backend_schedule_online_retry(APP_BACKEND_ONLINE_RETRY_MS);
    }
    app_backend_notify_changed();
}

static void app_backend_handle_ip_ready(void)
{
    char now_time[APP_BACKEND_TIME_MAX_LEN] = {0};
    char uptime[APP_BACKEND_UPTIME_MAX_LEN] = {0};
    char last_sync[APP_BACKEND_TIME_MAX_LEN] = {0};

    app_backend_prepare_ble_stack();
    vTaskDelay(pdMS_TO_TICKS(APP_BACKEND_IP_READY_SETTLE_MS));
    time_service_format_uptime(uptime, sizeof(uptime));

    app_backend_log_heap("before_mqtt_start");
    (void)mqtt_service_start();
    app_backend_log_heap("after_mqtt_start");

    if (time_service_sync(now_time, sizeof(now_time), last_sync, sizeof(last_sync)) == ESP_OK) {
        backend_store_set_time(now_time, uptime, last_sync);
        app_backend_notify_changed();
    } else {
        backend_store_set_time(NULL, uptime, "未同步");
        app_backend_notify_changed();
        app_backend_schedule_online_retry(APP_BACKEND_ONLINE_RETRY_MS);
    }

    esp_err_t ota_err = ota_service_report_current_version();
    if (ota_err != ESP_OK) {
        ESP_LOGW(TAG, "OTA version report after IP ready failed: %s", esp_err_to_name(ota_err));
    }

    app_backend_refresh_weather_with_diag();
}

static void app_backend_ip_ready_task(void *arg)
{
    (void)arg;
    app_backend_handle_ip_ready();
    s_ip_ready_task_running = false;
    vTaskDelete(NULL);
}

static esp_err_t app_backend_start_ip_ready_task(void)
{
    if (s_ip_ready_task_running) {
        ESP_LOGI(TAG, "online sync already running");
        return ESP_OK;
    }

    s_ip_ready_task_running = true;
    if (xTaskCreate(app_backend_ip_ready_task, "app_online_sync",
                    APP_BACKEND_ONLINE_TASK_STACK, NULL,
                    APP_BACKEND_ONLINE_TASK_PRIORITY, NULL) != pdPASS) {
        s_ip_ready_task_running = false;
        ESP_LOGE(TAG, "online sync task create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "online sync task started");
    return ESP_OK;
}

static void app_backend_set_ble_enabled(bool enabled)
{
    if (enabled == s_ble_enabled) {
        return;
    }

    if (enabled) {
        esp_err_t err = gyro_ble_start();
        if (err != ESP_OK) {
            backend_store_set_message("BLE start failed");
            app_backend_notify_changed();
            ESP_LOGE(TAG, "gyro_ble_start failed: %s", esp_err_to_name(err));
            return;
        }
        (void)gyro_ble_set_streaming_enabled(true);
        (void)gyro_ble_set_advertising_enabled(true);
        s_ble_enabled = true;
        backend_store_set_ble_enabled(true, "BLE enabled");
    } else {
        (void)gyro_ble_set_streaming_enabled(false);
        (void)gyro_ble_set_advertising_enabled(false);
        s_ble_enabled = false;
        backend_store_set_ble_enabled(false, "BLE disabled");
    }

    app_backend_notify_changed();
}

static void app_backend_task(void *arg)
{
    app_backend_cmd_t cmd = {0};
    TickType_t last_time_update = xTaskGetTickCount();

    (void)arg;
    (void)wifi_service_connect_saved();

    while (1) {
        if (xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (cmd.id) {
            case APP_BACKEND_CMD_WIFI_SCAN:
                (void)wifi_service_scan();
                break;
            case APP_BACKEND_CMD_WIFI_CONNECT:
                (void)wifi_service_connect(cmd.ssid, cmd.password);
                break;
            case APP_BACKEND_CMD_WIFI_DISCONNECT:
                (void)wifi_service_disconnect(cmd.forget_saved);
                break;
            case APP_BACKEND_CMD_WEATHER_REFRESH:
                app_backend_refresh_weather_with_diag();
                break;
            case APP_BACKEND_CMD_IP_READY:
                (void)app_backend_start_ip_ready_task();
                break;
            case APP_BACKEND_CMD_OTA_CHECK:
                (void)ota_service_check_update();
                break;
            case APP_BACKEND_CMD_OTA_CONFIRM:
                (void)ota_service_confirm_update();
                break;
            case APP_BACKEND_CMD_OTA_RESTART:
                (void)ota_service_restart_now();
                break;
            case APP_BACKEND_CMD_BLE_SET_ENABLED:
                app_backend_set_ble_enabled(cmd.ble_enabled);
                break;
            default:
                ESP_LOGW(TAG, "unknown cmd=%d", (int)cmd.id);
                break;
            }
        }

        if ((xTaskGetTickCount() - last_time_update) >= pdMS_TO_TICKS(30000)) {
            last_time_update = xTaskGetTickCount();
            app_backend_update_time_snapshot();
            app_backend_notify_changed();
        }

        if (s_online_retry_pending &&
            ((int32_t)(xTaskGetTickCount() - s_next_online_retry_tick) >= 0)) {
            s_online_retry_pending = false;
            if (app_backend_wifi_is_connected()) {
                ESP_LOGI(TAG, "online sync retry");
                (void)app_backend_start_ip_ready_task();
            }
        }
    }
}

static esp_err_t app_backend_post_cmd(const app_backend_cmd_t *cmd)
{
    if ((cmd == NULL) || (s_queue == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(s_queue, cmd, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void app_backend_notify_changed(void)
{
    for (size_t i = 0; i < APP_BACKEND_UPDATE_CB_MAX; ++i) {
        if (s_update_cbs[i] != NULL) {
            s_update_cbs[i](s_update_user_ctxs[i]);
        }
    }
}

esp_err_t app_backend_post_simple(app_backend_cmd_id_t id)
{
    app_backend_cmd_t cmd = {
        .id = id,
    };
    return app_backend_post_cmd(&cmd);
}

esp_err_t app_backend_register_update_callback(app_backend_update_cb_t cb, void *user_ctx)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < APP_BACKEND_UPDATE_CB_MAX; ++i) {
        if (s_update_cbs[i] == cb && s_update_user_ctxs[i] == user_ctx) {
            return ESP_OK;
        }
    }

    for (size_t i = 0; i < APP_BACKEND_UPDATE_CB_MAX; ++i) {
        if (s_update_cbs[i] == NULL) {
            s_update_cbs[i] = cb;
            s_update_user_ctxs[i] = user_ctx;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

esp_err_t app_backend_start(void)
{
    esp_err_t err;

    if (s_started) {
        return ESP_OK;
    }

    err = backend_store_init();
    if (err != ESP_OK) {
        return err;
    }
    err = time_service_init();
    if (err != ESP_OK) {
        return err;
    }

    s_queue = xQueueCreate(APP_BACKEND_QUEUE_LEN, sizeof(app_backend_cmd_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(app_backend_task, "app_backend", APP_BACKEND_TASK_STACK, NULL,
                    APP_BACKEND_TASK_PRIORITY, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    s_ble_enabled = false;
    backend_store_set_ble_enabled(false, "BLE disabled");
    app_backend_update_time_snapshot();
    ESP_LOGI(TAG, "backend started");
    ESP_LOGI(TAG, "APP_BACKEND_SELFTEST: store PASS");
    app_backend_notify_changed();
    return ESP_OK;
}

esp_err_t app_backend_wifi_scan_async(void)
{
    return app_backend_post_simple(APP_BACKEND_CMD_WIFI_SCAN);
}

esp_err_t app_backend_wifi_connect_async(const char *ssid, const char *password)
{
    app_backend_cmd_t cmd = {
        .id = APP_BACKEND_CMD_WIFI_CONNECT,
    };

    if ((ssid == NULL) || (ssid[0] == '\0')) {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(cmd.ssid, ssid, sizeof(cmd.ssid));
    strlcpy(cmd.password, password != NULL ? password : "", sizeof(cmd.password));
    return app_backend_post_cmd(&cmd);
}

esp_err_t app_backend_wifi_disconnect_async(bool forget_saved)
{
    app_backend_cmd_t cmd = {
        .id = APP_BACKEND_CMD_WIFI_DISCONNECT,
        .forget_saved = forget_saved,
    };
    return app_backend_post_cmd(&cmd);
}

esp_err_t app_backend_weather_refresh_async(void)
{
    return app_backend_post_simple(APP_BACKEND_CMD_WEATHER_REFRESH);
}

esp_err_t app_backend_ota_check_async(void)
{
    TickType_t now = xTaskGetTickCount();

    if ((s_last_ota_check_post_tick != 0) &&
        ((int32_t)(now - s_last_ota_check_post_tick) < pdMS_TO_TICKS(APP_BACKEND_OTA_CHECK_DEBOUNCE_MS))) {
        return ESP_OK;
    }

    s_last_ota_check_post_tick = now;
    return app_backend_post_simple(APP_BACKEND_CMD_OTA_CHECK);
}

esp_err_t app_backend_ota_confirm_async(void)
{
    return app_backend_post_simple(APP_BACKEND_CMD_OTA_CONFIRM);
}

esp_err_t app_backend_ota_restart_async(void)
{
    return app_backend_post_simple(APP_BACKEND_CMD_OTA_RESTART);
}

esp_err_t app_backend_ble_set_enabled_async(bool enabled)
{
    app_backend_cmd_t cmd = {
        .id = APP_BACKEND_CMD_BLE_SET_ENABLED,
        .ble_enabled = enabled,
    };
    return app_backend_post_cmd(&cmd);
}

bool app_backend_ble_is_enabled(void)
{
    return s_ble_enabled;
}

esp_err_t app_backend_get_snapshot(app_backend_snapshot_t *out)
{
    return backend_store_get_snapshot(out);
}
