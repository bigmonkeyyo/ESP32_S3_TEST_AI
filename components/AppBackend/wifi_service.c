#include "wifi_service.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "nvs.h"

#include "app_backend_internal.h"
#include "app_backend_types.h"
#include "backend_store.h"

static const char *TAG = "BACKEND_WIFI";
static const char *NVS_NS = "wifi_cfg";
static const char *NVS_KEY_SSID = "ssid";
static const char *NVS_KEY_PASS = "pass";

static bool s_initialized;
static bool s_netif_initialized;
static esp_netif_t *s_sta_netif;
static char s_pending_ssid[APP_BACKEND_SSID_MAX_LEN + 1];
static char s_pending_password[APP_BACKEND_PASSWORD_MAX_LEN + 1];
static bool s_reconnect_in_progress;
static bool s_disconnect_in_progress;

static void wifi_log_dns_info(esp_netif_dns_type_t type, const char *name)
{
    esp_netif_dns_info_t dns = {0};

    if ((s_sta_netif == NULL) || (name == NULL)) {
        return;
    }

    if (esp_netif_get_dns_info(s_sta_netif, type, &dns) != ESP_OK) {
        ESP_LOGW(TAG, "dns %s unavailable", name);
        return;
    }

    if (dns.ip.type == ESP_IPADDR_TYPE_V4) {
        ESP_LOGI(TAG, "dns %s " IPSTR, name, IP2STR(&dns.ip.u_addr.ip4));
    }
}

static void wifi_set_resolver_dns(uint8_t index, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    ip_addr_t dns = {0};

    IP_ADDR4(&dns, a, b, c, d);
    dns_setserver(index, &dns);
    ESP_LOGI(TAG, "resolver dns%u %u.%u.%u.%u", index, a, b, c, d);
}

static void wifi_configure_fallback_dns(void)
{
    esp_netif_dns_info_t dns = {0};
    esp_err_t err;

    if (s_sta_netif == NULL) {
        return;
    }

    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ESP_IP4TOADDR(223, 5, 5, 5);
    err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_FALLBACK, &dns);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set fallback dns failed: %s", esp_err_to_name(err));
    }

    wifi_set_resolver_dns(0, 223, 5, 5, 5);
    wifi_set_resolver_dns(1, 114, 114, 114, 114);
    wifi_set_resolver_dns(2, 8, 8, 8, 8);

    wifi_log_dns_info(ESP_NETIF_DNS_MAIN, "main");
    wifi_log_dns_info(ESP_NETIF_DNS_BACKUP, "backup");
    wifi_log_dns_info(ESP_NETIF_DNS_FALLBACK, "fallback");
}

static void bounded_copy(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0)) {
        return;
    }
    strlcpy(dst, src != NULL ? src : "", dst_size);
}

static void wifi_copy_field(uint8_t *dst, size_t dst_size, const char *src)
{
    size_t len;

    if ((dst == NULL) || (dst_size == 0)) {
        return;
    }

    memset(dst, 0, dst_size);
    if (src == NULL) {
        return;
    }

    len = strnlen(src, dst_size);
    memcpy(dst, src, len);
}

static const char *disconnect_reason_text(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
        return "连接失败：密码错误或认证失败";
    case WIFI_REASON_NO_AP_FOUND:
        return "连接失败：未找到该网络";
    case WIFI_REASON_ASSOC_LEAVE:
        return "WiFi 已断开";
    default:
        return "连接失败：请检查密码或信号";
    }
}

static int8_t wifi_get_connected_rssi(void)
{
    wifi_ap_record_t ap_info = {0};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

static void wifi_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open nvs failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs, NVS_KEY_SSID, ssid != NULL ? ssid : "");
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, NVS_KEY_PASS, password != NULL ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save credentials failed: %s", esp_err_to_name(err));
    }
    nvs_close(nvs);
}

static void wifi_clear_credentials(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open nvs for clear failed: %s", esp_err_to_name(err));
        return;
    }

    (void)nvs_erase_key(nvs, NVS_KEY_SSID);
    (void)nvs_erase_key(nvs, NVS_KEY_PASS);
    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "clear credentials failed: %s", esp_err_to_name(err));
    }
    nvs_close(nvs);
}

