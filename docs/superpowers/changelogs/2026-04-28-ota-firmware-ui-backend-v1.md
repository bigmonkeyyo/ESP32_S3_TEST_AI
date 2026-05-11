# OTA Firmware UI Backend V1 Changelog - 2026-04-28

## Summary

This update completes the firmware-version UI landing and the first backend adaptation for user-driven OneNET OTA checks.

The device no longer depends only on an automatic WiFi-connected popup. The user can now enter the firmware-version page, check for updates, confirm download, watch progress, and reboot after the OTA image is ready.

## Changed

- Added firmware-version page entry from device status.
- Added firmware-version page with current version display and a bottom `检查更新` action.
- Added OTA ready, OTA progress, OTA complete, and OTA failed UI states.
- Updated OTA UI buttons and fonts to match the Figma 320x240 design more closely.
- Removed light button outline styling from existing Home, Settings, Status, and WiFi page buttons for visual consistency.
- Added heavier generated Chinese fonts for OTA title and button text.
- Added backend failure closure for OTA unconfigured, version-report failure, and check failure states.
- Added `ota_service_report_current_version()` so the upgraded firmware reports the new version after reconnecting.
- Updated `app_backend_handle_ip_ready()` to report OTA version after MQTT/time sync and before weather refresh.
- Preserved the backend snapshot contract so UI pages still consume OTA state through `backend_store` and `ui_data_bindings`.

## Verification

### Simulator

Generated simulator screenshots:

```text
tmp\sim_screens\firmware_after_ota_backend.bmp
tmp\sim_screens\ota_failed_after_backend.bmp
tmp\sim_screens\firmware_fixed2.bmp
tmp\sim_screens\ota_ready_fixed2.bmp
tmp\sim_screens\ota_progress_fixed2.bmp
tmp\sim_screens\ota_complete_fixed2.bmp
```

### Build And Flash

ESP-IDF build passed with:

```text
App "ESP32S3-AIroot" version: 1.3.0
Project build complete
ESP32S3-AIroot.bin binary size 0x1b4170 bytes
0x24be90 bytes (57%) free
```

COM7 flash passed with all written segments verified:

```text
Hash of data verified.
Hard resetting via RTS pin...
```

### OneNET OTA

Uploaded OTA package:

```text
tmp\ota131.bin
version: 1.3.1
size: 1786224
MD5: A74229FAA6E771854712F2FECA723ED1
SHA256: 1D5306DAC4FAF8443C37924B29FD7099836F3F908D805B7D0277A913F13520A9
```

Successful OTA log:

```text
App version:      1.3.0
APP_OTA: version reported 1.3.0
APP_OTA: update available target=1.3.1 tid=1373828 size=1786224
APP_OTA: waiting for UI confirmation before download
APP_OTA: download start tid=1373828 target=1.3.1 size=1786224 content_length=1786224 partition=ota_1
APP_OTA: download progress 100% (1786224/1786224)
APP_OTA: OTA ready; waiting for user reboot to 1.3.1 md5=a74229faa6e771854712f2feca723ed1
APP_OTA: restarting by user request
Loaded app from partition at offset 0x410000
App version:      1.3.1
APP_OTA: version reported 1.3.1
APP_OTA: no OTA task
```

Evidence log:

```text
tmp\ota_130_to_131_com7.log
```

### Reset For Retest

After the first OTA success, the board was rebuilt and reflashed back to baseline `1.3.0` for another user-side OTA test.

Current board baseline proof:

```text
tmp\baseline_130_again_com7.log
Loaded app from partition at offset 0x10000
App version:      1.3.0
APP_MQTT: connected
APP_OTA: version reported 1.3.0
```

## Current State

- Board currently runs `1.3.0`.
- `sdkconfig` currently contains `CONFIG_APP_PROJECT_VER="1.3.0"`.
- OneNET OTA package `tmp\ota131.bin` is available for `1.3.0 -> 1.3.1` retesting.
- OTA functional chain is verified: check task, user confirmation, download progress, MD5 validation, boot partition switch, user reboot, and post-upgrade version report.

## Remaining Work

- Decide whether the production behavior should keep manual reboot after download or support optional automatic reboot.
- Move OneNET sensitive keys out of committed `sdkconfig` before public sharing.
- Add a dedicated failure-test pass for bad network, bad MD5, and no task states.
- If the package list changes on OneNET, create a new higher version package because OneNET will not re-upgrade the same current version to the same target version indefinitely.

---

## Incremental Update - 2026-05-03 (V1.3.0 Gyro Ball)

### Scope

- Added new `Gyro Verify` entry in settings navigation and routed to new `UI_PAGE_GYRO`.
- Added new gyro page (`components/UI/pages/page_gyro.c/.h`) and simulator gyro page route.
- Integrated QMI8658A driver into BSP (`components/BSP/QMI8658A/*`, CMake include/source wiring).
- Added runtime gyro task in `main/main.c` to drive LVGL ball position with damping and boundary bounce.

### UX/Runtime Adjustments

- Fixed delayed first-screen issue by starting LVGL first and moving QMI8658A init into async task.
- Removed artificial gravity bias so motion depends on tilt only.
- Added startup zero calibration and tilt deadzone to reduce drift and sticky edge behavior.
- Fixed left/right direction inversion by reversing X-axis force sign.

### Validation Snapshot

- Build: pass (`idf.py build`).
- Flash: pass on `COM8` at `460800`.
- Serial verification:
  - LVGL starts before IMU init completion.
  - QMI8658A identify/calibration success logs present.
  - Continuous `GYRO` output and bounce logs present during movement.

---

## Incremental Update - 2026-05-07 (V1.3.1 BLE + Web Gyro Calibration)

### Scope

