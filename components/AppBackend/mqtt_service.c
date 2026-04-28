#include "mqtt_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"

#if CONFIG_APP_ONENET_MQTT_ENABLE
#define ONENET_PRODUCT_ID CONFIG_APP_ONENET_PRODUCT_ID
#define ONENET_DEVICE_NAME CONFIG_APP_ONENET_DEVICE_NAME
#define ONENET_DEVICE_KEY CONFIG_APP_ONENET_DEVICE_KEY
#define ONENET_MQTT_URI CONFIG_APP_ONENET_MQTT_URI
#define ONENET_TOKEN_EXPIRE_UNIX CONFIG_APP_ONENET_TOKEN_EXPIRE_UNIX
#else
#define ONENET_PRODUCT_ID ""
#define ONENET_DEVICE_NAME ""
#define ONENET_DEVICE_KEY ""
#define ONENET_MQTT_URI ""
#define ONENET_TOKEN_EXPIRE_UNIX ""
#endif

#define ONENET_TOKEN_METHOD        "sha1"
#define ONENET_TOKEN_VERSION       "2018-10-31"
#define ONENET_TOKEN_MAX_LEN       384
#define ONENET_TOPIC_MAX_LEN       160
#define ONENET_PAYLOAD_MAX_LEN     256
#define ONENET_BASE64_KEY_MAX_LEN  128
#define ONENET_SIGN_MAX_LEN        64

static const char *TAG = "APP_MQTT";

static esp_mqtt_client_handle_t s_client;
static bool s_started;
static bool s_connected;
static uint32_t s_publish_seq = 1;

static bool mqtt_service_configured(void)
{
    return (ONENET_MQTT_URI[0] != '\0') &&
           (ONENET_PRODUCT_ID[0] != '\0') &&
           (ONENET_DEVICE_NAME[0] != '\0') &&
           (ONENET_DEVICE_KEY[0] != '\0') &&
           (ONENET_TOKEN_EXPIRE_UNIX[0] != '\0');
}

static bool url_is_unreserved(unsigned char c)
{
    return ((c >= 'A') && (c <= 'Z')) ||
           ((c >= 'a') && (c <= 'z')) ||
           ((c >= '0') && (c <= '9')) ||
           (c == '-') || (c == '_') || (c == '.') || (c == '~');
}

static esp_err_t url_encode(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;

    if ((src == NULL) || (dst == NULL) || (dst_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        if (url_is_unreserved(c)) {
            if ((out + 1) >= dst_size) {
                return ESP_ERR_NO_MEM;
            }
            dst[out++] = (char)c;
        } else {
            if ((out + 3) >= dst_size) {
                return ESP_ERR_NO_MEM;
            }
            snprintf(&dst[out], dst_size - out, "%%%02X", c);
            out += 3;
        }
    }
    dst[out] = '\0';
    return ESP_OK;
}

static esp_err_t onenet_make_token(char *token, size_t token_size)
{
    char res[ONENET_TOPIC_MAX_LEN] = {0};
    char res_encoded[ONENET_TOPIC_MAX_LEN] = {0};
    char sign_input[ONENET_TOPIC_MAX_LEN + 64] = {0};
    uint8_t decoded_key[ONENET_BASE64_KEY_MAX_LEN] = {0};
    unsigned char hmac[20] = {0};
    char sign_base64[ONENET_SIGN_MAX_LEN] = {0};
    char sign_encoded[ONENET_SIGN_MAX_LEN * 3] = {0};
    size_t decoded_len = 0;
    size_t sign_len = 0;
    int ret;

    if ((token == NULL) || (token_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(res, sizeof(res), "products/%s/devices/%s", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    ESP_RETURN_ON_ERROR(url_encode(res, res_encoded, sizeof(res_encoded)), TAG, "encode res failed");

    snprintf(sign_input, sizeof(sign_input), "%s\n%s\n%s\n%s",
             ONENET_TOKEN_EXPIRE_UNIX, ONENET_TOKEN_METHOD, res, ONENET_TOKEN_VERSION);

    ret = mbedtls_base64_decode(decoded_key, sizeof(decoded_key), &decoded_len,
                                (const unsigned char *)ONENET_DEVICE_KEY,
                                strlen(ONENET_DEVICE_KEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "device key decode failed");
        return ESP_ERR_INVALID_ARG;
    }

    ret = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
                          decoded_key, decoded_len,
                          (const unsigned char *)sign_input, strlen(sign_input),
                          hmac);
    if (ret != 0) {
        ESP_LOGE(TAG, "token hmac failed");
        return ESP_FAIL;
    }

    ret = mbedtls_base64_encode((unsigned char *)sign_base64, sizeof(sign_base64),
                                &sign_len, hmac, sizeof(hmac));
    if (ret != 0) {
        ESP_LOGE(TAG, "token sign encode failed");
        return ESP_FAIL;
    }
    sign_base64[sign_len] = '\0';

    ESP_RETURN_ON_ERROR(url_encode(sign_base64, sign_encoded, sizeof(sign_encoded)),
                        TAG, "encode sign failed");

    snprintf(token, token_size,
             "version=%s&res=%s&et=%s&method=%s&sign=%s",
             ONENET_TOKEN_VERSION, res_encoded, ONENET_TOKEN_EXPIRE_UNIX,
             ONENET_TOKEN_METHOD, sign_encoded);
    return ESP_OK;
}

static void mqtt_service_publish_set_reply(const char *request_payload, int payload_len)
{
    char topic[ONENET_TOPIC_MAX_LEN] = {0};
    char payload[ONENET_PAYLOAD_MAX_LEN] = {0};
    int msg_id;

    (void)request_payload;
    (void)payload_len;
    snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/set_reply",
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%lu\",\"code\":200,\"msg\":\"success\"}",
             (unsigned long)s_publish_seq++);

    msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "property set reply msg_id=%d", msg_id);
}

static void mqtt_service_publish_ota_inform_reply(const char *request_payload, int payload_len)
{
    char topic[ONENET_TOPIC_MAX_LEN] = {0};
    char payload[ONENET_PAYLOAD_MAX_LEN] = {0};
    char request_id[32] = {0};
    cJSON *root = NULL;
    cJSON *id = NULL;
    int msg_id;

    if ((request_payload != NULL) && (payload_len > 0)) {
        root = cJSON_ParseWithLength(request_payload, (size_t)payload_len);
        if (root != NULL) {
            id = cJSON_GetObjectItem(root, "id");
            if (cJSON_IsString(id) && (id->valuestring != NULL)) {
                strlcpy(request_id, id->valuestring, sizeof(request_id));
            }
        }
    }
    if (request_id[0] == '\0') {
        snprintf(request_id, sizeof(request_id), "%lu", (unsigned long)s_publish_seq++);
    }

    snprintf(topic, sizeof(topic), "$sys/%s/%s/ota/inform_reply",
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":{}}",
             request_id);

    msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "OTA inform reply msg_id=%d", msg_id);

    if (root != NULL) {
        cJSON_Delete(root);
    }
}

