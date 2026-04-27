# WiFi Time Weather Backend V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first real backend for WiFi scan/connect, SNTP time sync, and Shanghai weather, then surface those states in the existing LVGL Home, WiFi, and Status pages.

**Architecture:** Add an independent `components/AppBackend` component. UI pages call only async backend APIs and read immutable snapshots; backend tasks update `backend_store`, then request LVGL-safe refresh through `ui_data_bindings`.

**Tech Stack:** ESP-IDF 5.2.x, FreeRTOS task/queue/event groups, ESP WiFi STA, esp_netif, esp_event, NVS, SNTP, esp_http_client, cJSON, LVGL 8.

---

## Development Rules And Limits

- Backend implementation lives under `components/AppBackend/`; page files must not call `esp_wifi_*`, `esp_http_client_perform`, or SNTP APIs directly.
- UI event callbacks may only enqueue async backend actions and update local widgets; they must not block on WiFi scan/connect, HTTP, or time sync.
- Backend threads and ESP-IDF event callbacks must not access LVGL objects. UI refresh must go through `ui_data_bindings_notify_async()` and `lv_async_call`.
- `backend_store` is the single source of truth for WiFi, time, weather, and status messages.
- All external strings are bounded: SSID 32 bytes plus NUL, password 64 bytes plus NUL, status messages 64 bytes, HTTP response buffer fixed and checked.
- WiFi passwords must never be printed to logs.
- The weather provider must be replaceable behind `weather_service`; no weather URL, key, provider-specific parser, or weather fallback logic may live in UI page files.
- Weather API keys must not be logged to COM7. Documentation should mask keys unless the code path itself requires the literal key.
- Any new Chinese UI/backend display string must be checked against the generated LVGL font coverage. Prefer updating `tools/gen_ui_fonts.py` and regenerating all `ui_font_sc_*` files after batches of string changes.
- Do not change proven LVGL/display performance values during backend V1 unless a build failure proves it is required.
- Do not modify page manager lifecycle semantics unless COM7 evidence proves backend integration requires it.

## Backend V1 Progress Table

Status values: `未开始`, `进行中`, `已编码`, `已编译`, `已烧录`, `串口通过`, `需返工`.

| Phase | Goal | Main files | Verification | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | Confirm design and constraints | `docs/superpowers/specs/2026-04-27-wifi-time-weather-backend-design.md` | User confirmed option A | 串口通过 | Existing spec includes backend rules, subagent rules, and progress table |
| 1 | Create execution plan | `docs/superpowers/plans/2026-04-27-wifi-time-weather-backend-v1.md` | Plan file exists and maps to code files | 已编译 | This file is the active execution/progress record |
| 2 | AppBackend component shell | `components/AppBackend/CMakeLists.txt`, `include/app_backend*.h`, `app_backend.c` | `idf.py build` reaches component compile | 已烧录 | Backend task starts after UI init |
| 3 | Snapshot store | `backend_store.*`, `app_backend_types.h` | Backend self-test logs PASS/FAIL | 串口通过 | COM7: `APP_BACKEND_SELFTEST: store PASS` |
| 4 | UI binding dispatcher | `components/UI/core/ui_data_bindings.c`, `components/UI/include/ui_data_bindings.h` | `UI_BINDING: snapshot applied` log | 串口通过 | COM7 log shows repeated snapshot applies |
| 5 | Real WiFi scan | `wifi_service.*`, `page_wifi.c` | COM7 logs `scan done, ap_count=N` | 串口通过 | COM7: `scan done, ap_count=8` |
| 6 | Real WiFi connect and IP | `wifi_service.*`, NVS credential helpers | COM7 logs `got ip <ip>` | 需返工 | Code compiled/flashed; saved `SAT-Guest` disconnected with reason=2, no valid password proof yet |
| 7 | SNTP time sync | `time_service.*` | COM7 logs `SNTP synced <time>` | 已烧录 | Code present; waits for successful IP |
| 8 | AMap Shanghai weather | `weather_service.*` | HTTPS status and `AMap weather updated` logs | 串口通过 | Provider switched to 高德天气 WebService; AQI intentionally displays `AQI --` |
| 9 | Home/Status UI landing | `page_home.c`, `page_status.c` | Pages show real snapshot data | 已烧录 | Snapshot apply wired; full weather display waits for IP/weather |
| 10 | Full regression | All touched files | Build, flash, COM7, fixed route | 串口通过 | Startup + Settings -> WiFi + scan path passed; no failure keywords in log |
| 11 | Experience notes | `LVGL_UI_编译烧录调试与避坑经验.md` | New backend section updated | 串口通过 | Backend V1 AMap, font, WiFi state, COM7 findings recorded |

## Task 1: Component Shell And Types

