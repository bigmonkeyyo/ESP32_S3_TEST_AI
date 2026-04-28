# ESP32+ONENET+OTA开发经验

> 面向后续接手本工程的智能体。本文记录 `ESP32S3-AIroot` 在 ESP32-S3 / ESP-IDF / OneNET 上完成在线 OTA 的真实工程经验、踩坑和验证方式。

## 1. 当前结论

当前 OTA 功能已经由用户实机验证通过：

- 设备联网后可以向 OneNET 上报当前固件版本。
- 设备可以检测 OneNET `fuse-ota` 升级任务。
- UI 可以弹出“固件升级”确认框，让用户选择“更新”或“稍后”。
- 点击“更新”后，设备先完整下载升级包，再写入空闲 OTA 分区。
- 下载完成后会做大小和 MD5 校验。
- 校验通过后调用 `esp_ota_set_boot_partition()` 切换启动分区。
- 当前 UI 策略是下载完成后提示用户选择“立即重启”或“稍后重启”，用户确认后再重启。
- 重启后设备会从新 OTA 分区启动，并向 OneNET 上报新版本。

这次实机链路已经证明：问题不在 OTA 启动结构，而在版本显示曾经被 UI 写死为 `v1.2.7`。该显示问题已改为从 `esp_app_get_description()` 和后端快照读取真实版本。

2026-04-28 新增实机结论：固件版本页和 OTA 弹窗/进度/下载完成界面已落地到工程。OneNET `1.3.0 -> 1.3.1` 已通过 COM7 实机闭环，日志见 `tmp\ota_130_to_131_com7.log`。验证完成后，设备已重新烧回 `1.3.0` 作为再次 OTA 测试基线，日志见 `tmp\baseline_130_again_com7.log`。

## 2. 工程现状

项目路径：

```text
D:\ESP32-APP\18_touch
```

工程名：

```text
ESP32S3-AIroot
```

设备信息：

```text
产品名称：ESP32S3-AIroot
产品 ID：592ks3TWbW
设备名称：ESP32S3-AIroot-001
接入协议：MQTT
数据协议：OneJson
```

注意：设备密钥、产品级 access key、OTA dev_id 等敏感信息目前在 `sdkconfig` 中用于测试。后续提交或公开前必须改成 `sdkconfig.defaults` 不带密钥，或改为本地私有配置，不要把真实密钥提交到公共仓库。

## 3. 分区方案

当前分区表：`partitions-16MiB.csv`

```csv
# Name,     Type,     SubType,     Offset,     Size
nvs,        data,     nvs,         0x9000,     0x4000
otadata,    data,     ota,         0xd000,     0x2000
phy_init,   data,     phy,         0xf000,     0x1000
ota_0,      app,      ota_0,       0x10000,    0x400000
ota_1,      app,      ota_1,       0x410000,   0x400000
vfs,        data,     fat,         0x810000,   0x400000
storage,    data,     spiffs,      0xc10000,   0x3f0000
```

关键点：

- `otadata` 必须保留，它记录当前从 `ota_0` 还是 `ota_1` 启动。
- `ota_0` 和 `ota_1` 每个 4MB，当前固件约 1.7MB，空间足够。
- OTA 下载时不要自己指定写哪个分区，使用 `esp_ota_get_next_update_partition(NULL)` 让 ESP-IDF 选择当前启动分区之外的空闲分区。
- 当前 `vfs` 和 `storage` 还保留，后续如果固件继续膨胀，可以再评估裁剪或缩小。

## 4. 代码结构

OTA 相关主文件：

```text
components/AppBackend/ota_service.c
components/AppBackend/ota_service.h
components/AppBackend/mqtt_service.c
components/AppBackend/mqtt_service.h
components/AppBackend/backend_store.c
components/AppBackend/backend_store.h
components/AppBackend/include/app_backend.h
components/AppBackend/include/app_backend_types.h
components/UI/core/ui_data_bindings.c
components/UI/pages/page_status.c
components/UI/pages/page_firmware.c
components/AppBackend/Kconfig
```

启动链路：

```text
WiFi got IP
  -> app_backend_handle_ip_ready()
  -> mqtt_service_start()
  -> time_service_sync()
  -> ota_service_report_current_version()
  -> app_backend_refresh_weather_with_diag()
```

这样安排的原因是降低并发 TLS 握手压力。之前天气 HTTPS 和 OTA HTTPS 同时握手，出现过 `mbedtls_ssl_setup -0x7F00` 一类内存/资源问题。当前策略是联网后先完成 MQTT 和时间同步，再上报当前 OTA 版本，随后刷新天气。用户进入固件版本页点击“检查更新”时再主动执行 OTA 检查。

## 5. OneNET 平台侧经验

本工程不是“纯 MQTT 下载固件”。实际做法是：

- MQTT：用于设备上线、属性上报、OTA 通知回复。
- HTTPS `fuse-ota`：用于版本上报、检查升级任务、下载升级包。

