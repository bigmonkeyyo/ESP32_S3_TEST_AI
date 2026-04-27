# WiFi 时间天气后端设计（18_touch / ESP32-S3）

- 日期：2026-04-27
- 工程：`D:\ESP32-APP\18_touch`
- 目标：落地第一版真实后端能力，实现 WiFi 扫描、WiFi 连接、联网后同步真实时间和上海天气，并把数据适配到现有 LVGL 前端页面。
- 选定方案：方案 A，新增独立 `AppBackend` 组件，前端只通过后端接口发起动作和读取状态。

## 1. 范围

第一版必须完成：

1. WiFi 页从模拟 AP 列表改为真实扫描结果。
2. WiFi 页输入 SSID/密码后执行真实 STA 连接。
3. 连接成功后获取真实 IP、RSSI、连接状态。
4. 连接成功后执行 SNTP 时间同步。
5. 时间同步或网络可用后请求上海天气数据。
6. Home 页显示真实时间、城市天气、温度、湿度、风速/风向。
7. Status 页显示真实 IP、WiFi 信号、运行时长、上次同步时间。
8. WiFi 页显示扫描中、连接中、成功、失败等状态。

第一版不做：

1. 自动定位城市。
2. 多城市管理。
3. 配网二维码或 SoftAP 配网。
4. 天气 API key 管理。
5. 空气质量 API 的真实 AQI 接入。Home 页 AQI 胶囊第一版保留占位或显示 `AQI --`。
6. OTA、MQTT、云端账户体系。

## 2. 约束和现状

当前 UI 已有多页面框架：

- `components/UI/core/ui_page_manager.c` 负责页面栈、动画、生命周期。
- `components/UI/pages/page_wifi.c` 目前使用假 AP 列表和假密码校验。
- `components/UI/pages/page_home.c` 目前使用假时间、假天气。
- `components/UI/pages/page_status.c` 目前使用假 IP、假运行状态。
- `components/Middlewares/lvgl_port/lvgl_port.c` 是 LVGL 任务循环所在地。

关键约束：

1. 后端任务不能直接跨线程操作 LVGL 对象。
2. 页面跳转必须继续走 `ui_page_manager`。
3. 网络、HTTP、SNTP 不能阻塞 LVGL 主循环。
4. 第一版必须保持现有页面布局风格，不重做 UI。
5. 现有工作区可能有未提交的 UI 性能调优改动，实现时不得回退这些改动。

## 3. 外部数据源

天气第一版使用 Open-Meteo。

选择原因：

1. 官方提供免费非商业访问。
2. 不需要 API key、注册或信用卡。
3. 返回 JSON，适合 ESP-IDF `esp_http_client` + `cJSON` 解析。
4. 支持按经纬度查询，第一版可固定上海坐标，避免城市搜索复杂度。

第一版固定位置：

```text
city: 上海
latitude: 31.2304
longitude: 121.4737
timezone: Asia/Shanghai
```

第一版请求 URL：

```text
https://api.open-meteo.com/v1/forecast?latitude=31.2304&longitude=121.4737&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m&timezone=Asia%2FShanghai
```

官方资料：

- https://open-meteo.com/
- https://open-meteo.com/en/docs
- https://open-meteo.com/en/docs/geocoding-api

## 4. 总体架构

新增组件：

```text
components/AppBackend/
  CMakeLists.txt
  include/
    app_backend.h
    app_backend_types.h
  app_backend.c
  backend_store.c
  backend_store.h
  wifi_service.c
  wifi_service.h
  time_service.c
  time_service.h
  weather_service.c
  weather_service.h
```

职责：

1. `app_backend`
   - 后端总入口。
   - 初始化 `esp_netif`、`esp_event`、WiFi service、状态仓库。
   - 创建后端工作任务。
   - 对 UI 暴露扫描、连接、刷新天气等异步接口。

2. `backend_store`
   - 保存后端最新状态快照。
   - 提供线程安全的 get/set。
   - 保存 WiFi 状态、扫描结果、时间状态、天气状态、错误信息。

3. `wifi_service`
   - 封装 ESP-IDF WiFi STA 初始化。
   - 执行真实扫描。
   - 执行连接、断线处理、重试、IP 获取。
   - 将 SSID/密码保存到 NVS，下一次启动可自动尝试连接。

4. `time_service`
   - 使用 SNTP 同步网络时间。
   - 设置时区为 `CST-8` 或等效 `Asia/Shanghai` 规则。
   - 提供格式化时间字符串给 UI。

