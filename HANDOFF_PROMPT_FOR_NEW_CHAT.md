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

## 2026-05-03 Latest Progress (V1.3.0 Gyro Ball)

- Current branch: `feat/lvgl-ui-page-framework`
- New UI path:
  - Settings page adds `Gyro Verify` row
  - Navigation to new page id `UI_PAGE_GYRO`
- New files:
  - `components/UI/pages/page_gyro.c`
  - `components/UI/pages/page_gyro.h`
  - `components/BSP/QMI8658A/qmi8658a.c`
  - `components/BSP/QMI8658A/qmi8658a.h`
  - `components/BSP/QMI8658A/imu.c`
  - `components/BSP/QMI8658A/imu.h`
- Runtime behavior updates:
  - LVGL startup moved ahead of IMU init (IMU init in async task)
  - Gyro ball uses tilt-only control (no constant gravity bias)
  - Added zero calibration + deadzone to reduce static drift
  - Added boundary bounce behavior
  - Left/right control direction corrected by X-axis inversion
- Recent validation:
  - Build and flash verified on `COM8`
  - Serial logs show `QMI8658A init ok` and continuous `GYRO` updates
  - First frame/backlight and UI startup appear before IMU calibration completes

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

## 2026-05-08 Wi-Fi/BLE 专项交接更新（证据导向）

### 本轮用户关注问题
- 升级 BLE 功能后，Wi-Fi 上电首连不稳定，经常需要多次重连才能成功。
- 偶发现象：热点已开启且设备在身边，但扫描或自动连接仍可出现失败。

### 已完成的证据化排查
- 已在 `wifi_service.c` 增加诊断日志：
  - 扫描关注：`show_hidden=true`、ap total/kept 数量、每个 AP 的 bssid/channel/auth/hidden 信息
  - 连接关注：`disconnected reason`、自动重连次数、目标 SSID 丢失提示
  - 凭据记录（临时调试用）：已保存 SSID/密码/长度打印
- 已验证事实：
  - NVS 中已保存凭据，开机可正确读取
  - 失败主要发生在认证/关联阶段，常见：`reason=2`（AUTH_EXPIRE）、`reason=205`（CONNECTION_FAIL），偶发 `reason=201`（NO_AP_FOUND）

### 本轮 A/B 验证（按用户指定方案）
- B 组改动：Wi-Fi 连接窗口期暂停 BLE 广播，拿到 IP 后恢复。
  - `components/GyroBle/include/gyro_ble.h`
  - `components/GyroBle/gyro_ble.c`
  - `components/AppBackend/wifi_service.c`
  - `components/AppBackend/CMakeLists.txt`（AppBackend 依赖 `GyroBle`）
- 验证证据：
  - 编译成功、烧录成功（COM7）
  - 日志中可见明确流程：`pause BLE advertising for WiFi connect` -> `diag connect window done` -> `resume BLE advertising after WiFi: got_ip`

### 本轮 A/B 结论（基于日志而不是猜测）
- 结论：**仅暂停 BLE 广播不足以解决 Wi-Fi 首连慢和多次重连问题**。
- B 组（6 次复位采样）结果：
  - 首连成功数：`0/6`
  - 最终成功：`6/6`
  - 成功时连接窗口耗时：`avg 44210.8ms`（约 44s）
  - 成功前自动重连次数：基本固定 `8` 次
- A 组（改前基线）另有日志显示：约 45.6s 拿到 IP，重连约 6 次。整体表现与 B 组同一量级，因此不支持“BLE 广播是主因”的判断。

### 本轮关键日志文件
- 改前基线：`tmp/wifi_saved_cred_dump_20260508.log`
- B 组批量复位对比（6 次）：`tmp/wifi_ble_gate_ab_20260508_105752.log`
- B 组单次完整流程（含恢复 BLE 广播）：`tmp/wifi_ble_gate_resume_check_20260508_110543.log`

### 当前代码状态
- 已加入 BLE 广播开关 API：`gyro_ble_set_advertising_enabled(bool enabled)`
- Wi-Fi 端已加入连接窗口诊断：`diag connect window start/done`
- Wi-Fi 端在连接窗口内控制 BLE 广播暂停/恢复

