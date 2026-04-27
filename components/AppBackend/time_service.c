#include "time_service.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"

static const char *TAG = "BACKEND_TIME";
static int64_t s_boot_us;
static bool s_sntp_initialized;

static void format_local_time(char *out, size_t out_size)
{
    time_t now = 0;
    struct tm timeinfo = {0};

    if ((out == NULL) || (out_size == 0)) {
        return;
    }

    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2024 - 1900)) {
        strlcpy(out, "--:--", out_size);
        return;
    }

    strftime(out, out_size, "%H:%M", &timeinfo);
}

esp_err_t time_service_init(void)
{
    if (s_boot_us == 0) {
        s_boot_us = esp_timer_get_time();
    }
    setenv("TZ", "CST-8", 1);
    tzset();
    return ESP_OK;
}

esp_err_t time_service_sync(char *now_time, size_t now_time_size, char *last_sync, size_t last_sync_size)
{
    esp_err_t err;

    ESP_RETURN_ON_ERROR(time_service_init(), TAG, "time init failed");

    if (!s_sntp_initialized) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        err = esp_netif_sntp_init(&config);
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
            ESP_LOGW(TAG, "sntp init failed: %s", esp_err_to_name(err));
            return err;
        }
        s_sntp_initialized = true;
    }

    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP sync failed: %s", esp_err_to_name(err));
        return err;
    }

    format_local_time(now_time, now_time_size);
    if ((last_sync != NULL) && (last_sync_size > 0)) {
        strlcpy(last_sync, now_time != NULL ? now_time : "--:--", last_sync_size);
    }
    ESP_LOGI(TAG, "SNTP synced %s", now_time != NULL ? now_time : "");
    return ESP_OK;
}

void time_service_format_current(char *out, size_t out_size)
{
    (void)time_service_init();
    format_local_time(out, out_size);
}

void time_service_format_uptime(char *out, size_t out_size)
{
    int64_t elapsed_s;
    int hours;
    int minutes;
    int seconds;

    if ((out == NULL) || (out_size == 0)) {
        return;
    }

    (void)time_service_init();
    elapsed_s = (esp_timer_get_time() - s_boot_us) / 1000000;
    if (elapsed_s < 0) {
        elapsed_s = 0;
    }

    hours = (int)(elapsed_s / 3600);
    minutes = (int)((elapsed_s % 3600) / 60);
    seconds = (int)(elapsed_s % 60);
    snprintf(out, out_size, "%02d:%02d:%02d", hours, minutes, seconds);
}
