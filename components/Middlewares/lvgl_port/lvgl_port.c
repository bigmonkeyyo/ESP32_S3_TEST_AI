#include "lvgl_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lcd.h"
#include "touch.h"


#define LVGL_TICK_PERIOD_MS      1
#define LVGL_TASK_PERIOD_MS      5
#define LVGL_DRAW_BUF_LINES      40

static const char *TAG = "LVGL_PORT";

static void lvgl_create_ui(void);
static void lvgl_port_disp_init(void);
static void lvgl_port_indev_init(void);
static void lvgl_tick_timer_cb(void *arg);
static void lvgl_disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_btn_event_cb(lv_event_t *e);

/**
 * @brief       LVGL 入口
 * @param       无
 * @retval      无
 */
void lvgl_port_start(void)
{
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_timer_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;

    lv_init();
    lvgl_port_disp_init();
    lvgl_port_indev_init();
    lvgl_create_ui();

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "LVGL started.");

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

/**
 * @brief       初始化并注册显示设备
 * @param       无
 * @retval      无
 */
static void lvgl_port_disp_init(void)
{
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;
    lcd_cfg_t lcd_config_info = {0};

    /* 先完成 LCD 初始化，后续可直接通过 panel_handle 刷屏 */
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

/**
 * @brief       初始化并注册输入设备（触摸）
 * @param       无
 * @retval      无
 */
static void lvgl_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    tp_dev.init();
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}

/**
 * @brief       显示刷新回调
 * @param       disp_drv  显示驱动
 * @param       area      刷新区域
 * @param       color_map 颜色数据
 * @retval      无
 */
static void lvgl_disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t handle = (esp_lcd_panel_handle_t)disp_drv->user_data;
    esp_lcd_panel_draw_bitmap(handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(disp_drv);
}

/**
 * @brief       触摸读取回调
 * @param       indev_drv 输入驱动
 * @param       data      输入数据
 * @retval      无
 */
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

/**
 * @brief       LVGL 时基回调
 * @param       arg 无
 * @retval      无
 */
static void lvgl_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/**
 * @brief       按钮回调，验证触摸输入通路
 * @param       e 事件
 * @retval      无
 */
static void lvgl_btn_event_cb(lv_event_t *e)
{
    static uint32_t click_cnt = 0;
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    click_cnt++;
    lv_label_set_text_fmt(label, "Touch Count: %lu", (unsigned long)click_cnt);
}

/**
 * @brief       创建最小可运行 UI
 * @param       无
 * @retval      无
 */
static void lvgl_create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Port OK");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, "Touch Count: 0");
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 150, 56);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(btn, lvgl_btn_event_cb, LV_EVENT_CLICKED, status);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap Here");
    lv_obj_center(btn_label);
}
