# LVGL Page Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a modular LVGL page management framework with `Screen`-level routing, `push/pop` navigation stack, and mixed page lifetime policy for easy long-term expansion.

**Architecture:** Add a new `components/UI` module that owns page IDs, page interfaces, registry, stack, routing, and page animations. Pages are isolated in `components/UI/pages` and only interact through `ui_page_manager` APIs. Navigation and lifecycle logs are centralized for debugging and maintenance.

**Tech Stack:** ESP-IDF v5.1.2, CMake (`idf_component_register`), LVGL, FreeRTOS, ESP logging (`ESP_LOGx`), Unity-style assertions for in-firmware sanity checks.

---

## File Structure Map

- Create: `components/UI/CMakeLists.txt`
- Create: `components/UI/include/ui.h`
- Create: `components/UI/include/ui_types.h`
- Create: `components/UI/include/ui_page_ids.h`
- Create: `components/UI/include/ui_page_iface.h`
- Create: `components/UI/include/ui_page_registry.h`
- Create: `components/UI/include/ui_page_manager.h`
- Create: `components/UI/core/ui.c`
- Create: `components/UI/core/ui_page_stack.c`
- Create: `components/UI/core/ui_page_registry.c`
- Create: `components/UI/core/ui_page_anim.c`
- Create: `components/UI/core/ui_page_manager.c`
- Create: `components/UI/pages/page_home.h`
- Create: `components/UI/pages/page_home.c`
- Create: `components/UI/pages/page_settings.h`
- Create: `components/UI/pages/page_settings.c`
- Create: `components/UI/pages/page_about.h`
- Create: `components/UI/pages/page_about.c`
- Modify: `main/main.c`
- Modify: `main/CMakeLists.txt`
- Modify: `components/Middlewares/lvgl_port/lvgl_port.c`

Design responsibility boundaries:
- `ui_types.h`: enums and shared constants.
- `ui_page_iface.h`: page descriptor contract.
- `ui_page_registry.*`: page ID -> descriptor map.
- `ui_page_stack.c`: bounded stack state + bounds checks.
- `ui_page_anim.c`: animation policy translation.
- `ui_page_manager.c`: push/pop/replace/back-to-root orchestration.
- `page_*.c`: business page content only.

---

### Task 1: Create UI Component Skeleton and Build Wiring

**Files:**
- Create: `components/UI/CMakeLists.txt`
- Create: `components/UI/include/ui.h`
- Create: `components/UI/core/ui.c`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Write the failing integration expectation**

```c
/* main/CMakeLists.txt (intent):
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES BSP lvgl_port nvs_flash UI
)
*/
```

- [ ] **Step 2: Run build to verify it fails before UI component exists**

Run: `idf.py build`
Expected: FAIL with component resolution error containing `UI` not found.

- [ ] **Step 3: Create minimal UI component and public init API**

```cmake
# components/UI/CMakeLists.txt
idf_component_register(
    SRCS "core/ui.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
```

```c
// components/UI/include/ui.h
#pragma once
#include "esp_err.h"

esp_err_t ui_init(void);
```

```c
// components/UI/core/ui.c
#include "ui.h"
#include "esp_err.h"

esp_err_t ui_init(void)
{
    return ESP_OK;
}
```

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES BSP lvgl_port nvs_flash UI
)
```

- [ ] **Step 4: Run build to verify skeleton passes**

Run: `idf.py build`
Expected: PASS and generated app artifacts in `build/`.

- [ ] **Step 5: Commit**

```bash
git add components/UI/CMakeLists.txt components/UI/include/ui.h components/UI/core/ui.c main/CMakeLists.txt
git commit -m "feat(ui): scaffold UI component and init entry"
```

---

### Task 2: Define Page IDs and Page Interface Contracts

**Files:**
- Create: `components/UI/include/ui_types.h`
- Create: `components/UI/include/ui_page_ids.h`
- Create: `components/UI/include/ui_page_iface.h`
- Modify: `components/UI/include/ui.h`

- [ ] **Step 1: Write failing compile expectation by referencing missing page ID type**

```c
// components/UI/include/ui.h (temporary intent)
// ui_page_id_t ui_page_current(void); // should fail before type exists
```

- [ ] **Step 2: Run build to verify missing type failure**

Run: `idf.py build`
Expected: FAIL with unknown type name `ui_page_id_t`.

- [ ] **Step 3: Implement types, IDs, and descriptor contract**

```c
// components/UI/include/ui_types.h
#pragma once

typedef enum {
    UI_PAGE_CACHE_NONE = 0,
    UI_PAGE_CACHE_KEEP,
} ui_page_cache_mode_t;

