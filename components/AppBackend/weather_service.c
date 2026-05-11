#include "weather_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "BACKEND_WEATHER";

#define WEATHER_HTTP_BUF_SIZE 4096
#define AMAP_WEATHER_API_KEY "90f8d501e6dca5756eb44ea1db3a5ea9"
#define AMAP_WEATHER_CITY_ADCODE "310000"

typedef struct {
    const char *name;
    const char *url;
    int timeout_ms;
} weather_provider_t;

typedef struct {
    char *buf;
    size_t size;
    size_t used;
} weather_http_buf_t;

static const weather_provider_t s_amap_provider = {
    .name = "AMap",
    .url = "http://restapi.amap.com/v3/weather/weatherInfo"
           "?city=" AMAP_WEATHER_CITY_ADCODE
           "&key=" AMAP_WEATHER_API_KEY
           "&extensions=base"
           "&output=JSON",
    .timeout_ms = 8000,
};

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0)) {
        return;
    }
    strlcpy(dst, src != NULL ? src : "--", dst_size);
}

static const char *json_string_value(cJSON *parent, const char *name)
{
    cJSON *item = NULL;

    if ((parent == NULL) || (name == NULL)) {
        return NULL;
    }

    item = cJSON_GetObjectItem(parent, name);
    if (!cJSON_IsString(item) || (item->valuestring == NULL) || (item->valuestring[0] == '\0')) {
        return NULL;
    }
    return item->valuestring;
}

static bool json_string_equals(cJSON *parent, const char *name, const char *expected)
{
    const char *value = json_string_value(parent, name);

    return (value != NULL) && (expected != NULL) && (strcmp(value, expected) == 0);
}

static void weather_set_defaults(app_backend_weather_t *out)
{
    memset(out, 0, sizeof(*out));
    copy_text(out->city, sizeof(out->city), "上海");
    copy_text(out->condition, sizeof(out->condition), "--");
    copy_text(out->temperature, sizeof(out->temperature), "--");
    copy_text(out->humidity, sizeof(out->humidity), "--");
    copy_text(out->wind, sizeof(out->wind), "--");
    copy_text(out->aqi, sizeof(out->aqi), "AQI --");
    copy_text(out->updated_at, sizeof(out->updated_at), "--:--");
}

static esp_err_t weather_http_event_cb(esp_http_client_event_t *evt)
{
    weather_http_buf_t *capture = NULL;

    if ((evt == NULL) || (evt->user_data == NULL) || (evt->event_id != HTTP_EVENT_ON_DATA)) {
        return ESP_OK;
    }

    capture = (weather_http_buf_t *)evt->user_data;
    if ((capture->buf == NULL) || (capture->size == 0) || (evt->data == NULL) || (evt->data_len <= 0)) {
        return ESP_OK;
    }

    size_t room = capture->size - capture->used - 1;
    size_t copy_len = ((size_t)evt->data_len > room) ? room : (size_t)evt->data_len;
    if (copy_len > 0) {
        memcpy(capture->buf + capture->used, evt->data, copy_len);
        capture->used += copy_len;
        capture->buf[capture->used] = '\0';
    }

    if ((size_t)evt->data_len > copy_len) {
        ESP_LOGW(TAG, "weather response truncated");
    }

    return ESP_OK;
}

