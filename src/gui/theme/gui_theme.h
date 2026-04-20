#pragma once

#include "gui/core/gui_assets.h"
#include "gui/core/gui_types.h"

typedef struct {
    gui_color_t screen_bg;
    gui_color_t screen_grid;
    gui_color_t surface_base;
    gui_color_t surface_raised;
    gui_color_t surface_inverse;
    gui_color_t line_soft;
    gui_color_t line_strong;
    gui_color_t accent;
    gui_color_t accent_soft;
    gui_color_t text_inverse;
    gui_color_t danger;
    gui_color_t white;
    gui_color_t black;
    gui_color_t text_primary;
    gui_color_t text_muted;
    uint8_t border_thin;
    uint8_t border_heavy;
    uint8_t space_xs;
    uint8_t space_sm;
    uint8_t space_md;
    uint8_t space_lg;
    uint8_t title_scale;
    uint8_t value_scale;
    uint8_t button_scale;
    const gui_font_t *font_small;
    const gui_font_t *font_body;
} gui_theme_t;

void gui_theme_init_default(gui_theme_t *theme);
