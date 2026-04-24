#include "ui_page_stack.h"
#include <stdbool.h>

#define UI_PAGE_STACK_MAX_DEPTH 8

static ui_page_id_t s_stack[UI_PAGE_STACK_MAX_DEPTH];
static int s_top = -1;

static bool ui_page_id_is_valid(ui_page_id_t id)
{
    return (id >= 0) && (id < UI_PAGE_MAX);
}

esp_err_t ui_page_stack_reset(void)
{
    s_top = -1;
    return ESP_OK;
}

esp_err_t ui_page_stack_push(ui_page_id_t id)
{
    if (!ui_page_id_is_valid(id)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_top >= (UI_PAGE_STACK_MAX_DEPTH - 1)) {
        return ESP_ERR_NO_MEM;
    }

    s_stack[++s_top] = id;
    return ESP_OK;
}

esp_err_t ui_page_stack_pop(ui_page_id_t *out_id)
{
    if (out_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_top < 0) {
        return ESP_ERR_INVALID_STATE;
    }

    *out_id = s_stack[s_top--];
    return ESP_OK;
}

esp_err_t ui_page_stack_peek(ui_page_id_t *out_id)
{
    if (out_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_top < 0) {
        return ESP_ERR_INVALID_STATE;
    }

    *out_id = s_stack[s_top];
    return ESP_OK;
}

int ui_page_stack_depth(void)
{
    return s_top + 1;
}
