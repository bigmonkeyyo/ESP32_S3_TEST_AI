idf_component_register(
    SRCS
        "core/ui.c"
        "core/ui_page_registry.c"
        "core/ui_page_stack.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
