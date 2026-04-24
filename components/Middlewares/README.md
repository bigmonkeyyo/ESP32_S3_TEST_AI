# Middlewares Layer

This folder stores middleware components that sit between `main/` and `components/BSP/`.

## Current Layout

- `lvgl/`: third-party LVGL core library (v8.3)
- `lvgl_port/`: board-specific LVGL porting layer for display, touch, tick, and basic UI bootstrap

## Layering Rule

- `main/` should call middleware entry APIs only.
- `lvgl_port/` can depend on `BSP` and `lvgl`.
- `BSP` should stay focused on hardware drivers and should not include UI logic.

## Build Note

Project root `CMakeLists.txt` adds middleware components through `EXTRA_COMPONENT_DIRS` with explicit paths (`lvgl` and `lvgl_port`).
