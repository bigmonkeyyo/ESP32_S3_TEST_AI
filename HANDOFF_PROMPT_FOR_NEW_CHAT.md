# 新对话接手提示词

把下面整段内容复制给新的对话框，或者直接让新对话先阅读本文件。

```text
你现在接手的是 ESP32-S3 / ESP-IDF / LVGL 项目。

工程路径：
D:\ESP32-APP\18_touch

当前分支：
feat/lvgl-ui-page-framework

最新稳定提交：
8dc774a

提交信息：
V1.2.7稳定版  后端WIFI链接与天气功能已实现

当前项目状态：
- Backend V1.2.7 稳定版已经完成并推送。
- 已实现真实 WiFi 扫描、连接、断开、保存凭据、重启后自动连接。
- 已实现 SNTP 时间同步。
- 已实现高德 AMap 天气接口，当前固定城市为上海。
- 首页可显示天气、温度、湿度、风速。
- AQI 当前按 Backend V1 约定显示 `AQI --`。
- 前端 LVGL 首页、设置页、WiFi 页、设备状态页已经接入后端 snapshot。
- 用户已实机确认：天气、温度、湿度、风速、WiFi 状态显示均正常。
- 2026-04-28：固件版本页和 OneNET OTA 用户确认流程已落地，`1.3.0 -> 1.3.1` 已实机 OTA 成功。

WiFi 页面当前交互逻辑：
- 未连接：右上角按钮为蓝色 `扫描`。
- 扫描中：按钮为 `扫描中`，禁用。
- 连接中：按钮为 `连接中`，禁用。
- 已连接：右上角按钮为红色 `断开`。
- 点击 `断开` 会执行断开并清除保存凭据，避免自动重连。

已经修复的重要问题：
- 断开 WiFi 后，首页不再继续显示 `联网天气已同步`，而是显示 `WiFi 已断开`。
- WiFi 扫描完成后不再卡在 `扫描中`。
- 根因是 `backend_store_set_scan_results()` 之前没有把 `wifi_state` 从 `SCANNING` 切回空闲态，而且曾使用 `xSemaphoreTake(s_mutex, 0)`，可能在 UI 取快照时静默丢掉扫描结果更新。
- 当前已改为扫描结果提交时等待最多 `200ms`，并设置 `wifi_state = APP_BACKEND_WIFI_IDLE`。

重要文件：
- 后端组件：
  - `components/AppBackend/`
  - `components/AppBackend/app_backend.c`
  - `components/AppBackend/backend_store.c`
  - `components/AppBackend/wifi_service.c`
  - `components/AppBackend/time_service.c`
  - `components/AppBackend/weather_service.c`
  - `components/AppBackend/network_diag_service.c`
- 后端公开接口：
  - `components/AppBackend/include/app_backend.h`
  - `components/AppBackend/include/app_backend_types.h`
- UI 绑定：
  - `components/UI/core/ui_data_bindings.c`
  - `components/UI/include/ui_data_bindings.h`
- 页面：
  - `components/UI/pages/page_home.c`
  - `components/UI/pages/page_wifi.c`
  - `components/UI/pages/page_settings.c`
  - `components/UI/pages/page_status.c`
- 字体生成：
  - `tools/gen_ui_fonts.py`
  - `components/UI/fonts/generated/ui_font_sc_*.c`
- 后端计划和进度：
  - `docs/superpowers/plans/2026-04-27-wifi-time-weather-backend-v1.md`
- 编译烧录和避坑文档：
  - `LVGL_UI_编译烧录调试与避坑经验.md`

必须遵守的项目规则：
1. 后端代码放在 `components/AppBackend/`，UI 页面不要直接调用 `esp_wifi_*`、HTTP、SNTP。
2. UI 回调只能异步发起后端动作，不能阻塞扫描、连接、HTTP、时间同步。
3. 后端任务、WiFi event callback、HTTP 回调不能直接操作 LVGL 对象。
4. UI 更新必须走 backend snapshot + `ui_data_bindings`。
5. `backend_store` 是 WiFi、时间、天气、状态 message 的单一数据源。
6. 新增中文 UI 文案前，必须检查当前生成字体是否包含对应字形；如果没有，更新 `tools/gen_ui_fonts.py` 后统一重新生成字体。
7. 不要随便改 LVGL 刷新、触摸、PCLK、buffer 等已验证参数。
8. 每次固件修改后优先走：build -> flash COM7 -> monitor COM7 -> 搜索异常关键字。
9. 不要泄露或打印高德 API key 到串口或文档。
10. 用户希望你直接执行，不要只给建议；但遇到不确定状态先看代码和日志。

当前 ESP-IDF 环境：
- ESP-IDF: `D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1`
- Python: `D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe`
- 串口：`COM7`
- 烧录波特率：`460800`
- 串口监控波特率：`115200`

常用 build 命令：
```powershell
$env:IDF_PATH='D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1'
$env:IDF_TOOLS_PATH='D:\ESP-IDF\Espressif'
$env:IDF_PYTHON_ENV_PATH='D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env'
$prefix='D:\ESP-IDF\Espressif\tools\xtensa-esp-elf\esp-13.2.0_20230928\xtensa-esp-elf\bin;D:\ESP-IDF\Espressif\tools\cmake\3.24.0\bin;D:\ESP-IDF\Espressif\tools\ninja\1.11.1;D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts;D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools'
$env:PATH="$prefix;$env:PATH"
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools\idf.py' --no-hints --no-ccache -C 'D:\ESP32-APP\18_touch' build
```

## 2026-04-28 OneNET OTA 接手补充

当前 OneNET OTA 已由用户实机确认可用。接手 OTA 相关问题前必须先读：

```text
ESP32+ONENET+OTA开发经验.md
docs/superpowers/plans/2026-04-28-ota-ui-backend-adaptation-v1.md
docs/superpowers/changelogs/2026-04-28-ota-firmware-ui-backend-v1.md
docs/superpowers/plans/2026-04-27-wifi-time-weather-backend-v1.md
LVGL_UI_编译烧录调试与避坑经验.md
```

已完成的 OTA 事实：

- 工程名：`ESP32S3-AIroot`
- 产品 ID：`592ks3TWbW`
- 设备名：`ESP32S3-AIroot-001`
- OTA 分区结构：`otadata + ota_0 + ota_1`
- 每个 OTA app 分区：`0x400000`
- MQTT 负责 OneNET 连接、状态发布、OTA inform 回复。
- OneNET `fuse-ota` HTTPS 负责版本上报、任务检查和升级包下载。
- 用户已确认设备可从 `1.2.7` OTA 到 `1.2.8`。
- 重启后串口证据显示 `App version: 1.2.8`，并从 `ota_1` 启动。
- 屏幕曾继续显示 `v1.2.7` 的原因是状态页硬编码，不是 OTA 失败。
- 当前已修复状态页和固件版本页的真实版本显示。
- 用户已确认 `1.3.0 -> 1.3.1` OTA 闭环成功：发现新版本、用户确认、下载进度、MD5 校验、等待用户重启、重启后从 `ota_1` 启动并上报 `1.3.1`。
- 验证完成后已重新烧录 `1.3.0`，方便用户再次测试 OneNET OTA。

当前 OTA 包：

```text
tmp\ota131.bin
version: 1.3.1
MD5: A74229FAA6E771854712F2FECA723ED1
SHA256: 1D5306DAC4FAF8443C37924B29FD7099836F3F908D805B7D0277A913F13520A9
size: 1786224
source/current device version: 1.3.0
```

注意：

- 不要在回复或文档里展开 OneNET 设备密钥、产品 access key。
- 当前测试密钥仍在 `sdkconfig`，公开提交前必须迁出或替换。
- 如果串口 `App version` 和 OneNET 上报版本正确，但 UI 版本不对，优先查 `page_status.c` 和 backend snapshot，不要误判 OTA 失败。

常用 flash 命令：
```powershell
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools\idf.py' --no-hints --no-ccache -C 'D:\ESP32-APP\18_touch' -p COM7 -b 460800 flash
```

常用 monitor 命令：
```powershell
New-Item -ItemType Directory -Force -Path 'tmp' | Out-Null
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'C:\Users\Administrator\.codex\skills\serial-monitor\scripts\serial_monitor.py' --port COM7 --baud 115200 --duration 120 --save 'tmp\latest_com7.log' --timestamp -v
```

重点成功日志：
- `APP_BACKEND: backend started`
- `APP_BACKEND_SELFTEST: store PASS`
- `BACKEND_WIFI: scan start`
- `BACKEND_WIFI: scan done, ap_count=N`
- `BACKEND_WIFI: got ip 172.20.10.14`
- `BACKEND_TIME: SNTP synced`
- `BACKEND_WEATHER: AMap request start`
- `BACKEND_WEATHER: AMap weather updated`
- `UI_BINDING: snapshot applied`
- `APP_OTA: version reported 1.3.0`
- `APP_OTA: update available target=1.3.1`
- `APP_OTA: download progress 100%`
- `APP_OTA: OTA ready; waiting for user reboot to 1.3.1`
- `APP_OTA: restarting by user request`
- `Loaded app from partition at offset 0x410000`
- `App version:      1.3.1`
- `APP_OTA: no OTA task`

重点失败关键字：
- `Guru Meditation`
- `task_wdt`
- `panic`
- `abort`
- `backtrace`
- `stack overflow`
- `SCREEN_UNLOADED`

最近一次稳定验证：
- `build/ESP32S3-AIroot.bin` 大小约 `0x1b4170`
- OTA app 分区 `0x400000`
- 剩余约 `0x24be90`，约 57%
- 最近 OTA 成功日志：`tmp\ota_130_to_131_com7.log`
- 当前重新烧回 `1.3.0` 的基线日志：`tmp\baseline_130_again_com7.log`
- 注意：当前分区表已使用 `otadata + ota_0 + ota_1`，不要再按旧 factory 分区判断 OTA 空间。

接手后请先执行：
1. `git status --short --branch`
2. `git log -1 --oneline --decorate`
3. 阅读：
   - `docs/superpowers/plans/2026-04-27-wifi-time-weather-backend-v1.md`
   - `LVGL_UI_编译烧录调试与避坑经验.md`
4. 如果要继续开发，先说明你会修改哪些文件，再开始实现。
```