**Files:**
- Create: `components/AppBackend/CMakeLists.txt`
- Create: `components/AppBackend/include/app_backend.h`
- Create: `components/AppBackend/include/app_backend_types.h`
- Create: `components/AppBackend/app_backend.c`
- Modify: `components/Middlewares/lvgl_port/CMakeLists.txt`
- Modify: `components/Middlewares/lvgl_port/lvgl_port.c`

- [ ] Create the `AppBackend` component with public async APIs:
  - `app_backend_start`
  - `app_backend_wifi_scan_async`
  - `app_backend_wifi_connect_async`
  - `app_backend_weather_refresh_async`
  - `app_backend_get_snapshot`
- [ ] Add snapshot data types exactly once in `app_backend_types.h`.
- [ ] Start the backend after `ui_init()` in `lvgl_port_start()`.
- [ ] Build and confirm `APP_BACKEND: backend started` can be compiled into firmware.

## Task 2: Store And Self-Test

**Files:**
- Create: `components/AppBackend/backend_store.h`
- Create: `components/AppBackend/backend_store.c`
- Modify: `components/AppBackend/app_backend.c`

- [ ] Add mutex-protected snapshot initialization.
- [ ] Add bounded setters for AP list, connected SSID/IP/RSSI, time strings, weather, and message.
- [ ] Add a small boot self-test log guarded by component code, printing `APP_BACKEND_SELFTEST: store PASS` or `FAIL`.
- [ ] Build and verify no buffer-related compiler warnings are introduced.

## Task 3: UI Binding Layer

**Files:**
- Create: `components/UI/include/ui_data_bindings.h`
- Create: `components/UI/core/ui_data_bindings.c`
- Modify: `components/UI/CMakeLists.txt`
- Modify: `components/UI/pages/page_home.h`
- Modify: `components/UI/pages/page_wifi.h`
- Modify: `components/UI/pages/page_status.h`

- [ ] Add `ui_data_bindings_notify_async()` which schedules `lv_async_call`.
- [ ] In the async callback, fetch a backend snapshot and apply it to the currently alive pages.
- [ ] Add page-level apply declarations:
  - `page_home_apply_snapshot`
  - `page_wifi_apply_snapshot`
  - `page_status_apply_snapshot`
- [ ] Log `UI_BINDING: snapshot applied` only from LVGL context.

## Task 4: WiFi Scan And Connect

**Files:**
- Create: `components/AppBackend/wifi_service.h`
- Create: `components/AppBackend/wifi_service.c`
- Modify: `components/AppBackend/app_backend.c`
- Modify: `components/UI/pages/page_wifi.c`

- [ ] Initialize `esp_netif`, default event loop, and WiFi STA once.
- [ ] Implement backend queue commands for scan, connect, and weather refresh.
- [ ] Use real `esp_wifi_scan_start(..., true)` only from backend task context.
- [ ] Save successful credentials to NVS without logging password.
- [ ] Replace fake AP rows in WiFi page with snapshot AP rows.

## Task 5: Time And Weather

**Files:**
- Create: `components/AppBackend/time_service.h`
- Create: `components/AppBackend/time_service.c`
- Create: `components/AppBackend/weather_service.h`
- Create: `components/AppBackend/weather_service.c`
- Modify: `components/AppBackend/app_backend.c`

- [ ] Sync SNTP after IP is ready.
- [ ] Format `now_time`, `uptime`, and `last_sync`.
- [x] Fetch AMap Shanghai current weather by HTTPS.
- [ ] Parse JSON with cJSON and tolerate missing fields with `--`.
- [x] Map AMap `lives[0]` fields to Home UI: weather, temperature, humidity, wind, report time.
- [x] Keep AQI as `AQI --` for Backend V1 because the selected AMap weather endpoint does not provide air quality.

## Task 6: Home And Status Landing

**Files:**
- Modify: `components/UI/pages/page_home.c`
- Modify: `components/UI/pages/page_status.c`

- [ ] Store handles for Home time, main weather, humidity, wind, and AQI labels.
- [ ] Store handles for Status WiFi signal, IP, runtime, and sync labels.
- [ ] Apply snapshot on page create and page show.
- [ ] Keep offline placeholders readable when WiFi/weather are not valid.

## Task 7: Verification And Experience Notes

**Files:**
- Modify: `LVGL_UI_编译烧录调试与避坑经验.md`
- Update: this progress table

- [ ] Build with the project ESP-IDF command from the debug guide.
- [ ] Flash COM7 when build passes.
- [ ] Capture serial logs and search for:
  - `APP_BACKEND: backend started`
  - `BACKEND_WIFI: scan start`
  - `BACKEND_WIFI: scan done`
  - `BACKEND_WIFI: got ip`
  - `BACKEND_TIME: SNTP synced`
  - `BACKEND_WEATHER: AMap weather updated`
  - `UI_BINDING: snapshot applied`
