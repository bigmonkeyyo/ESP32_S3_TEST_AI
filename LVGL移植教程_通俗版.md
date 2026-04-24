# LVGL 移植教程（通俗版，基于 ESP32-S3 / IDF）

本文是对 `LVGL开发指南-ESP32S3 LVGL移植教程.pdf` 的“人话版整理”，目的是让你能快速上手并知道每一步为什么要做。

适用对象：
- 开发板：DNESP32S3 系列（本工程同类）
- 框架：ESP-IDF + FreeRTOS
- LVGL 版本：v8.3（教程对应版本）

---

## 1. 一句话先讲清楚：LVGL 移植到底要做什么

LVGL 移植本质就是 4 件事：
1. 把 LVGL 源码放进工程。
2. 把“显示驱动”注册给 LVGL（告诉它怎么把像素刷到屏幕）。
3. 把“输入驱动”注册给 LVGL（告诉它怎么读触摸/按键）。
4. 给 LVGL 提供时基 + 周期调度（`lv_tick_inc` + `lv_timer_handler`）。

只要这四件事打通，LVGL 就能跑起来。

---

## 2. 实操流程（按教程顺序）

## 2.1 准备 LVGL 源码

- 官方仓库：`https://github.com/lvgl/lvgl/`
- 教程配套用的是 `lvgl-release-v8.3.zip`

源码目录里最关键的是：
- `src/`：核心源码（必须）
- `lvgl.h`：总头文件
- `lv_conf_template.h`：配置模板
- `examples/porting/`：移植参考（可对照）

## 2.2 拷贝进工程

教程做法是：
- 在工程 `components/` 下解压
- 重命名为 `LVGL`

你的当前工程也是这个结构（已有 `lvgl-release-v8.3`），本质等价。

## 2.3 新建移植入口文件

教程建议在 `main/APP/` 下新建：
- `lvgl_demo.c`
- `lvgl_demo.h`

它们的职责：
- `lv_port_disp_init()`：显示初始化与注册
- `lv_port_indev_init()`：输入初始化与注册
- `lvgl_demo()`：统一启动入口

---

## 3. 显示驱动移植（最核心）

显示部分做 8 步：
1. 初始化 LCD（比如 `ltdc_init()`）。
2. 申请绘图缓冲区（`heap_caps_malloc`，建议 DMA 能力）。
3. `lv_disp_draw_buf_init()` 初始化 draw buffer。
4. `lv_disp_drv_init()` 初始化显示驱动结构体。
5. 设置分辨率（`hor_res` / `ver_res`）。
6. 设置刷屏回调 `flush_cb`。
7. 绑定 `draw_buf`。
8. `lv_disp_drv_register()` 注册。

`flush_cb` 做的事很简单：
- 调屏驱把 `color_map` 刷到指定区域
- 刷完必须调用 `lv_disp_flush_ready(drv)`

如果忘记 `lv_disp_flush_ready`，界面会卡死或不刷新。

---

## 4. 输入驱动移植（触摸）

触摸部分做 5 步：
1. 先初始化触摸硬件（`tp_dev.init()`）。
2. `lv_indev_drv_init()` 初始化输入驱动结构体。
3. 设置类型 `LV_INDEV_TYPE_POINTER`。
4. 设置读取回调 `read_cb`（例如 `touchpad_read`）。
5. `lv_indev_drv_register()` 注册到 LVGL。

读取回调的典型写法：
- 先判断是否按下
- 按下就更新坐标和 `LV_INDEV_STATE_PR`
- 松开返回 `LV_INDEV_STATE_REL`
- `data->point` 里始终给最后一次有效坐标

---

## 5. 启动顺序（必须按这个顺序）

在 `lvgl_demo()` 中：
1. `lv_init()`
2. `lv_port_disp_init()`
3. `lv_port_indev_init()`
4. 创建 1ms 定时器，周期调用 `lv_tick_inc(1)`
5. 启动一个 demo（如 `lv_demo_music()`）
6. 循环里周期调用 `lv_timer_handler()`（常见 5~10ms）

最少骨架是：