esp_err_t mqtt_service_publish_status(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    char topic[ONENET_TOPIC_MAX_LEN] = {0};
    char payload[ONENET_PAYLOAD_MAX_LEN] = {0};
    int msg_id;

    if (!s_connected || (s_client == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/post",
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{"
             "\"normalData\":{\"value\":\"firmware=%s;ota=idle\"}}}",
             (unsigned long)s_publish_seq++, desc != NULL ? desc->version : "unknown");

    msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "publish status failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "status published msg_id=%d", msg_id);
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    char topic[ONENET_TOPIC_MAX_LEN] = {0};

    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/post/reply",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
        (void)esp_mqtt_client_subscribe(s_client, topic, 1);
        snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/set",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
        (void)esp_mqtt_client_subscribe(s_client, topic, 1);
        snprintf(topic, sizeof(topic), "$sys/%s/%s/ota/inform",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
        (void)esp_mqtt_client_subscribe(s_client, topic, 1);
        (void)mqtt_service_publish_status();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "mqtt data received");
        if ((event->topic != NULL) && (event->topic_len > 0) &&
            (memmem(event->topic, (size_t)event->topic_len, "/thing/property/set", 19) != NULL)) {
            mqtt_service_publish_set_reply(event->data, event->data_len);
        } else if ((event->topic != NULL) && (event->topic_len > 0) &&
                   (memmem(event->topic, (size_t)event->topic_len, "/ota/inform", 11) != NULL)) {
            ESP_LOGI(TAG, "OTA inform received");
            mqtt_service_publish_ota_inform_reply(event->data, event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "event error");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_service_start(void)
{
    char password[ONENET_TOKEN_MAX_LEN] = {0};
    esp_err_t err;

#if !CONFIG_APP_ONENET_MQTT_ENABLE
    ESP_LOGI(TAG, "disabled");
    return ESP_OK;
#endif

    if (s_started) {
        if (s_connected) {
            (void)mqtt_service_publish_status();
        }
        return ESP_OK;
    }

    if (!mqtt_service_configured()) {
        ESP_LOGW(TAG, "not configured");
        return ESP_ERR_INVALID_STATE;
    }

    err = onenet_make_token(password, sizeof(password));
    if (err != ESP_OK) {
        return err;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ONENET_MQTT_URI,
        .credentials.client_id = ONENET_DEVICE_NAME,
        .credentials.username = ONENET_PRODUCT_ID,
        .credentials.authentication.password = password,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "client init failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                       mqtt_event_handler, NULL),
                        TAG, "register event failed");
    ESP_RETURN_ON_ERROR(esp_mqtt_client_start(s_client), TAG, "start failed");

    s_started = true;
    ESP_LOGI(TAG, "start requested");
    return ESP_OK;
}
