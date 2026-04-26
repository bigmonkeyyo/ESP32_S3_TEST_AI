cmake_minimum_required(VERSION 3.16)
project(lvgl_page_simulator C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h" CACHE STRING "LVGL config path")
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "Use simple lv_conf include")

add_subdirectory("${CMAKE_SOURCE_DIR}/../components/Middlewares/lvgl" "${CMAKE_BINARY_DIR}/lvgl")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(lvgl_sim
    src/main.c
    src/lv_port_pc_sdl.c
    ../ui_page_sim.c
    ../components/UI/fonts/generated/ui_font_sc_11.c
    ../components/UI/fonts/generated/ui_font_sc_12.c
    ../components/UI/fonts/generated/ui_font_sc_13.c
    ../components/UI/fonts/generated/ui_font_sc_14.c
    ../components/UI/fonts/generated/ui_font_sc_18.c
    ../components/UI/fonts/generated/ui_font_sc_24.c
    ../components/UI/fonts/generated/ui_font_sc_30.c
)

target_include_directories(lvgl_sim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/.."
    "${CMAKE_SOURCE_DIR}/../components/UI/include"
    ${SDL2_INCLUDE_DIRS}
)

target_compile_options(lvgl_sim PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(lvgl_sim PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(lvgl_sim PRIVATE lvgl ${SDL2_LIBRARIES})
