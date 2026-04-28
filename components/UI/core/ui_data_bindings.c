#include "ui_data_bindings.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "app_backend.h"
#include "ui_fonts.h"
#include "page_firmware.h"
#include "page_home.h"
#include "page_settings.h"
#include "page_status.h"
#include "page_wifi.h"

static const char *TAG = "UI_BINDING";
static volatile bool s_update_pending;
static bool s_initialized;

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
    page_firmware_apply_snapshot(&snapshot);
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
