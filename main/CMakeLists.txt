idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES BSP lvgl_port nvs_flash UI GyroBle
)
