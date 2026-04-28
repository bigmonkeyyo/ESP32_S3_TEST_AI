# LVGL UI 编译烧录调试与避坑经验（ESP32-S3 / 320x240）

## 1. 文档目的
- 给后续智能体一个可直接复用的实战手册，避免重复踩坑。
- 覆盖范围：编译、烧录、JTAG断点调试、页面卡死复位、UI显示不完整（布局裁切）问题。

## 2. 项目关键背景
- 工程路径：`d:\11\ESP32PRO\ESP32_S3_TEST_AI`
- 屏幕分辨率：`320 x 240`（横屏）
- UI框架：LVGL 多页面管理（`ui_page_manager`）
- 调试方式：`OpenOCD + xtensa-esp32s3-elf-gdb`（USB-JTAG）或串口日志
- 当前实机串口：远端设备使用 `USB-Enhanced-SERIAL CH343 (COM7)`，可用于烧录和日志抓取

## 3. 编译与烧录稳定SOP（推荐固定做法）

## 3.1 常见坑
- 直接执行 `idf.py` 报找不到命令（PATH未激活）。
- PowerShell 执行策略拦截 `export.ps1`。
- 系统 Python 版本不匹配（例如 3.7），导致 IDF 工具链报错。
- 虚拟环境路径不一致，构建器提示 Python 环境不匹配。

## 3.2 建议命令（已在本项目验证）
1. 编译：
```powershell
$env:IDF_PATH='D:\esp32\Espressif\frameworks\esp-idf-v5.2.1'
& "D:\esp32\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe" `
  "$env:IDF_PATH\tools\idf.py" --no-hints --no-ccache `
  -C d:\11\ESP32PRO\ESP32_S3_TEST_AI build
```

2. 烧录（COM口按实际替换）：
```powershell
$env:IDF_PATH='D:\esp32\Espressif\frameworks\esp-idf-v5.2.1'
& "D:\esp32\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe" `
  "$env:IDF_PATH\tools\idf.py" --no-hints --no-ccache `
  -C d:\11\ESP32PRO\ESP32_S3_TEST_AI -p COM9 -b 460800 flash
```

## 3.3 验收标准
- `build/18_touch.elf` 与 `build/18_touch.bin` 生成成功。
- 烧录日志出现 `Hash of data verified` 且 `Hard resetting via RTS pin`。

## 4. 页面跳转“卡死复位”问题总结（已定位并修复）

## 4.1 复现路径
- `首页 -> 设置 -> 设备状态 -> 返回首页`
- 二次复现路径：
  - `首页 -> 设置 -> WiFi -> 返回设置 -> 设备状态`

## 4.2 断点抓栈关键信息
- 崩溃路径稳定落在：
  - `lv_disp.c::scr_anim_ready`
  - `lv_event_send(... LV_EVENT_SCREEN_UNLOADED ...)`
  - `lv_obj_event_base(class_p=0x0, ...)`
- 说明：旧屏在 `SCREEN_UNLOADED` 生命周期附近发生对象时序竞态（提前销毁/冲突访问）。
- 二次复现时串口日志关键信息：
  - `UI_MGR: PUSH 1 -> 3 depth=3`
  - `UI_MGR: skip destroy active screen id=3 root=...`
  - `UI_MGR: POP 3 -> 1 depth=2`
  - 随后出现 `task_wdt`，CPU0 卡在 `main`。
- addr2line 解码后关键调用链：
  - `lv_anim_start` -> `lv_scr_load_anim`
  - `ui_page_anim_load` -> `ui_page_push`
  - `page_settings_nav_async`
  - `lv_timer_handler` / `lvgl_port_start`

## 4.3 根因
- 页面回退时，旧页面对象的删除时机与 LVGL 屏幕卸载动画/事件处理时机冲突。
- 固定延时删除策略在某些路径仍可能踩中竞态窗口。
- 二次根因：启动屏幕切换动画后，刚退出的 WiFi 页面在短时间内仍可能是 `lv_scr_act()`，旧逻辑把 `root == active` 判定为“不能销毁”并直接返回。
- 这个“跳过销毁”没有给旧屏绑定 `LV_EVENT_SCREEN_UNLOADED` 删除回调，导致 `CACHE_NONE` 页面残留在异常生命周期状态；下一次从设置页进入设备状态页时，再次启动屏幕动画，LVGL 动画链路卡死并触发任务看门狗。

## 4.4 修复策略（最终生效）
1. 中间回退路径使用无动画切换（降低叠加动画风险）：
   - `UI_ANIM_NONE` 走 `lv_scr_load(screen)` 立即切换。
2. 页面销毁从“固定延时删”改为“事件驱动删”：
   - 若对象是 `prev screen`，在 `LV_EVENT_SCREEN_UNLOADED` 回调里 `lv_obj_del_async`。
   - 若对象当前仍是 `active screen`，也不要直接跳过；它可能只是动画刚开始时的“即将卸载旧屏”。
   - 对 `root == active` 或 `root == prev` 的页面统一绑定 `LV_EVENT_SCREEN_UNLOADED` 回调，等卸载完成后异步删除。
   - 调度删除后立即清空 `s_page_roots[desc->id]`，避免后续导航继续复用已安排销毁的旧对象。
