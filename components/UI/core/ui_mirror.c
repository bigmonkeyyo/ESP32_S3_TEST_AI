#include "ui_mirror.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_backend.h"
#include "app_backend_types.h"
#include "gyro_ble.h"
#include "ui_page_manager.h"

#define UI_MIRROR_JSON_MAX      1024
#define UI_MIRROR_PERIODIC_US   1000000LL

static const char *TAG = "UI_MIRROR";
static volatile bool s_update_pending;
static bool s_initialized;
static int64_t s_last_publish_us;
static app_backend_snapshot_t s_publish_snapshot;
static char s_publish_json[UI_MIRROR_JSON_MAX];

static const char *ui_mirror_page_name(ui_page_id_t page)
{
    switch (page) {
    case UI_PAGE_HOME:
        return "home";
    case UI_PAGE_SETTINGS:
        return "settings";
    case UI_PAGE_STATUS:
        return "status";
    case UI_PAGE_FIRMWARE:
        return "firmware";
    case UI_PAGE_GYRO:
        return "gyro";
    case UI_PAGE_WIFI:
        return "wifi";
    case UI_PAGE_ABOUT:
        return "about";
    default:
        return "unknown";
    }
}

static const char *ui_mirror_wifi_state_name(app_backend_wifi_state_t state)
{
    switch (state) {
    case APP_BACKEND_WIFI_IDLE:
        return "idle";
    case APP_BACKEND_WIFI_SCANNING:
        return "scanning";
    case APP_BACKEND_WIFI_CONNECTING:
        return "connecting";
    case APP_BACKEND_WIFI_CONNECTED:
        return "connected";
    case APP_BACKEND_WIFI_FAILED:
        return "failed";
    case APP_BACKEND_WIFI_DISCONNECTED:
        return "disconnected";
    default:
        return "unknown";
    }
}

static const char *ui_mirror_ota_state_name(app_backend_ota_state_t state)
{
    switch (state) {
    case APP_BACKEND_OTA_IDLE:
        return "idle";
    case APP_BACKEND_OTA_CHECKING:
        return "checking";
    case APP_BACKEND_OTA_READY:
        return "ready";
    case APP_BACKEND_OTA_DOWNLOADING:
        return "downloading";
    case APP_BACKEND_OTA_APPLYING:
        return "applying";
    case APP_BACKEND_OTA_DONE:
        return "done";
    case APP_BACKEND_OTA_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

static size_t ui_mirror_json_append(char *dst, size_t dst_size, size_t pos, const char *text)
{
    int written;

    if ((dst == NULL) || (dst_size == 0U) || (pos >= dst_size)) {
        return pos;
    }

    written = snprintf(&dst[pos], dst_size - pos, "%s", text != NULL ? text : "");
    if (written < 0) {
        return pos;
    }
    if ((size_t)written >= (dst_size - pos)) {
        return dst_size - 1U;
    }
    return pos + (size_t)written;
}

static size_t ui_mirror_json_append_escaped(char *dst, size_t dst_size, size_t pos, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");

    if ((dst == NULL) || (dst_size == 0U) || (pos >= dst_size)) {
        return pos;
    }

    for (; *p != '\0' && pos < (dst_size - 1U); ++p) {
        if (*p == '"' || *p == '\\') {
            if (pos + 2U >= dst_size) {
                break;
            }
            dst[pos++] = '\\';
            dst[pos++] = (char)*p;
        } else if (*p < 0x20U) {
            if (pos + 7U >= dst_size) {
                break;
            }
            int written = snprintf(&dst[pos], dst_size - pos, "\\u%04x", (unsigned)*p);
            if (written < 0) {
                break;
            }
            pos += (size_t)written;
        } else {
            dst[pos++] = (char)*p;
        }
    }
    dst[pos] = '\0';
    return pos;
}

static size_t ui_mirror_json_append_string_field(char *dst,
                                                 size_t dst_size,
                                                 size_t pos,
                                                 const char *key,
                                                 const char *value,
                                                 bool comma)
{
    pos = ui_mirror_json_append(dst, dst_size, pos, comma ? ",\"" : "\"");
    pos = ui_mirror_json_append(dst, dst_size, pos, key);
    pos = ui_mirror_json_append(dst, dst_size, pos, "\":\"");
    pos = ui_mirror_json_append_escaped(dst, dst_size, pos, value);
    pos = ui_mirror_json_append(dst, dst_size, pos, "\"");
    return pos;
}

static size_t ui_mirror_json_append_int(char *dst, size_t dst_size, size_t pos, int value)
{
    int written;

    if ((dst == NULL) || (dst_size == 0U) || (pos >= dst_size)) {
        return pos;
    }

    written = snprintf(&dst[pos], dst_size - pos, "%d", value);
    if (written < 0) {
        return pos;
    }
    if ((size_t)written >= (dst_size - pos)) {
        return dst_size - 1U;
    }
    return pos + (size_t)written;
}

static void ui_mirror_build_json(const app_backend_snapshot_t *snapshot,
                                 ui_page_id_t page,
                                 char *out,
                                 size_t out_size)
{
    size_t pos = 0;

    if ((snapshot == NULL) || (out == NULL) || (out_size == 0U)) {
        return;
    }

    pos = ui_mirror_json_append(out, out_size, pos, "{");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "page", ui_mirror_page_name(page), false);
    pos = ui_mirror_json_append(out, out_size, pos, ",\"wifi\":{");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "state",
                                             ui_mirror_wifi_state_name(snapshot->wifi_state), false);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "ssid",
                                             snapshot->connected_ssid, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "ip", snapshot->ip, true);
    pos = ui_mirror_json_append(out, out_size, pos, ",\"rssi\":");
    pos = ui_mirror_json_append_int(out, out_size, pos, (int)snapshot->rssi);
    pos = ui_mirror_json_append(out, out_size, pos, "},\"ble\":{");
    pos = ui_mirror_json_append(out, out_size, pos, snapshot->ble_enabled ? "\"enabled\":true" : "\"enabled\":false");
    pos = ui_mirror_json_append(out, out_size, pos, "},\"time\":{");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "now", snapshot->now_time, false);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "uptime", snapshot->uptime, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "lastSync", snapshot->last_sync, true);
    pos = ui_mirror_json_append(out, out_size, pos, "},\"weather\":{");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "city", snapshot->weather.city, false);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "condition", snapshot->weather.condition, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "temp", snapshot->weather.temperature, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "humidity", snapshot->weather.humidity, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "wind", snapshot->weather.wind, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "aqi", snapshot->weather.aqi, true);
    pos = ui_mirror_json_append(out, out_size, pos, "},\"ota\":{");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "state",
                                             ui_mirror_ota_state_name(snapshot->ota.state), false);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "currentVersion",
                                             snapshot->ota.current_version, true);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "targetVersion",
                                             snapshot->ota.target_version, true);
    pos = ui_mirror_json_append(out, out_size, pos, ",\"progress\":");
    pos = ui_mirror_json_append_int(out, out_size, pos, snapshot->ota.progress);
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "message", snapshot->ota.message, true);
    pos = ui_mirror_json_append(out, out_size, pos, "}");
    pos = ui_mirror_json_append_string_field(out, out_size, pos, "message", snapshot->message, true);
    (void)ui_mirror_json_append(out, out_size, pos, "}");
}