typedef enum {
    UI_ANIM_NONE = 0,
    UI_ANIM_MOVE_LEFT,
    UI_ANIM_MOVE_RIGHT,
    UI_ANIM_FADE,
} ui_anim_t;
```

```c
// components/UI/include/ui_page_ids.h
#pragma once

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_SETTINGS,
    UI_PAGE_ABOUT,
    UI_PAGE_MAX,
} ui_page_id_t;
```

```c
// components/UI/include/ui_page_iface.h
#pragma once

#include "lvgl.h"
#include "ui_page_ids.h"
#include "ui_types.h"

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

```c
// components/UI/include/ui.h
#pragma once
#include "esp_err.h"
#include "ui_page_ids.h"

esp_err_t ui_init(void);
ui_page_id_t ui_page_current(void);
```

- [ ] **Step 4: Run build to verify contracts compile**

Run: `idf.py build`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add components/UI/include/ui_types.h components/UI/include/ui_page_ids.h components/UI/include/ui_page_iface.h components/UI/include/ui.h
git commit -m "feat(ui): define page ids and page descriptor interface"
```

---

### Task 3: Implement Registry and Stack Core (with Defensive Checks)

**Files:**
- Create: `components/UI/include/ui_page_registry.h`
- Create: `components/UI/core/ui_page_registry.c`
- Create: `components/UI/core/ui_page_stack.c`
- Modify: `components/UI/CMakeLists.txt`

- [ ] **Step 1: Write failing usage snippet for registry/stack APIs**

```c
/* target APIs used by manager task:
const ui_page_t *ui_page_registry_get(ui_page_id_t id);
esp_err_t ui_page_stack_push(ui_page_id_t id);
esp_err_t ui_page_stack_pop(ui_page_id_t *out_id);
*/
```

- [ ] **Step 2: Run build to verify unresolved symbol failures**

Run: `idf.py build`
Expected: FAIL with undefined references to registry/stack APIs.

- [ ] **Step 3: Add registry and stack implementation**

```c
// components/UI/include/ui_page_registry.h
#pragma once
#include "ui_page_iface.h"

const ui_page_t *ui_page_registry_get(ui_page_id_t id);
const ui_page_t *ui_page_registry_root(void);
```

```c
// components/UI/core/ui_page_stack.c
#include "esp_err.h"
#include "ui_page_ids.h"

#define UI_PAGE_STACK_MAX_DEPTH 8

static ui_page_id_t s_stack[UI_PAGE_STACK_MAX_DEPTH];
static int s_top = -1;

esp_err_t ui_page_stack_reset(void)
{
    s_top = -1;
    return ESP_OK;
}

esp_err_t ui_page_stack_push(ui_page_id_t id)
{
    if (s_top >= (UI_PAGE_STACK_MAX_DEPTH - 1)) return ESP_ERR_NO_MEM;
    s_stack[++s_top] = id;
    return ESP_OK;
}

esp_err_t ui_page_stack_pop(ui_page_id_t *out_id)
{
    if (s_top < 0 || out_id == NULL) return ESP_ERR_INVALID_STATE;
    *out_id = s_stack[s_top--];
    return ESP_OK;
}

esp_err_t ui_page_stack_peek(ui_page_id_t *out_id)
{
    if (s_top < 0 || out_id == NULL) return ESP_ERR_INVALID_STATE;
    *out_id = s_stack[s_top];
    return ESP_OK;
}