static esp_err_t http_get_to_buffer(const weather_provider_t *provider, char *response, size_t response_size)
{
    weather_http_buf_t capture = {
        .buf = response,
        .size = response_size,
        .used = 0,
    };
    esp_http_client_config_t config = {
        .url = provider->url,
        .method = HTTP_METHOD_GET,
        .event_handler = weather_http_event_cb,
        .user_data = &capture,
        .timeout_ms = provider->timeout_ms,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = NULL;
    esp_err_t err;
    int status = 0;

    if ((provider == NULL) || (response == NULL) || (response_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    response[0] = '\0';
    ESP_LOGI(TAG, "%s request start", provider->name);

    client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    err = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s HTTP failed: %s", provider->name, esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "%s HTTP status=%d", provider->name, status);
        return ESP_FAIL;
    }
    if (response[0] == '\0') {
        ESP_LOGW(TAG, "%s HTTP empty response", provider->name);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void copy_report_time_hhmm(const char *report_time, char *out, size_t out_size)
{
    if ((report_time == NULL) || (out == NULL) || (out_size == 0)) {
        return;
    }

    if (strlen(report_time) >= 16) {
        char hhmm[6] = {0};
        memcpy(hhmm, report_time + 11, 5);
        copy_text(out, out_size, hhmm);
        return;
    }

    copy_text(out, out_size, report_time);
}

static const char *normalize_amap_wind_power(const char *power)
{
    if (power == NULL) {
        return NULL;
    }

    if (strncmp(power, "≤", strlen("≤")) == 0) {
        static char normalized[APP_BACKEND_WEATHER_VALUE_LEN];
        snprintf(normalized, sizeof(normalized), "<=%s", power + strlen("≤"));
        return normalized;
    }

    return power;
}

static void format_amap_wind(const char *direction, const char *power, char *out, size_t out_size)
{
    const char *display_power = normalize_amap_wind_power(power);

    if ((out == NULL) || (out_size == 0)) {
        return;
    }

    if ((direction == NULL) && (display_power == NULL)) {
        copy_text(out, out_size, "--");
        return;
    }
    if ((direction == NULL) || (strcmp(direction, "无风向") == 0)) {
        snprintf(out, out_size, "风力 %s级", display_power != NULL ? display_power : "--");
        return;
    }
    snprintf(out, out_size, "%s风 %s级", direction, display_power != NULL ? display_power : "--");
}

static esp_err_t parse_amap_weather_json(const char *json, app_backend_weather_t *out)
{
    cJSON *root = NULL;
    cJSON *lives = NULL;
    cJSON *live = NULL;
    const char *weather = NULL;
    const char *temperature = NULL;
    const char *humidity = NULL;
    const char *winddirection = NULL;
    const char *windpower = NULL;
    const char *reporttime = NULL;
    const char *city = NULL;
    const char *info = NULL;
    const char *infocode = NULL;

    if ((json == NULL) || (out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGW(TAG, "AMap json parse failed");
        return ESP_FAIL;
    }

    info = json_string_value(root, "info");
    infocode = json_string_value(root, "infocode");
    if (!json_string_equals(root, "status", "1")) {
        ESP_LOGW(TAG, "AMap status failed info=%s infocode=%s",
                 info != NULL ? info : "--", infocode != NULL ? infocode : "--");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    if ((infocode != NULL) && (strcmp(infocode, "10000") != 0)) {
        ESP_LOGW(TAG, "AMap infocode=%s info=%s", infocode, info != NULL ? info : "--");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    lives = cJSON_GetObjectItem(root, "lives");
    live = cJSON_IsArray(lives) ? cJSON_GetArrayItem(lives, 0) : NULL;
    if (!cJSON_IsObject(live)) {
        ESP_LOGW(TAG, "AMap json missing lives");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    city = json_string_value(live, "city");
    weather = json_string_value(live, "weather");
    temperature = json_string_value(live, "temperature");
    humidity = json_string_value(live, "humidity");
    winddirection = json_string_value(live, "winddirection");
    windpower = json_string_value(live, "windpower");
    reporttime = json_string_value(live, "reporttime");

    copy_text(out->city, sizeof(out->city), city != NULL ? city : "上海");
    copy_text(out->condition, sizeof(out->condition), weather);
    if (temperature != NULL) {
        snprintf(out->temperature, sizeof(out->temperature), "%s℃", temperature);
    }
    if (humidity != NULL) {
        snprintf(out->humidity, sizeof(out->humidity), "湿度 %s%%", humidity);
    }
    format_amap_wind(winddirection, windpower, out->wind, sizeof(out->wind));
    copy_text(out->aqi, sizeof(out->aqi), "AQI --");
    copy_report_time_hhmm(reporttime, out->updated_at, sizeof(out->updated_at));
    out->valid = true;

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t weather_service_fetch_shanghai(app_backend_weather_t *out)
{
    char *response = NULL;
    esp_err_t err;

    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    weather_set_defaults(out);
    response = (char *)calloc(1, WEATHER_HTTP_BUF_SIZE);
    if (response == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = http_get_to_buffer(&s_amap_provider, response, WEATHER_HTTP_BUF_SIZE);
    if (err == ESP_OK) {
        err = parse_amap_weather_json(response, out);
    }

    free(response);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AMap weather updated");
    }
    return err;
}
