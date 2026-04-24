# LVGL 移植智能体避坑手册（18_touch）

> 目的：后续任何智能体在本工程做 LVGL 移植/编译/烧录/调试时，先看这份文档，直接规避已踩过的坑。  
> 适用工程：`d:\ESP32-APP\18_touch`

---

## 1. 当前已验证通过的基线

- 已完成 LVGL 接入并可运行（显示+触摸+tick+timer handler）。
- 已成功 `build` + `flash` + 串口看到启动标记：
  - `MAIN: Starting LVGL port demo...`
  - `LVGL_PORT: LVGL started.`
- 目标芯片：`esp32s3`

---

## 2. 本次实际踩过的坑（含症状、根因、解决）

## 坑 1：`idf.py` 命令找不到
- 症状：PowerShell 里直接执行 `idf.py --version` 报 `CommandNotFoundException`。
- 根因：IDF 工具链 PATH 未注入当前 shell。
- 解决：不要依赖系统 `python`，统一用 IDF 虚拟环境 Python + `idf_tools.py export` 注入环境变量（见第 3 节）。

## 坑 2：`build-idf` 脚本 `--detect` 直接失败/无输出
- 症状：`python ...idf_builder.py --detect` 返回码非 0，几乎无有效输出。
- 根因：调用了 Windows Store 的 `python.exe`（不是 IDF venv）。
- 解决：固定使用：
  - `D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe`

## 坑 3：`set-target` 失败并提示 `"cmake" must be available on PATH`
- 症状：`idf_builder.py --set-target esp32s3` 失败，日志包含：
  - `IDF_PYTHON_ENV_PATH is missing`
  - `"cmake" must be available on the PATH`
- 根因：虽然 `IDF_PATH` 有值，但 IDF tools PATH 没被注入，`cmake/ninja/xtensa` 不在 PATH。
- 解决：先执行 `idf_tools.py export --format key-value`，再把返回变量写入当前进程环境（见第 3 节脚本）。

## 坑 4：串口号会漂移（`COM8` 变 `COM7`）
- 症状：刚刚能用的端口突然 `FileNotFound` 或无日志。
- 根因：设备重枚举后 COM 号变化（常见于 reset/flash/monitor 后）。
- 解决：每次烧录前先 `flash-idf --detect` 重新确认端口，禁止硬编码旧 COM。

## 坑 5：串口被占用（`PermissionError(13, 拒绝访问)`）
- 症状：`serial-monitor` 或 pyserial 打不开 COM 口。
- 根因：残留 `esp_idf_monitor` 进程仍占用端口（可能是上次 `flash monitor` 超时后遗留）。
- 解决：先查并清理占用进程，再重试。
  - 关键字：`esp_idf_monitor`、`COMx`

## 坑 6：`idf.py flash monitor` 易超时并留下残留进程
- 症状：命令因超时退出，但 monitor 子进程仍活着。
- 根因：monitor 是长运行交互命令，不适合短超时一次性执行。
- 解决：分两步做：
  1. `flash`
  2. 独立串口脚本抓取固定时长日志

## 坑 7：并行“开串口监控 + 复位脚本”会互相抢占端口
- 症状：一边在监控，一边 pyserial 复位时报拒绝访问。
- 根因：Windows 串口独占。
- 解决：不要并行开两个串口句柄；用单脚本“开口->复位->读日志”。

## 坑 8：日志出现 Flash 大小告警
- 症状：启动日志有：
  - `Detected size(16384k) ... binary image header(2048k)`
- 根因：`sdkconfig` 里 `CONFIG_ESPTOOLPY_FLASHSIZE=2MB`，与实际 16MB 芯片不一致。
- 影响：可运行，但配置不匹配，后续分区/容量利用受限。
- 建议：后续单独做一次 `menuconfig` 调整到 16MB 并复测。

---

## 3. 推荐固定环境引导（PowerShell 直接可用）

> 每次新开 shell 都先跑一次，避免 80% 环境问题。

```powershell
$py  = "D:\ESP-IDF\Espressif\python_env\idf5.2_py3.11_env\Scripts\python.exe"
$idf = "D:\ESP-IDF\Espressif\frameworks\esp-idf-v5.2.1"
$kv  = & $py "$idf\tools\idf_tools.py" export --format key-value
foreach($line in $kv){
  if($line -match "^([^=]+)=(.*)$"){
    $k=$matches[1]; $v=$matches[2]
    if($k -eq "PATH"){ $env:PATH = $v.Replace("%PATH%", $env:PATH) }
    else { Set-Item -Path "Env:$k" -Value $v }
  }
}
& $py "$idf\tools\idf.py" --version
```

---

## 4. 推荐 SOP（后续智能体照此执行）

1. 环境注入（第 3 节脚本）。
2. `set-target esp32s3`（仅首次或 target 变化时）。
3. `build`。
4. 串口探测（每次都做，防 COM 漂移）。
5. `flash`（明确 `--port`）。
6. 单脚本串口验证：读取 boot + app 日志，检查关键标记。

---

## 5. 快速验收标准（必须同时满足）

- 编译成功，且产物存在：
  - `build\18_touch.bin`
  - `build\18_touch.elf`
- 烧录成功（无连接错误）。
- 串口启动日志中至少出现：
  - `Starting LVGL port demo...`
  - `LVGL started.`
- 屏幕有 UI，触摸有响应（建议点击计数按钮）。

---

## 6. 关键文件位置（移植相关）

- 工程入口：`main/main.c`
- LVGL 移植：`main/APP/lvgl_demo.c`、`main/APP/lvgl_demo.h`
- 主 CMake：`CMakeLists.txt`（含 `lvgl-release-v8.3` 组件目录）
- main CMake：`main/CMakeLists.txt`
- LVGL 源码：`lvgl-release-v8.3/`

---

## 7. 额外注意

- 本工程目录不是 git 仓库（`git status` 会失败），不要依赖 git 回滚。
- 串口调试优先“短时抓日志+关键字校验”，避免长时间 monitor 占资源。
- 若后续打开 LVGL 官方 demo（如 `lv_demo_music`），记得同步启用对应字体和 demo 配置，否则编译会报符号/字体缺失。