- [ ] Search logs for failure keywords:
  - `task_wdt`
  - `Guru Meditation`
  - `backtrace`
  - `panic`
  - `abort`
  - `skip destroy active screen`
  - `SCREEN_UNLOADED`
- [ ] Record new backend-specific findings in the experience document.

## Current Progress Table Update - 2026-04-27

This section supersedes the earlier progress table entries that were created before the hotspot verification run.

| Phase | Goal | Status | COM7 evidence |
| --- | --- | --- | --- |
| 5 | Real WiFi scan | serial-pass | `BACKEND_WIFI: scan done, ap_count=8` |
| 6 | Real WiFi connect and IP | serial-pass | `BACKEND_WIFI: got ip 172.20.10.14` |
| 7 | SNTP time sync | serial-pass | `BACKEND_TIME: SNTP synced 16:36` |
| 8 | AMap weather | serial-pass | `BACKEND_WEATHER: AMap weather updated` |
| 9 | UI snapshot landing | serial-pass | `UI_BINDING: snapshot applied` |
| 10 | Saved WiFi reboot persistence | serial-pass | `BACKEND_WIFI: connecting saved ssid zhangxiaohou` |
| 11 | Runtime regression | serial-pass | Latest 60s reboot log reported 0 error entries |

Verified artifacts and logs:
- Build passed with `build/18_touch.bin` size `0x16ffe0`; factory app partition `0x1f0000`, free `0x80020` bytes.
- Flash to COM7 passed with `Hash of data verified` and `Hard resetting via RTS pin`.
- Captive portal diagnosis is implemented in `network_diag_service.*`: old office WiFi returned HTTP probe `302`, while hotspot returned `204`.
- Saved logs: `tmp/backend_v1_hotspot_http_weather_com7.log`, `tmp/backend_v1_hotspot_saved_reboot_com7.log`.
- Latest 60s reboot log has no `task_wdt`, `Guru Meditation`, `backtrace`, `panic`, `abort`, `stack overflow`, or `SCREEN_UNLOADED`.

Implementation adjustments from verification:
- `wifi_service.c` now treats `WIFI_REASON_ASSOC_LEAVE` during an intentional reconnect as a normal transition, so pending credentials are not cleared before `IP_EVENT_STA_GOT_IP`.
- `weather_service.c` keeps the weather provider replaceable. Backend V1 has switched from Open-Meteo/wttr/WAQI experiments to the AMap weather WebService HTTPS endpoint.
- `page_home.c` shows the backend status message when WiFi is connected but weather data is not yet valid, so captive portal detection can land visibly in the UI.

## Current Progress Table Update - AMap Provider And Font Fix - 2026-04-27

This section supersedes all earlier weather-provider entries. Open-Meteo, wttr.in, and WAQI were temporary experiments and are no longer the active Backend V1 weather path.

| Phase | Goal | Status | COM7 / build evidence |
| --- | --- | --- | --- |
| 6 | WiFi connect and IP | serial-pass | `BACKEND_WIFI: got ip 172.20.10.14` |
| 7 | SNTP time sync | serial-pass | `BACKEND_TIME: SNTP synced 18:36` |
| 8 | AMap Shanghai weather | serial-pass | `BACKEND_WEATHER: AMap request start` then `BACKEND_WEATHER: AMap weather updated` |
| 8.1 | HTTPS certificate path | serial-pass | `esp-x509-crt-bundle: Certificate validated` |
| 8.2 | AQI display | accepted | User confirmed AQI can remain `AQI --` in Backend V1 |
| 8.3 | Weather font coverage | user-confirmed | User confirmed temperature, humidity, wind, city/weather, and WiFi disconnected text display normally |
| 10 | Runtime regression | serial-pass | Latest logs contain no `Guru Meditation`, `task_wdt`, `panic`, `abort`, or backtrace keywords |

Current implementation notes:
- Active endpoint: AMap Weather WebService `weatherInfo`, fixed city `310000` for Shanghai, `extensions=base`, `output=JSON`.
- Key is currently compiled into `weather_service.c` as a replaceable Backend V1 configuration constant. Do not print it to logs.
- Field mapping:
  - `lives[0].city` -> `weather.city`
  - `lives[0].weather` -> `weather.condition`
  - `lives[0].temperature` -> `weather.temperature` as `xx℃`
  - `lives[0].humidity` -> `weather.humidity` as `湿度 xx%`
  - `lives[0].winddirection` + `lives[0].windpower` -> `weather.wind`
  - `lives[0].reporttime` -> `weather.updated_at` as `HH:MM`
  - AQI remains `AQI --`
