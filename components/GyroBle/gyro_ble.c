#include "gyro_ble.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

#define GYRO_BLE_DEVICE_NAME "ESP32-GYRO"
#define GYRO_BLE_CMD_START_STREAM 0x01
#define GYRO_BLE_CMD_STOP_STREAM  0x02
#define GYRO_BLE_CMD_RECALIBRATE  0x03
#define GYRO_BLE_UI_FRAME_PAYLOAD_LEN 14U
#define GYRO_BLE_UI_FRAME_MAX_CHUNKS  255U

static const char *TAG = "GYRO_BLE";
static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_pose_val_handle;
static uint16_t s_imu_val_handle;
static uint16_t s_quat_val_handle;
static uint16_t s_ctrl_val_handle;
static uint16_t s_ui_state_val_handle;
static bool s_started;
static bool s_streaming_enabled = true;
static bool s_advertising_enabled = true;
static bool s_pose_notify_enabled;
static bool s_imu_notify_enabled;
static bool s_quat_notify_enabled;
static bool s_ui_state_notify_enabled;
static bool s_recalibrate_pending;
static uint16_t s_seq;
static uint8_t s_ui_seq;
static uint8_t s_pose_frame[GYRO_BLE_FRAME_LEN];
static uint8_t s_imu_frame[GYRO_BLE_FRAME_LEN];
static uint8_t s_quat_frame[GYRO_BLE_FRAME_LEN];
static uint8_t s_ui_state_frame[GYRO_BLE_FRAME_LEN];
static SemaphoreHandle_t s_lock;
void ble_store_config_init(void);

static void gyro_ble_log_heap(const char *label)
{
    ESP_LOGI(TAG, "heap %s: internal free=%u largest=%u psram free=%u largest=%u",
             label,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

static const ble_uuid128_t s_uuid_service = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x00, 0x20, 0x3a, 0x9f);
static const ble_uuid128_t s_uuid_pose = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x01, 0x20, 0x3a, 0x9f);
static const ble_uuid128_t s_uuid_imu = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x02, 0x20, 0x3a, 0x9f);
static const ble_uuid128_t s_uuid_quat = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x04, 0x20, 0x3a, 0x9f);
static const ble_uuid128_t s_uuid_ctrl = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x03, 0x20, 0x3a, 0x9f);
static const ble_uuid128_t s_uuid_ui_state = BLE_UUID128_INIT(
    0x00, 0x10, 0x5a, 0x1f, 0x6e, 0x8b, 0x2b, 0x9f,
    0x7b, 0x4a, 0x9d, 0x5c, 0x05, 0x20, 0x3a, 0x9f);

static void gyro_ble_advertise(void);
static int gyro_ble_gap_event(struct ble_gap_event *event, void *arg);
static int gyro_ble_pose_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gyro_ble_imu_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gyro_ble_quat_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gyro_ble_ctrl_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gyro_ble_ui_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_uuid_service.u,
        .characteristics =
            (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &s_uuid_pose.u,
                    .access_cb = gyro_ble_pose_access_cb,
                    .val_handle = &s_pose_val_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = &s_uuid_imu.u,
                    .access_cb = gyro_ble_imu_access_cb,
                    .val_handle = &s_imu_val_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = &s_uuid_quat.u,
                    .access_cb = gyro_ble_quat_access_cb,
                    .val_handle = &s_quat_val_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = &s_uuid_ctrl.u,
                    .access_cb = gyro_ble_ctrl_access_cb,
                    .val_handle = &s_ctrl_val_handle,
                    .flags = BLE_GATT_CHR_F_WRITE,
                },
                {
                    .uuid = &s_uuid_ui_state.u,
                    .access_cb = gyro_ble_ui_state_access_cb,
                    .val_handle = &s_ui_state_val_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {0},
            },
    },
    {0},
};

static int16_t gyro_ble_to_i16(float value, float scale)
{
    float scaled = value * scale;

    if (scaled > 32767.0f) {
        scaled = 32767.0f;
    } else if (scaled < -32768.0f) {
        scaled = -32768.0f;
    }
    return (int16_t)lroundf(scaled);
}

static void gyro_ble_put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xffU);
    dst[1] = (uint8_t)((value >> 8) & 0xffU);
    dst[2] = (uint8_t)((value >> 16) & 0xffU);
    dst[3] = (uint8_t)((value >> 24) & 0xffU);
}

static void gyro_ble_put_i16_le(uint8_t *dst, int16_t value)
{
    dst[0] = (uint8_t)(value & 0xff);
    dst[1] = (uint8_t)((value >> 8) & 0xff);
}

