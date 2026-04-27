#pragma once

#include "esp_err.h"
#include "app_backend_types.h"

esp_err_t weather_service_fetch_shanghai(app_backend_weather_t *out);
