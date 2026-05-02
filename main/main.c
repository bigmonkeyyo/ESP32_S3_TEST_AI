/**
 ****************************************************************************************************
 * @file        main.c
 * @author      姝ｇ偣鍘熷瓙鍥㈤槦(ALIENTEK)
 * @version     V1.0
 * @date        2026-04-24
 * @brief       LVGL 绉绘鍏ュ彛
 ****************************************************************************************************
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <math.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "iic.h"
#include "xl9555.h"
#include "led.h"
#include "lvgl_port.h"
#include "qmi8658a.h"
#include "imu.h"
#include "page_gyro.h"


static const char *TAG = "MAIN";
i2c_obj_t i2c0_master;
static const char *GYRO_TAG = "GYRO";
static void gyro_monitor_task(void *arg);

#define GYRO_SAMPLE_MS            20
#define GYRO_LOG_EVERY_N_SAMPLES  25
#define GYRO_BALL_TILT_DEG        12.0f
#define GYRO_BALL_FORCE_GAIN      7.5f
#define GYRO_BALL_DAMPING         0.992f
#define GYRO_BALL_MAX_SPEED       2.2f
#define GYRO_BALL_RESTITUTION     0.72f
#define GYRO_ZERO_CALIB_SAMPLES   80U
#define GYRO_TILT_DEADZONE        0.08f
#define RAD_TO_DEG                57.2957795f

static float clampf_range(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void gyro_init_task(void *arg)
{
    (void)arg;

    if (qmi8658_init(i2c0_master) == 0)
    {
        ESP_LOGI(TAG, "QMI8658A init ok");
        if (xTaskCreate(gyro_monitor_task, "gyro_monitor", 4096, NULL, 4, NULL) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create gyro monitor task");
        }
    }
    else
    {
        ESP_LOGE(TAG, "QMI8658A init failed");
    }

    vTaskDelete(NULL);
}

static void gyro_monitor_task(void *arg)
{
    float acc[3] = {0.0f};
    float gyro[3] = {0.0f};
    float rpy[3] = {0.0f};
    float ball_x = 0.0f;
    float ball_y = 0.0f;
    float ball_vx = 0.0f;
    float ball_vy = 0.0f;
    float zero_roll = 0.0f;
    float zero_pitch = 0.0f;
    float zero_roll_sum = 0.0f;
    float zero_pitch_sum = 0.0f;
    uint32_t zero_count = 0;
    bool zero_ready = false;
    uint32_t sample_count = 0;
    uint32_t last_bounce_sample = 0;
    const float dt_s = (float)GYRO_SAMPLE_MS / 1000.0f;

    (void)arg;

    while (1)
    {
        float tilt_x;
        float tilt_y;
        float tilt_roll_deg;
        float tilt_pitch_deg;
        float acc_norm;
        float accel_x;
        float accel_y;
        bool hit_x = false;
        bool hit_y = false;

        qmi8658_read_xyz(acc, gyro);
        imu_get_eulerian_angles(acc, gyro, rpy, dt_s);
        acc_norm = sqrtf((acc[0] * acc[0]) + (acc[1] * acc[1]) + (acc[2] * acc[2]));
        if (acc_norm < 0.2f) {
            vTaskDelay(pdMS_TO_TICKS(GYRO_SAMPLE_MS));
            continue;
        }

        tilt_roll_deg = atan2f(acc[1], acc[2]) * RAD_TO_DEG;
        tilt_pitch_deg = atan2f(-acc[0], sqrtf((acc[1] * acc[1]) + (acc[2] * acc[2]))) * RAD_TO_DEG;

        if (!zero_ready)
        {
            if ((acc_norm > 0.7f) && (acc_norm < 1.3f)) {
                zero_roll_sum += tilt_roll_deg;
                zero_pitch_sum += tilt_pitch_deg;
                zero_count++;
            }
            if (zero_count >= GYRO_ZERO_CALIB_SAMPLES)
            {
                zero_roll = zero_roll_sum / (float)zero_count;
                zero_pitch = zero_pitch_sum / (float)zero_count;
                zero_ready = true;
                ESP_LOGI(GYRO_TAG, "zero calibrated: roll=%.2f pitch=%.2f", zero_roll, zero_pitch);
            }
            page_gyro_set_ball_norm(0.0f, 0.0f);
            vTaskDelay(pdMS_TO_TICKS(GYRO_SAMPLE_MS));
            continue;
        }

        tilt_x = clampf_range((tilt_roll_deg - zero_roll) / GYRO_BALL_TILT_DEG, -1.0f, 1.0f);
        tilt_y = clampf_range((tilt_pitch_deg - zero_pitch) / GYRO_BALL_TILT_DEG, -1.0f, 1.0f);
        if (fabsf(tilt_x) < GYRO_TILT_DEADZONE) {
            tilt_x = 0.0f;
        }
        if (fabsf(tilt_y) < GYRO_TILT_DEADZONE) {
            tilt_y = 0.0f;
        }
        accel_x = -tilt_x * GYRO_BALL_FORCE_GAIN;
        accel_y = tilt_y * GYRO_BALL_FORCE_GAIN;

        ball_vx = (ball_vx + (accel_x * dt_s)) * GYRO_BALL_DAMPING;
        ball_vy = (ball_vy + (accel_y * dt_s)) * GYRO_BALL_DAMPING;
        ball_vx = clampf_range(ball_vx, -GYRO_BALL_MAX_SPEED, GYRO_BALL_MAX_SPEED);
        ball_vy = clampf_range(ball_vy, -GYRO_BALL_MAX_SPEED, GYRO_BALL_MAX_SPEED);
        ball_x += ball_vx * dt_s;
        ball_y += ball_vy * dt_s;

        if (ball_x > 1.0f) {
            ball_x = 1.0f;
            if (ball_vx > 0.0f) {
                if (accel_x > 0.0f) {
                    ball_vx = 0.0f;
                } else {
                    ball_vx = -ball_vx * GYRO_BALL_RESTITUTION;
                    hit_x = true;
                }
            }
        } else if (ball_x < -1.0f) {
            ball_x = -1.0f;
            if (ball_vx < 0.0f) {
                if (accel_x < 0.0f) {
                    ball_vx = 0.0f;
                } else {
                    ball_vx = -ball_vx * GYRO_BALL_RESTITUTION;
                    hit_x = true;
                }
            }
        }

        if (ball_y > 1.0f) {
            ball_y = 1.0f;
            if (ball_vy > 0.0f) {
                if (accel_y > 0.0f) {
                    ball_vy = 0.0f;
                } else {
                    ball_vy = -ball_vy * GYRO_BALL_RESTITUTION;
                    hit_y = true;
                }
            }
        } else if (ball_y < -1.0f) {
            ball_y = -1.0f;
            if (ball_vy < 0.0f) {
                if (accel_y < 0.0f) {
                    ball_vy = 0.0f;
                } else {
                    ball_vy = -ball_vy * GYRO_BALL_RESTITUTION;
                    hit_y = true;
                }
            }
        }

        page_gyro_set_ball_norm(ball_x, ball_y);

        if ((hit_x || hit_y) && ((sample_count - last_bounce_sample) >= 10U))
        {
            ESP_LOGI(GYRO_TAG, "bounce: hit_x=%d hit_y=%d ball=[%.2f, %.2f] vel=[%.2f, %.2f]",
                     hit_x ? 1 : 0, hit_y ? 1 : 0, ball_x, ball_y, ball_vx, ball_vy);
            last_bounce_sample = sample_count;
        }

        if ((sample_count++ % GYRO_LOG_EVERY_N_SAMPLES) == 0)
        {
            ESP_LOGI(GYRO_TAG,
                     "acc=[%.3f, %.3f, %.3f] gyro=[%.3f, %.3f, %.3f] rpy=[%.2f, %.2f, %.2f] ball=[%.2f, %.2f] vel=[%.2f, %.2f]",
                     acc[0], acc[1], acc[2], gyro[0], gyro[1], gyro[2], rpy[0], rpy[1], rpy[2],
                     ball_x, ball_y, ball_vx, ball_vy);
        }

        vTaskDelay(pdMS_TO_TICKS(GYRO_SAMPLE_MS));
    }
}

/**
 * @brief       绋嬪簭鍏ュ彛
 * @param       鏃? * @retval      鏃? */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init(); /* 鍒濆鍖?NVS */

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();
    i2c0_master = iic_init(I2C_NUM_0); /* 鍒濆鍖?IIC0 */
    xl9555_init(i2c0_master);           /* 鍒濆鍖?IO 鎵╁睍鑺墖 */

    if (xTaskCreate(gyro_init_task, "gyro_init", 4096, NULL, 4, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create gyro init task");
    }

    ESP_LOGI(TAG, "Starting LVGL middleware...");
    lvgl_port_start();

    /* 涓嶅簲杩愯鍒拌繖閲岋紝淇濆簳闃叉浠诲姟杩斿洖 */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

