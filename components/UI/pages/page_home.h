#pragma once

#include "app_backend_types.h"
#include "ui_page_iface.h"

extern const ui_page_t g_page_home;

void page_home_apply_snapshot(const app_backend_snapshot_t *snapshot);
