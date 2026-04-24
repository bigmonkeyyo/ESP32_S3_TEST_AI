#pragma once

#include "lvgl.h"
#include "ui_page_ids.h"
#include "ui_types.h"

typedef struct ui_page {
    ui_page_id_t id;
    const char *name;
    ui_page_cache_mode_t cache_mode;
    lv_obj_t *(*create)(void);
    void (*on_show)(void *args);
    void (*on_hide)(void);
    void (*on_destroy)(void);
} ui_page_t;
