#ifndef LV_PORT_PC_SDL_H
#define LV_PORT_PC_SDL_H

#include <stdbool.h>

bool lv_port_pc_sdl_init(int hor_res, int ver_res, bool hidden_window);
void lv_port_pc_sdl_poll_events(void);
bool lv_port_pc_sdl_is_quit_requested(void);
bool lv_port_pc_sdl_save_screenshot(const char *path);
void lv_port_pc_sdl_deinit(void);

#endif /* LV_PORT_PC_SDL_H */