3. WiFi 页、设备状态页这类子页面使用 `UI_PAGE_CACHE_NONE`，不要长期缓存动态页面根对象。

## 4.5 关键代码位置
- `components/UI/core/ui_page_anim.c`
  - `ui_page_anim_load(...)` 对 `UI_ANIM_NONE` 特殊处理。
- `components/UI/core/ui_page_manager.c`
  - `ui_page_destroy_if_needed(...)` 改为按 `SCREEN_UNLOADED` 事件删除。
  - 新增 `ui_page_delete_on_unloaded_cb(...)`。
  - 注意：不能再把 `root == active` 简单当成“跳过销毁”，否则会复现 WiFi 返回后再进状态页卡死。
- `components/UI/pages/page_settings.c`
  - `page_settings_nav_async(...)` 是设置页进入子页面的异步导航入口，串口栈里会出现它。
- `components/UI/pages/page_wifi.c` / `components/UI/pages/page_status.c`
  - 子页面缓存策略应保持为 `UI_PAGE_CACHE_NONE`。

## 4.6 本次二次定位验证记录（2026-04-26）
- 复现前日志：`tmp\repro_com8_before_instrument.log`
  - 已出现 `skip destroy active screen`、`task_wdt` 和 backtrace。
- 修复后日志：`tmp\repro_com8_after_manager_fix.log`
  - 已连续验证以下路径：
    - `PUSH 0 -> 1`
    - `PUSH 1 -> 3`
    - `POP 3 -> 1`
    - `PUSH 1 -> 2`
    - `POP 2 -> 1`
    - `POP 1 -> 0`
    - `BACK_TO_ROOT depth=1`
  - 未再出现 `task_wdt`、`skip destroy active` 或 backtrace。
- 构建命令（本次验证使用）：
```powershell
cmd /c "call D:\esp32\Espressif\idf_cmd_init.bat esp-idf-9df0ad0b8292cf243ded566bf3cb03fd && idf.py --no-hints --no-ccache -C D:\11\ESP32PRO\ESP32_S3_TEST_AI build"
```
- 烧录命令（COM7）：
```powershell
cmd /c "call D:\esp32\Espressif\idf_cmd_init.bat esp-idf-9df0ad0b8292cf243ded566bf3cb03fd && idf.py --no-hints --no-ccache -C D:\11\ESP32PRO\ESP32_S3_TEST_AI -p COM7 -b 460800 flash"
```
- 串口排查原则：复现卡死后优先抓完整日志，再用 backtrace 地址做 addr2line；如果日志里出现 `skip destroy active screen`，优先检查 `ui_page_destroy_if_needed(...)` 的卸载后删除逻辑。

## 5. JTAG 调试实战经验（避免工具链坑）

## 5.1 已踩坑
- GDB 启动失败：`ModuleNotFoundError: encodings`
  - 原因：GDB 的 Python 运行时路径异常。
  - 处理：设置 `PYTHONHOME=D:\esp32\Espressif\tools\idf-python\3.11.2` 再启动 gdb。
- OpenOCD 链路偶发不稳：`missing data from bitq interface`
  - 处理：优先使用历史稳定脚本链路（已验证可抓栈），必要时降低适配器速度并重连。

## 5.2 断点建议（最小集合）
- `ui_page_back_to_root`
- `ui_page_pop`
- `page_status_home_cb`
- 异常候选：`lv_obj_event_base if class_p==0`

## 5.3 调试原则
- 先抓证据（堆栈、寄存器、线程回溯），再改代码。
- 每次只做一个最小改动，改后立即编译烧录复测同一路径。

## 6. UI“显示不完整/裁切”问题经验（320x240 横屏）

## 6.1 常见表现
- 顶部信息（时间/城市天气）被中间卡片遮挡。
- 底部按钮显示不全。
- 子页面右侧滚动区域、底部操作栏被裁切。

## 6.2 原因模式
- 超出 240 高度预算，且容器叠层/定位未预留底部操作区。
- 文本字体大小、行高、内边距与设计稿不一致。
- 同时放置过多固定区块，导致可用高度不足。

## 6.3 已验证有效做法
- 严格分区：`Header + ScrollView + Footer`，Footer固定留高。
- 子页面内容超出时只放到 `ScrollView`，不要挤占 Footer。
- 必要时删除非关键装饰区（例如底部说明文案区）以释放高度。
- 返回按钮做隐式文本按钮，减少头部占位冲突。
- 优先使用中文专用生成字库，不用 LVGL 自带不全字库。

## 7. 刷新率优化任务交接记录（2026-04-26）

## 7.1 当前任务概述
- 目标：在不改页面渲染效果的前提下，逐步提升 UI 跳转、滚动和触摸跟手性。
- 限制：不要改页面布局、颜色、渐变、圆角、阴影、字体和组件结构；优先只调 LVGL/ESP-IDF 调度、显示刷新和触摸读取参数。
- 当前判断：设备不是内存完全不够；更主要是 LVGL 默认刷新周期、触摸读取周期、绘制/flush 链路共同限制帧率。