### 下一步（证据导向）
1. 做热点端参数 A/B：2.4G + 20MHz + 固定信道（1/6/11），复用同样 6 次复位口径统计。
2. ESP 端做 BSSID/信道锁定实例对照，观察 `reason=201/2/205` 变化。
3. 根据新的 A/B 结果决定是否固化策略，并在确认后移除敏感调试打印（密码日志）。

## 2026-05-08 Wi-Fi/BLE 结论更新（用户实测确认）

### 新增验证目标
- 按用户要求，先将蓝牙链路全部断开（固件运行时不启动 BLE），只保留 Wi-Fi 路径，验证“上电自动连接”是否恢复稳定。

### 本轮固件改动（用于验证）
- `main/main.c`
  - 移除 `gyro_ble.h` 引用。
  - 移除 `gyro_ble_start()` 启动调用。
  - 移除 `gyro_ble_take_recalibrate_request()` 处理路径。
  - 移除 `gyro_ble_publish_sample()` 数据发布路径。
- `main/CMakeLists.txt`
  - `REQUIRES` 移除 `GyroBle`，避免主工程依赖 BLE 组件启动链路。

### 构建/烧录/日志证据
- 编译：成功（`idf.py build`）
- 烧录：成功（`idf.py -p COM7 -b 460800 flash`）
- 关键日志：
  - `tmp/wifi_no_ble_boot_20260508_after_flash.log`
  - 日志中不再出现 `BLE_INIT`、`GYRO_BLE: started`、`advertising as ESP32-GYRO`。
  - 同时可见 Wi-Fi 上电自动连接流程日志（`BACKEND_WIFI: connecting saved ssid ...`）。

### 结论更新（覆盖上一轮“主因不成立”的范围限定）
- 结论 1（保留）：**仅“连接窗口暂停 BLE 广播”不足以显著改善首连表现**。
- 结论 2（新增）：**“运行时完全关闭 BLE”后，Wi-Fi 上电自动连接明显恢复，用户实测“基本每次都能快速连上”**。
- 因此应将问题定位从“仅广播冲突”升级为“BLE 子系统整体（控制器/协议栈/共存时序）对 Wi-Fi 首连存在实质影响”。

### 对后续智能体的执行约束
1. 后续排查优先以“Wi-Fi 先成功，再启 BLE”为主线，不要再只围绕“暂停广播”单点优化。
2. 所有新结论必须附带复位次数、首连成功率、平均拿 IP 时间，避免只给单次日志。
3. 在恢复 BLE 相关实验时，避免回退或覆盖本段验证改动；如需恢复，先新建对照分支并保留可回滚点。

## 2026-05-08 OneNET OTA HTTPS 排查与当前可用方案

### 本轮用户关注问题
- 设备已连上 Wi-Fi 且网络正常，但在“设备检查更新”时立即提示“版本上报失败，请检查网络”。
- 用户明确要求关注“为什么 OTA 不可用”和“如何让 OTA 功能恢复”，不关心失败提示文案。

### 本轮已完成排查
- 已按用户要求执行多轮闭环验证：`build -> flash COM7 -> 60s 串口监听 -> 用户复现“检查更新”`。
- 已确认 Wi-Fi 正常拿到 IP，SNTP 也已同步，不是断网问题。
- 已确认问题发生在 OTA HTTPS 握手/证书校验阶段，而不是业务 JSON 或 OneNET 鉴权阶段。
- 已给 OTA 请求链路补充更细的 TLS 诊断日志，并抓到明确证据：
  - `tls_code=0x2700`
  - `tls_flags=0x8`
  - 含义：`The certificate is not correctly signed by the trusted CA`

### 已验证过但未彻底解决的 HTTPS 方案
- 方案 1：OTA 改用 `esp_crt_bundle_attach`
  - 现象：AMap HTTPS 正常，但 OneNET OTA 仍可失败。
  - 日志曾出现：
    - `esp-x509-crt-bundle: Certificate validated`
    - `esp-x509-crt-bundle: Failed to verify certificate`
    - `mbedtls_ssl_handshake returned -0x3000`