OneNET 产品创建建议：

```text
产品名称：ESP32S3-AIroot
接入协议：MQTT
数据协议：OneJson
```

设备创建建议：

```text
设备名称：ESP32S3-AIroot-001
设备 ID：ESP32S3-AIroot-001
```

升级包页面填写经验：

- 包版本号必须大于设备当前上报版本。例如设备当前是 `1.2.8`，下一包应填 `1.2.9`。
- 上传文件建议用短英文文件名，例如 `ota129.bin`，避免 OneNET 页面或下载接口因文件名过长出现问题。
- 待升级版本要和设备当前版本匹配，否则平台可能不会给设备下发任务。
- 通知方式不要依赖单一路径。本工程会主动调用 `check` 接口检查任务，因此即使 MQTT 通知不稳定，也能在设备联网后检测到升级任务。
- 如果 OneNET 的“开始验证”页面一直转圈，不要只盯页面状态，优先看设备串口是否已经出现 `APP_OTA` 相关日志。

## 6. 设备端 OTA 流程

### 6.1 版本上报

设备读取当前固件描述：

```c
const esp_app_desc_t *desc = esp_app_get_description();
const char *version = (desc != NULL) ? desc->version : "unknown";
```

然后调用 OneNET 版本上报接口：

```text
POST /fuse-ota/{product_id}/{device_name}/version
payload: {"f_version":"当前版本","s_version":"当前版本"}
```

成功日志：

```text
APP_OTA: version reported 1.3.0
```

### 6.2 检查升级任务

设备调用：

```text
GET /fuse-ota/{product_id}/{device_name}/check?type={type}&version={current_version}
```

有任务时会记录：

```text
APP_OTA: update available target=1.3.1 tid=1373828 size=1786224 type=... md5=...
APP_OTA: waiting for UI confirmation before download
```

没有任务时会记录：

```text
APP_OTA: no OTA task
```

### 6.3 UI 用户确认

后端把状态写入 `backend_store`：

```text
APP_BACKEND_OTA_READY
target_version = 新版本
message = 发现新版本，等待确认
```

`ui_data_bindings.c` 看到 `APP_BACKEND_OTA_READY` 后弹出：

```text
标题：固件升级
正文：检测到新版本 x.x.x，是否现在更新？
按钮：更新 / 稍后
```

点击“更新”后调用：

```c
app_backend_ota_confirm_async();
```

最终由后端任务调用：

```c
ota_service_confirm_update();
```

### 6.4 下载、校验、写入分区

下载接口：

```text
GET /fuse-ota/{product_id}/{device_name}/{tid}/download
```

写入流程：

```text
esp_ota_get_next_update_partition(NULL)
esp_ota_begin(...)
循环 esp_http_client_read()
循环 esp_ota_write()
同步计算 MD5
校验下载大小
校验 MD5
esp_ota_end()
esp_ota_set_boot_partition()
等待用户点击立即重启后 esp_restart()
```

成功日志：

```text
APP_OTA: download start tid=1373828 target=1.3.1 size=1786224 content_length=1786224 partition=ota_1
APP_OTA: download progress 100% (1786224/1786224)
APP_OTA: OTA ready; waiting for user reboot to 1.3.1 md5=a74229faa6e771854712f2feca723ed1
APP_OTA: restarting by user request
```

重启后成功证据：

```text
Loaded app from partition at offset 0x410000
App version: 1.3.1
APP_OTA: version reported 1.3.1
APP_OTA: no OTA task
```

## 7. 遇到的问题和解决方式

### 7.1 误判：屏幕仍显示 1.2.7

现象：

```text
OTA 重启后，串口显示 App version: 1.2.8，但设备状态页仍显示 v1.2.7。
```

根因：

```text
components/UI/pages/page_status.c 里固件版本被写死为 "v1.2.7"。
```

解决：

- `backend_store.c` 初始化时用 `esp_app_get_description()` 写入 `snapshot.ota.current_version`。
- `page_status.c` 保存固件版本 label 指针。
- `page_status_apply_snapshot()` 根据 `snapshot->ota.current_version` 更新页面。
- 固件版本页 `page_firmware.c` 继续读取同一个 OTA snapshot，避免再次写死版本号。
- `1.3.0 -> 1.3.1` OTA 后已验证串口版本、OneNET 上报版本和 UI 版本来源均来自真实固件描述。

经验：

串口的 `App version` 和 OneNET 上报版本比 UI 文案更可信。UI 显示异常时先查是不是写死或没有绑定 snapshot。

### 7.2 OneNET 升级包文件名过长

现象：

```text
新包上传后，OneNET 页面提示升级包文件名错误或处理异常。
```

解决：

使用短英文文件名：

```text
tmp/ota128.bin
tmp/ota129.bin
tmp/ota131.bin
```

经验：

OneNET 升级包文件名保持短、英文、无空格、无中文，版本号靠平台字段表达，不靠文件名表达。