static esp_err_t wifi_load_credentials(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    nvs_handle_t nvs = 0;
    size_t ssid_len = ssid_size;
    size_t pass_len = password_size;
    esp_err_t err;

    if ((ssid == NULL) || (password == NULL) || (ssid_size == 0) || (password_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    ssid[0] = '\0';
    password[0] = '\0';

    err = nvs_open(NVS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_str(nvs, NVS_KEY_SSID, ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs, NVS_KEY_PASS, password, &pass_len);
    }
    nvs_close(nvs);

    if ((err == ESP_OK) && (ssid[0] == '\0')) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    return err;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        const wifi_event_sta_disconnected_t *event = (const wifi_event_sta_disconnected_t *)event_data;
        const char *message = disconnect_reason_text(event != NULL ? event->reason : 0);
        uint8_t reason = event != NULL ? event->reason : 0;

        if (s_reconnect_in_progress && (reason == WIFI_REASON_ASSOC_LEAVE)) {
            ESP_LOGI(TAG, "disconnected for reconnect");
            return;
        }

        if (s_disconnect_in_progress) {
            ESP_LOGI(TAG, "disconnected by user");
            s_disconnect_in_progress = false;
            s_pending_ssid[0] = '\0';
            s_pending_password[0] = '\0';
            backend_store_set_disconnected("WiFi 已断开");
            app_backend_notify_changed();
            return;
        }

        ESP_LOGW(TAG, "disconnected, reason=%u", event != NULL ? event->reason : 0);
        s_reconnect_in_progress = false;
        s_disconnect_in_progress = false;
        s_pending_ssid[0] = '\0';
        s_pending_password[0] = '\0';
        backend_store_set_wifi_state(APP_BACKEND_WIFI_FAILED, message);
        app_backend_notify_changed();
        return;
    }

    if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        char ip[APP_BACKEND_IP_MAX_LEN] = {0};
        wifi_config_t cfg = {0};
        int8_t rssi = wifi_get_connected_rssi();

        if (event != NULL) {
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        } else {
            bounded_copy(ip, sizeof(ip), "--");
        }

        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
            backend_store_set_connected((const char *)cfg.sta.ssid, ip, rssi);
        } else {
            backend_store_set_connected("", ip, rssi);
        }
        if (s_pending_ssid[0] != '\0') {
            wifi_save_credentials(s_pending_ssid, s_pending_password);
            s_pending_ssid[0] = '\0';
            s_pending_password[0] = '\0';
        }
        s_reconnect_in_progress = false;
        s_disconnect_in_progress = false;
        wifi_configure_fallback_dns();

        ESP_LOGI(TAG, "got ip %s", ip);
        app_backend_notify_changed();
        (void)app_backend_post_simple(APP_BACKEND_CMD_IP_READY);
    }
}

esp_err_t wifi_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (!s_netif_initialized) {
        esp_err_t err = esp_netif_init();
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
            ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
            return err;
        }

        err = esp_event_loop_create_default();
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
            ESP_LOGE(TAG, "event loop create failed: %s", esp_err_to_name(err));
            return err;
        }
        s_netif_initialized = true;
    }

    if (s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (s_sta_netif == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL),
                        TAG, "wifi handler register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL),
                        TAG, "ip handler register failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage set failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode set failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "wifi ps disable failed");

    s_initialized = true;
    ESP_LOGI(TAG, "wifi sta initialized");
    return ESP_OK;
}