- 方案 2：OTA 改用显式 `cert_pem`
  - 先后尝试过：
    - 仅 OneNET 中级证书
    - 中级 + DigiCert Global Root G2
    - 扩充多张 DigiCert 根证书
  - 结果：仍会出现 `tls_flags=0x8`，说明当前设备侧信任链与 OneNET 实际返回链路之间仍存在不稳定匹配问题。
- 方案 3：重复点击导致 TLS 资源打爆问题
  - 已新增去抖与冷却，抑制了频繁点击导致的资源失败。
  - 相关文件：
    - `components/AppBackend/app_backend.c`
    - `components/AppBackend/ota_service.c`

### 当前可用方案（已实机验证）
- 为了先恢复 OTA 可用性，当前已将 OTA 基地址从 HTTPS 临时切换为 HTTP：
  - `CONFIG_APP_ONENET_OTA_BASE_URL="http://iot-api.heclouds.com/fuse-ota"`
- 这次切换仅用于 OTA 路径，目的是绕过当前 TLS 证书链不稳定问题，先恢复版本上报、任务检查与后续下载流程可用。

### 当前代码状态
- `components/AppBackend/ota_service.c`
  - 保留了 OTA TLS 诊断日志逻辑
  - 保留了 OTA 请求冷却保护
  - 当前 OTA HTTP 客户端仍保留 `cert_pem` 相关调试代码，方便后续再切回 HTTPS 时继续排查
- `components/AppBackend/app_backend.c`
  - 已保留 OTA 检查按钮去抖逻辑
- `sdkconfig`
  - 当前 OTA 基地址已切到 `http://iot-api.heclouds.com/fuse-ota`

### 本轮最终验证结果
- 已重新编译并烧录到 `COM7`。
- 60 秒串口复现日志已确认 OTA 链路恢复可用：
  - `APP_OTA: version reported 1.3.0`
  - `APP_OTA: no OTA task`
  - 统计结果：`ERROR: 0`
- 关键验证日志：
  - `tmp/ota_http_mode_60s_20260508.log`

### 关键中间证据日志
- 证书失败阶段：
  - `tmp/ota_bundle_retry2_60s_20260508.log`
  - `tmp/ota_flags_60s_20260508.log`
  - 可见 `tls_flags=0x8`
- HTTP 可用验证阶段：
  - `tmp/ota_http_mode_60s_20260508.log`

### 接手后的判断建议
- 如果目标是“先让用户能用 OTA”，当前 HTTP 方案已经满足。
- 如果目标是“恢复 HTTPS 且保留安全校验”，不要直接回退当前改动，应基于本次证据继续做：
  1. 固定并对齐 OneNET 实际下发证书链
  2. 重新验证 ESP-IDF 5.2.1 下 `esp-tls`/`mbedtls` 的证书解析与内存行为
  3. 在 HTTPS 复测通过前，不要移除当前 HTTP 兜底方案

### 当前结论
- 根因不是 Wi-Fi 断网，也不是用户误操作。
- 根因是 OneNET OTA HTTPS 在当前设备环境下存在证书链校验不稳定问题。
- 当前分支已经通过“OTA 改走 HTTP”恢复功能可用，属于明确的临时可交付方案。

## 2026-05-11 Web Bluetooth UI 同屏兼容实现

### 本轮目标
- 在不破坏旧陀螺仪 Web Bluetooth 校准页的前提下，新增“设备 UI 状态同步到网页，由网页重绘同款界面”的第一版能力。
- 第一版只做状态同屏，不传 LVGL 像素、截图或 bitmap。

### 兼容性约束
- 旧网页 `web/gyro_ble_viewer.html` 保持不改。
- 旧 BLE 设备名继续是 `ESP32-GYRO`。
- 旧 service UUID 保持 `9f3a2000-5c9d-4a7b-9f2b-8b6e1f5a1000`。
- 旧特征保持：
  - pose: `9f3a2001-5c9d-4a7b-9f2b-8b6e1f5a1000`
  - imu: `9f3a2002-5c9d-4a7b-9f2b-8b6e1f5a1000`
  - ctrl: `9f3a2003-5c9d-4a7b-9f2b-8b6e1f5a1000`