## 7.2 阶段稳定配置（Step 4）
- Step 4 阶段已稳定烧录验证：
  - `CONFIG_LV_DISP_DEF_REFR_PERIOD=16`
  - `CONFIG_LV_INDEV_DEF_READ_PERIOD=15`
  - `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240`
  - `CONFIG_FREERTOS_HZ=100`
  - `CONFIG_ESP_MAIN_TASK_AFFINITY=0x0`（main task 在 CPU0）
  - `components/Middlewares/lvgl_port/lvgl_port.c`：`LVGL_DRAW_BUF_LINES=40`
  - `components/BSP/LCD/lcd.c`：`LCD_I80_PCLK_HZ = 20MHz`
- 已验证路径：
  - `首页 -> 设置 -> WiFi -> 返回设置`
  - `首页 -> 设置 -> 设备状态 -> 返回首页`
  - 多次 `首页 -> 设备状态 -> 返回首页`
- Step 1 日志：`tmp\step1_refr20_verify.log`
- Step 2 日志：`tmp\step2_refr20_indev15_verify.log`
- Step 3 日志：`tmp\step3_cpu240_verify.log`、`tmp\step3_cpu240_settings_wifi_status_verify.log`
- Step 4 日志：`tmp\step4_cpu240_refr16_verify.log`、`tmp\step4_cpu240_refr16_full_route_verify.log`
- 结果：未出现 `task_wdt`、`backtrace`、`skip destroy active screen`、`SCREEN_UNLOADED`。

## 7.3 已执行计划与进度
1. Step 1：仅把 `CONFIG_LV_DISP_DEF_REFR_PERIOD` 从 `30` 改为 `20`。
   - 目的：显示刷新理论上从约 33FPS 上限提升到约 50FPS 上限。
   - 状态：已编译、烧录、串口验证通过。
2. Step 2：仅把 `CONFIG_LV_INDEV_DEF_READ_PERIOD` 从 `30` 改为 `15`。
   - 目的：提升触摸读取频率，改善滚动跟手性。
   - 状态：已编译、烧录、串口验证通过。
