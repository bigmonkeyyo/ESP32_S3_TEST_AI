# LVGL 页面管理框架设计（ESP32-S3）

- 日期：2026-04-24
- 项目：`ESP32_S3_TEST_AI`
- 目标：建立一套结构清晰、可扩展、易维护的 LVGL 页面管理框架，支持后续快速新增页面。

## 1. 背景与目标
当前工程已具备 LVGL 运行能力，但页面管理尚未形成统一框架。随着页面数量增长，若缺少统一接口与生命周期约束，会出现耦合升高、维护成本增加、切换逻辑分散等问题。

本设计目标：
1. 采用 `Screen` 级页面切换，确保页面隔离和结构清晰。
2. 采用 `push/pop` 导航栈，支持自然返回路径。
3. 生命周期采用混合策略：核心页常驻、次级页按需销毁。
4. 新增页面流程标准化，做到“新增文件 + 注册一行”即可接入。

## 2. 关键决策
1. 页面切换机制：`lv_scr_load_anim`（`Screen` 级切换）
2. 导航模型：`push/pop` 返回栈
3. 生命周期策略：混合缓存（核心页 `KEEP`，次级页 `NONE`）
4. 架构方案：描述符注册 + 页面路由器（推荐方案）

## 3. 总体架构
采用四层结构：

1. `app` 层
- 职责：系统启动、初始化调用。
- 边界：不直接写页面业务逻辑。

2. `ui_core` 层
- 职责：页面注册、栈管理、生命周期管理、统一切换动画。
- 边界：不感知页面具体业务细节。

3. `pages` 层
- 职责：各页面 UI 构建与页面内交互。
- 边界：不维护全局栈，不直接依赖其他页面实现。

4. `common_ui` 层
- 职责：可复用组件（标题栏、通用弹窗、公共样式）。
- 边界：仅提供复用能力，不承担路由职责。

## 4. 目录设计
新增 `components/UI` 组件，结构如下：

```text
components/UI/
  CMakeLists.txt
  include/
    ui.h
    ui_types.h
    ui_page_ids.h
    ui_page_iface.h
    ui_page_manager.h
    ui_page_registry.h
  core/
    ui.c
    ui_page_manager.c
    ui_page_stack.c
    ui_page_registry.c
    ui_page_anim.c
  pages/
    page_home.c
    page_home.h
    page_settings.c
    page_settings.h
    page_about.c
    page_about.h
  widgets/
    ui_topbar.c
    ui_topbar.h
    ui_dialog.c
    ui_dialog.h
```

现有工程最小改动点：
1. `main/main.c`：在 LVGL 初始化后调用 `ui_init()`。
2. `main/CMakeLists.txt`：`REQUIRES` 增加 `UI`。
3. `components/UI/CMakeLists.txt`：声明对 `lvgl` 等依赖。

## 5. 页面接口规范
每个页面必须通过统一描述符接入：

```c
typedef enum {
    UI_PAGE_CACHE_NONE = 0,
    UI_PAGE_CACHE_KEEP,
} ui_page_cache_mode_t;

typedef struct ui_page {
    ui_page_id_t id;
    const char *name;
    ui_page_cache_mode_t cache_mode;
    lv_obj_t *(*create)(void);
    void (*on_show)(void *args);
    void (*on_hide)(void);
    void (*on_destroy)(void);
} ui_page_t;
```

说明：
1. `create` 返回页面根 `screen` 对象。
2. `on_show` 接收外部参数（可为空）。
3. `on_hide` 在页面切走时执行。
4. `on_destroy` 仅在销毁策略命中时执行。

## 6. 页面管理器 API
业务层仅调用管理器，不直接操作栈：

```c
esp_err_t ui_init(void);
esp_err_t ui_page_push(ui_page_id_t id, void *args);
esp_err_t ui_page_pop(void);
esp_err_t ui_page_replace(ui_page_id_t id, void *args);
esp_err_t ui_page_back_to_root(void);
ui_page_id_t ui_page_current(void);
```

行为约束：
1. `push`：进入新页，当前页保留在栈中。
2. `pop`：返回上一页；根页不可继续 `pop`。
3. `replace`：当前页出栈并以新页替换。
4. `back_to_root`：回到根页并清理中间层。

## 7. 生命周期与缓存策略
- 核心页（如 `HOME`）使用 `UI_PAGE_CACHE_KEEP`。
- 次级页（如 `ABOUT/DETAIL`）使用 `UI_PAGE_CACHE_NONE`。

切换流程：
1. 目标页不存在时创建。
2. 目标页存在且可复用时直接加载。
3. 当前页切出时执行 `on_hide`。
4. 若当前页为 `CACHE_NONE` 且退出路径命中销毁，执行 `on_destroy + lv_obj_del`。

## 8. 新增页面标准流程
1. 新建 `page_xxx.c/.h`，实现页面描述符。
2. 在 `ui_page_ids.h` 增加 `UI_PAGE_XXX`。
3. 在 `ui_page_registry.c` 注册一条映射。
4. 在事件中调用 `ui_page_push(UI_PAGE_XXX, args)`。

## 9. 命名与边界规范
命名：
1. 文件：`page_<name>.c/.h`
2. 页面描述符：`g_page_<name>`
3. 页面根对象静态变量：`s_screen`
4. 页面内部事件函数：`<name>_on_<event>`

边界：
1. 页面间不直接 include 对方头文件。
2. 页面切换只通过 `ui_page_manager`。
3. 公共组件放入 `widgets`，页面内不复制实现。

## 10. 错误处理策略
所有管理器 API 返回 `esp_err_t`，禁止静默失败。

标准错误码：
1. `ESP_ERR_NOT_FOUND`：页面未注册
2. `ESP_ERR_INVALID_STATE`：非法状态（如空栈 pop）
3. `ESP_ERR_NO_MEM`：内存不足导致页面创建失败

回滚要求：
1. `push` 失败：保持当前页不变
2. `replace` 失败：恢复旧页
3. `pop` 失败：保持当前页可见

## 11. 日志与调试
日志 TAG：
1. `UI_MGR`
2. `UI_REG`
3. `UI_PAGE_<NAME>`

标准切换日志样式：
`[UI_MGR] PUSH HOME -> SETTINGS (cache=none, depth=2)`

调试能力：
1. 提供 `ui_page_dump_stack()` 输出当前栈。
2. 提供 `UI_DEBUG_LOG` 配置开关，发布版本可关闭。

## 12. 测试与验收标准（V1）
功能验收：
1. 连续 `push/pop` 50 次，无崩溃、无黑屏。
2. 缓存页返回后状态保持。
3. 非缓存页返回后页面重建。
4. 未注册页面跳转返回正确错误码并打印日志。

内存验收：
1. 记录最小剩余堆与峰值。
2. 重复切换无持续泄漏趋势。

基础代码保障：
1. 注册表完整性检查。
2. 栈边界检查（空栈、深度上限）。

## 13. 分阶段实施建议
1. 阶段一：落地 `ui_core`（manager/stack/registry/anim）。
2. 阶段二：迁移已有示例页为 `page_home`。
3. 阶段三：新增两个演示页验证 `push/pop/replace`。
4. 阶段四：补充日志、栈导出、压力切换验证。

## 14. 非目标（V1 不做）
1. 多任务并发页面调度。
2. 跨设备主题系统。
3. 动态页面插件热插拔。

以上非目标用于控制范围，确保先把页面框架稳定落地。
