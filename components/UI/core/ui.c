#include "ui.h"

#include <stdbool.h>

#include "esp_log.h"
#include "ui_data_bindings.h"
#include "ui_mirror.h"
#include "ui_page_registry.h"
#include "ui_page_stack.h"

static const char *TAG = "UI";
static bool s_ui_initialized = false;

esp_err_t ui_init(void)
{
    const ui_page_t *root = NULL;
    esp_err_t err;

    if (s_ui_initialized) {
        ESP_LOGW(TAG, "ui_init called more than once, skip.");
        return ESP_OK;
    }

    root = ui_page_registry_root();
    if (root == NULL) {
        ESP_LOGE(TAG, "root page not registered.");
        return ESP_ERR_NOT_FOUND;
    }

    err = ui_page_stack_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ui_page_stack_reset failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ui_page_push(root->id, NULL, UI_ANIM_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initial root push failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ui_data_bindings_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ui_data_bindings_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ui_mirror_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ui_mirror_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ui_mirror_notify_page_changed();

    s_ui_initialized = true;
    ESP_LOGI(TAG, "UI initialized, root page id=%ld", (long)root->id);
    return ESP_OK;
}