5. `weather_service`
   - 封装天气源实现。
   - 第一版实现 Open-Meteo provider。
   - 后续更换天气源时，只改 provider，不改 UI 页面。

## 5. 前后端接口

UI 调用后端：

```c
esp_err_t app_backend_start(void);
esp_err_t app_backend_wifi_scan_async(void);
esp_err_t app_backend_wifi_connect_async(const char *ssid, const char *password);
esp_err_t app_backend_weather_refresh_async(void);
esp_err_t app_backend_get_snapshot(app_backend_snapshot_t *out);
```

后端状态类型：

```c
typedef enum {
    APP_BACKEND_WIFI_IDLE = 0,
    APP_BACKEND_WIFI_SCANNING,
    APP_BACKEND_WIFI_CONNECTING,
    APP_BACKEND_WIFI_CONNECTED,
    APP_BACKEND_WIFI_FAILED,
    APP_BACKEND_WIFI_DISCONNECTED,
} app_backend_wifi_state_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;
    bool selected;
} app_backend_wifi_ap_t;

typedef struct {
    char city[16];
    char condition[24];
    char temperature[16];
    char humidity[16];
    char wind[24];
    char aqi[12];
    char updated_at[16];
    bool valid;
} app_backend_weather_t;

typedef struct {
    app_backend_wifi_state_t wifi_state;
    app_backend_wifi_ap_t aps[8];
    size_t ap_count;
    char connected_ssid[33];
    char ip[16];
    int8_t rssi;
    char now_time[8];
    char uptime[16];
    char last_sync[16];
    app_backend_weather_t weather;
    char message[64];
} app_backend_snapshot_t;
```

UI 刷新策略：

1. 页面 `on_show` 时调用 `app_backend_get_snapshot()`，把当前快照渲染出来。
2. 后端状态变化时，投递一个 LVGL 安全刷新请求。
3. UI 刷新函数必须运行在 LVGL 任务上下文。
4. 如果页面当前未创建或已销毁，刷新函数只更新 store，不直接访问空指针。

建议新增 UI 数据绑定层：

```text
components/UI/include/ui_data_bindings.h
components/UI/core/ui_data_bindings.c
```

职责：

1. 接收后端快照。
2. 判断当前页面和页面对象是否存在。
3. 调用各页面暴露的轻量刷新函数。
4. 保证后端不直接 include 具体页面内部对象。

页面新增接口示例：

```c
void page_home_apply_snapshot(const app_backend_snapshot_t *snapshot);
void page_wifi_apply_snapshot(const app_backend_snapshot_t *snapshot);
void page_status_apply_snapshot(const app_backend_snapshot_t *snapshot);
```

## 6. 数据流

启动：

```text
app_main
  -> nvs_flash_init
  -> led/iic/xl9555 init
  -> lvgl_port_start
      -> lv_init
      -> display/touch init
      -> ui_init
      -> app_backend_start
      -> lv_timer_handler loop
```

WiFi 扫描：

```text
WiFi 页点击“扫描”
  -> app_backend_wifi_scan_async
  -> 后端任务执行 esp_wifi_scan_start
  -> esp_wifi_scan_get_ap_records
  -> backend_store 更新 AP 列表
  -> UI 安全刷新 WiFi 页列表
```

WiFi 连接：

```text
WiFi 页点击“连接WiFi”
  -> app_backend_wifi_connect_async(ssid, password)
  -> 后端任务设置 STA config 并 esp_wifi_connect
  -> WIFI_EVENT_STA_CONNECTED / DISCONNECTED
  -> IP_EVENT_STA_GOT_IP
  -> backend_store 更新 connected_ssid / ip / rssi
  -> 保存凭据到 NVS
  -> 触发 SNTP 同步和天气刷新
```

时间天气刷新：

```text
IP ready
  -> time_service_sync
  -> weather_service_fetch_shanghai
  -> backend_store 更新 now_time / last_sync / weather
  -> UI 安全刷新 Home / Status
```

## 7. UI 适配计划

`page_wifi.c`：

1. 删除 `k_wifi_aps` 假数据和假密码校验。
2. 扫描按钮调用 `app_backend_wifi_scan_async()`。
3. AP 行来自 `snapshot.aps`。
4. 点击 AP 填充 SSID。
5. 连接按钮调用 `app_backend_wifi_connect_async()`。
6. 状态条显示：
   - `正在扫描 WiFi...`
   - `扫描完成：找到 N 个网络`
   - `正在连接：<ssid>`
   - `连接成功：WiFi 已连接`
   - `连接失败：<原因>`