3. Step 3：仅把 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` 从 `160` 改为 `240`。
   - 目的：提升 LVGL 绘制与页面切换期间的 CPU 余量。
   - 状态：已编译、烧录、串口验证通过，用户实机确认可正常运行。
4. Step 4：仅把 `CONFIG_LV_DISP_DEF_REFR_PERIOD` 从 `20` 改为 `16`。
   - 目的：显示刷新理论上从约 50FPS 上限提升到约 62.5FPS 上限。
   - 状态：已编译、烧录、串口验证通过。
5. Step 4 后续建议曾为：只改一个变量继续推进。
   - 候选 A：仅把 `LCD_I80_PCLK_HZ` 从 `20MHz` 提到 `30MHz`，只测 LCD 总线余量。
   - 候选 B：仅把 `LVGL_DRAW_BUF_LINES` 从 `40` 提到 `80`，只测 draw buffer 余量。
   - 二选一，不要同时改。
   - 后续实测结论见 7.6：PCLK `30MHz` 会导致设置页卡死，当前应保持 `20MHz`。

## 7.4 已踩坑：不要一次性推多个性能项
- 曾经同时尝试过多项激进优化，导致点击设置后卡死或串口无输出：
  - `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240`
  - `CONFIG_FREERTOS_HZ=1000`
  - main task 切到 CPU1
  - i80 PCLK 提到 `30/40MHz`
  - draw buffer 提到 `80/120` 行
  - 给 LVGL 挂 `monitor_cb` 性能统计
- 结论：这些改动不能打包提交；后续必须单变量验证。
- 如果设备点击设置直接卡死，优先回退到 7.2 的稳定配置。

## 7.5 后续开发规范限制
- 每次只改一个性能变量，烧录后必须跑固定路径回归。
- 改 `sdkconfig` 会触发 ESP-IDF reconfigure，可能接近全量编译；命令超时时间至少给 `600000 ms`（10分钟）。
- 普通 `.c` 小改动可以直接 `idf.py build` 增量编译，通常会快很多。
- 烧录后先抓 COM7 串口日志，再主观判断流畅度。
- 若串口出现 `task_wdt`、`backtrace` 或 10 秒完全无输出，立即停止继续优化并回退最近一次变量。
- 不要通过删除 UI 视觉效果来换帧率，除非用户明确允许改界面渲染。

## 7.6 刷新率优化续做记录（2026-04-27）

### 最终实机确认可用配置
- 当前已烧录并通过串口回归的配置：
  - `CONFIG_LV_DISP_DEF_REFR_PERIOD=8`
  - `CONFIG_LV_INDEV_DEF_READ_PERIOD=6`
  - `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240`
  - `CONFIG_FREERTOS_HZ=100`
  - `CONFIG_ESP_MAIN_TASK_AFFINITY=0x0`
  - `LVGL_DRAW_BUF_LINES=120`
  - `LCD_I80_PCLK_HZ=20MHz`
  - `lv_indev_drv_t.scroll_limit=3`
  - `CONFIG_LV_USE_PERF_MONITOR` / `CONFIG_LV_USE_MEM_MONITOR` 最终已关闭，仅 Step 13-16 调试期间临时打开。
- 验证路径：
  - `首页 -> 设置 -> WiFi -> 返回设置 -> 设备状态 -> 返回首页`
  - 多次 `首页 -> 设备状态 -> 返回首页`
  - 多次进入 `设置` 页面，确认不再出现点击设置即卡死。
- 串口重点搜索：
  - `task_wdt`
  - `backtrace`
  - `Guru Meditation`
  - `panic`
  - `abort`
  - `skip destroy active screen`
  - `SCREEN_UNLOADED`
- 当前结论：Step 16 串口关键异常词无命中，用户实机确认设置页滑动跟手感“可以了”。

## 8. 后端 V1 接入记录（2026-04-27）

### 8.1 本次已验证证据
- 工程路径：`D:\ESP32-APP\18_touch`
- 分支：`feat/lvgl-ui-page-framework`
- 构建命令使用 ESP-IDF Python 直调 `idf.py`，因为 `idf_cmd_init.bat` 在本机本次输出过 `'null' is not recognized` 且未把 `idf.py` 放进 PATH。
- 编译通过：
  - `build/18_touch.elf`
  - `build/18_touch.bin`
  - 当前 bin 大小：`0x17ff40`
- 烧录 COM7 通过：
  - `Hash of data verified`
  - `Hard resetting via RTS pin`
- 最终串口日志：
  - `tmp/backend_v1_final_boot_com7.log`
- 已出现关键日志：
  - `APP_BACKEND: backend started`
  - `APP_BACKEND: APP_BACKEND_SELFTEST: store PASS`
  - `UI_BINDING: snapshot applied`
  - `BACKEND_WIFI: scan start`
  - `BACKEND_WIFI: scan done, ap_count=8`
- 异常关键字搜索为空：
  - `task_wdt`
  - `Guru Meditation`
  - `backtrace`
  - `panic`
  - `abort`
  - `skip destroy active screen`
  - `SCREEN_UNLOADED`

### 8.2 新踩坑与处理
- 加入 WiFi/HTTPS/cJSON 后，默认单 App 分区 `0x100000` 装不下，构建报 `app partition is too small`。
  - 处理：切换到工程已有 `partitions-16MiB.csv`，并把 `CONFIG_ESPTOOLPY_FLASHSIZE` 同步为 `16MB`。
  - 验证：分区表显示 factory 为 `0x1f0000`，当前镜像剩余约 23%。
- `idf.py monitor` 超时退出后，COM7 可能短时间被残留 Python/monitor 进程占用。
  - 处理：先查杀残留 monitor/python，再用 pyserial 抓取 30 秒日志。
- NVS 凭据保存时机必须在 `IP_EVENT_STA_GOT_IP` 后。
  - 处理：不要在 `esp_wifi_connect()` 返回后立即保存密码；那只代表连接请求已发出，不代表密码有效。

### 8.3 当前未完成实机闭环
- 这次日志中保存的 `SAT-Guest` 自动连接失败，断开 reason=2，因此还没有出现：
  - `BACKEND_WIFI: got ip <x.x.x.x>`
  - `BACKEND_TIME: SNTP synced`
  - `BACKEND_WEATHER: Open-Meteo updated`
- 下一次需要在 WiFi 页面输入有效 SSID/密码后，再抓 COM7 日志确认 IP、SNTP、天气三段闭环。

### 本轮推进步骤
1. Step 5：曾尝试激进组合，包含 PCLK 提升到 `30MHz`、buffer 提升、触摸周期降低。
   - 结果：点击设置页卡死。
   - 日志：`tmp\step5_aggressive_pclk30_buf80_touch10.log`
   - 结论：组合改动不可作为判断依据，必须拆回单变量。
2. Step 6：仅验证 `LCD_I80_PCLK_HZ 20MHz -> 30MHz`。
   - 结果：仍然点击设置卡死。
   - 日志：`tmp\step6_pclk30_only_verify.log`
   - 回退日志：`tmp\step6_pclk30_failed_revert_pclk20.log`
   - 结论：PCLK 30MHz 是当前板级/屏幕链路风险点，后续不要再优先碰。
3. Step 7：避开 PCLK，只做非 PCLK 激进项组合验证。
   - 结果：未复现设置页卡死。
   - 日志：`tmp\step7_non_pclk_aggressive_touch10_buf80.log`
4. Step 8：仅提升 draw buffer，`LVGL_DRAW_BUF_LINES 80 -> 120`。
   - 结果：编译、烧录、串口验证通过。
   - 日志：`tmp\step8_buf120_touch10_pclk20_verify.log`
5. Step 9：仅提升触摸读取，`CONFIG_LV_INDEV_DEF_READ_PERIOD 10 -> 8`。
   - 结果：编译、烧录、串口验证通过。
   - 日志：`tmp\step9_touch8_buf120_pclk20_verify.log`
6. Step 10：仅提升显示刷新，`CONFIG_LV_DISP_DEF_REFR_PERIOD 16 -> 12`。
   - 结果：编译、烧录、串口验证通过。
   - 日志：`tmp\step10_refr12_touch8_buf120_pclk20_verify.log`
7. Step 11：仅提升显示刷新，`CONFIG_LV_DISP_DEF_REFR_PERIOD 12 -> 10`。
   - 结果：编译、烧录、串口验证通过。
   - 日志：`tmp\step11_refr10_touch8_buf120_pclk20_verify.log`
8. Step 12：仅提升触摸读取，`CONFIG_LV_INDEV_DEF_READ_PERIOD 8 -> 6`。
   - 结果：编译、烧录、串口验证通过。
   - 日志：`tmp\step12_refr10_touch6_buf120_pclk20_verify.log`
9. Step 13：临时打开 LVGL FPS 与内存占用监控显示。
   - 目的：让实机直接观察帧率和 LVGL 内存占用。
   - 结果：编译、烧录、串口验证通过，无异常关键字。
   - 日志：`tmp\step13_lvgl_perf_mem_monitor_com7.log`
   - 最终处理：调试结束后已关闭监控显示，避免遮挡正式 UI。
10. Step 14：仅提升显示刷新，`CONFIG_LV_DISP_DEF_REFR_PERIOD 10 -> 8`。
   - 结果：编译、烧录、串口验证通过，无异常关键字。
   - 日志：`tmp\step14_refr8_perf_mem_monitor_com7.log`
   - 结论：页面切换体感已可接受，保留 `8ms`。
11. Step 15：尝试降低 CHSC5xxx 空闲触摸扫描限流，`t % 5 -> t % 2`。
   - 结果：编译、烧录、串口验证通过，但用户反馈设置页滑动体感变化不明显。
   - 日志：`tmp\step15_touch_scan_throttle2_refr8_com7.log`
   - 结论：该改动主要影响空闲后首次点按，对按住滑动收益不明显，已回退。
12. Step 16：仅降低 LVGL 滚动启动阈值，`lv_indev_drv_t.scroll_limit 10px -> 3px`。
   - 结果：编译、烧录、串口验证通过，无异常关键字。
   - 日志：`tmp\step16_scroll_limit3_refr8_com7.log`
   - 用户反馈：设置页滑动选项栏跟手感可以了。
   - 结论：保留 `scroll_limit=3`，这是针对小屏触摸滚动启动迟滞的有效优化。

### 本轮踩坑与经验
- `LCD_I80_PCLK_HZ=30MHz` 是明确踩坑项：
  - 单独提升 PCLK 也会导致点击设置页卡死。
  - 这说明问题不只是“改太多项”，而是当前 LCD i80 总线/面板/flush 链路对 30MHz 不稳定。
  - 后续若必须再测 PCLK，需要先加更细的 flush 统计、总线错误日志和更长时间压力测试；普通流畅度优化不要优先碰它。
- 提升刷新周期不等于一定肉眼明显变顺：
  - `30ms -> 20ms -> 16ms -> 12ms -> 10ms` 只是提高 LVGL 调度上限。
  - 若页面绘制、flush、动画本身耗时较高，肉眼提升会被瓶颈抵消。
  - 真正有效的判断要同时看用户主观手感、页面跳转路径、串口是否有异常。
- draw buffer 提升到 `120` 行目前可用：
  - 对 flush 分块次数有帮助，但继续加大可能进入内存余量和 DMA/PSRAM 访问风险区。
  - 下一次若继续提升 buffer，必须只改 buffer，并同步观察 heap 日志。
- 触摸读取 `6ms` 当前可用：
  - 能改善点击和滑动跟手感。
  - 继续低于 `6ms` 可能带来无意义轮询和 CPU 占用增加，收益可能变小。
- LVGL 指针滚动启动阈值 `scroll_limit=3` 当前可用：
  - 默认 `10px` 在 2.4 寸屏上会让设置页列表滑动起步显得迟钝。
  - 降到 `3px` 后，用户确认设置页滑动跟手感可以了。
  - 若后续出现轻点误判为滑动，再单独回调到 `5px` 验证，不要同时改触摸周期或刷新周期。
- FreeRTOS tick、task affinity 暂时不要再碰：
  - 之前激进组合里包含过 `CONFIG_FREERTOS_HZ=1000` 和 main task 切核，风险大、收益不直观。
  - 当前稳定路线已经能继续推进刷新/触摸/buffer，不需要先改调度基础。
- 每次优化必须保留“失败日志 + 回退日志”：
  - 失败日志用于确认踩坑边界。
  - 回退日志用于证明回到稳定点后设备确实恢复。
  - 这比只记录成功配置更有价值。

### 当前建议
- 当前版本已经进入相对激进但可运行的状态，不建议继续盲目加速。
- 如果后续还要压榨流畅度，优先顺序建议：
  1. 保持当前 `CONFIG_LV_DISP_DEF_REFR_PERIOD=8` 与 `scroll_limit=3`，先不要继续盲目加速。
  2. 如仍需进一步压榨，只测 `LVGL_DRAW_BUF_LINES 120 -> 160`，但必须先看内存余量。
  3. 不要测 PCLK 30MHz，除非本轮任务目标改成 LCD 总线专项排查。

## 8. 后续智能体执行清单（建议复制到任务模板）
1. 先看本文件 + `LVGL移植_智能体避坑手册.md`。
2. 先在模拟器完成 UI，再迁移到实机。
3. 修改页面跳转逻辑时，优先看 `ui_page_manager.c` 生命周期处理。
4. 每次改动后按固定路径回归：
   - `首页 -> 设置 -> 设备状态 -> 返回首页`
   - `首页 -> 设置 -> WiFi -> 返回`
   - `首页 -> 设置 -> WiFi -> 返回设置 -> 设备状态 -> 返回首页`
5. 若出现复位或卡死，先抓 GDB 栈，不要盲改样式代码。
6. 若有串口条件，优先用当前 COM7 抓实机日志，重点搜索 `task_wdt`、`backtrace`、`skip destroy active screen`、`SCREEN_UNLOADED`。
7. 刷新率优化必须按第 7 节单变量推进；当前最终实机确认配置见 7.6：显示 `8ms`、触摸 `6ms`、CPU `240MHz`、PCLK `20MHz`、buffer `120` 行、滚动阈值 `3px`。

## 9. 结论
- 本次卡死复位已通过“页面销毁时机重构 + active/prev 统一卸载后删除”解决。
- 刷新率与触摸跟手优化已完成 Step 1 至 Step 16，当前稳定进度为 LVGL 显示刷新 `8ms` + 触摸读取 `6ms` + CPU `240MHz` + PCLK `20MHz` + buffer `120` 行 + 滚动阈值 `3px`。
- 编译烧录与调试链路已有稳定 SOP。
- UI显示不完整问题已有可复用布局规范。
- 后续开发请优先复用本手册流程，可显著降低回归风险。

## Backend V1 validation notes - 2026-04-27

These notes record the WiFi/time/weather backend validation so future agents do not repeat the same detours.

- Build command: use the ESP-IDF Python directly on this machine:
  - `D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe`
  - `D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1\tools\idf.py`
- Current successful build artifact:
  - `build/18_touch.elf`
  - `build/18_touch.bin`
  - bin size after switching weather to HTTP provider: `0x16ffe0`
  - factory partition size: `0x1f0000`
  - free app partition space: `0x80020`
- COM7 flash passed:
  - `Hash of data verified`
  - `Hard resetting via RTS pin`
- Captive portal finding:
  - Office WiFi `Stzn-office` can obtain IP `10.10.103.135`, but HTTP connectivity probes returned `302`.
  - Treat this as "IP acquired, but public internet likely blocked by captive portal/auth page".
  - Do not diagnose this as a WiFi password failure or stack problem.
- Hotspot success path:
  - User hotspot SSID: `zhangxiaohou`
  - COM7 log: `BACKEND_WIFI: got ip 172.20.10.14`
  - COM7 log: `BACKEND_NET: network internet probe open`
  - COM7 log: `BACKEND_TIME: SNTP synced 16:36`
  - COM7 log: `BACKEND_WEATHER: Open-Meteo updated`
- NVS credential timing:
  - Save credentials only after `IP_EVENT_STA_GOT_IP`.
  - When switching from one connected AP to another, `esp_wifi_disconnect()` produces `WIFI_REASON_ASSOC_LEAVE`.
  - During intentional reconnect, do not clear pending SSID/password on that reason; otherwise the new hotspot will connect once but not persist after reboot.
  - Reboot persistence proof: `BACKEND_WIFI: connecting saved ssid zhangxiaohou`.
- Weather provider note:
  - HTTPS Open-Meteo validated the certificate on hotspot, but later timed out and was followed by `TG1WDT_SYS_RST`.
  - Backend V1 now uses Open-Meteo HTTP behind `weather_service` to avoid TLS timeout/WDT risk while keeping the provider replaceable.
  - If HTTPS is required later, test it as a separate task with longer watchdog diagnostics instead of mixing it with WiFi bring-up.
- Latest passing logs:
  - `tmp/backend_v1_hotspot_http_weather_com7.log`
  - `tmp/backend_v1_hotspot_saved_reboot_com7.log`
- Latest 60s reboot log had 0 error entries and no `task_wdt`, `Guru Meditation`, `backtrace`, `panic`, `abort`, `stack overflow`, or `SCREEN_UNLOADED`.

## Backend V1 AMap weather and font coverage addendum - 2026-04-27

This section supersedes the earlier Open-Meteo weather-provider note. Open-Meteo, wttr.in, and WAQI were temporary validation paths. The current Backend V1 weather source is AMap Weather WebService.

### Current verified backend state
- Active weather provider:
  - AMap WebService `weatherInfo`
  - fixed city/adcode: `310000` for Shanghai
  - request mode: `extensions=base`, `output=JSON`
  - API key is stored in code as a replaceable Backend V1 constant; do not print the key to serial logs or repeat the full key in docs.
- Current UI mapping:
  - `lives[0].city` -> city label, e.g. `上海市`
  - `lives[0].weather` -> condition label, e.g. `阴`, `多云`
  - `lives[0].temperature` -> `xx℃`
  - `lives[0].humidity` -> `湿度 xx%`
  - `lives[0].winddirection` + `lives[0].windpower` -> wind label
  - `lives[0].reporttime` -> `HH:MM`
  - AQI intentionally remains `AQI --` in Backend V1.
- Current passing build after AMap + font autoscan:
  - `build/18_touch.bin` size: `0x1916a0`
  - factory partition: `0x1f0000`
  - free app partition: `0x5e960` bytes, about 19%.
- Current passing logs:
  - `tmp/amap_weather_com7.log`
  - `tmp/amap_weather_fontfix_com7.log`
  - `tmp/amap_weather_font_autoscan_com7.log`
- COM7 evidence:
  - `BACKEND_WIFI: got ip 172.20.10.14`
  - `BACKEND_TIME: SNTP synced 18:36`
  - `BACKEND_WEATHER: AMap request start`
  - `esp-x509-crt-bundle: Certificate validated`
  - `BACKEND_WEATHER: AMap weather updated`
  - `UI_BINDING: snapshot applied`
- User final visual check:
  - temperature displays normally
  - humidity displays normally
  - wind displays normally
  - city/weather text displays normally
  - WiFi disconnected/connection-info text displays normally

### AMap weather pitfalls
- AMap `windpower` can return `≤3`. The `≤` symbol was not in the generated LVGL font and caused wind display乱码.
  - Current fix: normalize `≤` to ASCII `<=` inside `weather_service.c`.
- AMap city can return `上海市`. Earlier font samples only included `上海`, so `市` caused the city/weather line to乱码.
- AMap weather can return dynamic Chinese condition words not present in old samples, such as `阴`.
- AQI is not provided by the selected AMap weather endpoint; Backend V1 intentionally shows `AQI --` instead of adding another API.

### Font-generation rule for future work
- Before referencing a Chinese string or dynamic provider-returned Chinese text, verify that the active generated font contains the needed glyphs.
- Prefer batching all new strings, then regenerate all font sizes once.
- Current generator is `tools/gen_ui_fonts.py`.
- The generator now scans:
  - `components/UI`
  - `components/AppBackend`
  - explicit weather/provider sample strings
- After modifying UI/backend strings, run:
```powershell
& 'D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe' 'tools\gen_ui_fonts.py'
```
- Quick glyph proof example:
```powershell
Select-String -Path 'components\UI\fonts\generated\ui_font_sc_12.c' -Pattern 'U\+5E02|U\+9634|U\+7B49|U\+5F85|U\+65E0'
```
- Known glyphs added during this round:
  - `市`
  - `阴`
  - `无`
  - `等`
  - `待`
  - weather words including `雨夹雪`, `冰雹`, `霾`, `浮尘`, `扬沙`, `沙尘暴`

### LVGL/backend threading pitfall
- Backend task, WiFi event callback, and HTTP code must not directly mutate LVGL objects.
- A previous WiFi scan/connect freeze was traced to unsafe LVGL async/timer interaction from backend context.
- Current safer pattern:
  - backend updates `backend_store`
  - backend notifies a pending UI refresh
  - LVGL main loop calls the pending processor
  - UI pages apply immutable snapshots in LVGL context
- If clicking scan/connect freezes again, first inspect the UI binding path before blaming WiFi stack size or HTTP.

### Network diagnosis pitfall
- A WiFi that obtains IP can still be unusable for time/weather because of captive portal web authentication.
- Treat these as separate states:
  - WiFi connected with IP
  - internet reachable
  - SNTP synced
  - weather provider reachable
- Do not diagnose captive portal behavior as a wrong password, stack overflow, or ESP-IDF WiFi failure without probe/log evidence.

### Recommended validation for backend changes
1. Build with direct ESP-IDF Python + `idf.py`.
2. Flash COM7 at `460800`.
3. Capture COM7 at `115200`.
4. Search for success markers:
   - `APP_BACKEND: backend started`
   - `BACKEND_WIFI: got ip`
   - `BACKEND_TIME: SNTP synced`
   - `BACKEND_WEATHER: AMap weather updated`
   - `UI_BINDING: snapshot applied`
5. Search for failure markers:
   - `Guru Meditation`
   - `task_wdt`
   - `backtrace`
   - `panic`
   - `abort`
   - `stack overflow`
   - `SCREEN_UNLOADED`

## Backend V1 WiFi state and Home status addendum - 2026-04-27

This section records the WiFi state-machine fixes after AMap weather and font display were confirmed normal on device.

### User-visible issues fixed
- Home page showed `联网天气已同步` even after WiFi was disconnected.
  - Root cause: Home UI used `snapshot->weather.valid` alone. Cached weather stayed valid after disconnect, so the subtitle still claimed an online sync.
  - Fix: Home subtitle now says `联网天气已同步` only when `wifi_state == APP_BACKEND_WIFI_CONNECTED` and weather is valid. Offline state displays `WiFi 已断开`.
- WiFi page appeared to stay in `扫描中` forever after disconnect and scan.
  - Root cause 1: `backend_store_set_scan_results()` updated AP rows and message but did not set `wifi_state` out of `APP_BACKEND_WIFI_SCANNING`.
  - Root cause 2: the same function used `xSemaphoreTake(s_mutex, 0)`. If UI binding was reading the snapshot at the same moment, the scan result update could be skipped silently even though COM7 still logged `scan done`.
  - Fix: scan result commit now waits up to `200ms` for the store mutex and sets `wifi_state = APP_BACKEND_WIFI_IDLE`.

### Verified evidence
- Build passed:
  - `build/18_touch.bin` size: `0x191800`
  - factory partition: `0x1f0000`
  - free app partition: `0x5e800` bytes, about 19%
- COM7 flash passed with hash verification.
- Runtime log:
  - `tmp/wifi_scan_state_fix_com7.log`
- COM7 evidence:
  - `BACKEND_WIFI: scan start`
  - `BACKEND_WIFI: scan done, ap_count=8`
  - `BACKEND_WIFI: disconnect requested, forget_saved=1`
  - `BACKEND_WIFI: disconnected by user`
  - `BACKEND_WIFI: got ip 172.20.10.14`
  - `BACKEND_WEATHER: AMap weather updated`
  - `UI_BINDING: snapshot applied`
- No `Guru Meditation`, `task_wdt`, `panic`, `abort`, `backtrace`, or `SCREEN_UNLOADED` was found in the latest verification log.
- One `SNTP sync failed: ESP_ERR_TIMEOUT` warning appeared, but weather updated afterward. Treat it as a transient SNTP warning unless it repeats consistently.

### New backend state-machine rules
- `SCANNING` must be a short-lived operation state.
- Any successful or failed scan path must leave `SCANNING`.
- A COM7 `scan done` log is not enough proof that UI state changed; verify the snapshot store was actually updated and `UI_BINDING: snapshot applied` followed.
- User-visible backend state commits should not use zero-time mutex waits if dropping the update would leave the UI stale.
- Cached weather validity is not the same as live network connectivity. UI copy should distinguish:
  - WiFi connected and weather valid: `联网天气已同步`
  - WiFi disconnected: `WiFi 已断开`
  - WiFi connected but weather pending/failing: show backend message or `等待联网同步`

### Font note
- The offline text uses `已`, `断`, and `开`.
- These glyphs were checked in:
  - `components/UI/fonts/generated/ui_font_sc_11.c`
  - `components/UI/fonts/generated/ui_font_sc_12.c`
  - `components/UI/fonts/generated/ui_font_sc_14.c`
- No font regeneration was required for this fix.

## OneNET OTA validation notes - 2026-04-28

This section records the ESP32-S3 online OTA work that was verified by the user after Backend V1.

### Current verified OTA state
- Project name is now `ESP32S3-AIroot`.
- OneNET product:
  - product name: `ESP32S3-AIroot`
  - product ID: `592ks3TWbW`
  - protocol: MQTT
  - data protocol: OneJson
- OneNET device:
  - device name: `ESP32S3-AIroot-001`
- Partition table is now standard OTA layout:
  - `otadata` at `0xd000`, size `0x2000`
  - `ota_0` at `0x10000`, size `0x400000`
  - `ota_1` at `0x410000`, size `0x400000`
- User confirmed OTA is actually implemented and working.

### OTA success evidence
- OneNET task detected:
  - `APP_OTA: update available target=1.2.8 tid=1373763 size=1691760 ...`
- User confirmation path reached:
  - UI prompt title: `固件升级`
  - buttons: `更新`, `稍后`
- Download and partition write reached:
  - `APP_OTA: download start ... partition=ota_1`
- Apply and reboot reached:
  - `APP_OTA: OTA ready; rebooting to 1.2.8`
- Rebooted into the new partition:
  - boot log loaded app from offset `0x410000`
  - boot log showed `App version: 1.2.8`
- Version report after reboot:
  - `APP_OTA: version reported 1.2.8`
  - `APP_OTA: no OTA task`

### OTA implementation pitfalls
- MQTT is not the firmware download transport in this project. MQTT is used for OneNET connectivity/status/inform reply; version report, task check, and package download use OneNET `fuse-ota` HTTPS APIs.
- Do not start all HTTPS work at once after IP. Weather HTTPS and OTA HTTPS can contend for TLS memory. Current order is MQTT -> SNTP -> weather -> OTA check.
- Keep OneNET upgrade package names short and ASCII-only. `ota128.bin` and `ota129.bin` are safe examples.
- Each OTA test must increase `CONFIG_APP_PROJECT_VER`. Otherwise the device will keep reporting the old version even if the binary filename changes.
- If the screen shows an old version but serial shows a new `App version`, inspect UI binding before assuming OTA failed.
- The status page previously hardcoded `v1.2.7`; the fix now uses `snapshot->ota.current_version`.
- Do not print or document full OneNET keys. Current test credentials are present in `sdkconfig`; sanitize before committing publicly.

### Current follow-up package
- The UI version-display fix is built as version `1.2.9`.
- Build output: `build/ESP32S3-AIroot.bin`.
- Short OTA package copy: `tmp/ota129.bin`.
- MD5: `835423FD1F3B20852F562B9061C9F2AC`.
- Size: `1691952` bytes.

### Future-agent rule
For OTA bugs, always compare these three sources before editing:

1. Boot log `App version`.
2. OneNET reported version log `APP_OTA: version reported`.
3. Status page `固件版本` label.

If 1 and 2 match but 3 is wrong, this is a UI snapshot/binding problem, not an OTA problem.