- 旧控制命令保持：
  - `0x01` start stream
  - `0x02` stop stream
  - `0x03` recalibrate

### 新增实现
- `components/GyroBle/`
  - 新增 UI state 特征：`9f3a2005-5c9d-4a7b-9f2b-8b6e1f5a1000`
  - 新增 `gyro_ble_publish_ui_state_json(const char *json)`
  - UI state 使用固定 20-byte frame 分片发送 compact JSON。
- `components/UI/core/ui_mirror.c`
  - 从 `ui_page_current()` 获取当前页面。
  - 从 `app_backend_get_snapshot()` 获取 Wi-Fi、天气、OTA、BLE、时间等状态。
  - 页面变化和 backend snapshot 更新都会触发同步。
  - 另有 1 秒低频保活发送，方便网页刚连接后自动拿到当前状态。
- `components/AppBackend/app_backend.c`
  - backend update callback 从单订阅改为最多 4 个订阅者，保证 `ui_data_bindings` 和 `ui_mirror` 可同时工作。
- 新网页入口：
  - `tools/web_ble_mirror/index.html`
  - 本地启动命令：
    ```powershell
    & 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' -m http.server 8765 -d tools\web_ble_mirror
    ```
  - 浏览器打开：`http://localhost:8765`

### 已完成验证
- `idf.py build` 成功。
- `idf.py -p COM7 -b 460800 flash` 成功。
- 120 秒串口日志：`tmp\web_ble_mirror_com7.log`
- 日志中可见：
  - `APP_BACKEND: backend started`
  - `APP_BACKEND_SELFTEST: store PASS`
  - `BACKEND_WIFI: got ip 172.20.10.14`
  - `GYRO_BLE: started`
  - `APP_MQTT: connected`
  - `BACKEND_WEATHER: AMap weather updated`
  - `GYRO: zero calibrated`
- 异常关键字扫描未命中：
  - `Guru Meditation`
  - `task_wdt`
  - `panic`
  - `abort`
  - `backtrace`
  - `stack overflow`
  - `ERROR`

### 后续人工验收
1. 设备设置页手动打开蓝牙。
2. 用 Chrome/Edge 打开 `http://localhost:8765` 并连接 `ESP32-GYRO`。
3. 在设备触摸屏切换 Home / Settings / WiFi / Status / Firmware / Gyro / About，确认网页 500ms 左右同步。
4. 单独打开旧 `web/gyro_ble_viewer.html`，确认旧姿态流、IMU 流和 `0x03` 重新校准命令仍可用。
5. 注意：BLE 外设通常只允许单连接，旧网页和新网页需要分别验证，不要同时连接同一设备。

## 2026-05-11 Web Bluetooth UI 同屏复位排查更新

### 用户反馈
- 设备手动打开蓝牙后，网页端连接成功附近出现设备复位。
- 本轮目标不是先改协议，而是在关键路径加日志并抓 60 秒串口窗口，定位复位触发点。

### 已加入的调试探针
- `main/main.c`
  - 启动时打印 `esp_reset_reason()`：`MAIN: boot reset_reason=...`
- `components/GyroBle/gyro_ble.c`
  - GAP connect/disconnect/subscribe/MTU 事件打印。
  - UI state publish 打印 JSON 长度、分片数量、首尾分片。
  - UI state publish 前后打印 internal heap / largest block / PSRAM heap。
- `components/UI/core/ui_mirror.c`
  - 每次 mirror publish 打印 page、json_len、wifi、ble、ota 状态。

### 已定位问题
- 初版 `ui_mirror_publish()` 在 LVGL/main 任务栈上放了较大的局部对象：
  - `app_backend_snapshot_t snapshot`
  - `char json[1024]`
- 60 秒复现日志曾出现：
  - `***ERROR*** A stack overflow in task main has been detected.`
  - 复位后 `MAIN: boot reset_reason=4`