esp_err_t wifi_service_scan(void)
{
    wifi_ap_record_t records[APP_BACKEND_MAX_APS] = {0};
    app_backend_wifi_ap_t aps[APP_BACKEND_MAX_APS] = {0};
    uint16_t ap_count = APP_BACKEND_MAX_APS;
    esp_err_t err;

    ESP_RETURN_ON_ERROR(wifi_service_init(), TAG, "wifi init failed");

    ESP_LOGI(TAG, "scan start");
    backend_store_set_wifi_state(APP_BACKEND_WIFI_SCANNING, "正在扫描 WiFi...");
    app_backend_notify_changed();

    err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan failed: %s", esp_err_to_name(err));
        backend_store_set_wifi_state(APP_BACKEND_WIFI_FAILED, "扫描失败，请重试");
        app_backend_notify_changed();
        return err;
    }

    err = esp_wifi_scan_get_ap_records(&ap_count, records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get ap records failed: %s", esp_err_to_name(err));
        backend_store_set_wifi_state(APP_BACKEND_WIFI_FAILED, "扫描失败，请重试");
        app_backend_notify_changed();
        return err;
    }

    for (uint16_t i = 0; i < ap_count; i++) {
        bounded_copy(aps[i].ssid, sizeof(aps[i].ssid), (const char *)records[i].ssid);
        aps[i].rssi = records[i].rssi;
        aps[i].authmode = (uint8_t)records[i].authmode;
        aps[i].selected = (i == 0);
    }

    char message[APP_BACKEND_MESSAGE_MAX_LEN] = {0};
    snprintf(message, sizeof(message), "扫描完成：找到 %u 个网络", (unsigned)ap_count);
    backend_store_set_scan_results(aps, ap_count, message);
    ESP_LOGI(TAG, "scan done, ap_count=%u", (unsigned)ap_count);
    app_backend_notify_changed();
    return ESP_OK;
}

esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {0};

    if ((ssid == NULL) || (ssid[0] == '\0')) {
        backend_store_set_wifi_state(APP_BACKEND_WIFI_FAILED, "连接失败：请输入 SSID");
        app_backend_notify_changed();
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(wifi_service_init(), TAG, "wifi init failed");

    wifi_copy_field(wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), ssid);
    wifi_copy_field(wifi_config.sta.password, sizeof(wifi_config.sta.password), password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    backend_store_set_wifi_state(APP_BACKEND_WIFI_CONNECTING, "正在连接 WiFi...");
    app_backend_notify_changed();
    bounded_copy(s_pending_ssid, sizeof(s_pending_ssid), ssid);
    bounded_copy(s_pending_password, sizeof(s_pending_password), password);
    s_reconnect_in_progress = true;

    ESP_LOGI(TAG, "connecting to %s", ssid);
    (void)esp_wifi_disconnect();
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        s_reconnect_in_progress = false;
        s_pending_ssid[0] = '\0';
        s_pending_password[0] = '\0';
        ESP_LOGE(TAG, "set sta config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "connect failed: %s", esp_err_to_name(err));
        s_reconnect_in_progress = false;
        s_pending_ssid[0] = '\0';
        s_pending_password[0] = '\0';
        backend_store_set_wifi_state(APP_BACKEND_WIFI_FAILED, "连接失败：请检查密码或信号");
        app_backend_notify_changed();
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_service_disconnect(bool forget_saved)
{
    esp_err_t err;

    ESP_RETURN_ON_ERROR(wifi_service_init(), TAG, "wifi init failed");

    if (forget_saved) {
        wifi_clear_credentials();
    }

    s_reconnect_in_progress = false;
    s_disconnect_in_progress = true;
    s_pending_ssid[0] = '\0';
    s_pending_password[0] = '\0';
    backend_store_set_disconnected("WiFi 已断开");
    app_backend_notify_changed();

    ESP_LOGI(TAG, "disconnect requested, forget_saved=%d", forget_saved ? 1 : 0);
    err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        s_disconnect_in_progress = false;
        ESP_LOGW(TAG, "disconnect returned: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t wifi_service_connect_saved(void)
{
    char ssid[APP_BACKEND_SSID_MAX_LEN + 1] = {0};
    char password[APP_BACKEND_PASSWORD_MAX_LEN + 1] = {0};
    esp_err_t err = wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password));

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "no saved wifi credentials");
        return err;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "load credentials failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "connecting saved ssid %s", ssid);
    return wifi_service_connect(ssid, password);
}