`page_home.c`：

1. 时间 label 改为从 `snapshot.now_time` 刷新。
2. 城市天气 label 显示 `上海 · <天气>`。
3. 温度主文案显示 `<天气>  <温度>`。
4. 胶囊显示湿度、风、AQI 占位。
5. 未联网时显示原有默认占位或 `未联网`。

`page_status.c`：

1. WiFi 信号显示 RSSI，例如 `-54 dBm`。
2. IP 地址显示真实 STA IP。
3. 运行时长来自后端 uptime。
4. 上次同步来自天气或时间同步完成时间。
5. 固件版本第一版可继续显示静态版本，后续再接 `esp_app_get_description()`。

## 8. 错误处理

WiFi 扫描失败：

- UI 显示 `扫描失败，请重试`。
- 日志输出 `BACKEND_WIFI: scan failed: <err>`。

WiFi 连接失败：

- UI 显示 `连接失败：密码错误或信号弱`。
- 后端记录最近错误。
- 不清空用户输入。

WiFi 断开：

- UI 状态变为 `WiFi 已断开`。
- 后端可尝试有限次数重连。
- 第一版不做复杂退避策略。

SNTP 失败：

- Status 显示 `时间未同步`。
- 继续允许天气请求。

天气请求失败：

- Home 显示 `天气同步失败` 或保留上一次有效天气。
- Status 的 WiFi/IP 仍正常显示。
- 日志输出 HTTP 状态码和解析错误。

JSON 字段缺失：

- 对单字段使用默认 `--`。
- 不因单字段缺失导致整体崩溃。

## 9. 构建配置影响

组件依赖：

```text
AppBackend REQUIRES
  esp_wifi
  esp_netif
  esp_event
  nvs_flash
  esp_http_client
  esp_timer
  lwip
  json
```

`main` 或 `lvgl_port` 依赖：

- 推荐 `lvgl_port` 增加 `AppBackend` 依赖，并在 `ui_init()` 之后启动后端。
- `main` 不直接管理后端细节，保持入口简洁。

可能涉及的配置：

1. 保持当前 `CONFIG_ESP_WIFI_ENABLED=y`。
2. 保持当前 `CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y`。
3. 保持当前 mbedTLS 证书 bundle 配置。
4. 不改 PCLK、LVGL 刷新周期、触摸周期等已验证 UI 性能参数。

## 10. 验证计划

构建：

```powershell
idf.py build
```

烧录：

```powershell
idf.py -p <PORT> -b 460800 flash
```

串口监控：

```powershell
idf.py -p <PORT> monitor
```

关键日志：

```text
APP_BACKEND: backend started
BACKEND_WIFI: scan start
BACKEND_WIFI: scan done, ap_count=N
BACKEND_WIFI: connecting to <ssid>
BACKEND_WIFI: got ip <x.x.x.x>
BACKEND_TIME: SNTP synced
BACKEND_WEATHER: Open-Meteo updated
UI_BINDING: snapshot applied
```

实机路径：

1. 冷启动后进入 Home，未联网时页面不崩溃。
2. Home -> 设置 -> WiFi。
3. 点击扫描，真实 AP 列表出现。
4. 选择 AP，输入密码，点击连接。
5. 连接成功后显示成功提示。
6. 返回首页，时间和天气变为真实数据。
7. 进入设备状态，IP、RSSI、同步时间为真实数据。
8. 重复执行 `首页 -> 设置 -> WiFi -> 返回设置 -> 设备状态 -> 返回首页`。

失败回归关键字：

```text
task_wdt
Guru Meditation
backtrace
panic
abort
skip destroy active screen
```

验收标准：

1. 编译通过。
2. 烧录通过。
3. 串口日志出现扫描、连接、IP、时间同步、天气更新证据。
4. 前端页面显示真实 WiFi、时间、天气数据。
5. 固定页面切换路径无卡死、无看门狗、无崩溃。

## 11. 后续扩展

第二版可扩展：

1. 使用 Open-Meteo Geocoding API 支持城市搜索。
2. 接入 Open-Meteo Air Quality API 或其他空气质量源，补齐真实 AQI。
3. 自动重连策略和连接历史管理。
4. 使用 `esp_app_get_description()` 显示真实固件版本。
5. 增加网络状态图标或更细的错误码显示。
6. 增加天气定时刷新，例如每 15 分钟刷新一次。
