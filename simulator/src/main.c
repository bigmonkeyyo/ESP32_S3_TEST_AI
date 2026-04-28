#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "SDL.h"
#include "lvgl.h"

#include "lv_port_pc_sdl.h"
#include "ui_page_sim.h"

static void print_usage(const char *app)
{
    printf("Usage: %s [--page <home|settings|status|firmware|ota_ready|ota_progress|ota_complete|ota_failed|wifi>] [--screenshot <bmp_path>]\n", app);
}

static bool parse_page_id(const char *name, ui_page_sim_id_t *out_page)
{
    if ((name == NULL) || (out_page == NULL)) {
        return false;
    }

    if (strcmp(name, "home") == 0) {
        *out_page = UI_PAGE_SIM_HOME;
        return true;
    }
    if (strcmp(name, "settings") == 0) {
        *out_page = UI_PAGE_SIM_SETTINGS;
        return true;
    }
    if (strcmp(name, "status") == 0) {
        *out_page = UI_PAGE_SIM_STATUS;
        return true;
    }
    if (strcmp(name, "firmware") == 0) {
        *out_page = UI_PAGE_SIM_FIRMWARE;
        return true;
    }
    if (strcmp(name, "ota_ready") == 0) {
        *out_page = UI_PAGE_SIM_OTA_READY;
        return true;
    }
    if (strcmp(name, "ota_progress") == 0) {
        *out_page = UI_PAGE_SIM_OTA_PROGRESS;
        return true;
    }
    if (strcmp(name, "ota_complete") == 0) {
        *out_page = UI_PAGE_SIM_OTA_COMPLETE;
        return true;
    }
    if (strcmp(name, "ota_failed") == 0) {
        *out_page = UI_PAGE_SIM_OTA_FAILED;
        return true;
    }
    if (strcmp(name, "wifi") == 0) {
        *out_page = UI_PAGE_SIM_WIFI;
        return true;
    }

    return false;
}

int main(int argc, char **argv)
{
    int i;
    const char *screenshot_path = NULL;
    bool hidden_window = false;
    uint32_t start_tick = 0;
    ui_page_sim_id_t start_page = UI_PAGE_SIM_HOME;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--screenshot") == 0) {
            if ((i + 1) >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            screenshot_path = argv[++i];
            hidden_window = true;
            continue;
        }

        if (strcmp(argv[i], "--page") == 0) {
            if (((i + 1) >= argc) || !parse_page_id(argv[i + 1], &start_page)) {
                print_usage(argv[0]);
                return 1;
            }
            i++;
            continue;
        }

        print_usage(argv[0]);
        return 1;
    }

    lv_init();
    if (!lv_port_pc_sdl_init(320, 240, hidden_window)) {
        return 2;
    }

    ui_page_sim_create();
    if (start_page != UI_PAGE_SIM_HOME) {
        ui_page_sim_open(start_page);
    }

    if (screenshot_path != NULL) {
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
        start_tick = SDL_GetTicks();
        while ((SDL_GetTicks() - start_tick) < 250U) {
            lv_port_pc_sdl_poll_events();
            lv_tick_inc(5);
            lv_timer_handler();
            SDL_Delay(5);
        }

        if (!lv_port_pc_sdl_save_screenshot(screenshot_path)) {
            fprintf(stderr, "screenshot save failed: %s\n", screenshot_path);
            lv_port_pc_sdl_deinit();
            return 3;
        }

        printf("Screenshot saved: %s\n", screenshot_path);
        lv_port_pc_sdl_deinit();
        return 0;
    }

    while (!lv_port_pc_sdl_is_quit_requested()) {
        lv_port_pc_sdl_poll_events();
        lv_tick_inc(5);
        lv_timer_handler();
        SDL_Delay(5);
    }

    lv_port_pc_sdl_deinit();
    return 0;
}