static void gyro_ble_encode_pose_frame(const gyro_ble_sample_t *sample, uint8_t out[GYRO_BLE_FRAME_LEN])
{
    out[0] = GYRO_BLE_FRAME_VERSION;
    out[1] = sample->zero_ready ? 0x01U : 0x00U;
    gyro_ble_put_u32_le(&out[2], sample->timestamp_ms);
    gyro_ble_put_i16_le(&out[6], gyro_ble_to_i16(sample->roll_deg, 100.0f));
    gyro_ble_put_i16_le(&out[8], gyro_ble_to_i16(sample->pitch_deg, 100.0f));
    gyro_ble_put_i16_le(&out[10], gyro_ble_to_i16(sample->yaw_deg, 100.0f));
    gyro_ble_put_i16_le(&out[12], gyro_ble_to_i16(sample->ball_x_norm, 1000.0f));
    gyro_ble_put_i16_le(&out[14], gyro_ble_to_i16(sample->ball_y_norm, 1000.0f));
    gyro_ble_put_i16_le(&out[16], gyro_ble_to_i16(sample->ball_vx, 1000.0f));
    gyro_ble_put_i16_le(&out[18], gyro_ble_to_i16(sample->ball_vy, 1000.0f));
}

static void gyro_ble_encode_imu_frame(const gyro_ble_sample_t *sample,
                                      uint8_t out[GYRO_BLE_FRAME_LEN])
{
    out[0] = GYRO_BLE_FRAME_VERSION;
    out[1] = sample->zero_ready ? 0x01U : 0x00U;
    gyro_ble_put_u32_le(&out[2], sample->timestamp_ms);
    gyro_ble_put_i16_le(&out[6], gyro_ble_to_i16(sample->acc_x, 1000.0f));
    gyro_ble_put_i16_le(&out[8], gyro_ble_to_i16(sample->acc_y, 1000.0f));
    gyro_ble_put_i16_le(&out[10], gyro_ble_to_i16(sample->acc_z, 1000.0f));
    gyro_ble_put_i16_le(&out[12], gyro_ble_to_i16(sample->gyro_x, 1000.0f));
    gyro_ble_put_i16_le(&out[14], gyro_ble_to_i16(sample->gyro_y, 1000.0f));
    gyro_ble_put_i16_le(&out[16], gyro_ble_to_i16(sample->gyro_z, 1000.0f));
    gyro_ble_put_i16_le(&out[18], (int16_t)sample->sequence);
}

static void gyro_ble_encode_quat_frame(const gyro_ble_sample_t *sample,
                                       uint8_t out[GYRO_BLE_FRAME_LEN])
{
    out[0] = GYRO_BLE_FRAME_VERSION;
    out[1] = sample->zero_ready ? 0x01U : 0x00U;
    gyro_ble_put_u32_le(&out[2], sample->timestamp_ms);
    gyro_ble_put_i16_le(&out[6], gyro_ble_to_i16(sample->quat_w, 10000.0f));
    gyro_ble_put_i16_le(&out[8], gyro_ble_to_i16(sample->quat_x, 10000.0f));
    gyro_ble_put_i16_le(&out[10], gyro_ble_to_i16(sample->quat_y, 10000.0f));
    gyro_ble_put_i16_le(&out[12], gyro_ble_to_i16(sample->quat_z, 10000.0f));
    gyro_ble_put_i16_le(&out[14], gyro_ble_to_i16(sample->ball_x_norm, 1000.0f));
    gyro_ble_put_i16_le(&out[16], gyro_ble_to_i16(sample->ball_y_norm, 1000.0f));
    gyro_ble_put_i16_le(&out[18], (int16_t)sample->sequence);
}

