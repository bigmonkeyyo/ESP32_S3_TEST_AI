idf_component_register(
    SRCS
        "core/ui.c"
        "core/ui_page_anim.c"
        "core/ui_page_manager.c"
        "core/ui_page_registry.c"
        "core/ui_page_stack.c"
        "pages/page_home.c"
        "pages/page_settings.c"
        "pages/page_about.c"
    INCLUDE_DIRS
        "include"
        "pages"
    REQUIRES lvgl
)
