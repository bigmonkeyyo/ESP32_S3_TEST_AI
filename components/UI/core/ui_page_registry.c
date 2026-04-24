#include "ui_page_registry.h"
#include <stddef.h>

static const ui_page_t *s_pages[UI_PAGE_MAX] = {0};
static const ui_page_id_t s_root_page_id = UI_PAGE_HOME;

const ui_page_t *ui_page_registry_get(ui_page_id_t id)
{
    if ((id < 0) || (id >= UI_PAGE_MAX)) {
        return NULL;
    }

    return s_pages[id];
}

const ui_page_t *ui_page_registry_root(void)
{
    return ui_page_registry_get(s_root_page_id);
}
