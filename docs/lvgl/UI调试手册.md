# UI调试手册（LVGL / ESP32-S3 / OpenOCD）

## 1. 目标
- 用固定流程快速定位 UI 卡死、复位、白屏、卡顿问题。
- 减少“猜测式修复”，确保每次改动都有证据链。

## 2. 调试总流程（建议严格按顺序）
1. 环境确认：`idf.py`、`openocd-esp32`、`xtensa-esp32s3-elf-gdb` 可用。
2. 产物确认：`build/18_touch.elf` 与 `build/18_touch.bin` 存在。
3. 上板确认：JTAG 可连通、目标可 `reset halt`。
4. 复现问题：明确操作步骤（例如“Home -> Settings -> Back”）。
5. 断点取证：先抓调用链，再改代码。
6. 最小修复：只改根因点，不做无关重构。
7. 回归验证：至少做“高频切页 + 长时运行”。

## 3. 链路打通命令（ESP32-S3）

### 3.1 启动 OpenOCD（ESP-IDF 自带版本）
```powershell
$ocd='D:\esp32\Espressif\tools\openocd-esp32\v0.12.0-esp32-20230921\openocd-esp32\bin\openocd.exe'
$scripts='D:\esp32\Espressif\tools\openocd-esp32\v0.12.0-esp32-20230921\openocd-esp32\share\openocd\scripts'
& $ocd -s $scripts -f interface/esp_usb_jtag.cfg -f target/esp32s3.cfg
```

### 3.2 GDB 连接
```powershell
$gdb='D:\esp32\Espressif\tools\xtensa-esp-elf-gdb\12.1_20221002\xtensa-esp-elf-gdb\bin\xtensa-esp32s3-elf-gdb.exe'
& $gdb build\18_touch.elf
```

GDB 内执行：
```gdb
target remote :3333
monitor reset run
```

## 4. 常用断点模板

### 4.1 页面切换链路
```gdb
break ui_page_push
break ui_page_pop
break ui_page_destroy_if_needed
continue
```

### 4.2 崩溃现场
```gdb
break panic_abort
break _DoubleExceptionVector
commands
  silent
  thread apply all bt
  info registers
  quit
end
continue
```

### 4.3 显示刷新链路
```gdb
break lvgl_disp_flush_cb
break lvgl_notify_flush_ready_cb
continue
```

## 5. 本项目已知高频问题与定位策略

### 5.1 现象：设置页点击返回后卡死/复位
- 触发路径：
- `page_settings_back_cb`
- `ui_page_pop`
- `ui_page_destroy_if_needed`
- 风险点：
- 在切页动画/布局更新窗口内过早删除旧页面对象，导致布局或事件路径访问冲突。
- 现行修复策略：
- 旧页面延后删除（`lv_obj_del_delayed`），删除时机晚于动画窗口。

### 5.2 现象：上电白屏明显
- 检查点：
- 背光是否在首帧完成前已开启
- `flush_ready` 是否由真实传输完成回调触发
- 现行策略：
- 先关背光，首帧完成后开背光，并设置超时兜底。

### 5.3 现象：界面卡顿
- 检查点：
- `flush_ready` 是否过早调用
- 总线频率、双缓冲、任务周期配置
- 现行策略：
- 使用 DMA 传输完成回调通知 + 双缓冲 + 合理总线频率。

## 6. 调试效率规则
- 一次只验证一个假设。
- 每个假设必须对应“可观测证据”（断点、栈、寄存器、日志）。
- 禁止在未复现/未取证时连续堆叠修复。
- 若连续 2 次修复失败，回到复现步骤重新构建证据链。

## 7. 回归验证模板（每次修复后必须执行）
1. 冷启动 10 次：确认无黑屏卡住。
2. `Home -> Settings/About -> Back` 连续 100 次：确认无卡死复位。
3. 动画/高频触控运行 10 分钟：确认无花屏、无明显掉帧。
4. 重新连接 OpenOCD 检查线程状态：确认系统处于稳定运行态。

## 8. 调试记录模板（建议复制使用）
```text
[问题编号]：
[复现步骤]：
[实测现象]：
[断点与证据]：
[根因判断]：
[修复点（文件/函数）]：
[回归结果]：
[未决风险]：
```