int ui_page_stack_depth(void)
{
    return s_top + 1;
}
```

```cmake
# components/UI/CMakeLists.txt
idf_component_register(
    SRCS
        "core/ui.c"
        "core/ui_page_registry.c"
        "core/ui_page_stack.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
```

- [ ] **Step 4: Run build to verify core services compile**

Run: `idf.py build`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add components/UI/include/ui_page_registry.h components/UI/core/ui_page_registry.c components/UI/core/ui_page_stack.c components/UI/CMakeLists.txt
git commit -m "feat(ui): add page registry and bounded navigation stack"
```

---

### Task 4: Implement Router/Manager + Lifecycle + Animation Adapter

**Files:**
- Create: `components/UI/include/ui_page_manager.h`
- Create: `components/UI/core/ui_page_anim.c`
- Create: `components/UI/core/ui_page_manager.c`
- Modify: `components/UI/include/ui.h`
- Modify: `components/UI/CMakeLists.txt`

- [ ] **Step 1: Write failing API expectation for routing operations**

```c
// target APIs
esp_err_t ui_page_push(ui_page_id_t id, void *args);
esp_err_t ui_page_pop(void);
esp_err_t ui_page_replace(ui_page_id_t id, void *args);
esp_err_t ui_page_back_to_root(void);
```

- [ ] **Step 2: Run build to verify routing APIs are missing**

Run: `idf.py build`
Expected: FAIL with undefined references for `ui_page_push/pop/...`.

- [ ] **Step 3: Implement manager and animation adapter**

```c
// components/UI/include/ui_page_manager.h
#pragma once
#include "esp_err.h"
#include "ui_page_ids.h"

esp_err_t ui_page_push(ui_page_id_t id, void *args);
esp_err_t ui_page_pop(void);
esp_err_t ui_page_replace(ui_page_id_t id, void *args);
esp_err_t ui_page_back_to_root(void);
ui_page_id_t ui_page_current(void);
void ui_page_dump_stack(void);
```

```c
// components/UI/core/ui_page_anim.c
#include "lvgl.h"
#include "ui_types.h"

lv_scr_load_anim_t ui_anim_to_lv(ui_anim_t anim)
{
    switch (anim) {
        case UI_ANIM_MOVE_LEFT: return LV_SCR_LOAD_ANIM_MOVE_LEFT;
        case UI_ANIM_MOVE_RIGHT: return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        case UI_ANIM_FADE: return LV_SCR_LOAD_ANIM_FADE_ON;
        case UI_ANIM_NONE:
        default: return LV_SCR_LOAD_ANIM_NONE;
    }
}
```

```c
// components/UI/core/ui_page_manager.c (core logic outline)
// - keep current page id
// - call registry get
// - execute create/on_show/on_hide/on_destroy by cache mode
// - perform lv_scr_load_anim for screen transition
// - maintain stack with push/pop/replace/back_to_root
// - print standard logs: [UI_MGR] ... (depth=...)
```

```c
// components/UI/include/ui.h
#pragma once
#include "esp_err.h"
#include "ui_page_manager.h"

esp_err_t ui_init(void);
```

```cmake
# components/UI/CMakeLists.txt
idf_component_register(
    SRCS
        "core/ui.c"
        "core/ui_page_registry.c"
        "core/ui_page_stack.c"
        "core/ui_page_anim.c"
        "core/ui_page_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
```

- [ ] **Step 4: Run build to verify manager compiles and links**

Run: `idf.py build`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add components/UI/include/ui_page_manager.h components/UI/core/ui_page_anim.c components/UI/core/ui_page_manager.c components/UI/include/ui.h components/UI/CMakeLists.txt
git commit -m "feat(ui): implement page manager with stack routing and lifecycle"
```

---

### Task 5: Add Three Isolated Pages and Registry Mapping

**Files:**
- Create: `components/UI/pages/page_home.h`
- Create: `components/UI/pages/page_home.c`
- Create: `components/UI/pages/page_settings.h`
- Create: `components/UI/pages/page_settings.c`
- Create: `components/UI/pages/page_about.h`
- Create: `components/UI/pages/page_about.c`
- Modify: `components/UI/core/ui_page_registry.c`
- Modify: `components/UI/CMakeLists.txt`

- [ ] **Step 1: Write failing registry mapping expectation**

```c
// expected mapping
// UI_PAGE_HOME -> g_page_home (CACHE_KEEP)
// UI_PAGE_SETTINGS -> g_page_settings (CACHE_NONE)
// UI_PAGE_ABOUT -> g_page_about (CACHE_NONE)
```

- [ ] **Step 2: Run build to verify unresolved descriptor symbols**

Run: `idf.py build`
Expected: FAIL referencing `g_page_home/g_page_settings/g_page_about`.

- [ ] **Step 3: Implement page descriptors and navigation buttons**

```c
// page_home.c (key behavior)
// - create screen with title "Home"
// - button: Go Settings -> ui_page_push(UI_PAGE_SETTINGS, NULL)
// - button: Go About -> ui_page_push(UI_PAGE_ABOUT, NULL)
// cache mode: UI_PAGE_CACHE_KEEP
```

```c
// page_settings.c (key behavior)
// - create screen with title "Settings"
// - back button -> ui_page_pop()
// cache mode: UI_PAGE_CACHE_NONE
```

```c
// page_about.c (key behavior)
// - create screen with title "About"
// - back button -> ui_page_pop()
// cache mode: UI_PAGE_CACHE_NONE
```

```c
// ui_page_registry.c (mapping)
// static const ui_page_t *s_pages[UI_PAGE_MAX] = {
//   [UI_PAGE_HOME] = &g_page_home,
//   [UI_PAGE_SETTINGS] = &g_page_settings,
//   [UI_PAGE_ABOUT] = &g_page_about,
// };
```

```cmake
# components/UI/CMakeLists.txt
idf_component_register(
    SRCS
        "core/ui.c"
        "core/ui_page_registry.c"
        "core/ui_page_stack.c"
        "core/ui_page_anim.c"
        "core/ui_page_manager.c"
        "pages/page_home.c"
        "pages/page_settings.c"
        "pages/page_about.c"
    INCLUDE_DIRS "include" "pages"
    REQUIRES lvgl
)
```

- [ ] **Step 4: Run build to verify pages and registry pass**

Run: `idf.py build`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add components/UI/pages/page_home.h components/UI/pages/page_home.c components/UI/pages/page_settings.h components/UI/pages/page_settings.c components/UI/pages/page_about.h components/UI/pages/page_about.c components/UI/core/ui_page_registry.c components/UI/CMakeLists.txt
git commit -m "feat(ui): add home/settings/about pages and registry bindings"
```

---

### Task 6: Integrate Startup Flow and Runtime Diagnostics

**Files:**
- Modify: `components/UI/core/ui.c`
- Modify: `components/Middlewares/lvgl_port/lvgl_port.c`
- Modify: `main/main.c`

- [ ] **Step 1: Write failing startup behavior expectation**

```c
// expected runtime order:
// lvgl init -> ui_init() -> load HOME page -> enter main lvgl loop
```

- [ ] **Step 2: Run firmware and verify missing UI startup behavior**

Run: `idf.py flash monitor`
Expected: UI start logs missing or no route change logs.

- [ ] **Step 3: Wire init and log standards**

```c
// components/UI/core/ui.c
// - reset stack
// - push root page(UI_PAGE_HOME)
// - call ui_page_push(UI_PAGE_HOME, NULL)
```

```c
// components/Middlewares/lvgl_port/lvgl_port.c
#include "ui.h"
// after lvgl_create_ui replacement: call ui_init()
```

```c
// main/main.c
// keep lvgl_port_start() as app startup entry (no direct page create in main)
```

- [ ] **Step 4: Verify runtime navigation + stack behavior**

Run: `idf.py flash monitor`
Expected:
- Logs include `[UI_MGR] PUSH HOME -> SETTINGS ...`
- Back navigation returns to HOME.
- Re-enter SETTINGS reconstructs page (CACHE_NONE).

- [ ] **Step 5: Commit**

```bash
git add components/UI/core/ui.c components/Middlewares/lvgl_port/lvgl_port.c main/main.c
git commit -m "feat(ui): integrate router startup and runtime diagnostics"
```

---

### Task 7: Validation Sweep (Required Review Gate)

**Files:**
- Modify: none (verification-only task unless bug fixes are found)

- [ ] **Step 1: Run clean build verification**

Run: `idf.py fullclean build`
Expected: PASS.

- [ ] **Step 2: Run navigation stress verification**

Run: `idf.py flash monitor`
Expected:
- 50 consecutive push/pop interactions complete without crash.
- No black screen during transitions.
- Stack dump output remains consistent with operations.

- [ ] **Step 3: Run memory stability checks**

Run: `idf.py monitor` and trigger repeated transitions
Expected:
- Heap low-watermark stabilizes.
- No monotonic leak trend across repeated loops.

- [ ] **Step 4: Audit against spec requirements**

Checklist:
- Screen-level transition: implemented
- push/pop stack: implemented
- mixed cache policy: implemented
- page isolation and easy add-page flow: implemented
- standardized logs and error handling: implemented

- [ ] **Step 5: Commit (only if fixes were needed)**

```bash
# Only if Task 7 found issues requiring code changes
git add components/UI main/main.c components/Middlewares/lvgl_port/lvgl_port.c
git commit -m "fix(ui): address validation findings"
```

---

## Subagent Execution Contract (Per User Requirement)

For execution phase, each implementation task uses two subagents:
1. **Implementation subagent**: owns code changes for one task.
2. **QA review subagent**: independent from implementation subagent, must run tests/build and perform code review before task is considered done.

Completion rule per task:
- Implementation subagent returns patch + changed files.
- QA subagent verifies behavior and reports pass/fail + risks.
- Main agent merges only after QA pass.

---

## Self-Review

### 1) Spec Coverage Check
- Architecture split and file boundaries: covered by File Structure Map + Tasks 1-5.
- Screen-level routing + push/pop + replace/back_to_root: covered by Tasks 4 and 6.
- Mixed cache policy: covered by Tasks 4-6.
- Add-page simplicity and registration flow: covered by Task 5.
- Logging, error handling, validation: covered by Tasks 4, 6, 7.

No coverage gaps found.

### 2) Placeholder Scan
Placeholder scan completed; no unresolved placeholders found.

### 3) Type Consistency Check
Consistent names used across tasks:
- `ui_page_id_t`
- `ui_page_t`
- `ui_page_push/pop/replace/back_to_root`
- `UI_PAGE_CACHE_NONE/KEEP`

No naming contradictions found.
