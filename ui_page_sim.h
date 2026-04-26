#ifndef UI_PAGE_SIM_H
#define UI_PAGE_SIM_H

typedef enum {
    UI_PAGE_SIM_HOME = 0,
    UI_PAGE_SIM_SETTINGS,
    UI_PAGE_SIM_STATUS,
    UI_PAGE_SIM_WIFI,
} ui_page_sim_id_t;

void ui_page_sim_create(void);
void ui_page_sim_open(ui_page_sim_id_t page_id);

#endif /* UI_PAGE_SIM_H */