- `windpower` may contain the non-ASCII symbol `≤`. Backend normalizes it to ASCII `<=` before it reaches LVGL.
- Build after AMap + auto font coverage passed with `build/18_touch.bin` size `0x1916a0`; factory partition `0x1f0000`; free space `0x5e960` bytes, about 19%.
- Verified logs:
  - `tmp/amap_weather_com7.log`
  - `tmp/amap_weather_fontfix_com7.log`
  - `tmp/amap_weather_font_autoscan_com7.log`

Backend V1 lessons from this round:
- Do not call `lv_async_call()` directly from backend or ESP-IDF event task context unless the path is explicitly LVGL-safe. The safer pattern is backend sets a pending flag and the LVGL main loop processes `ui_data_bindings_process_pending()`.
- Do not assume IP means internet. Captive portal WiFi can connect at the 802.11/IP layer but still block weather/time access with a web-auth page.
- Do not keep layering weather fallbacks after network symptoms. Once the user provides a stable provider, remove temporary providers and simplify logs.
- Do not rely on manual font samples only. `tools/gen_ui_fonts.py` now scans UI and AppBackend source files plus provider-specific weather words, then regenerates all sizes together.
- When users report "font乱码", check both provider-returned dynamic text and static UI text. In this round the missing characters included `市`, `阴`, `等`, `待`, and `无`; the wind symbol `≤` was better handled by backend normalization.
- Keep COM7 validation as the source of truth for backend work: build -> flash -> serial monitor -> keyword search.

## Current Progress Table Update - WiFi State And Home Status Fix - 2026-04-27

This section records the post-AMap UI state fixes after the user confirmed weather display was normal but found two state bugs.

| Phase | Goal | Status | COM7 / build evidence |
| --- | --- | --- | --- |
| 5.1 | Connected WiFi top-right action | serial-pass | Connected state shows red `断开`; clicking it logs `BACKEND_WIFI: disconnect requested, forget_saved=1` |
| 5.2 | Scan button exits busy state | serial-pass | `BACKEND_WIFI: scan start` then `BACKEND_WIFI: scan done, ap_count=8`; `UI_BINDING: snapshot applied` follows |
| 5.3 | Scan result store write reliability | coded-and-verified | `backend_store_set_scan_results()` now waits up to `200ms` for the store mutex instead of silently skipping on lock contention |
| 9.1 | Home sync status after disconnect | coded-and-verified | Home subtitle now shows `联网天气已同步` only when WiFi is connected and weather is valid; disconnected state lands as `WiFi 已断开` |
| 10.1 | Runtime regression after state fixes | serial-pass | Latest log has no `Guru Meditation`, `task_wdt`, `panic`, `abort`, `backtrace`, or `SCREEN_UNLOADED` |

Verified artifacts and logs:
- Build passed with `build/18_touch.bin` size `0x191800`; factory app partition `0x1f0000`; free app space `0x5e800` bytes, about 19%.
- Flash to COM7 passed with hash verification.
- Runtime log: `tmp/wifi_scan_state_fix_com7.log`.
- Expected scan sequence observed twice:
  - `BACKEND_WIFI: scan start`
  - `BACKEND_WIFI: scan done, ap_count=8`
  - `UI_BINDING: snapshot applied`
- One `BACKEND_TIME: SNTP sync failed: ESP_ERR_TIMEOUT` warning was observed, but AMap weather updated afterward. Treat this as a transient time-sync warning, not a WiFi scan regression.

Implementation adjustments from verification:
- `backend_store_set_scan_results()` now sets `s_snapshot.wifi_state = APP_BACKEND_WIFI_IDLE` after writing AP results, so the WiFi page can leave `扫描中`.
- `backend_store_set_scan_results()` no longer uses a zero-time mutex take. The old path could log `scan done` while skipping the actual snapshot update if the UI binding was reading the store at the same moment.
- `page_home.c` now gates `联网天气已同步` on both `APP_BACKEND_WIFI_CONNECTED` and `snapshot->weather.valid`, so cached weather does not imply current connectivity.
- Font coverage for the added/used offline text was checked in `ui_font_sc_11.c`, `ui_font_sc_12.c`, and `ui_font_sc_14.c`; `已`, `断`, and `开` are present.

Backend V1 lessons from this fix:
- A serial log that shows `scan done` does not prove the UI snapshot changed. For state bugs, check the store write path and mutex behavior as well as the WiFi driver log.
- Do not use `xSemaphoreTake(..., 0)` for user-visible state commits unless dropping the update is explicitly acceptable.
- Cached successful weather data and current network state are separate concepts. UI text must not describe cached data as "联网" after WiFi disconnects.
- For WiFi page behavior, `SCANNING` is a short-lived backend operation state, not a disconnected idle state.