static int gyro_ble_pose_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t frame[GYRO_BLE_FRAME_LEN];
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        memcpy(frame, s_quat_frame, sizeof(frame));
        xSemaphoreGive(s_lock);
    } else {
        memset(frame, 0, sizeof(frame));
    }
    return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gyro_ble_imu_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t frame[GYRO_BLE_FRAME_LEN];
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        memcpy(frame, s_imu_frame, sizeof(frame));
        xSemaphoreGive(s_lock);
    } else {
        memset(frame, 0, sizeof(frame));
    }
    return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gyro_ble_quat_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t frame[GYRO_BLE_FRAME_LEN];
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        memcpy(frame, s_pose_frame, sizeof(frame));
        xSemaphoreGive(s_lock);
    } else {
        memset(frame, 0, sizeof(frame));
    }
    return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gyro_ble_ctrl_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t cmd = 0;
    int rc;
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if ((ctxt->om == NULL) || (OS_MBUF_PKTLEN(ctxt->om) < 1)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = os_mbuf_copydata(ctxt->om, 0, 1, &cmd);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (cmd == GYRO_BLE_CMD_START_STREAM) {
        s_streaming_enabled = true;
        ESP_LOGI(TAG, "command: start stream");
    } else if (cmd == GYRO_BLE_CMD_STOP_STREAM) {
        s_streaming_enabled = false;
        ESP_LOGI(TAG, "command: stop stream");
    } else if (cmd == GYRO_BLE_CMD_RECALIBRATE) {
        s_recalibrate_pending = true;
        ESP_LOGI(TAG, "command: request recalibration");
    } else {
        ESP_LOGW(TAG, "unknown command: 0x%02x", cmd);
    }

    xSemaphoreGive(s_lock);
    return 0;
}

static int gyro_ble_ui_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t frame[GYRO_BLE_FRAME_LEN];
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        memcpy(frame, s_ui_state_frame, sizeof(frame));
        xSemaphoreGive(s_lock);
    } else {
        memset(frame, 0, sizeof(frame));
    }
    return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void gyro_ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name = ble_svc_gap_device_name();
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gyro_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising as %s", GYRO_BLE_DEVICE_NAME);
}

static void gyro_ble_restart_advertise_if_needed(void)
{
    if (!s_started || !s_advertising_enabled) {
        return;
    }

    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        if (!ble_gap_adv_active()) {
            gyro_ble_advertise();
        }
    }
}

static int gyro_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "gap connect event status=%d conn_handle=%u",
                 event->connect.status, (unsigned)event->connect.conn_handle);
        gyro_ble_log_heap("gap_connect_event");
        if (event->connect.status == 0) {
            if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
                s_conn_handle = event->connect.conn_handle;
                s_pose_notify_enabled = false;
                s_imu_notify_enabled = false;
                s_quat_notify_enabled = false;
                s_ui_state_notify_enabled = false;
                xSemaphoreGive(s_lock);
            }
            ESP_LOGI(TAG, "connected, conn_handle=%u", (unsigned)event->connect.conn_handle);
        } else {
            ESP_LOGW(TAG, "connect failed, status=%d", event->connect.status);
            if (s_advertising_enabled) {
                gyro_ble_advertise();
            }
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "gap disconnect event reason=%d conn_handle=%u",
                 event->disconnect.reason, (unsigned)event->disconnect.conn.conn_handle);
        gyro_ble_log_heap("gap_disconnect_event");
        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_pose_notify_enabled = false;
            s_imu_notify_enabled = false;
            s_quat_notify_enabled = false;
            s_ui_state_notify_enabled = false;
            xSemaphoreGive(s_lock);
        }
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
        if (s_advertising_enabled) {
            gyro_ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_advertising_enabled) {
            gyro_ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe attr=%u prev_notify=%d cur_notify=%d",
                 (unsigned)event->subscribe.attr_handle,
                 event->subscribe.prev_notify,
                 event->subscribe.cur_notify);
        gyro_ble_log_heap("gap_subscribe_event");
        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
            if (event->subscribe.attr_handle == s_pose_val_handle) {
                s_pose_notify_enabled = event->subscribe.cur_notify != 0;
                ESP_LOGI(TAG, "pose notify=%d", s_pose_notify_enabled ? 1 : 0);
            } else if (event->subscribe.attr_handle == s_imu_val_handle) {
                s_imu_notify_enabled = event->subscribe.cur_notify != 0;
                ESP_LOGI(TAG, "imu notify=%d", s_imu_notify_enabled ? 1 : 0);
            } else if (event->subscribe.attr_handle == s_quat_val_handle) {
                s_quat_notify_enabled = event->subscribe.cur_notify != 0;
                ESP_LOGI(TAG, "quat notify=%d", s_quat_notify_enabled ? 1 : 0);
            } else if (event->subscribe.attr_handle == s_ui_state_val_handle) {
                s_ui_state_notify_enabled = event->subscribe.cur_notify != 0;
                ESP_LOGI(TAG, "ui state notify=%d", s_ui_state_notify_enabled ? 1 : 0);
            }
            xSemaphoreGive(s_lock);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu updated: %d", event->mtu.value);
        gyro_ble_log_heap("gap_mtu_event");
        return 0;

    default:
        return 0;
    }
}

static void gyro_ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, reason=%d", reason);
}

