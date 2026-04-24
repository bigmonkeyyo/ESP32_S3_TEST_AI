/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2026-04-24
 * @brief       LVGL 移植入口
 ****************************************************************************************************
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "iic.h"
#include "xl9555.h"
#include "led.h"
#include "lvgl_port.h"


static const char *TAG = "MAIN";
i2c_obj_t i2c0_master;

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init(); /* 初始化 NVS */

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();
    i2c0_master = iic_init(I2C_NUM_0); /* 初始化 IIC0 */
    xl9555_init(i2c0_master);           /* 初始化 IO 扩展芯片 */

    ESP_LOGI(TAG, "Starting LVGL middleware...");
    lvgl_port_start();

    /* 不应运行到这里，保底防止任务返回 */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
