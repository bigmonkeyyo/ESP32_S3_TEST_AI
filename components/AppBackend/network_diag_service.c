#include "network_diag_service.h"

#include <stdbool.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "BACKEND_NET";

typedef struct {
    char sample[64];
    size_t used;
} network_diag_capture_t;

static const char *const s_probe_urls[] = {
    "http://connectivitycheck.platform.hicloud.com/generate_204",
    "http://connectivitycheck.gstatic.com/generate_204",
};

static esp_err_t network_diag_event_cb(esp_http_client_event_t *evt)
{
    network_diag_capture_t *capture = NULL;

    if ((evt == NULL) || (evt->user_data == NULL) || (evt->event_id != HTTP_EVENT_ON_DATA)) {
        return ESP_OK;
    }

    capture = (network_diag_capture_t *)evt->user_data;
    if ((evt->data == NULL) || (evt->data_len <= 0) || (capture->used >= sizeof(capture->sample) - 1)) {
        return ESP_OK;
    }

    size_t room = sizeof(capture->sample) - capture->used - 1;
    size_t copy_len = ((size_t)evt->data_len > room) ? room : (size_t)evt->data_len;
    memcpy(capture->sample + capture->used, evt->data, copy_len);
    capture->used += copy_len;
    capture->sample[capture->used] = '\0';
    return ESP_OK;
}

static esp_err_t network_diag_probe_once(const char *url, network_diag_status_t *status_out)
{
    network_diag_capture_t capture = {0};
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = network_diag_event_cb,
        .user_data = &capture,
        .timeout_ms = 5000,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    int status_code;

    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = esp_http_client_perform(client);
    status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "probe failed url=%s err=%s", url, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "probe status=%d body=%u url=%s", status_code, (unsigned)capture.used, url);
    if (status_code == 204) {
        *status_out = NETWORK_DIAG_STATUS_OPEN;
    } else {
        *status_out = NETWORK_DIAG_STATUS_CAPTIVE_PORTAL;
    }
    return ESP_OK;
}

esp_err_t network_diag_probe_http(network_diag_status_t *status_out)
{
    esp_err_t last_err = ESP_FAIL;
    bool captive_seen = false;

    if (status_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *status_out = NETWORK_DIAG_STATUS_UNKNOWN;
    ESP_LOGI(TAG, "captive portal probe start");

    for (size_t i = 0; i < (sizeof(s_probe_urls) / sizeof(s_probe_urls[0])); i++) {
        network_diag_status_t status = NETWORK_DIAG_STATUS_UNKNOWN;
        esp_err_t err = network_diag_probe_once(s_probe_urls[i], &status);
        if (err != ESP_OK) {
            last_err = err;
            continue;
        }
        if (status == NETWORK_DIAG_STATUS_OPEN) {
            *status_out = NETWORK_DIAG_STATUS_OPEN;
            ESP_LOGI(TAG, "network internet probe open");
            return ESP_OK;
        }
        captive_seen = true;
    }

    if (captive_seen) {
        *status_out = NETWORK_DIAG_STATUS_CAPTIVE_PORTAL;
        ESP_LOGW(TAG, "network may need captive portal auth");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "network probe inconclusive");
    return last_err;
}