static void gyro_ble_on_sync(void)
{
    uint8_t addr_val[6] = {0};
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(s_own_addr_type, addr_val, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "BLE addr: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }
    if (s_advertising_enabled) {
        gyro_ble_advertise();
    }
}

static void gyro_ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void gyro_ble_cleanup_controller_state(void)
{
    esp_bt_controller_status_t status = esp_bt_controller_get_status();

    if (status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        (void)esp_bt_controller_disable();
        status = esp_bt_controller_get_status();
    }
    if (status == ESP_BT_CONTROLLER_STATUS_INITED) {
        (void)esp_bt_controller_deinit();
    }
}

esp_err_t gyro_ble_start(void)
{
    int rc;

    if (s_started) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    gyro_ble_cleanup_controller_state();
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        gyro_ble_cleanup_controller_state();
        return ESP_FAIL;
    }
    ble_hs_cfg.reset_cb = gyro_ble_on_reset;
    ble_hs_cfg.sync_cb = gyro_ble_on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_svc_gap_device_name_set(GYRO_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    ble_store_config_init();
    nimble_port_freertos_init(gyro_ble_host_task);

    s_started = true;
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}

esp_err_t gyro_ble_set_advertising_enabled(bool enabled)
{
    if (!s_started || (s_lock == NULL)) {
        s_advertising_enabled = enabled;
        return ESP_OK;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_advertising_enabled = enabled;
    xSemaphoreGive(s_lock);

    if (!enabled) {
        if (ble_gap_adv_active()) {
            (void)ble_gap_adv_stop();
        }
        if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            (void)ble_gap_terminate(s_conn_handle, 0x13);
        }
        ESP_LOGI(TAG, "advertising disabled");
    } else {
        gyro_ble_restart_advertise_if_needed();
        ESP_LOGI(TAG, "advertising enabled");
    }

    return ESP_OK;
}

esp_err_t gyro_ble_set_streaming_enabled(bool enabled)
{
    if (!s_started || (s_lock == NULL)) {
        s_streaming_enabled = enabled;
        return ESP_OK;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_streaming_enabled = enabled;
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "streaming %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

void gyro_ble_publish_sample(const gyro_ble_sample_t *sample)
{
    uint8_t pose_frame[GYRO_BLE_FRAME_LEN];
    uint8_t imu_frame[GYRO_BLE_FRAME_LEN];
    uint8_t quat_frame[GYRO_BLE_FRAME_LEN];
    struct os_mbuf *om = NULL;
    uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
    bool notify_pose = false;
    bool notify_imu = false;
    bool notify_quat = false;
    uint16_t pose_handle = 0;
    uint16_t imu_handle = 0;
    uint16_t quat_handle = 0;
    uint16_t seq;
    int rc;

    if ((sample == NULL) || !s_started) {
        return;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    seq = s_seq++;
    gyro_ble_sample_t enriched = *sample;
    enriched.sequence = seq;
    gyro_ble_encode_pose_frame(&enriched, s_pose_frame);
    gyro_ble_encode_imu_frame(&enriched, s_imu_frame);
    gyro_ble_encode_quat_frame(&enriched, s_quat_frame);
    memcpy(pose_frame, s_pose_frame, sizeof(pose_frame));
    memcpy(imu_frame, s_imu_frame, sizeof(imu_frame));
    memcpy(quat_frame, s_quat_frame, sizeof(quat_frame));
    if ((s_conn_handle != BLE_HS_CONN_HANDLE_NONE) && s_streaming_enabled) {
        conn_handle = s_conn_handle;
        notify_pose = s_pose_notify_enabled;
        notify_imu = s_imu_notify_enabled;
        notify_quat = s_quat_notify_enabled;
        pose_handle = s_pose_val_handle;
        imu_handle = s_imu_val_handle;
        quat_handle = s_quat_val_handle;
    }

    xSemaphoreGive(s_lock);

    if (notify_pose) {
        om = ble_hs_mbuf_from_flat(pose_frame, sizeof(pose_frame));
        if (om != NULL) {
            rc = ble_gatts_notify_custom(conn_handle, pose_handle, om);
            if (rc != 0 && rc != BLE_HS_ENOTCONN) {
                ESP_LOGW(TAG, "pose notify failed: %d", rc);
            }
        }
    }

    if (notify_imu) {
        om = ble_hs_mbuf_from_flat(imu_frame, sizeof(imu_frame));
        if (om != NULL) {
            rc = ble_gatts_notify_custom(conn_handle, imu_handle, om);
            if (rc != 0 && rc != BLE_HS_ENOTCONN) {
                ESP_LOGW(TAG, "imu notify failed: %d", rc);
            }
        }
    }

    if (notify_quat) {
        om = ble_hs_mbuf_from_flat(quat_frame, sizeof(quat_frame));
        if (om != NULL) {
            rc = ble_gatts_notify_custom(conn_handle, quat_handle, om);
            if (rc != 0 && rc != BLE_HS_ENOTCONN) {
                ESP_LOGW(TAG, "quat notify failed: %d", rc);
            }
        }
    }
}

void gyro_ble_publish_ui_state_json(const char *json)
{
    uint8_t frame[GYRO_BLE_FRAME_LEN];
    uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
    uint16_t ui_state_handle = 0;
    bool notify_ui_state = false;
    size_t len;
    size_t chunk_count;
    uint8_t seq;

    if ((json == NULL) || !s_started) {
        return;
    }

    len = strlen(json);
    if (len > (GYRO_BLE_UI_FRAME_PAYLOAD_LEN * GYRO_BLE_UI_FRAME_MAX_CHUNKS)) {
        len = GYRO_BLE_UI_FRAME_PAYLOAD_LEN * GYRO_BLE_UI_FRAME_MAX_CHUNKS;
    }
    chunk_count = (len + GYRO_BLE_UI_FRAME_PAYLOAD_LEN - 1U) / GYRO_BLE_UI_FRAME_PAYLOAD_LEN;
    if (chunk_count == 0U) {
        chunk_count = 1U;
    }

    ESP_LOGI(TAG, "ui state publish request len=%u chunks=%u",
             (unsigned)len, (unsigned)chunk_count);
    gyro_ble_log_heap("ui_state_publish_request");

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    seq = s_ui_seq++;
    if ((s_conn_handle != BLE_HS_CONN_HANDLE_NONE) && s_ui_state_notify_enabled) {
        conn_handle = s_conn_handle;
        ui_state_handle = s_ui_state_val_handle;
        notify_ui_state = true;
    }
    xSemaphoreGive(s_lock);

    if (!notify_ui_state) {
        ESP_LOGI(TAG, "ui state publish skipped: conn=%u notify=%d handle=%u",
                 (unsigned)conn_handle, notify_ui_state ? 1 : 0, (unsigned)ui_state_handle);
        return;
    }

    for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
        const size_t offset = chunk * GYRO_BLE_UI_FRAME_PAYLOAD_LEN;
        const size_t remaining = (offset < len) ? (len - offset) : 0U;
        const size_t payload_len = remaining > GYRO_BLE_UI_FRAME_PAYLOAD_LEN ?
                                   GYRO_BLE_UI_FRAME_PAYLOAD_LEN : remaining;
        struct os_mbuf *om = NULL;
        int rc;

        memset(frame, 0, sizeof(frame));
        frame[0] = GYRO_BLE_FRAME_VERSION;
        frame[1] = GYRO_BLE_UI_FRAME_TYPE;
        frame[2] = seq;
        frame[3] = (uint8_t)chunk;
        frame[4] = (uint8_t)chunk_count;
        frame[5] = (uint8_t)payload_len;
        if (payload_len > 0U) {
            memcpy(&frame[6], &json[offset], payload_len);
        }

        if ((chunk == 0U) || (chunk + 1U == chunk_count)) {
            ESP_LOGI(TAG, "ui state notify seq=%u chunk=%u/%u payload=%u",
                     (unsigned)seq,
                     (unsigned)(chunk + 1U),
                     (unsigned)chunk_count,
                     (unsigned)payload_len);
        }

        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            memcpy(s_ui_state_frame, frame, sizeof(s_ui_state_frame));
            xSemaphoreGive(s_lock);
        }

        om = ble_hs_mbuf_from_flat(frame, sizeof(frame));
        if (om != NULL) {
            rc = ble_gatts_notify_custom(conn_handle, ui_state_handle, om);
            if (rc != 0 && rc != BLE_HS_ENOTCONN) {
                ESP_LOGW(TAG, "ui state notify failed: %d", rc);
                gyro_ble_log_heap("ui_state_notify_failed");
                return;
            }
        }
    }
    gyro_ble_log_heap("ui_state_publish_done");
}

bool gyro_ble_take_recalibrate_request(void)
{
    bool pending = false;

    if ((s_lock != NULL) && (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE)) {
        pending = s_recalibrate_pending;
        s_recalibrate_pending = false;
        xSemaphoreGive(s_lock);
    }
    return pending;
}