- 因此该轮复位的直接证据指向 main/LVGL 任务栈溢出，而不是 BLE GAP connect 本身。

### 已修复
- `components/UI/core/ui_mirror.c`
  - 将 publish 用的 snapshot 和 JSON 缓冲改为静态全局缓冲：
    - `static app_backend_snapshot_t s_publish_snapshot;`
    - `static char s_publish_json[UI_MIRROR_JSON_MAX];`
  - `ui_mirror_publish()` 不再在 main/LVGL 任务栈上创建这两个大对象。

### 最新验证
- `idf.py build` 成功。
- `idf.py -p COM7 -b 460800 flash` 成功。
- 60 秒串口日志：`tmp\web_ble_mirror_reset_probe_staticbuf_60s.log`
- 本轮用户在窗口内执行了：设置页打开 BLE -> 网页连接 -> 订阅 UI state / pose -> 多次页面切换。
- 日志结果：
  - 只出现一次上电启动：`rst:0x1 (POWERON)`，`MAIN: boot reset_reason=1`
  - 未出现 `Guru Meditation`、`panic`、`abort`、`task_wdt`、`stack overflow`、`Backtrace`
  - 可见网页连接成功：`GYRO_BLE: gap connect event status=0 conn_handle=1`
  - 可见 UI state 订阅成功：`GYRO_BLE: subscribe attr=27 ... cur_notify=1`、`ui state notify=1`
  - 可见 pose 订阅继续工作：`GYRO_BLE: subscribe attr=16 ... cur_notify=1`
  - UI JSON 通知连续发送完成：`ui state notify seq=... chunk=1/28` 到 `chunk=28/28`
  - heap 稳定在约 `internal free=36KB`、`largest=34KB`，未见持续塌陷。

### 后续注意
- 当前 60 秒窗口内未复现复位，先不要再把“网页连接即复位”当成已确认事实；优先以栈溢出修复后的固件继续做更长时间人工复测。
- `GYRO_BLE: ble_gap_adv_set_fields failed: 22` 在本轮日志中出现一次，但随后仍看到 `advertising as ESP32-GYRO` 和网页成功连接；暂不视为本轮复位根因。
- 如果后续仍复位，下一步重点看：
  1. 复位前最后一条 `GYRO_BLE` / `UI_MIRROR` 日志。
  2. 是否再次出现 `stack overflow`，若是则考虑提高 main task stack 或继续迁移 LVGL 循环里的大栈对象。
  3. UI state 与 pose 同时订阅时 notify 频率较高，必要时把 UI JSON 分片改为跨 tick 发送，而不是一次 publish 连续发完 28 个分片。

### 2026-05-11 网页同屏效果修正
- 用户确认 BLE 已连接且调试信息存在，但网页没有形成“设备触摸跳转后网页同步切页”的投屏观感。
- 已确认设备端日志其实已经发出页面变化：
  - `UI_MIRROR: publish page=settings`
  - `UI_MIRROR: publish page=status`
  - `UI_MIRROR: publish page=home`
  - 同时可见 `ui state notify seq=... chunk=1/28` 到 `chunk=28/28`
- 因此本次修正集中在 `tools/web_ble_mirror/index.html`：
  - 将网页中心预览改成固定 320x240 设备坐标渲染，更接近 LVGL 页面布局。
  - 按实际页面分别重绘 Home / Settings / WiFi / Status / Firmware / Gyro / About。
  - 新增可见同步状态：帧序号、分片进度、更新时间。
  - 新增事件日志：能直接看到 `页面切换 -- -> settings`、`settings -> home` 等。
  - 保留新 UI State notify 和旧 pose notify；不写旧 ctrl 特征，不影响旧陀螺仪校准页。
- 验证：
  - `node` 静态检查网页脚本语法通过：`script syntax OK`
  - 本地服务 `http://localhost:8765` 返回 HTTP 200
- 本次只改网页文件，不需要重新烧录固件；刷新 `http://localhost:8765` 后重新连接设备即可测试。
