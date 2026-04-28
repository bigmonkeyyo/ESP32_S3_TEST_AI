#pragma once

#include "app_backend_types.h"
#include "ui_page_iface.h"

extern const ui_page_t g_page_firmware;

void page_firmware_apply_snapshot(const app_backend_snapshot_t *snapshot);