- Added BLE realtime gyro streaming module on device side (`components/GyroBle`).
- Extended IMU pipeline to publish posture and quaternion-related data for web visualization.
- Added Web Bluetooth 3D viewer first version (`web/gyro_ble_viewer.html`), including:
  - device connection/disconnection and stream control
  - realtime posture display (`roll/pitch/yaw`)
  - on-page three-pose calibration flow
  - status, FPS, timestamp, and ball state panel

### Runtime/UX Notes

- Current web visualization path has been rolled back to non-quaternion render mapping to keep interaction behavior stable.
- Device model orientation has been corrected to match:
  - screen facing up on flat placement
  - physical slot/notch facing direction aligned with rendered model
- Calibration matrix now disables cross-coupling terms (`m12/m21`) to reduce left-right tilt causing unintended in-out tilt.

### Validation Snapshot

- Firmware build/flash: completed in this iteration and BLE stream path available on board.
- Web Bluetooth connection: verified with Chrome/Edge flow.
- Realtime updates: posture and state panel refresh continuously after BLE connect.
- Regression checks: existing LVGL main pages remain available after BLE feature integration.

---

## Incremental Update - 2026-05-09 (QMI8658A Delay Unit Fix)

### Scope

- Fixed QMI8658A startup delay units in `components/BSP/QMI8658A/qmi8658a.c`.
- Added local helper `qmi8658_delay_ms()` to convert millisecond waits through `pdMS_TO_TICKS()`.
- Replaced bare numeric `vTaskDelay(...)` calls in the QMI8658A driver, including reset, calibration, Ctrl9 command polling, and sensor-enable settle waits.

### Root Cause

- The driver used calls such as `vTaskDelay(2000)` while the project runs with `CONFIG_FREERTOS_HZ=100`.
- `vTaskDelay()` expects FreeRTOS ticks, not milliseconds.
- At 100 Hz, `vTaskDelay(2000)` became about 20 seconds, so the QMI8658A reset and calibration path added roughly 40 seconds before gyro output started.

### Validation Snapshot

- Static check: pass; no bare numeric `vTaskDelay(...)` remains in `components/BSP/QMI8658A/qmi8658a.c`.
- Build: pass (`idf.py --no-hints --no-ccache -C D:\ESP32-APP\18_touch build`).
- Flash: pass on `COM7` at `460800`; flash log showed `Hash of data verified` and `Hard resetting via RTS pin`.
- Serial log: `tmp\qmi8658_delay_ms_fix_com7.log`.
- Runtime evidence after fix:
  - `qmi8658` WhoAmI detected at `2.788s`.
  - `QMI8658A init ok` at `4.808s`.
  - `GYRO: zero calibrated` at `8.728s`.
  - Continuous `GYRO` output followed normally.
- Failure keyword scan: pass; no `Guru Meditation`, `task_wdt`, `panic`, `abort`, `backtrace`, `stack overflow`, or `SCREEN_UNLOADED`.

### Current Result

- Gyro startup was reduced from about 44 seconds to about 8.7 seconds on the verified COM7 run.
- This change fixes only the tick/millisecond unit bug; further calibration-duration optimization remains a separate task if needed.

---

## Incremental Update - 2026-05-09 (Wi-Fi/BLE Memory Pressure + Weather Sync)

### Scope

- Enabled the ESP32-S3 onboard 8MB PSRAM in `sdkconfig` and added it to the system heap.
- Switched LVGL allocations to the system malloc path so page objects can use PSRAM instead of a fixed internal LVGL heap.
- Reduced backend online task stack usage and tuned MQTT task/buffer sizing to preserve internal RAM.
- Increased the reserved internal/DMA pool for Wi-Fi, BLE, I2C, task stacks, and LCD DMA-sensitive allocations.
- Changed weather sync from AMap HTTPS to HTTP to avoid TLS handshake memory pressure after Wi-Fi, BLE, and MQTT are already running.
- Updated I2C transfer handling so transient command allocation/write failures return errors instead of forcing a whole-device abort.

### Root Cause

- The hardware reports `Embedded PSRAM 8MB`, but the active `sdkconfig` had PSRAM disabled.
- With Wi-Fi + BLE + LVGL + MQTT running together, internal RAM became too tight for small internal-only allocations.
- Serial evidence before the fix included:
  - `BLE_INIT: Malloc failed`
  - `mqtt_client: Error create mqtt task`
  - `i2c command link malloc error`
  - SNTP/HTTP allocation failures and reset/assert paths
- Weather sync used HTTPS with certificate bundle verification, which required more TLS memory than remained after the online stack was active.

### Validation Snapshot

- Static memory config check: pass.
- Build: pass (`idf.py --no-hints --no-ccache -C D:\ESP32-APP\18_touch build`).
- Flash: pass on `COM7` at `460800`.
- Serial logs:
  - `tmp\wifi_ip_reset_memory_tuned_com7.log`
  - `tmp\weather_http_fix_com7.log`
- Runtime evidence:
  - PSRAM initialized: `Found 8MB PSRAM device` and `Adding pool of 8192K of PSRAM memory to heap allocator`.
  - BLE prepared after Wi-Fi IP without BLE malloc failure.
  - MQTT connected and published status after BLE stack preparation.
  - Weather sync succeeded: `BACKEND_WEATHER: AMap weather updated`.
  - BLE UI switch test reached `GYRO_BLE: streaming enabled` and `advertising enabled`.
  - No `Guru Meditation`, `abort`, `assert failed`, or MQTT task-create failure in the final weather verification run.

### Notes

- Final log still shows one early `i2c transfer failed` warning during sensor probing, but it recovers and QMI8658A initializes normally.
- A later `rst:0x1 POWERON` observed in one earlier long run had no software backtrace or abort log before it, so it looks more like external reset/power behavior than a firmware panic.
