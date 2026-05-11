#include "ota_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_check.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "app_backend_internal.h"
#include "backend_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/x509_crt.h"

#if CONFIG_APP_ONENET_OTA_ENABLE
#define ONENET_OTA_BASE_URL CONFIG_APP_ONENET_OTA_BASE_URL
#define ONENET_OTA_PRODUCT_ID CONFIG_APP_ONENET_PRODUCT_ID
#define ONENET_OTA_DEVICE_NAME CONFIG_APP_ONENET_DEVICE_NAME
#define ONENET_OTA_ACCESS_KEY CONFIG_APP_ONENET_OTA_PRODUCT_ACCESS_KEY
#define ONENET_OTA_DEV_ID CONFIG_APP_ONENET_OTA_DEV_ID
#define ONENET_OTA_TOKEN_EXPIRE_UNIX CONFIG_APP_ONENET_OTA_TOKEN_EXPIRE_UNIX
#define ONENET_OTA_MANUF CONFIG_APP_ONENET_OTA_MANUF
#define ONENET_OTA_MODEL CONFIG_APP_ONENET_OTA_MODEL
#define ONENET_OTA_TYPE CONFIG_APP_ONENET_OTA_TYPE
#else
#define ONENET_OTA_BASE_URL ""
#define ONENET_OTA_PRODUCT_ID ""
#define ONENET_OTA_DEVICE_NAME ""
#define ONENET_OTA_ACCESS_KEY ""
#define ONENET_OTA_DEV_ID ""
#define ONENET_OTA_TOKEN_EXPIRE_UNIX ""
#define ONENET_OTA_MANUF ""
#define ONENET_OTA_MODEL ""
#define ONENET_OTA_TYPE 1
#endif

#define ONENET_OTA_AUTH_METHOD        "sha1"
#define ONENET_OTA_AUTH_VERSION       "2018-10-31"
#define ONENET_OTA_AUTH_MAX_LEN       384
#define ONENET_OTA_URL_MAX_LEN        384
#define ONENET_OTA_PAYLOAD_MAX_LEN    256
#define ONENET_OTA_RESPONSE_MAX_LEN   2048
#define ONENET_OTA_DOWNLOAD_BUF_SIZE  4096
#define ONENET_OTA_BASE64_KEY_MAX_LEN 128
#define ONENET_OTA_SIGN_MAX_LEN       64
#define ONENET_OTA_MD5_HEX_LEN        32
#define ONENET_OTA_TASK_STACK         8192
#define ONENET_OTA_DOWNLOAD_TASK_STACK 8192
#define ONENET_OTA_TASK_PRIORITY      4
#define ONENET_OTA_CHECK_COOLDOWN_MS  5000

typedef struct {
    char *buf;
    size_t size;
    size_t used;
} ota_http_buf_t;

typedef struct {
    bool valid;
    int tid;
    int size;
    int type;
    char target[APP_BACKEND_OTA_VERSION_LEN];
    char md5[ONENET_OTA_MD5_HEX_LEN + 1];
} ota_pending_task_t;

static const char *TAG = "APP_OTA";

static bool s_check_in_progress;
static bool s_download_started;
static ota_pending_task_t s_pending_task;
static int64_t s_last_check_start_us;

