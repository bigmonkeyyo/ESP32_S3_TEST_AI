#pragma once

#include "esp_err.h"

esp_err_t mqtt_service_start(void);
esp_err_t mqtt_service_publish_status(void);

