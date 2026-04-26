#include "lvgl_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"
#include "lcd.h"
#include "touch.h"
#include "ui.h"

#define LVGL_TICK_PERIOD_MS      1
#define LVGL_TASK_PERIOD_MS      5
#define LVGL_DRAW_BUF_LINES      120
#define LVGL_BACKLIGHT_FALLBACK_MS 1500

static const char *TAG = "LVGL_PORT";
static volatile bool s_first_flush_done = false;
static bool s_backlight_on = false;
static int64_t s_lvgl_start_us = 0;

static void lvgl_port_disp_init(void);
static void lvgl_port_indev_init(void);
static void lvgl_tick_timer_cb(void *arg);
static bool lvgl_notify_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                                       esp_lcd_panel_io_event_data_t *edata,
                                       void *user_ctx);
static void lvgl_disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

void lvgl_port_start(void)
{
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_timer_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_err_t err;

    lv_init();
    lvgl_port_disp_init();
    lvgl_port_indev_init();

    err = ui_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UI init failed: %s", esp_err_to_name(err));
        abort();
    }

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
    s_lvgl_start_us = esp_timer_get_time();

    ESP_LOGI(TAG, "LVGL started.");

    while (1)
    {
        const bool wait_too_long = ((esp_timer_get_time() - s_lvgl_start_us) >
                                    (LVGL_BACKLIGHT_FALLBACK_MS * 1000));
        lv_timer_handler();
        if ((s_first_flush_done || wait_too_long) && !s_backlight_on)
        {
            LCD_BL(1);
            s_backlight_on = true;
            if (s_first_flush_done)
            {
                ESP_LOGI(TAG, "Display first frame ready, backlight on.");
            }
            else
            {
                ESP_LOGW(TAG, "Backlight fallback fired after %d ms.", LVGL_BACKLIGHT_FALLBACK_MS);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

static void lvgl_port_disp_init(void)
{
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;
    lcd_cfg_t lcd_config_info = {
        .user_ctx = &disp_drv,
        .notify_flush_ready = lvgl_notify_flush_ready_cb,
    };

    lcd_init(lcd_config_info);

    const size_t buf_pixels = lcd_dev.width * LVGL_DRAW_BUF_LINES;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);

    if (buf1 == NULL || buf2 == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers.");
        abort();
    }

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_pixels);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = lcd_dev.width;
    disp_drv.ver_res = lcd_dev.height;
    disp_drv.flush_cb = lvgl_disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);
}

static void lvgl_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    tp_dev.init();
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}

static void lvgl_disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t handle = (esp_lcd_panel_handle_t)disp_drv->user_data;
    esp_err_t err = esp_lcd_panel_draw_bitmap(handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
        lv_disp_flush_ready(disp_drv);
    }
}

static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    (void)indev_drv;

    tp_dev.scan(0);
    if (tp_dev.sta & TP_PRES_DOWN)
    {
        last_x = tp_dev.x[0];
        last_y = tp_dev.y[0];
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

static void lvgl_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static bool lvgl_notify_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                                       esp_lcd_panel_io_event_data_t *edata,
                                       void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    (void)panel_io;
    (void)edata;

    s_first_flush_done = true;
    lv_disp_flush_ready(disp_drv);
    return false;
}