static const char ONENET_OTA_CA_CERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFxjCCBK6gAwIBAgIQDwa7CTBhSCZ/yhtxwduAfTANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQG\n"
    "EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAw\n"
    "HgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0yMjEyMTUwMDAwMDBaFw0zMjEyMTQy\n"
    "MzU5NTlaMFsxCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5EaWdpQ2VydCwgSW5jLjEzMDEGA1UEAxMq\n"
    "R2VvVHJ1c3QgRzIgVExTIENOIFJTQTQwOTYgU0hBMjU2IDIwMjIgQ0ExMIICIjANBgkqhkiG9w0B\n"
    "AQEFAAOCAg8AMIICCgKCAgEAn1dxOo4OzYa4O1EJd0nhFI5QB/kXAJTHRI6C1J2Gz86BGe9+DD8R\n"
    "4vexG7/QnUvV+5887o+G4enlkDwJV1Pehq4i0n+X6VKIPg5cThCx6/o03bUkLWld7slhi3Hli/Ma\n"
    "osZZuytdU1uCzQlGpaLB2TiTZbDImiVaykdfwl8V6AXP0Ab4wIcvPggl4qlwyPsBY6NODbP884Bm\n"
    "L1ntdXfzecGst30FnAtm4w+PTo0I1T3FITYXaNuIJnKonD1xXaN4Ar/rvcpKpntxVyZQ2T1Lb7vl\n"
    "fYISqNF+1ugpTzKnI1z7xV7tSqXd2vs6Csy6yFN+QLWwMnGWuQkvrO1m+F7V85S9ECpbqn9qNtKl\n"
    "8gIQ7JeZBmBdLroW+4j1PDzBWmSWKB+gvRubVSTuoDR3VCtlI4G/Uh+ObF4UStbhKjr1SNCWJcrB\n"
    "1oF+wWBl8Bvozm4xflyyGDY47KCP/Fn9X5GHDNLQA9R0zqDKRumipp1UXswFuzH0I+gOuqSdZSsY\n"
    "ID9XSVq6adcJ3rIMAcTuODcrPrJssOFKGFUbmY/VxbxMgYHI/07Zfdxoa+q0osWpofbMhLPZz9Uu\n"
    "JFBCLsjgSckkSU02/4itmLm9e3lt6E/4q9McMCaS8+qrLWKG9wQtviC8zGynEXpxjEQE8WyrfeYK\n"
    "yXwZFCPB4rpTw1joJmGFeLUCAwEAAaOCAX4wggF6MBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYDVR0O\n"
    "BBYEFEFOjmmd9G4l7HgUHH7XzR2Zz/lrMB8GA1UdIwQYMBaAFE4iVCAYlebjbuYP+vq5Eu0GF485\n"
    "MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdAYIKwYBBQUH\n"
    "AQEEaDBmMCMGCCsGAQUFBzABhhdodHRwOi8vb2NzcC5kaWdpY2VydC5jbjA/BggrBgEFBQcwAoYz\n"
    "aHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY24vRGlnaUNlcnRHbG9iYWxSb290RzIuY3J0MEAGA1Ud\n"
    "HwQ5MDcwNaAzoDGGL2h0dHA6Ly9jcmwuZGlnaWNlcnQuY24vRGlnaUNlcnRHbG9iYWxSb290RzIu\n"
    "Y3JsMD0GA1UdIAQ2MDQwCwYJYIZIAYb9bAIBMAcGBWeBDAEBMAgGBmeBDAECATAIBgZngQwBAgIw\n"
    "CAYGZ4EMAQIDMA0GCSqGSIb3DQEBCwUAA4IBAQAkpDVI+nCKnAEEpDfldytHXUYOr2ysaGeboE3d\n"
    "1KAN4Gt+eioRvgNdmDKbVFCYY0C6ErIXIvSGXafsprU2dclka/kmH+9U4q3v5Acx6KwcnEuYLN0Q\n"
    "OtrlQ9s3Z4IbIzdYUv8IauXC27a3x99x2WMVxNu1KGJy0r1Z+Xya1adf9/e1LFj1CGdq9HfQ/HFW\n"
    "P2khzxQZ4u62sOHuZdPgtNidFOhQLC+25cNsPgqsaCrGLbSjzni7VN7NhAbTsLhA1oz41vHHB+q4\n"
    "ZzNlC01elmPBOtupe2HM/lqMA/J/nTxc718THdxpszFllAET1/lFEolnqihpUFPaPjRwU7yZIseS\n"
    "-----END CERTIFICATE-----\n"
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
    "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
    "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
    "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
    "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
    "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
    "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
    "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
    "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
    "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
    "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
    "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
    "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
    "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
    "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
    "-----END CERTIFICATE-----\n"
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
    "MrY=\n"
    "-----END CERTIFICATE-----\n"
    "-----BEGIN CERTIFICATE-----\n"
    "MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw\n"
    "CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\n"
    "ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\n"
    "Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw\n"
    "EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x\n"
    "IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF\n"
    "K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG\n"
    "fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO\n"
    "Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd\n"
    "BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx\n"
    "AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/\n"
    "oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8\n"
    "sycX\n"
    "-----END CERTIFICATE-----\n"
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n"
    ;


