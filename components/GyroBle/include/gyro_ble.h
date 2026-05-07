#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define GYRO_BLE_FRAME_VERSION 1U
#define GYRO_BLE_FRAME_LEN     20U

typedef struct {
    uint32_t timestamp_ms;
    uint16_t sequence;
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    float quat_w;
    float quat_x;
    float quat_y;
    float quat_z;
    float ball_x_norm;
    float ball_y_norm;
    float ball_vx;
    float ball_vy;
    float acc_x;
    float acc_y;
    float acc_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    bool zero_ready;
} gyro_ble_sample_t;

esp_err_t gyro_ble_start(void);
void gyro_ble_publish_sample(const gyro_ble_sample_t *sample);
bool gyro_ble_take_recalibrate_request(void);