static void ui_mirror_publish(void)
{
    ui_page_id_t page = ui_page_current();

    memset(&s_publish_snapshot, 0, sizeof(s_publish_snapshot));
    memset(s_publish_json, 0, sizeof(s_publish_json));

    if (app_backend_get_snapshot(&s_publish_snapshot) != ESP_OK) {
        ESP_LOGW(TAG, "snapshot get failed");
        return;
    }

    ui_mirror_build_json(&s_publish_snapshot, page, s_publish_json, sizeof(s_publish_json));
    ESP_LOGI(TAG, "publish page=%s json_len=%u wifi=%s ble=%d ota=%s",
             ui_mirror_page_name(page),
             (unsigned)strlen(s_publish_json),
             ui_mirror_wifi_state_name(s_publish_snapshot.wifi_state),
             s_publish_snapshot.ble_enabled ? 1 : 0,
             ui_mirror_ota_state_name(s_publish_snapshot.ota.state));
    gyro_ble_publish_ui_state_json(s_publish_json);
    s_last_publish_us = esp_timer_get_time();
}

static void ui_mirror_backend_cb(void *user_ctx)
{
    (void)user_ctx;
    ui_mirror_notify_backend_changed();
}

esp_err_t ui_mirror_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(app_backend_register_update_callback(ui_mirror_backend_cb, NULL),
                        TAG, "register backend callback failed");
    s_initialized = true;
    s_update_pending = true;
    return ESP_OK;
}

void ui_mirror_notify_page_changed(void)
{
    s_update_pending = true;
}

void ui_mirror_notify_backend_changed(void)
{
    s_update_pending = true;
}

void ui_mirror_process_pending(void)
{
    const int64_t now = esp_timer_get_time();
    const bool periodic_due = s_initialized &&
                              ((s_last_publish_us == 0) ||
                               ((now - s_last_publish_us) >= UI_MIRROR_PERIODIC_US));

    if (!s_initialized || (!s_update_pending && !periodic_due)) {
        return;
    }

    s_update_pending = false;
    ui_mirror_publish();
}