static void ota_log_tls_error_flags(esp_tls_error_handle_t error_handle, const char *where)
{
    int tls_code = 0;
    int tls_flags = 0;
    esp_err_t err = ESP_OK;

    if (error_handle == NULL) {
        ESP_LOGW(TAG, "%s tls error handle is NULL", where);
        return;
    }
    err = esp_tls_get_and_clear_last_error(error_handle, &tls_code, &tls_flags);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s get tls error failed: %s (tls_code=0x%X tls_flags=0x%X)",
                 where, esp_err_to_name(err), tls_code, tls_flags);
    } else if ((tls_code == 0) && (tls_flags == 0)) {
        return;
    }
    ESP_LOGW(TAG, "%s tls_code=0x%X tls_flags=0x%X", where, tls_code, tls_flags);
    if (tls_flags != 0) {
        char verify_info[256] = {0};
        mbedtls_x509_crt_verify_info(verify_info, sizeof(verify_info), "  ! ", (uint32_t)tls_flags);
        ESP_LOGW(TAG, "TLS verify flags:\n%s", verify_info);
    }
}

static bool ota_service_configured(void)
{
    return (ONENET_OTA_BASE_URL[0] != '\0') &&
           (ONENET_OTA_PRODUCT_ID[0] != '\0') &&
           (ONENET_OTA_ACCESS_KEY[0] != '\0') &&
           (ONENET_OTA_DEV_ID[0] != '\0') &&
           (ONENET_OTA_TOKEN_EXPIRE_UNIX[0] != '\0');
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

static esp_err_t onenet_ota_make_authorization(char *auth, size_t auth_size)
{
    char res[96] = {0};
    char res_encoded[128] = {0};
    char sign_input[160] = {0};
    uint8_t decoded_key[ONENET_OTA_BASE64_KEY_MAX_LEN] = {0};
    unsigned char hmac[20] = {0};
    char sign_base64[ONENET_OTA_SIGN_MAX_LEN] = {0};
    char sign_encoded[ONENET_OTA_SIGN_MAX_LEN * 3] = {0};
    size_t decoded_len = 0;
    size_t sign_len = 0;
    int ret;

    if ((auth == NULL) || (auth_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(res, sizeof(res), "products/%s", ONENET_OTA_PRODUCT_ID);
    ESP_RETURN_ON_ERROR(url_encode(res, res_encoded, sizeof(res_encoded)),
                        TAG, "encode auth res failed");
    snprintf(sign_input, sizeof(sign_input), "%s\n%s\n%s\n%s",
             ONENET_OTA_TOKEN_EXPIRE_UNIX, ONENET_OTA_AUTH_METHOD,
             res, ONENET_OTA_AUTH_VERSION);

    ret = mbedtls_base64_decode(decoded_key, sizeof(decoded_key), &decoded_len,
                                (const unsigned char *)ONENET_OTA_ACCESS_KEY,
                                strlen(ONENET_OTA_ACCESS_KEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "OTA access key decode failed");
        return ESP_ERR_INVALID_ARG;
    }

    ret = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
                          decoded_key, decoded_len,
                          (const unsigned char *)sign_input, strlen(sign_input),
                          hmac);
    if (ret != 0) {
        ESP_LOGE(TAG, "OTA auth hmac failed");
        return ESP_FAIL;
    }

    ret = mbedtls_base64_encode((unsigned char *)sign_base64, sizeof(sign_base64),
                                &sign_len, hmac, sizeof(hmac));
    if (ret != 0) {
        ESP_LOGE(TAG, "OTA sign encode failed");
        return ESP_FAIL;
    }
    sign_base64[sign_len] = '\0';

    ESP_RETURN_ON_ERROR(url_encode(sign_base64, sign_encoded, sizeof(sign_encoded)),
                        TAG, "encode auth sign failed");
    snprintf(auth, auth_size,
             "version=%s&res=%s&et=%s&method=%s&sign=%s",
             ONENET_OTA_AUTH_VERSION, res_encoded, ONENET_OTA_TOKEN_EXPIRE_UNIX,
             ONENET_OTA_AUTH_METHOD, sign_encoded);
    return ESP_OK;
}

static esp_err_t ota_http_event_cb(esp_http_client_event_t *evt)
{
    ota_http_buf_t *capture = NULL;

    if (evt == NULL) {
        return ESP_OK;
    }

    if ((evt->event_id == HTTP_EVENT_ERROR) || (evt->event_id == HTTP_EVENT_DISCONNECTED)) {
        ota_log_tls_error_flags((esp_tls_error_handle_t)evt->data, "OTA HTTP event");
        return ESP_OK;
    }

    if ((evt->event_id != HTTP_EVENT_ON_DATA) || (evt->user_data == NULL)) {
        return ESP_OK;
    }

    capture = (ota_http_buf_t *)evt->user_data;
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
        ESP_LOGW(TAG, "OTA HTTP response truncated");
    }
    return ESP_OK;
}

static esp_err_t ota_http_json_request(const char *url,
                                       esp_http_client_method_t method,
                                       const char *body,
                                       char *response,
                                       size_t response_size)
{
    char authorization[ONENET_OTA_AUTH_MAX_LEN] = {0};
    ota_http_buf_t capture = {
        .buf = response,
        .size = response_size,
        .used = 0,
    };
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .event_handler = ota_http_event_cb,
        .user_data = &capture,
        .timeout_ms = 10000,
        .cert_pem = ONENET_OTA_CA_CERT_PEM,
    };
    esp_http_client_handle_t client = NULL;
    esp_err_t err;
    int status = 0;

    if ((url == NULL) || (response == NULL) || (response_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    response[0] = '\0';
    ESP_RETURN_ON_ERROR(onenet_ota_make_authorization(authorization, sizeof(authorization)),
                        TAG, "make OTA auth failed");

    client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Authorization", authorization);
    if ((body != NULL) && (body[0] != '\0')) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }

    err = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA HTTP failed: %s", esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "OTA HTTP status=%d", status);
        return ESP_FAIL;
    }
    if (response[0] == '\0') {
        ESP_LOGW(TAG, "OTA HTTP empty response");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static const char *json_string_value(cJSON *parent, const char *name)
{
    cJSON *item = cJSON_GetObjectItem(parent, name);

    if (!cJSON_IsString(item) || (item->valuestring == NULL)) {
        return NULL;
    }
    return item->valuestring;
}

static int json_int_value(cJSON *parent, const char *name, int fallback)
{
    cJSON *item = cJSON_GetObjectItem(parent, name);

    if (!cJSON_IsNumber(item)) {
        return fallback;
    }
    return item->valueint;
}

static void ota_notify_changed(void)
{
    app_backend_notify_changed();
}

static void ota_set_state(app_backend_ota_state_t state,
                          const char *current_version,
                          const char *target_version,
                          int progress,
                          const char *message)
{
    backend_store_set_ota(state, current_version, target_version, progress, message);
    ota_notify_changed();
}

static void ota_set_failed_check_state(const char *version, const char *message)
{
    ota_set_state(APP_BACKEND_OTA_FAILED,
                  version,
                  NULL,
                  0,
                  message != NULL ? message : "检测失败，请稍后重试");
}

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    if ((bytes == NULL) || (out == NULL) || (out_size < ((len * 2) + 1))) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex[(bytes[i] >> 4) & 0x0f];
        out[(i * 2) + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static esp_err_t parse_onenet_result(const char *response, int *code_out, const char **msg_out)
{
    cJSON *root = NULL;

    if ((response == NULL) || (code_out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_Parse(response);
    if (root == NULL) {
        ESP_LOGW(TAG, "OTA json parse failed");
        return ESP_FAIL;
    }

    *code_out = json_int_value(root, "code", json_int_value(root, "errno", -1));
    if (msg_out != NULL) {
        *msg_out = json_string_value(root, "msg");
        if (*msg_out == NULL) {
            *msg_out = json_string_value(root, "error");
        }
    }
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ota_service_report_version(const char *version)
{
    char url[ONENET_OTA_URL_MAX_LEN] = {0};
    char payload[ONENET_OTA_PAYLOAD_MAX_LEN] = {0};
    char response[ONENET_OTA_RESPONSE_MAX_LEN] = {0};
    const char *msg = NULL;
    int code = -1;
    esp_err_t err;

    snprintf(url, sizeof(url), "%s/%s/%s/version",
             ONENET_OTA_BASE_URL, ONENET_OTA_PRODUCT_ID, ONENET_OTA_DEVICE_NAME);
    snprintf(payload, sizeof(payload), "{\"f_version\":\"%s\",\"s_version\":\"%s\"}",
             version, version);

    err = ota_http_json_request(url, HTTP_METHOD_POST, payload, response, sizeof(response));
    if (err != ESP_OK) {
        return err;
    }
    err = parse_onenet_result(response, &code, &msg);
    if (err != ESP_OK) {
        return err;
    }
    if (code != 0) {
        ESP_LOGW(TAG, "version report rejected code=%d msg=%s",
                 code, msg != NULL ? msg : "--");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "version reported %s", version);
    return ESP_OK;
}

static esp_err_t ota_service_check_task(const char *version)
{
    char url[ONENET_OTA_URL_MAX_LEN] = {0};
    char response[ONENET_OTA_RESPONSE_MAX_LEN] = {0};
    cJSON *root = NULL;
    cJSON *data = NULL;
    const char *msg = NULL;
    const char *target = NULL;
    const char *md5 = NULL;
    int code = -1;
    int tid = 0;
    int size = 0;
    int type = 0;
    esp_err_t err;

    snprintf(url, sizeof(url),
             "%s/%s/%s/check?type=%d&version=%s",
             ONENET_OTA_BASE_URL, ONENET_OTA_PRODUCT_ID, ONENET_OTA_DEVICE_NAME,
             ONENET_OTA_TYPE, version);

    err = ota_http_json_request(url, HTTP_METHOD_GET, NULL, response, sizeof(response));
    if (err != ESP_OK) {
        return err;
    }

    root = cJSON_Parse(response);
    if (root == NULL) {
        ESP_LOGW(TAG, "OTA task json parse failed");
        return ESP_FAIL;
    }

    code = json_int_value(root, "code", json_int_value(root, "errno", -1));
    msg = json_string_value(root, "msg");
    if (msg == NULL) {
        msg = json_string_value(root, "error");
    }
    if (code == 12012) {
        ESP_LOGI(TAG, "no OTA task");
        ota_set_state(APP_BACKEND_OTA_IDLE, version, NULL, 0, "当前已是最新版本");
        cJSON_Delete(root);
        return ESP_OK;
    }
    if (code != 0) {
        ESP_LOGW(TAG, "task check rejected code=%d msg=%s",
                 code, msg != NULL ? msg : "--");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGW(TAG, "OTA task missing data");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    target = json_string_value(data, "target");
    md5 = json_string_value(data, "md5");
    tid = json_int_value(data, "tid", 0);
    size = json_int_value(data, "size", 0);
    type = json_int_value(data, "type", 0);

    ESP_LOGI(TAG, "update available target=%s tid=%d size=%d type=%d md5=%s",
             target != NULL ? target : "--", tid, size, type, md5 != NULL ? md5 : "--");
    ESP_LOGI(TAG, "waiting for UI confirmation before download");
    memset(&s_pending_task, 0, sizeof(s_pending_task));
    s_pending_task.valid = true;
    s_pending_task.tid = tid;
    s_pending_task.size = size;
    s_pending_task.type = type;
    if (target != NULL) {
        strlcpy(s_pending_task.target, target, sizeof(s_pending_task.target));
    }
    if (md5 != NULL) {
        strlcpy(s_pending_task.md5, md5, sizeof(s_pending_task.md5));
    }
    ota_set_state(APP_BACKEND_OTA_READY, version, target, 0, "发现新版本，等待确认");
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ota_http_download_to_partition(const ota_pending_task_t *task)
{
    char url[ONENET_OTA_URL_MAX_LEN] = {0};
    char authorization[ONENET_OTA_AUTH_MAX_LEN] = {0};
    char actual_md5[ONENET_OTA_MD5_HEX_LEN + 1] = {0};
    uint8_t md5_raw[16] = {0};
    uint8_t *buffer = NULL;
    const esp_partition_t *update_partition = NULL;
    esp_ota_handle_t ota_handle = 0;
    esp_http_client_handle_t client = NULL;
    mbedtls_md_context_t md_ctx;
    const mbedtls_md_info_t *md_info = NULL;
    int content_length = 0;
    int progress_total = 0;
    int status = 0;
    int read_total = 0;
    int last_progress = -1;
    int last_logged_progress = -5;
    esp_err_t err = ESP_OK;

    if ((task == NULL) || !task->valid || (task->tid <= 0)) {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(url, sizeof(url), "%s/%s/%s/%d/download",
             ONENET_OTA_BASE_URL, ONENET_OTA_PRODUCT_ID, ONENET_OTA_DEVICE_NAME, task->tid);
    ESP_RETURN_ON_ERROR(onenet_ota_make_authorization(authorization, sizeof(authorization)),
                        TAG, "make OTA download auth failed");

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .cert_pem = ONENET_OTA_CA_CERT_PEM,
        .event_handler = ota_http_event_cb,
        .keep_alive_enable = true,
    };

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGW(TAG, "no OTA update partition");
        return ESP_FAIL;
    }
    if ((task->size > 0) && ((uint32_t)task->size > update_partition->size)) {
        ESP_LOGW(TAG, "image too large size=%d partition=%lu", task->size, (unsigned long)update_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    buffer = (uint8_t *)malloc(ONENET_OTA_DOWNLOAD_BUF_SIZE);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    client = esp_http_client_init(&config);
    if (client == NULL) {
        free(buffer);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Accept", "application/octet-stream");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Authorization", authorization);

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    mbedtls_md_init(&md_ctx);
    if ((md_info == NULL) || (mbedtls_md_setup(&md_ctx, md_info, 0) != 0) ||
        (mbedtls_md_starts(&md_ctx) != 0)) {
        ESP_LOGW(TAG, "md5 init failed");
        err = ESP_FAIL;
        goto cleanup;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "download open failed: %s", esp_err_to_name(err));
        goto cleanup_md;
    }

    content_length = esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "download HTTP status=%d", status);
        err = ESP_FAIL;
        goto cleanup_http;
    }
    if ((task->size > 0) && (content_length > 0) && (content_length != task->size)) {
        ESP_LOGW(TAG, "download size mismatch expected=%d actual=%d", task->size, content_length);
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup_http;
    }
    if ((task->size <= 0) && (content_length > 0) && ((uint32_t)content_length > update_partition->size)) {
        ESP_LOGW(TAG, "image too large content_length=%d partition=%lu",
                 content_length, (unsigned long)update_partition->size);
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup_http;
    }
    progress_total = task->size > 0 ? task->size : content_length;

    ESP_LOGI(TAG, "download start tid=%d target=%s size=%d content_length=%d partition=%s",
             task->tid, task->target, task->size, content_length, update_partition->label);
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ota begin failed: %s", esp_err_to_name(err));
        goto cleanup_http;
    }

    while (1) {
        int read_len = esp_http_client_read(client, (char *)buffer, ONENET_OTA_DOWNLOAD_BUF_SIZE);
        int progress = 0;

        if (read_len < 0) {
            ESP_LOGW(TAG, "download read failed");
            err = ESP_FAIL;
            break;
        }
        if (read_len == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (mbedtls_md_update(&md_ctx, buffer, (size_t)read_len) != 0) {
            err = ESP_FAIL;
            break;
        }
        err = esp_ota_write(ota_handle, buffer, (size_t)read_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ota write failed: %s", esp_err_to_name(err));
            break;
        }

        read_total += read_len;
        if (progress_total > 0) {
            progress = (read_total * 100) / progress_total;
            if (progress > 100) {
                progress = 100;
            }
            if (progress != last_progress) {
                last_progress = progress;
                backend_store_set_ota(APP_BACKEND_OTA_DOWNLOADING, NULL, task->target, progress, "正在下载升级包");
                ota_notify_changed();
                if ((progress == 100) || (progress >= (last_logged_progress + 5))) {
                    last_logged_progress = progress;
                    ESP_LOGI(TAG, "download progress %d%% (%d/%d)", progress, read_total, progress_total);
                }
            }
        }
    }

    if (err != ESP_OK) {
        goto cleanup_ota;
    }
    if ((progress_total > 0) && (read_total != progress_total)) {
        ESP_LOGW(TAG, "download incomplete expected=%d actual=%d", progress_total, read_total);
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup_ota;
    }
    if (mbedtls_md_finish(&md_ctx, md5_raw) != 0) {
        err = ESP_FAIL;
        goto cleanup_ota;
    }
    bytes_to_hex(md5_raw, sizeof(md5_raw), actual_md5, sizeof(actual_md5));
    if ((task->md5[0] != '\0') && (strcasecmp(task->md5, actual_md5) != 0)) {
        ESP_LOGW(TAG, "md5 mismatch expected=%s actual=%s", task->md5, actual_md5);
        err = ESP_ERR_INVALID_CRC;
        goto cleanup_ota;
    }

    ota_set_state(APP_BACKEND_OTA_APPLYING, NULL, task->target, 100, "升级包下载完成，正在写入");
    err = esp_ota_end(ota_handle);
    ota_handle = 0;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ota end failed: %s", esp_err_to_name(err));
        goto cleanup_http;
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set boot partition failed: %s", esp_err_to_name(err));
        goto cleanup_http;
    }

    ESP_LOGI(TAG, "OTA ready; waiting for user reboot to %s md5=%s", task->target, actual_md5);
    ota_set_state(APP_BACKEND_OTA_DONE, NULL, task->target, 100, "升级包已准备完成");
    s_download_started = false;

cleanup_ota:
    if (ota_handle != 0) {
        esp_ota_abort(ota_handle);
    }
cleanup_http:
    esp_http_client_close(client);
cleanup_md:
    mbedtls_md_free(&md_ctx);
cleanup:
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    free(buffer);
    return err;
}

static void ota_download_task(void *arg)
{
    ota_pending_task_t task = *(ota_pending_task_t *)arg;
    esp_err_t err;

    free(arg);
    ota_set_state(APP_BACKEND_OTA_DOWNLOADING, NULL, task.target, 0, "正在下载升级包");
    err = ota_http_download_to_partition(&task);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA download/apply failed: %s", esp_err_to_name(err));
        ota_set_state(APP_BACKEND_OTA_FAILED, NULL, task.target, 0, "升级失败，请稍后重试");
        s_download_started = false;
    }
    vTaskDelete(NULL);
}

static void ota_service_task(void *arg)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    const char *version = (desc != NULL) ? desc->version : "unknown";
    esp_err_t err;

    (void)arg;

    if (!ota_service_configured()) {
        ESP_LOGW(TAG, "not configured; set product access key and numeric dev_id");
        ota_set_failed_check_state(version, "OTA 未配置，请检查参数");
        s_check_in_progress = false;
        vTaskDelete(NULL);
        return;
    }

    ota_set_state(APP_BACKEND_OTA_CHECKING, version, NULL, 0, "正在检测新版本");
    err = ota_service_report_version(version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "version report failed: %s", esp_err_to_name(err));
        ota_set_failed_check_state(version, "版本上报失败，请检查网络");
        s_check_in_progress = false;
        vTaskDelete(NULL);
        return;
    }

    err = ota_service_check_task(version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "task check failed: %s", esp_err_to_name(err));
        ota_set_failed_check_state(version, "检测失败，请稍后重试");
    }

    s_check_in_progress = false;
    vTaskDelete(NULL);
}

esp_err_t ota_service_check_update(void)
{
#if !CONFIG_APP_ONENET_OTA_ENABLE
    ESP_LOGI(TAG, "disabled");
    return ESP_OK;
#endif
    int64_t now_us = esp_timer_get_time();

    if (s_check_in_progress) {
        return ESP_OK;
    }
    if ((s_last_check_start_us > 0) &&
        ((now_us - s_last_check_start_us) < ((int64_t)ONENET_OTA_CHECK_COOLDOWN_MS * 1000LL))) {
        ESP_LOGI(TAG, "check request ignored: cooldown");
        return ESP_OK;
    }

    s_check_in_progress = true;
    s_last_check_start_us = now_us;
    if (xTaskCreate(ota_service_task, "app_ota", ONENET_OTA_TASK_STACK, NULL,
                    ONENET_OTA_TASK_PRIORITY, NULL) != pdPASS) {
        s_check_in_progress = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "start requested");
    return ESP_OK;
}

esp_err_t ota_service_report_current_version(void)
{
#if !CONFIG_APP_ONENET_OTA_ENABLE
    ESP_LOGI(TAG, "version report disabled");
    return ESP_OK;
#else
    const esp_app_desc_t *desc = esp_app_get_description();
    const char *version = (desc != NULL) ? desc->version : "unknown";
    esp_err_t err;

    if (!ota_service_configured()) {
        ESP_LOGW(TAG, "version report skipped: OTA not configured");
        return ESP_ERR_INVALID_STATE;
    }

    err = ota_service_report_version(version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "current version report failed: %s", esp_err_to_name(err));
    }
    return err;
#endif
}

esp_err_t ota_service_confirm_update(void)
{
    ota_pending_task_t *task = NULL;

    if (!s_pending_task.valid) {
        ESP_LOGW(TAG, "no pending OTA task");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_download_started) {
        return ESP_OK;
    }

    task = (ota_pending_task_t *)calloc(1, sizeof(*task));
    if (task == NULL) {
        return ESP_ERR_NO_MEM;
    }
    *task = s_pending_task;
    s_download_started = true;
    if (xTaskCreate(ota_download_task, "app_ota_dl", ONENET_OTA_DOWNLOAD_TASK_STACK, task,
                    ONENET_OTA_TASK_PRIORITY, NULL) != pdPASS) {
        s_download_started = false;
        free(task);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t ota_service_restart_now(void)
{
    ESP_LOGI(TAG, "restarting by user request");
    esp_restart();
    return ESP_OK;
}
