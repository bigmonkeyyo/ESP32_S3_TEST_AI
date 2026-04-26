#include "ui_page_registry.h"
#include <stddef.h>

#include "page_about.h"
#include "page_home.h"
#include "page_settings.h"
#include "page_status.h"
#include "page_wifi.h"

static const ui_page_t *s_pages[UI_PAGE_MAX] = {
    [UI_PAGE_HOME] = &g_page_home,
    [UI_PAGE_SETTINGS] = &g_page_settings,
    [UI_PAGE_STATUS] = &g_page_status,
    [UI_PAGE_WIFI] = &g_page_wifi,
    [UI_PAGE_ABOUT] = &g_page_about,
};
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