### 7.3 TLS 并发导致 OTA/天气失败

现象：

```text
天气 HTTPS 和 OTA HTTPS 并发时，出现 mbedTLS 初始化失败，例如 mbedtls_ssl_setup -0x7F00。
```

解决：

在 `app_backend_handle_ip_ready()` 中按顺序执行：

```text
MQTT start
SNTP sync
Weather HTTPS
OTA check
```

经验：

ESP32-S3 虽然有 PSRAM，但 TLS 握手仍然会吃堆和内部资源。联网功能不要一拿到 IP 就并发全开，先做顺序闭环，再考虑优化。

### 7.4 OneNET MQTT 和 OTA Access Key 容易混淆

现象：

```text
设备密钥可以连 MQTT，但 OTA HTTP 接口鉴权失败。
```

根因：

MQTT token 使用设备级 device key；OTA `fuse-ota` HTTP 接口使用产品级 access key 和数字 dev_id。

解决：

在 `components/AppBackend/Kconfig` 中分开配置：

```text
APP_ONENET_DEVICE_KEY
APP_ONENET_OTA_PRODUCT_ACCESS_KEY
APP_ONENET_OTA_DEV_ID
```

经验：

不要把 MQTT 能连上当成 OTA 鉴权一定正确。两个鉴权材料不同。

### 7.5 版本号没有升高导致不触发升级

现象：

```text
设备已经上报某版本，但 OneNET 不再给同版本包任务。
```

解决：

每次要测试 OTA，都要把固件版本升高：

```text
CONFIG_APP_PROJECT_VER="1.3.1"
```

然后重新 build，上传新 bin，并在 OneNET 新增对应版本升级包。

经验：

ESP-IDF 固件版本来自 `sdkconfig` 的 `CONFIG_APP_PROJECT_VER`，不要只改文件名。

## 8. 后续智能体接手检查清单

接手后先看这些文件：

```text
ESP32+ONENET+OTA开发经验.md
HANDOFF_PROMPT_FOR_NEW_CHAT.md
docs/superpowers/plans/2026-04-27-wifi-time-weather-backend-v1.md
LVGL_UI_编译烧录调试与避坑经验.md
components/AppBackend/ota_service.c
components/AppBackend/mqtt_service.c
components/UI/core/ui_data_bindings.c
components/UI/pages/page_status.c
components/UI/pages/page_firmware.c
partitions-16MiB.csv
sdkconfig
```

然后执行：

```powershell
git status --short --branch
git log -1 --oneline --decorate
```

构建命令：

```powershell
$env:IDF_PATH='D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1'
$env:IDF_TOOLS_PATH='D:\ESP-IDF\Espressif'
$env:IDF_PYTHON_ENV_PATH='D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env'
$prefix='D:\ESP-IDF\Espressif\tools\xtensa-esp-elf\esp-13.2.0_20230928\xtensa-esp-elf\bin;D:\ESP-IDF\Espressif\tools\cmake\3.24.0\bin;D:\ESP-IDF\Espressif\tools\ninja\1.11.1;D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts;D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools'
$env:PATH="$prefix;$env:PATH"
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools\idf.py' --no-hints --no-ccache -C 'D:\ESP32-APP\18_touch' build
```

烧录命令：

```powershell
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools\idf.py' --no-hints --no-ccache -C 'D:\ESP32-APP\18_touch' -p COM7 -b 460800 flash
```

串口监听：

```powershell
New-Item -ItemType Directory -Force -Path 'tmp' | Out-Null
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'C:\Users\Administrator\.codex\skills\serial-monitor\scripts\serial_monitor.py' --port COM7 --baud 115200 --duration 120 --save 'tmp\latest_com7.log' --timestamp -v
```

OTA 成功关键字：

```text
APP_OTA: start requested
APP_OTA: version reported
APP_OTA: update available
APP_OTA: waiting for UI confirmation before download
APP_OTA: download start
APP_OTA: download progress 100%
APP_OTA: OTA ready; waiting for user reboot to
APP_OTA: restarting by user request
Loaded app from partition at offset
App version:
APP_OTA: no OTA task
```

异常关键字：

```text
Guru Meditation
task_wdt
panic
abort
backtrace
stack overflow
md5 mismatch
download incomplete
set boot partition failed
ota end failed
mbedtls_ssl_setup
```

## 9. 下一阶段建议

当前 OTA 是可用闭环，后续可以按风险从低到高推进：

1. 把 OneNET 密钥从 `sdkconfig` 中迁出，避免误提交。
2. 增加 OTA 失败原因细分验证，例如鉴权失败、无任务、下载失败、MD5 不匹配。
4. 增加启动后 `esp_ota_mark_app_valid_cancel_rollback()` 之类的回滚确认策略。
5. 如果后续固件超过 4MB，再重新评估分区表，优先裁剪 `vfs/storage` 或增大 OTA 分区。
