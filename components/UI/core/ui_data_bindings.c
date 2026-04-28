#include "ui_data_bindings.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "app_backend.h"
#include "ui_fonts.h"
#include "page_home.h"
#include "page_settings.h"
#include "page_status.h"
#include "page_wifi.h"

static const char *TAG = "UI_BINDING";
static volatile bool s_update_pending;
static bool s_initialized;
static bool s_ota_prompt_dismissed;
static char s_ota_prompt_target[APP_BACKEND_OTA_VERSION_LEN];
static lv_obj_t *s_ota_msgbox;

static void ui_data_bindings_close_ota_prompt(void)
{
    if (s_ota_msgbox != NULL) {
        lv_obj_del(s_ota_msgbox);
        s_ota_msgbox = NULL;
    }
}

static void ui_data_bindings_ota_msgbox_cb(lv_event_t *e)
{
    const char *btn_text = NULL;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    btn_text = lv_msgbox_get_active_btn_text(s_ota_msgbox);
    if ((btn_text != NULL) && (strcmp(btn_text, "更新") == 0)) {
        if (app_backend_ota_confirm_async() != ESP_OK) {
            ESP_LOGW(TAG, "OTA confirm enqueue failed");
        }
    } else {
        s_ota_prompt_dismissed = true;
    }

    ui_data_bindings_close_ota_prompt();
}

static void ui_data_bindings_update_ota_prompt(const app_backend_snapshot_t *snapshot)
{
    static const char *buttons[] = {"更新", "稍后", ""};
    char body[96] = {0};

    if (snapshot == NULL) {
        return;
    }

    if (snapshot->ota.state != APP_BACKEND_OTA_READY) {
        ui_data_bindings_close_ota_prompt();
        s_ota_prompt_dismissed = false;
        s_ota_prompt_target[0] = '\0';
        return;
    }

    if (strcmp(s_ota_prompt_target, snapshot->ota.target_version) != 0) {
        strlcpy(s_ota_prompt_target, snapshot->ota.target_version, sizeof(s_ota_prompt_target));
        s_ota_prompt_dismissed = false;
    }
    if (s_ota_prompt_dismissed || (s_ota_msgbox != NULL)) {
        return;
    }

    snprintf(body, sizeof(body), "检测到新版本 %s，是否现在更新？",
             snapshot->ota.target_version[0] != '\0' ? snapshot->ota.target_version : "--");
    s_ota_msgbox = lv_msgbox_create(NULL, "固件升级", body, buttons, false);
    lv_obj_set_width(s_ota_msgbox, 252);
    lv_obj_set_style_text_font(s_ota_msgbox, &ui_font_sc_14, 0);
    lv_obj_center(s_ota_msgbox);
    lv_obj_add_event_cb(s_ota_msgbox, ui_data_bindings_ota_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_data_bindings_apply_snapshot(void)
{
    app_backend_snapshot_t snapshot = {0};

    if (app_backend_get_snapshot(&snapshot) != ESP_OK) {
        ESP_LOGW(TAG, "snapshot get failed");
        return;
    }

    page_home_apply_snapshot(&snapshot);
    page_settings_apply_snapshot(&snapshot);
    page_wifi_apply_snapshot(&snapshot);
    page_status_apply_snapshot(&snapshot);
    ui_data_bindings_update_ota_prompt(&snapshot);
    ESP_LOGI(TAG, "snapshot applied");
}

static void ui_data_bindings_backend_cb(void *user_ctx)
{
    (void)user_ctx;
    ui_data_bindings_notify_async();
}

esp_err_t ui_data_bindings_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(app_backend_register_update_callback(ui_data_bindings_backend_cb, NULL),
                        TAG, "register backend callback failed");
    s_initialized = true;
    return ESP_OK;
}

void ui_data_bindings_notify_async(void)
{
    s_update_pending = true;
}

void ui_data_bindings_process_pending(void)
{
    if (!s_update_pending) {
        return;
    }

    s_update_pending = false;
    ui_data_bindings_apply_snapshot();
}
