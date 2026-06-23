#pragma once

#include "gui/core/gui_widget.h"

typedef struct {
    gui_widget_t base;
    const gui_point_t *points;
    uint16_t point_count;
    uint8_t stroke_width;
    gui_color_t color;
} gui_freehand_t;

void gui_freehand_init(gui_freehand_t *freehand);
void gui_freehand_set_points(gui_freehand_t *freehand, const gui_point_t *points, uint16_t point_count);
void gui_freehand_set_style(gui_freehand_t *freehand, gui_color_t color, uint8_t stroke_width);
