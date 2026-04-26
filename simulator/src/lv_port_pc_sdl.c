#include "lv_port_pc_sdl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "lvgl.h"

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static uint32_t *s_framebuffer = NULL;
static lv_color_t *s_draw_buf_mem = NULL;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_t *s_disp = NULL;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;

static int s_hor_res = 0;
static int s_ver_res = 0;
static int s_mouse_x = 0;
static int s_mouse_y = 0;
static bool s_quit_requested = false;

static void update_mouse_pos_from_window(int window_x, int window_y)
{
    int win_w = 0;
    int win_h = 0;

    s_mouse_x = window_x;
    s_mouse_y = window_y;

    if (s_window != NULL) {
        SDL_GetWindowSize(s_window, &win_w, &win_h);
    }

    if ((s_mouse_x >= s_hor_res || s_mouse_y >= s_ver_res) && (win_w > 0) && (win_h > 0)) {
        s_mouse_x = (window_x * s_hor_res) / win_w;
        s_mouse_y = (window_y * s_ver_res) / win_h;
    }

    if (s_mouse_x < 0) {
        s_mouse_x = 0;
    } else if (s_mouse_x >= s_hor_res) {
        s_mouse_x = s_hor_res - 1;
    }

    if (s_mouse_y < 0) {
        s_mouse_y = 0;
    } else if (s_mouse_y >= s_ver_res) {
        s_mouse_y = s_ver_res - 1;
    }
}

static uint32_t color_to_argb8888(lv_color_t c)
{
    return lv_color_to32(c);
}

static void sdl_present(void)
{
    if ((s_renderer == NULL) || (s_texture == NULL) || (s_framebuffer == NULL)) {
        return;
    }

    SDL_UpdateTexture(s_texture, NULL, s_framebuffer, s_hor_res * (int)sizeof(uint32_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

static void disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2;
    int y2 = area->y2;
    int src_w = area->x2 - area->x1 + 1;
    lv_color_t *src_row = color_p;

    if ((x2 < 0) || (y2 < 0) || (x1 >= s_hor_res) || (y1 >= s_ver_res)) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 >= s_hor_res) {
        x2 = s_hor_res - 1;
    }
    if (y2 >= s_ver_res) {
        y2 = s_ver_res - 1;
    }

    for (int y = area->y1; y <= area->y2; y++) {
        if ((y < y1) || (y > y2)) {
            src_row += src_w;
            continue;
        }

        uint32_t *dst = &s_framebuffer[y * s_hor_res + x1];
        lv_color_t *src = src_row + (x1 - area->x1);
        for (int x = x1; x <= x2; x++) {
            *dst++ = color_to_argb8888(*src++);
        }
        src_row += src_w;
    }

    sdl_present();
    lv_disp_flush_ready(disp_drv);
}

static void mouse_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    int wx = 0;
    int wy = 0;
    uint32_t buttons = 0;

    LV_UNUSED(indev_drv);

    buttons = SDL_GetMouseState(&wx, &wy);
    update_mouse_pos_from_window(wx, wy);

    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state = (buttons & SDL_BUTTON_LMASK) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool lv_port_pc_sdl_init(int hor_res, int ver_res, bool hidden_window)
{
    uint32_t window_flags = (uint32_t)SDL_WINDOW_ALLOW_HIGHDPI;
    uint32_t renderer_flags = (uint32_t)(SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    s_hor_res = hor_res;
    s_ver_res = ver_res;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    if (hidden_window) {
        window_flags |= (uint32_t)SDL_WINDOW_HIDDEN;
    }

    s_window = SDL_CreateWindow("LVGL Simulator 320x240", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        s_hor_res * 2, s_ver_res * 2, window_flags);
    if (s_window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        lv_port_pc_sdl_deinit();
        return false;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, renderer_flags);
    if (s_renderer == NULL) {
        renderer_flags = SDL_RENDERER_SOFTWARE;
        s_renderer = SDL_CreateRenderer(s_window, -1, renderer_flags);
    }
    if (s_renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        lv_port_pc_sdl_deinit();
        return false;
    }

    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, s_hor_res, s_ver_res);
    if (s_texture == NULL) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        lv_port_pc_sdl_deinit();
        return false;
    }

    s_framebuffer = (uint32_t *)calloc((size_t)s_hor_res * (size_t)s_ver_res, sizeof(uint32_t));
    if (s_framebuffer == NULL) {
        fprintf(stderr, "framebuffer alloc failed\n");
        lv_port_pc_sdl_deinit();
        return false;
    }

    s_draw_buf_mem = (lv_color_t *)malloc((size_t)s_hor_res * 40U * sizeof(lv_color_t));
    if (s_draw_buf_mem == NULL) {
        fprintf(stderr, "draw buffer alloc failed\n");
        lv_port_pc_sdl_deinit();
        return false;
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_draw_buf_mem, NULL, (uint32_t)s_hor_res * 40U);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = s_hor_res;
    s_disp_drv.ver_res = s_ver_res;
    s_disp_drv.flush_cb = disp_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp = lv_disp_drv_register(&s_disp_drv);
    if (s_disp == NULL) {
        fprintf(stderr, "lv_disp_drv_register failed\n");
        lv_port_pc_sdl_deinit();
        return false;
    }

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = mouse_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    SDL_RenderSetLogicalSize(s_renderer, s_hor_res, s_ver_res);
    sdl_present();
    return true;
}

void lv_port_pc_sdl_poll_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            s_quit_requested = true;
        } else if (e.type == SDL_MOUSEMOTION) {
            update_mouse_pos_from_window(e.motion.x, e.motion.y);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            update_mouse_pos_from_window(e.button.x, e.button.y);
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            update_mouse_pos_from_window(e.button.x, e.button.y);
        }
    }
}

bool lv_port_pc_sdl_is_quit_requested(void)
{
    return s_quit_requested;
}

bool lv_port_pc_sdl_save_screenshot(const char *path)
{
    SDL_Surface *surface = NULL;
    int ret;

    if ((path == NULL) || (s_framebuffer == NULL)) {
        return false;
    }

    surface = SDL_CreateRGBSurfaceWithFormatFrom((void *)s_framebuffer, s_hor_res, s_ver_res, 32,
        s_hor_res * (int)sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        return false;
    }

    ret = SDL_SaveBMP(surface, path);
    SDL_FreeSurface(surface);
    return ret == 0;
}

void lv_port_pc_sdl_deinit(void)
{
    if (s_texture != NULL) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    if (s_renderer != NULL) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window != NULL) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    if (s_draw_buf_mem != NULL) {
        free(s_draw_buf_mem);
        s_draw_buf_mem = NULL;
    }
    if (s_framebuffer != NULL) {
        free(s_framebuffer);
        s_framebuffer = NULL;
    }
    SDL_Quit();
}
