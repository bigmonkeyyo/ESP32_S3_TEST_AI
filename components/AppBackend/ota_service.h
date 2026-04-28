#pragma once

#include "esp_err.h"

esp_err_t ota_service_check_update(void);
esp_err_t ota_service_report_current_version(void);
esp_err_t ota_service_confirm_update(void);
esp_err_t ota_service_restart_now(void);
