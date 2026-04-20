#pragma once

#include "gui/core/gui_widget.h"

typedef enum {
    GUI_LAYOUT_NONE = 0,
    GUI_LAYOUT_ROW,
    GUI_LAYOUT_COLUMN,
} gui_layout_t;

typedef struct {
    gui_widget_t base;
    gui_layout_t layout;
    gui_color_t background;
    gui_color_t border_color;
    uint8_t border_width;
    uint8_t padding;
    uint8_t gap;
    bool draw_background;
    bool draw_border;
} gui_panel_t;

typedef gui_panel_t gui_screen_t;

void gui_panel_init(gui_panel_t *panel);
void gui_panel_set_layout(gui_panel_t *panel, gui_layout_t layout);
void gui_panel_set_style(gui_panel_t *panel, gui_color_t background, gui_color_t border_color, uint8_t border_width);
void gui_panel_set_chrome(gui_panel_t *panel, bool draw_background, bool draw_border);
void gui_panel_set_spacing(gui_panel_t *panel, uint8_t padding, uint8_t gap);
void gui_screen_init(gui_screen_t *screen);
