# LVGL 中间层结构规范（18_touch）

## 1. 目标

把 LVGL 相关内容从 `main/` 抽离到 `components/Middlewares/`，让工程层次清晰、职责单一、便于后续维护。

## 2. 当前结构

```text
18_touch/
|-- main/
|   |-- main.c
|
|-- components/
|   |-- BSP/
|   |   |-- IIC/
|   |   |-- LCD/
|   |   |-- LED/
|   |   |-- TOUCH/
|   |   `-- XL9555/
|   |
|   `-- Middlewares/
|       |-- lvgl/        (LVGL v8.3 core)
|       `-- lvgl_port/   (board port + LVGL bootstrap)
```

## 3. 分层职责

- `main/`:
  - 仅做系统入口和业务流程编排
  - 通过 `lvgl_port_start()` 启动 UI 中间层

- `components/BSP/`:
  - 只保留硬件驱动能力（LCD、Touch、I2C、IO 扩展等）
  - 不直接承载 UI 业务逻辑

- `components/Middlewares/lvgl/`:
  - 第三方 LVGL 核心源码
  - 尽量不做业务改动，便于升级版本

- `components/Middlewares/lvgl_port/`:
  - LVGL 与板级驱动的适配层
  - 包含显示 flush、触摸 read、tick/timer、基础 UI 启动逻辑

## 4. 构建关系

- 根 `CMakeLists.txt` 通过 `EXTRA_COMPONENT_DIRS` 引入：
  - `components/Middlewares/lvgl`
  - `components/Middlewares/lvgl_port`
- `lvgl_port` 组件依赖：
  - `BSP`
  - `lvgl`
  - `esp_timer`
  - `heap`
- `main` 组件依赖：
  - `BSP`
  - `lvgl_port`
  - `nvs_flash`

## 5. 维护建议

- 新增 UI 页面优先放在 `lvgl_port`（或再拆 `ui_app` 中间层组件），不要回灌到 `main/`。
- 升级 LVGL 版本时，先替换 `components/Middlewares/lvgl/`，再验证 `lvgl_port` 编译与显示触摸行为。
- 若后续 UI 复杂度继续增长，建议把 `lvgl_port` 再分成：
  - `lvgl_hal`（纯端口）
  - `ui_scene`（页面与交互）
