# OTA UI Backend Adaptation V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the first three backend gaps for the firmware-version UI: OTA check failure feedback, OTA unconfigured/network failure feedback, and automatic version reporting after the upgraded firmware reconnects.

**Architecture:** The UI already consumes `app_backend_snapshot_t.ota` through `ui_data_bindings.c`, so the fix should keep the snapshot contract stable. OTA service owns all OneNET HTTP details and writes user-visible state through `backend_store_set_ota()`. App backend owns WiFi/IP-ready orchestration and should trigger version reporting after network becomes usable.

**Tech Stack:** ESP-IDF v5.2.1, FreeRTOS tasks/queue, LVGL snapshot binding, OneNET HTTP OTA API.

---

## Current Progress - 2026-04-28

Status values: `未开始`, `已编码`, `已编译`, `已烧录`, `串口通过`, `需返工`.

| Phase | Goal | Verification | Status | Notes |
| --- | --- | --- | --- | --- |
| 1 | OTA check failure and unconfigured state closure | ESP-IDF build | 已编译 | Failure states now write `APP_BACKEND_OTA_FAILED` instead of silently returning |
| 2 | Automatic version report after IP ready | COM7 serial log | 串口通过 | `APP_OTA: version reported 1.3.0/1.3.1` appears after reconnect |
| 3 | Firmware UI snapshot behavior | Simulator screenshots + COM7 OTA log | 串口通过 | Firmware page, OTA ready/progress/complete/failed states wired through backend snapshot |
| 4 | OneNET OTA validation | `tmp\ota_130_to_131_com7.log` | 串口通过 | `1.3.0 -> 1.3.1` passed download, MD5, boot partition switch, user reboot |
| 5 | Retest baseline reset | `tmp\baseline_130_again_com7.log` | 串口通过 | Board reflashed to `1.3.0` for another OTA test |

Development changelog:

```text
docs/superpowers/changelogs/2026-04-28-ota-firmware-ui-backend-v1.md
```

### Task 1: OTA Check Failure And Unconfigured State Closure

**Files:**
- Modify: `components/AppBackend/ota_service.c`
- Test/verify: ESP-IDF build, serial log/manual UI check

- [x] Add a small helper in `ota_service.c` that maps check/report failures into `APP_BACKEND_OTA_FAILED` with a Chinese UI message.

Expected helper shape:

```c
static void ota_set_failed_check_state(const char *version, const char *message)
{
    ota_set_state(APP_BACKEND_OTA_FAILED,
                  version,
                  NULL,
                  0,
                  message != NULL ? message : "检测失败，请稍后重试");
}
```

- [x] In `ota_service_task()`, when `ota_service_configured()` is false, write snapshot state before returning.

Expected behavior:
- `state = APP_BACKEND_OTA_FAILED`
- `current_version = esp_app_get_description()->version`
- `target_version` unchanged or `NULL`
- `progress = 0`
- `message = "OTA 未配置，请检查参数"`
- `s_check_in_progress = false`

- [x] In `ota_service_task()`, check return values from `ota_service_report_version()` and `ota_service_check_task()`.

Expected flow:

```c
ota_set_state(APP_BACKEND_OTA_CHECKING, version, NULL, 0, "正在检测新版本");
err = ota_service_report_version(version);
if (err != ESP_OK) {
    ota_set_failed_check_state(version, "版本上报失败，请检查网络");
    s_check_in_progress = false;
    vTaskDelete(NULL);
    return;
}
err = ota_service_check_task(version);
if (err != ESP_OK) {
    ota_set_failed_check_state(version, "检测失败，请稍后重试");
}
```

- [x] Keep the existing successful states unchanged:
  - no update: `APP_BACKEND_OTA_IDLE`, message `当前已是最新版本`
  - update available: `APP_BACKEND_OTA_READY`
  - download flow untouched

- [x] Build with the existing ESP-IDF command. Expected: `Project build complete`.

### Task 2: Automatic Version Report After IP Ready

**Files:**
- Modify: `components/AppBackend/ota_service.h`
- Modify: `components/AppBackend/ota_service.c`
- Modify: `components/AppBackend/app_backend.c`
- Test/verify: ESP-IDF build, serial log after WiFi reconnect

- [x] Add a public API in `ota_service.h`:

```c
esp_err_t ota_service_report_current_version(void);
```

- [x] Implement it in `ota_service.c` as a synchronous best-effort function.

Expected behavior:
- If `CONFIG_APP_ONENET_OTA_ENABLE` is off, return `ESP_OK`.
- If OTA is not configured, log a warning and return `ESP_ERR_INVALID_STATE`.
- Read current version from `esp_app_get_description()`.
- Call existing `ota_service_report_version(version)`.
- Do not change the visible UI state on success.
- On failure, log warning but do not force an OTA popup by itself.

- [x] In `app_backend_handle_ip_ready()` after MQTT start and time-sync attempt, call `ota_service_report_current_version()` before weather refresh.

Expected code location:

```c
(void)mqtt_service_start();
...
if (ota_service_report_current_version() != ESP_OK) {
    ESP_LOGW(TAG, "OTA version report after IP ready failed");
}
app_backend_refresh_weather_with_diag();
```

- [x] Build with the existing ESP-IDF command. Expected: `Project build complete`.

### Task 3: Integration Verification For Frontend Snapshot Behavior

**Files:**
- Modify only if needed after Task 1/2 review.
- Verify: `components/UI/pages/page_firmware.c`, `components/UI/core/ui_data_bindings.c`, simulator screenshots, ESP-IDF build.

- [x] Confirm `page_firmware_apply_snapshot()` displays `APP_BACKEND_OTA_FAILED` by enabling the button again and showing the failure message.

Expected existing behavior:
- check button text returns to `检查更新`
- state label turns red for `APP_BACKEND_OTA_FAILED`
- failed modal appears unless the user dismissed the same failure state/target

- [x] Generate simulator screenshots for:

```powershell
$env:PATH='C:\msys64\ucrt64\bin;' + $env:PATH
.\simulator\build\lvgl_sim.exe --page firmware --screenshot tmp\sim_screens\firmware_after_ota_backend.bmp
.\simulator\build\lvgl_sim.exe --page ota_failed --screenshot tmp\sim_screens\ota_failed_after_backend.bmp
```

- [x] Run ESP-IDF build:

```powershell
$idf='D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1'
$py='D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe'
$export=& $py "$idf\tools\idf_tools.py" export --format key-value
foreach($line in $export){ if($line -match '^(.*?)=(.*)$'){ [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') } }
& $py "$idf\tools\idf.py" build
```

Expected: `Project build complete`, app partition still has enough free space.

- [x] Optional board check on COM7 if requested:

```powershell
& $py "$idf\tools\idf.py" -p COM7 flash
```

Expected: `Hash of data verified` and `Hard resetting via RTS pin...`.