```c
lv_init();
lv_port_disp_init();
lv_port_indev_init();
// 1ms tick -> lv_tick_inc(1)
while (1) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

---

## 6. menuconfig 里要改什么

教程重点提到 `LVGL Configuration`：
- 字体：开启 demo 用到的字号（否则会报某字号未定义）
- Demo：开启你调用的 demo（否则会报 `lv_demo_xxx` 找不到）

常用重点项：
- `Color depth`：要匹配屏幕色深（常见 RGB565 -> 16bit）
- `Swap the 2 bytes of RGB565 color`：SPI 屏颜色不对时尝试打开
- 内存相关：内存池大小、中间 buffer 数量
- 刷新与输入读取周期：`Default display refresh period`、`Input device read period`
- 自定义 tick source：若用 RTOS 时钟源可启用

---

## 7. 缓冲区怎么选（性能/内存权衡）

LVGL 提供 3 种策略：
- 单缓冲：省内存，刷新性能一般
- 双缓冲：渲染和传输可并行，常用平衡方案
- 全尺寸双缓冲：性能高，但很吃内存

教程示例给出 800x480、16bit、双全屏时内存约：
- `800 * 480 * 2 * 2 = 1,536,000 bytes`（约 1.46MB）

对 MCU 来说通常太大，所以常见做法是“行块双缓冲”（例如 `width * 60` 行）。

---

## 8. 你最容易踩的坑（实战版）

1. `lv_init()` 没先调用就注册显示/输入。
2. `flush_cb` 里漏掉 `lv_disp_flush_ready()`。
3. `lv_timer_handler()` 调度太慢（建议 <= 5~10ms）。
4. demo 没开、字体没开，导致链接或编译报错。
5. 色深不匹配，导致偏色/花屏。
6. 多线程直接调 LVGL API（未加互斥），导致卡顿/显示异常。
7. 在中断里大量调 LVGL API（官方建议尽量避免；`lv_tick_inc`、`lv_disp_flush_ready` 是例外场景）。

---

## 9. 进阶知识（教程第 2 章要点）

## 9.1 初始化流程
- `lv_init` -> 注册显示/输入 -> 提供 tick -> 定时 `lv_timer_handler`

## 9.2 显示接口 API（高频）
- `lv_disp_drv_init`
- `lv_disp_draw_buf_init`
- `lv_disp_drv_register`
- `lv_disp_set_rotation`

## 9.3 输入接口 API（高频）
- `lv_indev_drv_init`
- `lv_indev_drv_register`
- `lv_indev_delete`

## 9.4 低功耗
- 可通过 `lv_disp_get_inactive_time(NULL)` 判断空闲时间
- 空闲后停 tick、进睡眠
- 输入唤醒后补 tick 并恢复调度

## 9.5 OS 线程安全
- LVGL 默认不是线程安全
- 多任务访问 LVGL 必须统一加同一把互斥锁

---

## 10. 给本工程的落地建议

1. 先保证“最小闭环”：
- 能显示一个 label
- 触摸能点到按钮
- 串口无持续报错

2. 再做性能优化：
- 从单缓冲改双缓冲
- 调整 draw buffer 行数（例如 `width * 40/60/80`）对比帧率和内存

3. 再做功能扩展：
- 打开需要的控件/字体/第三方库
- 最后再开大型 demo 或复杂页面

---

## 11. 快速检查清单（拷到项目里就能用）

- [ ] `lv_init()` 已调用
- [ ] 显示驱动已注册（含 `flush_cb`）
- [ ] 输入驱动已注册（`LV_INDEV_TYPE_POINTER`）
- [ ] `lv_tick_inc(1)` 周期 1ms 正常运行
- [ ] `lv_timer_handler()` 周期调度（建议 5~10ms）
- [ ] `LVGL Configuration` 中 demo 和字体已使能
- [ ] 色深与屏幕一致（必要时试 `RGB565 swap`）
- [ ] 多线程访问 LVGL 有互斥保护

---

如果你愿意，我下一步可以基于你这个 `18_touch` 工程，再出一份“只针对当前代码结构”的 `LVGL最小可运行模板.md`，直接对应到现有 `main/`、`components/BSP/` 文件。 
