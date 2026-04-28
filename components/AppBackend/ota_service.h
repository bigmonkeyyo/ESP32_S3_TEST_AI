#pragma once

#include "esp_err.h"

esp_err_t ota_service_start(void);
esp_err_t ota_service_confirm_update(void);
