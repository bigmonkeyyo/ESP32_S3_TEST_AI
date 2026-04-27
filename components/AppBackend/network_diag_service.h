#pragma once

#include "esp_err.h"

typedef enum {
    NETWORK_DIAG_STATUS_UNKNOWN = 0,
    NETWORK_DIAG_STATUS_OPEN,
    NETWORK_DIAG_STATUS_CAPTIVE_PORTAL,
} network_diag_status_t;

esp_err_t network_diag_probe_http(network_diag_status_t *status_out);

