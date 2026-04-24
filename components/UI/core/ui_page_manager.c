#include "ui_page_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#include "esp_log.h"
#include "ui_page_anim.h"
#include "ui_page_iface.h"
#include "ui_page_registry.h"
#include "ui_page_stack.h"

#define UI_MGR_TAG "UI_MGR"

static lv_obj_t *s_page_roots[UI_PAGE_MAX] = {0};

static bool ui_page_id_is_valid(ui_page_id_t id)
{
    return (id >= 0) && (id < UI_PAGE_MAX);
}

static const ui_page_t *ui_page_get_desc_or_log(ui_page_id_t id)
{
    const ui_page_t *desc = ui_page_registry_get(id);
    if (desc == NULL) {
        ESP_LOGW(UI_MGR_TAG, "page descriptor not found: id=%" PRId32, (int32_t)id);
    }
    return desc;
}

static esp_err_t ui_page_ensure_created(const ui_page_t *desc)
{
    if ((desc == NULL) || !ui_page_id_is_valid(desc->id)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_page_roots[desc->id] != NULL) {
        return ESP_OK;
    }

    if (desc->create == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_page_roots[desc->id] = desc->create();
    if (s_page_roots[desc->id] == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void ui_page_delete_root(ui_page_id_t id)
{
    lv_obj_t *root = NULL;

    if (!ui_page_id_is_valid(id)) {
        return;
    }

    root = s_page_roots[id];
    if (root == NULL) {
        return;
    }

    lv_obj_del(root);
    s_page_roots[id] = NULL;
}

static void ui_page_call_hide(const ui_page_t *desc)
{
    if ((desc != NULL) && (desc->on_hide != NULL)) {
        desc->on_hide();
    }
}

static void ui_page_call_show(const ui_page_t *desc, void *args)
{
    if ((desc != NULL) && (desc->on_show != NULL)) {
        desc->on_show(args);
    }
}

static void ui_page_destroy_if_needed(const ui_page_t *desc)
{
    lv_obj_t *root = NULL;

    if ((desc == NULL) || !ui_page_id_is_valid(desc->id)) {
        return;
    }

    if (desc->cache_mode != UI_PAGE_CACHE_NONE) {
        return;
    }

    root = s_page_roots[desc->id];
    if (root == NULL) {
        return;
    }

    if (desc->on_destroy != NULL) {
        desc->on_destroy();
    }

    lv_obj_del(root);
    s_page_roots[desc->id] = NULL;
}

ui_page_id_t ui_page_current(void)
{
    ui_page_id_t id = UI_PAGE_MAX;
    if (ui_page_stack_peek(&id) != ESP_OK) {
        return UI_PAGE_MAX;
    }
    return id;
}

void ui_page_dump_stack(void)
{
    const int depth = ui_page_stack_depth();
    const ui_page_id_t current = ui_page_current();
    ESP_LOGI(UI_MGR_TAG, "stack depth=%d current=%" PRId32, depth, (int32_t)current);
}

esp_err_t ui_page_push(ui_page_id_t id, void *args, ui_anim_t anim)
{
    const ui_page_id_t from_id = ui_page_current();
    const ui_page_t *from_desc = NULL;
    const ui_page_t *to_desc = ui_page_get_desc_or_log(id);
    const bool to_page_preexisting = ui_page_id_is_valid(id) && (s_page_roots[id] != NULL);
    bool from_page_hidden = false;
    esp_err_t err;

    if (to_desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    err = ui_page_ensure_created(to_desc);
    if (err != ESP_OK) {
        return err;
    }

    if (ui_page_id_is_valid(from_id)) {
        from_desc = ui_page_get_desc_or_log(from_id);
        ui_page_call_hide(from_desc);
        from_page_hidden = (from_desc != NULL);
    }

    err = ui_page_stack_push(id);
    if (err != ESP_OK) {
        if (!to_page_preexisting) {
            ui_page_delete_root(id);
        }
        if (from_page_hidden) {
            ui_page_call_show(from_desc, NULL);
        }
        return err;
    }

    ui_page_anim_load(s_page_roots[to_desc->id], anim);
    ui_page_call_show(to_desc, args);

    ESP_LOGI(UI_MGR_TAG, "PUSH %ld -> %ld depth=%d", (long)from_id, (long)id, ui_page_stack_depth());
    return ESP_OK;
}

esp_err_t ui_page_pop(ui_anim_t anim)
{
    ui_page_id_t current_id = UI_PAGE_MAX;
    ui_page_id_t target_id = UI_PAGE_MAX;
    const ui_page_t *current_desc = NULL;
    const ui_page_t *target_desc = NULL;
    esp_err_t err;

    if (ui_page_stack_depth() <= 1) {
        return ESP_ERR_INVALID_STATE;
    }

    err = ui_page_stack_peek(&current_id);
    if (err != ESP_OK) {
        return err;
    }

    current_desc = ui_page_get_desc_or_log(current_id);
    if (current_desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    err = ui_page_stack_pop(&current_id);
    if (err != ESP_OK) {
        return err;
    }

    err = ui_page_stack_peek(&target_id);
    if (err != ESP_OK) {
        ui_page_stack_push(current_id);
        return err;
    }

    err = ui_page_stack_push(current_id);
    if (err != ESP_OK) {
        return err;
    }

    target_desc = ui_page_get_desc_or_log(target_id);
    if (target_desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    err = ui_page_ensure_created(target_desc);
    if (err != ESP_OK) {
        return err;
    }

    /* Keep callback ordering consistent: hide while current page is still stack top. */
    ui_page_call_hide(current_desc);

    err = ui_page_stack_pop(&current_id);
    if (err != ESP_OK) {
        return err;
    }

    ui_page_anim_load(s_page_roots[target_desc->id], anim);
    ui_page_call_show(target_desc, NULL);
    ui_page_destroy_if_needed(current_desc);

    ESP_LOGI(UI_MGR_TAG, "POP %ld -> %ld depth=%d", (long)current_id, (long)target_id, ui_page_stack_depth());
    return ESP_OK;
}

esp_err_t ui_page_replace(ui_page_id_t id, void *args, ui_anim_t anim)
{
    ui_page_id_t old_id = UI_PAGE_MAX;
    const ui_page_t *old_desc = NULL;
    const ui_page_t *new_desc = ui_page_get_desc_or_log(id);
    esp_err_t err;

    if (new_desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    err = ui_page_ensure_created(new_desc);
    if (err != ESP_OK) {
        return err;
    }

    if (ui_page_stack_depth() == 0) {
        return ui_page_push(id, args, anim);
    }

    err = ui_page_stack_peek(&old_id);
    if (err != ESP_OK) {
        return err;
    }

    old_desc = ui_page_get_desc_or_log(old_id);
    if (old_desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    if (old_id == id) {
        /* Same-page replace refreshes callbacks but must not destroy active root. */
        ui_page_call_hide(old_desc);
        ui_page_anim_load(s_page_roots[new_desc->id], anim);
        ui_page_call_show(new_desc, args);
        ESP_LOGI(UI_MGR_TAG, "REPLACE %ld -> %ld (same) depth=%d", (long)old_id, (long)id, ui_page_stack_depth());
        return ESP_OK;
    }

    /* Keep callback ordering consistent: hide while old page is still stack top. */
    ui_page_call_hide(old_desc);

    err = ui_page_stack_pop(&old_id);
    if (err != ESP_OK) {
        return err;
    }

    err = ui_page_stack_push(id);
    if (err != ESP_OK) {
        ui_page_stack_push(old_id);
        return err;
    }

    ui_page_anim_load(s_page_roots[new_desc->id], anim);
    ui_page_call_show(new_desc, args);
    ui_page_destroy_if_needed(old_desc);

    ESP_LOGI(UI_MGR_TAG, "REPLACE %ld -> %ld depth=%d", (long)old_id, (long)id, ui_page_stack_depth());
    return ESP_OK;
}

esp_err_t ui_page_back_to_root(ui_anim_t anim)
{
    esp_err_t err = ESP_OK;

    while (ui_page_stack_depth() > 1) {
        err = ui_page_pop(anim);
        if (err != ESP_OK) {
            return err;
        }
    }

    ESP_LOGI(UI_MGR_TAG, "BACK_TO_ROOT depth=%d", ui_page_stack_depth());
    return ESP_OK;
}
